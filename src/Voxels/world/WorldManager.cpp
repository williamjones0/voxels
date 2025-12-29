#include "WorldManager.hpp"

#include <fstream>
#include <cmath>

#include "Mesher.hpp"
#include "RunMesher.hpp"
#include "../util/Util.hpp"
#include "tracy/Tracy.hpp"

WorldManager::WorldManager(
    Camera& camera,
    std::function<size_t(size_t)> outOfCapacityCallback,
    const GenerationType generationType,
    const std::filesystem::path& levelFile)
    : generationType(generationType),
      levelFile(levelFile),
      allocator(FreeListAllocator(
          InitialVertexBufferSize,
          4096,
          outOfCapacityCallback
      )),
      camera(camera),
      outOfCapacityCallback(std::move(outOfCapacityCallback))
{
    chunks.reserve(MaxChunks);
    chunkData.resize(MaxChunks);

    std::ranges::fill(palette, glm::vec3());
    palette[0] = glm::vec3(0.278, 0.600, 0.141);
    palette[1] = glm::vec3(0.600, 0.100, 0.100);
    palette[2] = glm::vec3(212.0/255.0, 138.0/255.0, 0);
    palette[3] = glm::vec3(212.0/255.0, 212.0/255.0, 0);
    palette[4] = glm::vec3(99.0/255.0, 212.0/255.0, 0);
    palette[5] = glm::vec3(0.0, 212.0/255.0, 212.0/255.0);
    palette[6] = glm::vec3(0.0, 99.0/255.0, 212.0/255.0);
    palette[7] = glm::vec3(138.0/255.0, 0.0, 212.0/255.0);

    threadPool.start();

    if (generationType == GenerationType::LevelLoad) {
        level.read(levelFile);
    }
}

bool WorldManager::updateFrontierChunks() {
    ZoneScoped;

    destroyFrontierChunks();
    return createNewFrontierChunks();
}

bool WorldManager::createNewFrontierChunks() {
    ZoneScoped;

    std::ranges::sort(frontierChunks, [this](const Chunk* a, const Chunk* b) {
        return squaredDistanceToChunk(a->cx, a->cz) < squaredDistanceToChunk(b->cx, b->cz);
    });

    for (size_t i = 0, s = frontierChunks.size(); i < s; i++) {
        const Chunk* chunk = frontierChunks[i];

        if (chunkTasksCount >= MaxChunkTasks) {
            break;
        }

        if (ensureChunkIfVisible(chunk->cx - 1, chunk->cz) ||
            ensureChunkIfVisible(chunk->cx + 1, chunk->cz) ||
            ensureChunkIfVisible(chunk->cx, chunk->cz - 1) ||
            ensureChunkIfVisible(chunk->cx, chunk->cz + 1)) {
            return true;
        }
    }

    return false;
}

void WorldManager::destroyFrontierChunks() {
    ZoneScoped;

    {
        ZoneScopedN("Iterate over frontier chunks");
        for (size_t i = 0, s = frontierChunks.size(); i < s; i++) {
            Chunk* chunk = frontierChunks[i];
            // If the chunk's still being initialised, don't destroy it yet since this will invalidate references
            // It will be destroyed later on anyway
            if (chunkInRenderDistance(chunk->cx, chunk->cz) || chunk->beingMeshed) {
                continue;
            }

            // Promote neighbours to frontier if necessary and destroy chunk
            const int numPromoted = onFrontierChunkRemoved(chunk);
            frontierChunks.erase(frontierChunks.begin() + i);
            chunkByCoords.erase(key(chunk->cx, chunk->cz));

            // This should only happen if the chunk has already had its region allocated
            if (chunk->bufferRegionAllocated) {
                ZoneScopedN("Deallocate chunk vertices");
                allocator.deallocate(chunk->firstIndex, chunk->numVertices);
                chunk->bufferRegionAllocated = false;
            }

            chunk->destroyed = true;
            chunkData[chunk->index].numVertices = 0;  // Don't render the chunk any more
            s += numPromoted - 1;
            i--;
        }
    }
}

bool WorldManager::ensureChunkIfVisible(const int cx, const int cz) {
    const int maxChunkX = static_cast<int>(std::ceil(static_cast<double>(level.maxX) / ChunkSize)) - 1;
    const int maxChunkZ = static_cast<int>(std::ceil(static_cast<double>(level.maxZ) / ChunkSize)) - 1;

    if (!chunkInRenderDistance(cx, cz) ||
        generationType == GenerationType::LevelLoad && (cx < 0 || cx > maxChunkX || cz < 0 || cz > maxChunkZ)) {
        return false;
    }

    return ensureChunk(cx, cz) != nullptr;
}

Chunk* WorldManager::ensureChunk(const int cx, const int cz) {
    if (chunkByCoords.contains(key(cx, cz))) {
        return nullptr;
    }

    return createChunk(cx, cz);
}

Chunk* WorldManager::createChunk(const int cx, const int cz) {
    ZoneScoped;

    // Find the first free slot in the chunks vector
    size_t index = 0;
    while (index < chunks.size() && !chunks[index]->destroyed) {
        ++index;
    }

    auto* chunk = new Chunk(cx, cz);

    if (index < chunks.size()) {
        chunks[index] = chunk;
    } else {
        chunks.emplace_back(chunk);
    }

    chunk->index = index;
    chunkByCoords[key(cx, cz)] = chunk;
    addFrontier(chunk);
    chunkData[index] = {
            .cx = cx,
            .cz = cz,
            .minY = ChunkHeight,
            .maxY = 0,
            .numVertices = 0,
            .firstIndex = 0,
            ._pad0 = 0,
            ._pad1 = 0,
    };

    ++chunkTasksCount;

    threadPool.queueTask([chunk, this] {
        ZoneScoped;

        Mesher::MeshResult meshResult;

        // No other thread should access the chunk while it's generating
        // Without this lock, block breaking/placing could break the meshing
        {
            std::scoped_lock lock(chunk->mutex);
            chunk->init();
            chunk->generate(generationType, level);

            chunk->beingMeshed = true;
            meshResult = Mesher::meshChunk(chunk);
            // RunMesher runMesher(chunk, generationType);
            // meshResult = runMesher.meshChunk();
            chunk->beingMeshed = false;

            chunk->debug = 1;
        }

        // If pendingMeshResults is currently being iterated through, we need to lock the mutex
        // Additionally, only one thread should be in this critical section at the same time
        {
            std::scoped_lock lock(pendingMeshResultsMutex);
            chunk->debug = 2;
            pendingMeshResults.push_back(std::move(meshResult));
        }
    });

    return chunk;
}

void WorldManager::addFrontier(Chunk* chunk) {
    frontierChunks.push_back(chunk);
    const int cx = chunk->cx;
    const int cz = chunk->cz;

    updateFrontierNeighbour(chunk, cx - 1, cz);
    updateFrontierNeighbour(chunk, cx + 1, cz);
    updateFrontierNeighbour(chunk, cx, cz - 1);
    updateFrontierNeighbour(chunk, cx, cz + 1);
}

void WorldManager::updateFrontierNeighbour(Chunk* frontier, const int cx, const int cz) {
    if (!chunkByCoords.contains(key(cx, cz))) {
        return;
    }

    Chunk* neighbour = chunkByCoords[key(cx, cz)];
    ++neighbour->neighbours;
    ++frontier->neighbours;
    if (neighbour->neighbours == 4) {
        std::erase(frontierChunks, neighbour);
    }
}

int WorldManager::onFrontierChunkRemoved(const Chunk* frontierChunk) {
    const int cx = frontierChunk->cx;
    const int cz = frontierChunk->cz;
    const double d = squaredDistanceToChunk(cx, cz);
    return onFrontierChunkRemoved(cx - 1, cz, d) +
           onFrontierChunkRemoved(cx + 1, cz, d) +
           onFrontierChunkRemoved(cx, cz - 1, d) +
           onFrontierChunkRemoved(cx, cz + 1, d);
}

int WorldManager::onFrontierChunkRemoved(const int cx, const int cz, const double distance) {
    if (!chunkByCoords.contains(key(cx, cz))) {
        return 0;
    }

    Chunk* chunk = chunkByCoords[key(cx, cz)];
    chunk->neighbours--;
    if (std::ranges::find(frontierChunks, chunk) == frontierChunks.end() &&
        (chunkInRenderDistance(cx, cz) ||
         squaredDistanceToChunk(cx, cz) < distance)) {
        frontierChunks.push_back(chunk);
        return 1;
    }
    return 0;
}

bool WorldManager::chunkInRenderDistance(const int cx, const int cz) const {
    return squaredDistanceToChunk(cx, cz) < MaxRenderDistanceMetres * MaxRenderDistanceMetres;
}

double WorldManager::squaredDistanceToChunk(const int cx, const int cz) const {
    const double dx = camera.transform.position.x - (cx + 0.5) * ChunkSize;
    const double dz = camera.transform.position.z - (cz + 0.5) * ChunkSize;
    return dx * dx + dz * dz;
}

size_t WorldManager::key(const int i, const int j) {
    return static_cast<size_t>(i) << 32 | static_cast<unsigned int>(j);
}

void WorldManager::updateVerticesBuffer(const GLuint& verticesBuffer, const GLuint& chunkDataBuffer) {
    ZoneScoped;

    {
        std::scoped_lock lock(pendingMeshResultsMutex);

        for (const Mesher::MeshResult& meshResult : pendingMeshResults) {
            Chunk* chunk = meshResult.chunk;
            const std::vector<uint32_t>& vertices = meshResult.vertices;

            std::scoped_lock chunkLock(chunk->mutex);
            // If the chunk was already destroyed in destroyFrontierChunks, we don't want to allocate, so just skip it
            if (chunk->destroyed) {
                continue;
            }

            // If the chunk wasn't created yet (TODO: somehow?)
            if (chunk->debug == 0) {
                continue;
            }

            // First, free up the chunk's old region in the vertex buffer (if it exists)
            if (chunk->bufferRegionAllocated) {
                allocator.deallocate(chunk->firstIndex, chunk->numVertices);
                chunk->bufferRegionAllocated = false;
            }

            // Update number of vertices
            chunk->numVertices = static_cast<uint32_t>(vertices.size());

            // Now, allocate a new region in the vertex buffer
            Region region{};
            {
                region = allocator.allocate(chunk->numVertices);
                chunk->bufferRegionAllocated = true;
            }

            chunk->firstIndex = region.offset;

            glNamedBufferSubData(verticesBuffer,
                                 region.offset * sizeof(uint32_t),
                                 chunk->numVertices * sizeof(uint32_t),
                                 static_cast<const void*>(vertices.data()));

            // Update chunk data
            const ChunkData cd = {
                    .cx = chunk->cx,
                    .cz = chunk->cz,
                    .minY = chunk->minY,
                    .maxY = chunk->maxY,
                    .numVertices = chunk->numVertices,
                    .firstIndex = chunk->firstIndex,
                    ._pad0 = 0,
                    ._pad1 = 0,
            };
            chunkData[chunk->index] = cd;

            if (chunk->numVertices == 0) {
                std::cerr << "Chunk has no vertices!" << std::endl;
            }

            chunk->debug = 3;
        }

        // Update chunk data buffer
        glNamedBufferData(chunkDataBuffer, sizeof(ChunkData) * chunkData.size(),
                          static_cast<const void*>(chunkData.data()), GL_DYNAMIC_DRAW);

        // Reset vector ready for next update
        pendingMeshResults.clear();
    }
}

Chunk* WorldManager::getChunk(const int cx, const int cz) {
    if (const auto it = chunkByCoords.find(key(cx, cz)); it != chunkByCoords.end()) {
        return it->second;
    }
    return nullptr;
}

void WorldManager::queueMeshChunk(Chunk* chunk) {
    threadPool.queueTask([chunk, this] {
        Mesher::MeshResult meshResult;
        {
            std::scoped_lock lock(chunk->mutex);
            chunk->beingMeshed = true;
            meshResult = Mesher::meshChunk(chunk);
            // RunMesher runMesher(chunk, generationType);
            // meshResult = runMesher.meshChunk();
            chunk->beingMeshed = false;
        }

        // If newMeshResults is currently being iterated through, we need to wait
        {
            std::scoped_lock lock(pendingMeshResultsMutex);
            pendingMeshResults.push_back(meshResult);
        }
    });
}

void WorldManager::save() {
    level.data.clear();
    level.data.reserve(level.maxX * level.maxZ * level.maxY);
    for (int y = 0; y < level.maxY; ++y) {
        for (int z = 0; z < level.maxZ; ++z) {
            for (int x = 0; x < level.maxX; ++x) {
                int voxel = load(x, y, z);
                level.data.push_back(voxel);
            }
        }
    }
    level.save(levelFile);

    std::cout << "Level saved to " << levelFile << std::endl;
}

int WorldManager::load(const int x, const int y, const int z) {
    const int cx = x >> ChunkSizeShift;
    const int cz = z >> ChunkSizeShift;

    const auto it = chunkByCoords.find(key(cx, cz));
    if (it == chunkByCoords.end() || !it->second || !it->second->bufferRegionAllocated) {
        return 0;
    }
    const Chunk* chunk = it->second;

    const int lx = x - (cx << ChunkSizeShift);
    const int lz = z - (cz << ChunkSizeShift);

    return chunk->voxels[getVoxelIndex(lx + 1, y + 1, lz + 1, ChunkSize + 2)];
}

std::optional<RaycastResult> WorldManager::raycast() {
    constexpr float big = 1e30f;

    const float ox = camera.transform.position.x;
    const float oy = camera.transform.position.y - 1;
    const float oz = camera.transform.position.z;

    const float dx = camera.front.x;
    const float dy = camera.front.y;
    const float dz = camera.front.z;

    int px = static_cast<int>(std::floor(ox));
    int py = static_cast<int>(std::floor(oy));
    int pz = static_cast<int>(std::floor(oz));

    const float dxi = 1.0f / dx;
    const float dyi = 1.0f / dy;
    const float dzi = 1.0f / dz;

    const int sx = dx > 0 ? 1 : -1;
    const int sy = dy > 0 ? 1 : -1;
    const int sz = dz > 0 ? 1 : -1;

    const float dtx = std::min(dxi * sx, big);
    const float dty = std::min(dyi * sy, big);
    const float dtz = std::min(dzi * sz, big);

    float tx = abs((px + std::max(sx, 0) - ox) * dxi);
    float ty = abs((py + std::max(sy, 0) - oy) * dyi);
    float tz = abs((pz + std::max(sz, 0) - oz) * dzi);

    constexpr int maxSteps = 16;

    int cmpx = 0, cmpy = 0, cmpz = 0;

    int faceHit = -1;

    auto step = [](const float edge, const float x) -> int {
        return x < edge ? 0 : 1;
    };

    for (int i = 0; i < maxSteps && py >= 0; i++) {
        if (i > 0 && py < ChunkHeight) {
            const int cx = std::floor(static_cast<float>(px) / ChunkSize);
            const int cz = std::floor(static_cast<float>(pz) / ChunkSize);

            const int localX = (px % ChunkSize + ChunkSize) % ChunkSize;
            const int localZ = (pz % ChunkSize + ChunkSize) % ChunkSize;

            const Chunk* chunk = getChunk(cx, cz);
            if (!chunk || chunk->debug == 0) {
                std::cout << "Chunk not found at (" << cx << ", " << cz << ")" << std::endl;
                return std::nullopt;
            }

            if (const int v = chunk->load(localX, py, localZ); v != 0) {
                // std::cout << "Voxel hit at " << px << ", " << py << ", " << pz << ", face: " << faceHit << std::endl;
                return RaycastResult {
                    .cx = cx,
                    .cz = cz,
                    .x = localX,
                    .y = py,
                    .z = localZ,
                    .face = faceHit
                };
            }
        }

        // Advance to next voxel
        cmpx = step(tx, tz) * step(tx, ty);
        cmpy = step(ty, tx) * step(ty, tz);
        cmpz = step(tz, ty) * step(tz, tx);

        if (cmpx) {
            faceHit = sx < 0 ? 0 : 1;  // 0: +x, 1: -x
        } else if (cmpy) {
            faceHit = sy < 0 ? 2 : 3;  // 2: +y, 3: -y
        } else if (cmpz) {
            faceHit = sz < 0 ? 4 : 5;  // 4: +z, 5: -z
        }

        tx += dtx * cmpx;
        ty += dty * cmpy;
        tz += dtz * cmpz;

        px += sx * cmpx;
        py += sy * cmpy;
        pz += sz * cmpz;
    }

    return std::nullopt;
}

void WorldManager::tryStoreVoxel(const int cx, const int cz, const int x, const int y, const int z, const int voxelType, std::unordered_set<Chunk*>& chunksToMesh) {
    Chunk* chunk = getChunk(cx, cz);
    if (!chunk) {
        return;
    }

    {
        std::scoped_lock lock(chunk->mutex);
        chunk->store(x, y, z, voxelType);
    }
    chunksToMesh.insert(chunk);
}

void WorldManager::updateVoxel(RaycastResult result, const bool place) {
    auto [cx, cz, x, y, z, face] = result;

    if (place) {
        switch (face) {
            case 0: x == ChunkSize - 1 ? (++cx, x = 0) : ++x; break;
            case 1: x == 0             ? (--cx, x = ChunkSize - 1) : --x; break;
            case 2: if (y < ChunkHeight - 2) ++y; break;
            case 3: if (y > 0) --y; break;
            case 4: z == ChunkSize - 1 ? (++cz, z = 0) : ++z; break;
            case 5: z == 0             ? (--cz, z = ChunkSize - 1) : --z; break;
            default:
                std::cerr << "Invalid face: " << face << std::endl;
                return;
        }
    }

    updateVoxels({{cx, cz, x, y, z, static_cast<int>(place ? paletteIndex + 1 : 0)}});
}

void WorldManager::updateVoxels(std::vector<Edit>& edits) {
    std::unordered_set<Chunk*> chunksToMeshSet;

    for (auto& edit : edits) {
        auto [cx, cz, x, y, z, voxelType, _] = edit;
        if (y >= ChunkHeight || y < 0) {
            continue;
        }

        Chunk* chunk = getChunk(cx, cz);
        if (!chunk) {
            continue;
        }

        {
            std::scoped_lock lock(chunk->mutex);
            edit.oldVoxelType = chunk->load(x, y, z);
            chunk->store(x, y, z, voxelType);
        }

        chunksToMeshSet.insert(chunk);

        // If the voxel is on a chunk boundary, update the neighboring chunk(s)
        if (x == 0) {
            tryStoreVoxel(cx - 1, cz, ChunkSize, y, z, voxelType, chunksToMeshSet);
            if (z == 0) {
                tryStoreVoxel(cx - 1, cz - 1, ChunkSize, y, ChunkSize, voxelType, chunksToMeshSet);
            } else if (z == ChunkSize - 1) {
                tryStoreVoxel(cx - 1, cz + 1, ChunkSize, y, -1, voxelType, chunksToMeshSet);
            }
        } else if (x == ChunkSize - 1) {
            tryStoreVoxel(cx + 1, cz, -1, y, z, voxelType, chunksToMeshSet);
            if (z == 0) {
                tryStoreVoxel(cx + 1, cz - 1, -1, y, ChunkSize, voxelType, chunksToMeshSet);
            } else if (z == ChunkSize - 1) {
                tryStoreVoxel(cx + 1, cz + 1, -1, y, -1, voxelType, chunksToMeshSet);
            }
        }
        if (z == 0) {
            tryStoreVoxel(cx, cz - 1, x, y, ChunkSize, voxelType, chunksToMeshSet);
        } else if (z == ChunkSize - 1) {
            tryStoreVoxel(cx, cz + 1, x, y, -1, voxelType, chunksToMeshSet);
        }
    }

    for (Chunk* chunk : chunksToMeshSet) {
        queueMeshChunk(chunk);
    }
}

void WorldManager::updateVoxels(std::vector<Edit> &&edits) {
    std::unordered_set<Chunk*> chunksToMeshSet;

    for (auto& edit : edits) {
        auto [cx, cz, x, y, z, voxelType, _] = edit;
        if (y >= ChunkHeight || y < 0) {
            continue;
        }

        Chunk* chunk = getChunk(cx, cz);
        if (!chunk) {
            continue;
        }

        {
            std::scoped_lock lock(chunk->mutex);
            // edit.oldVoxelType = chunk->load(x, y, z);
            chunk->store(x, y, z, voxelType);
        }

        chunksToMeshSet.insert(chunk);

        // If the voxel is on a chunk boundary, update the neighboring chunk(s)
        if (x == 0) {
            tryStoreVoxel(cx - 1, cz, ChunkSize, y, z, voxelType, chunksToMeshSet);
            if (z == 0) {
                tryStoreVoxel(cx - 1, cz - 1, ChunkSize, y, ChunkSize, voxelType, chunksToMeshSet);
            } else if (z == ChunkSize - 1) {
                tryStoreVoxel(cx - 1, cz + 1, ChunkSize, y, -1, voxelType, chunksToMeshSet);
            }
        } else if (x == ChunkSize - 1) {
            tryStoreVoxel(cx + 1, cz, -1, y, z, voxelType, chunksToMeshSet);
            if (z == 0) {
                tryStoreVoxel(cx + 1, cz - 1, -1, y, ChunkSize, voxelType, chunksToMeshSet);
            } else if (z == ChunkSize - 1) {
                tryStoreVoxel(cx + 1, cz + 1, -1, y, -1, voxelType, chunksToMeshSet);
            }
        }
        if (z == 0) {
            tryStoreVoxel(cx, cz - 1, x, y, ChunkSize, voxelType, chunksToMeshSet);
        } else if (z == ChunkSize - 1) {
            tryStoreVoxel(cx, cz + 1, x, y, -1, voxelType, chunksToMeshSet);
        }
    }

    for (Chunk* chunk : chunksToMeshSet) {
        queueMeshChunk(chunk);
    }
}

void WorldManager::addPrimitive(std::unique_ptr<Primitive> primitive) {
    placePrimitive(*primitive);
    primitives.push_back(std::move(primitive));
}

void WorldManager::placePrimitive(Primitive &primitive) {
    primitive.edits.clear();
    primitive.edits = primitive.generateEdits();
    updateVoxels(primitive.edits);
}

void WorldManager::movePrimitive(Primitive& primitive, const glm::ivec3& newOrigin) {
    // For each edit, if the current voxel is not the same as edit.oldVoxelType, don't change it (someone else edited on top there)
    // If the current voxel is the same, then we were the last edit, change it back to oldVoxelType

    // Make a set of the old edits and a set of the new edits
    const std::unordered_set<Edit, EditHash> oldEdits(primitive.edits.begin(), primitive.edits.end());

    primitive.origin = newOrigin;
    std::vector<Edit> newEditsVec = primitive.generateEdits();
    const std::unordered_set<Edit, EditHash> newEdits(newEditsVec.begin(), newEditsVec.end());

    // The edits we need to remove are the ones in the old set that are not in the new set
    std::vector<Edit> toRemove = {};
    for (const auto& oldEdit : primitive.edits) {
        if (!newEdits.contains(oldEdit)) {
            toRemove.push_back(oldEdit);
        }
    }

    // The edits we need to add are the ones in the new set that are not in the old set
    std::vector<Edit> toAdd = {};
    for (const auto& newEdit : newEdits) {
        if (!oldEdits.contains(newEdit)) {
            toAdd.push_back(newEdit);
        }
    }

    // First, revert old edits
    std::vector<Edit> revertEdits;
    for (const auto& [cx, cz, x, y, z, voxelType, oldVoxelType] : toRemove) {
        int currentVoxelType = 0;
        Chunk* chunk = getChunk(cx, cz);
        if (!chunk) {
            continue;
        }
        {
            std::scoped_lock lock(chunk->mutex);
            currentVoxelType = chunk->load(x, y, z);
        }

        // If the voxel has been changed since we last edited it, don't change it
        if (currentVoxelType != voxelType) {
            // Someone else edited this voxel; leave it
            continue;
        }

        // Change back to the old voxel type
        revertEdits.emplace_back(Edit{cx, cz, x, y, z, oldVoxelType});
    }
    updateVoxels(revertEdits);

    // Now, apply new edits
    updateVoxels(toAdd);

    // Update the primitive's edits
    for (auto& edit : toRemove) {
        std::erase(primitive.edits, edit);
    }
    for (auto& edit : toAdd) {
        primitive.edits.push_back(edit);
    }
}

void WorldManager::cleanup() {
    threadPool.stop();
}
