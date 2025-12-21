#include "WorldManager.hpp"

#include <fstream>

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

void WorldManager::cleanup() {
    threadPool.stop();
}
