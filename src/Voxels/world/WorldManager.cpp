#include "WorldManager.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>

#include <nlohmann/json.hpp>

#include "Mesher.hpp"
#include "RunMesher.hpp"
#include "tracy/Tracy.hpp"

using json = nlohmann::json;

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
    if (!chunkInRenderDistance(cx, cz) || (levelChunkBounds.has_value() &&
        (cx < levelChunkBounds->first.x || cx > levelChunkBounds->second.x ||
         cz < levelChunkBounds->first.y || cz > levelChunkBounds->second.y))) {
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
            chunk->generate(generationType);

            applyEditsToChunk(chunk);

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

void WorldManager::applyEditsToChunk(Chunk* chunk) {
    const int chunkMinX = chunk->cx * ChunkSize - 1;
    const int chunkMinZ = chunk->cz * ChunkSize - 1;
    const int chunkMaxX = (chunk->cx + 1) * ChunkSize;
    const int chunkMaxZ = (chunk->cz + 1) * ChunkSize;

    // User edits
    for (const auto& [pos, voxelType] : userEdits) {
        if (pos.x < chunkMinX || pos.x > chunkMaxX ||
            pos.z < chunkMinZ || pos.z > chunkMaxZ ||
            pos.y < 0 || pos.y > ChunkHeight - 1) {
            continue;
        }

        const int lx = pos.x - (chunk->cx << ChunkSizeShift);
        const int lz = pos.z - (chunk->cz << ChunkSizeShift);

        if (voxelType == 0) {
            chunk->store(lx, pos.y, lz, EmptyVoxel);
        } else {
            chunk->store(lx, pos.y, lz, voxelType);
        }
    }

    // Primitives

    // - check bounding box against corners
    // - if any intersect then find bounding box intersection
    // - loop over the positions in this and look in edit map

    for (const auto& primitive : primitives) {
        // AABB check
        const glm::ivec3 primMin = primitive->start + primitive->origin;
        const glm::ivec3 primMax = primitive->end + primitive->origin;

        if (primMax.x < chunkMinX || primMin.x > chunkMaxX ||
            primMax.z < chunkMinZ || primMin.z > chunkMaxZ ||
            primMax.y < 0 || primMin.y > ChunkHeight - 1) {
            continue;
        }

        // Find intersection AABB
        const int minX = std::max(chunkMinX, primMin.x);
        const int maxX = std::min(chunkMaxX, primMax.x);
        const int minY = std::max(0, primMin.y);
        const int maxY = std::min(ChunkHeight - 1, primMax.y);
        const int minZ = std::max(chunkMinZ, primMin.z);
        const int maxZ = std::min(chunkMaxZ, primMax.z);

        // Loop over intersection AABB and apply edits (the chunk will be meshed afterwards by createChunk)
        for (int x = minX; x <= maxX; ++x) {
            for (int y = minY; y <= maxY; ++y) {
                for (int z = minZ; z <= maxZ; ++z) {
                    const auto it = primitive->edits.find({x, y, z});
                    if (it != primitive->edits.end()) {
                        const std::optional<Edit>& editOpt = it->second;

                        const int lx = x - (chunk->cx << ChunkSizeShift);
                        const int lz = z - (chunk->cz << ChunkSizeShift);

                        if (editOpt.has_value()) {
                            chunk->store(lx, y, lz, editOpt->voxelType);
                        } else {
                            chunk->store(lx, y, lz, EmptyVoxel);
                        }
                    }
                }
            }
        }
    }
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

void WorldManager::saveLevel() {
    json levelJson;

    // Generation type
    switch (generationType) {
        case GenerationType::Flat:
            levelJson["generationType"] = "Flat";
            break;
        case GenerationType::Perlin2D:
            levelJson["generationType"] = "Perlin2D";
            break;
        case GenerationType::Perlin3D:
            levelJson["generationType"] = "Perlin3D";
            break;
    }

    // Palette
    json paletteJson = json::array();
    for (const auto& color : palette) {
        paletteJson.push_back({color.r, color.g, color.b});
    }
    levelJson["palette"] = paletteJson;

    // Primitives
    json primitivesJson = json::array();
    for (const auto& primitive : primitives) {
        json primitiveJson;
        // Store the type of primitive.
        // For any type, store userEdits (but not edits themselves; they will be regenerated on load)
        // as well as the voxelType, origin, start and end
        // For sphere, also store radius; for cylinder, radius and height; for plane, axis

        primitiveJson["voxelType"] = primitive->voxelType;
        primitiveJson["origin"] = {primitive->origin.x, primitive->origin.y, primitive->origin.z};
        // Only cuboid and plane need the start and end in order to reconstruct the shape, so don't store them for the others

        // User edits
        json userEditsJson;
        for (const auto& [pos, voxelType] : primitive->userEdits) {
            userEditsJson.push_back({
                {"pos", {pos.x, pos.y, pos.z}},
                {"voxelType", voxelType}
            });
        }
        primitiveJson["userEdits"] = userEditsJson;

        // Type-specific data
        if (dynamic_cast<Cuboid*>(primitive.get())) {
            primitiveJson["type"] = "Cuboid";
            primitiveJson["start"] = {primitive->start.x, primitive->start.y, primitive->start.z};
            primitiveJson["end"] = {primitive->end.x, primitive->end.y, primitive->end.z};
        } else if (dynamic_cast<Sphere*>(primitive.get())) {
            primitiveJson["type"] = "Sphere";
            Sphere* sphere = dynamic_cast<Sphere*>(primitive.get());
            primitiveJson["radius"] = sphere->radius;
        } else if (dynamic_cast<Cylinder*>(primitive.get())) {
            primitiveJson["type"] = "Cylinder";
            Cylinder* cylinder = dynamic_cast<Cylinder*>(primitive.get());
            primitiveJson["radius"] = cylinder->radius;
            primitiveJson["height"] = cylinder->height;
        } else if (dynamic_cast<Plane*>(primitive.get())) {
            primitiveJson["type"] = "Plane";
            Plane* plane = dynamic_cast<Plane*>(primitive.get());
            std::string axisStr;
            switch (plane->axis) {
                case Plane::Axis::X:
                    axisStr = "X";
                    break;
                case Plane::Axis::Y:
                    axisStr = "Y";
                    break;
                case Plane::Axis::Z:
                    axisStr = "Z";
                    break;
            }
            primitiveJson["axis"] = axisStr;
            primitiveJson["start"] = {primitive->start.x, primitive->start.y, primitive->start.z};
            primitiveJson["end"] = {primitive->end.x, primitive->end.y, primitive->end.z};
        }
        primitivesJson.push_back(primitiveJson);
    }
    levelJson["primitives"] = primitivesJson;

    // User edits
    json userEditsJson;
    for (const auto& [pos, voxelType] : userEdits) {
        userEditsJson.push_back({
            {"pos", {pos.x, pos.y, pos.z}},
            {"voxelType", voxelType}
        });
    }
    levelJson["userEdits"] = userEditsJson;

    // Save to file
    std::ofstream outfile(levelFile);
    if (!outfile.is_open()) {
        std::cerr << "Failed to open level file for saving: " << levelFile << std::endl;
        return;
    }

    outfile << levelJson.dump(4);
    outfile.close();

    std::cout << "Level saved to " << levelFile << std::endl;
}

void WorldManager::loadLevel() {
    std::cout << "Loading level from " << levelFile << std::endl;
    std::ifstream infile(levelFile);
    if (!infile.is_open()) {
        std::cerr << "Failed to open level file for loading: " << levelFile << std::endl;
        return;
    }

    json levelJson;
    infile >> levelJson;
    infile.close();

    // Generation type
    std::string generationTypeStr = levelJson.value("generationType", "Flat");
    if (generationTypeStr == "Flat") {
        generationType = GenerationType::Flat;
    } else if (generationTypeStr == "Perlin2D") {
        generationType = GenerationType::Perlin2D;
    } else if (generationTypeStr == "Perlin3D") {
        generationType = GenerationType::Perlin3D;
    } else {
        std::cerr << "Unknown generation type: " << generationTypeStr << ", defaulting to Flat" << std::endl;
        generationType = GenerationType::Flat;
    }

    // Palette
    palette.fill(glm::vec3());
    json paletteJson = levelJson.value("palette", json::array());
    for (size_t i = 0; i < paletteJson.size() && i < palette.size(); ++i) {
        const auto& colorArray = paletteJson[i];
        if (colorArray.is_array() && colorArray.size() == 3) {
            palette[i] = glm::vec3(colorArray[0], colorArray[1], colorArray[2]);
        }
    }

    // Primitives
    primitives.clear();
    json primitivesJson = levelJson.value("primitives", json::array());
    for (const auto& primitiveJson : primitivesJson) {
        // Common data
        const int voxelType = primitiveJson.value("voxelType", 1);
        const auto originArray = primitiveJson.value("origin", json::array({0, 0, 0}));
        const auto startArray = primitiveJson.value("start", json::array({0, 0, 0}));
        const auto endArray = primitiveJson.value("end", json::array({0, 0, 0}));
        glm::ivec3 origin = glm::ivec3(originArray[0], originArray[1], originArray[2]);
        glm::ivec3 start = glm::ivec3(startArray[0], startArray[1], startArray[2]);
        glm::ivec3 end = glm::ivec3(endArray[0], endArray[1], endArray[2]);

        // User edits
        Primitive::UserEditMap primitiveUserEdits;
        json userEditsJson = primitiveJson.value("userEdits", json::array());
        for (const auto& editJson : userEditsJson) {
            const auto posArray = editJson.value("pos", json::array({0, 0, 0}));
            glm::ivec3 pos = glm::ivec3(posArray[0], posArray[1], posArray[2]);
            const int editVoxelType = editJson.value("voxelType", 1);
            primitiveUserEdits[pos] = editVoxelType;
        }

        std::string type = primitiveJson.value("type", "Cuboid");
        if (type == "Cuboid") {
            auto primitive = std::make_unique<Cuboid>(voxelType, start, end);
            primitive->userEdits = primitiveUserEdits;
            primitives.push_back(std::move(primitive));
        } else if (type == "Sphere") {
            const int radius = primitiveJson.value("radius", 1);
            auto primitive = std::make_unique<Sphere>(voxelType, origin, radius);
            primitive->userEdits = primitiveUserEdits;
            primitives.push_back(std::move(primitive));
        } else if (type == "Cylinder") {
            const int radius = primitiveJson.value("radius", 1);
            const int height = primitiveJson.value("height", 1);
            auto primitive = std::make_unique<Cylinder>(voxelType, origin, radius, height);
            primitive->userEdits = primitiveUserEdits;
            primitives.push_back(std::move(primitive));
        } else if (type == "Plane") {
            std::string axisStr = primitiveJson.value("axis", "Y");
            Plane::Axis axis = Plane::Axis::Y;
            if (axisStr == "X") {
                axis = Plane::Axis::X;
            } else if (axisStr == "Z") {
                axis = Plane::Axis::Z;
            }
            auto primitive = std::make_unique<Plane>(voxelType, start, end, axis);
            primitive->userEdits = primitiveUserEdits;
            primitives.push_back(std::move(primitive));
        }
    }

    // User edits
    userEdits.clear();
    json userEditsJson = levelJson.value("userEdits", json::array());
    for (const auto& editJson : userEditsJson) {
        const auto posArray = editJson.value("pos", json::array({0, 0, 0}));
        glm::ivec3 pos = glm::ivec3(posArray[0], posArray[1], posArray[2]);
        const int editVoxelType = editJson.value("voxelType", 1);
        userEdits[pos] = editVoxelType;
    }

    // Reload the world
    chunks.clear();
    frontierChunks.clear();
    chunkByCoords.clear();
    chunkData.clear();
    chunkData.resize(MaxChunks);

    for (const auto& primitive : primitives) {
        placePrimitive(*primitive);
    }

    createChunk(0, 0);

    std::cout << "Level loaded from " << levelFile << std::endl;
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

    return chunk->load(lx, y, lz);
}

std::optional<RaycastResult> WorldManager::raycast() {
    constexpr float big = 1e30f;

    const float ox = camera.transform.position.x;
    const float oy = camera.transform.position.y;
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

    // If the position is inside a primitive, update its edits so that the edit is preserved when moving it
    const glm::ivec3 worldPos = glm::ivec3((cx << ChunkSizeShift) + x, y, (cz << ChunkSizeShift) + z);
    for (const auto& primitive : primitives) {
        // Coarse search; can produce false positives but not false negatives
        if (primitive->isPosInside(worldPos)) {
            // Now check the position actually matches, and make sure the corresponding edit's voxelType is the same as the world's actual voxel type
            // (i.e., we should only edit the primitive that's on top)

            const glm::ivec3 localPos = worldPos - primitive->origin;
            const auto it = primitive->edits.find(worldPos);
            if (it != primitive->edits.end()) {
                auto& editOpt = it->second;
                if (editOpt.has_value()) {
                    assert(!place);  // if we are removing, there should be an edit here
                    auto& [voxelType, oldVoxelType] = editOpt.value();
                    // Check real world voxel type matches
                    if (voxelType == load(worldPos.x, worldPos.y, worldPos.z)) {
                        // Add a userEdit so that we can apply it when regenerating edits later
                        primitive->userEdits[localPos] = 0;

                        // Change the edit directly as well so that moving etc. will work properly

                        // We know that the update is a removal because an edit exists at this position

                        // Remove the edit by setting it to nullopt. The key should remain in the map, though;
                        // this is because we need it to be there when checking if a user edit is within a primitive
                        editOpt = std::nullopt;

                        break;
                    }
                } else {
                    // No edit at this position
                    // We know that we are placing here, because if we were removing, there would be an edit there
                    assert(place);
                    editOpt = {static_cast<int>(paletteIndex + 1), 0};
                    primitive->userEdits[localPos] = static_cast<int>(paletteIndex + 1);
                    break;
                }
            }
        }
    }

    userEdits[{(cx << ChunkSizeShift) + x, y, (cz << ChunkSizeShift) + z}] = place ? static_cast<int>(paletteIndex + 1) : 0;

    Primitive::EditMap edits;
    edits[{(cx << ChunkSizeShift) + x, y, (cz << ChunkSizeShift) + z}] = {place ? static_cast<int>(paletteIndex + 1) : 0, 0};
    updateVoxels(edits);
}

void WorldManager::updateVoxels(Primitive::EditMap& edits) {
    std::unordered_set<Chunk*> chunksToMeshSet;

    for (auto& [pos, editOpt] : edits) {
        if (!editOpt.has_value()) continue;

        const int cx = pos.x >> ChunkSizeShift;
        const int cz = pos.z >> ChunkSizeShift;
        const int x = pos.x - (cx << ChunkSizeShift);
        const int y = pos.y;
        const int z = pos.z - (cz << ChunkSizeShift);

        const int voxelType = editOpt->voxelType;

        if (y >= ChunkHeight || y < 0) {
            continue;
        }

        Chunk* chunk = getChunk(cx, cz);
        if (!chunk) {
            continue;
        }

        {
            std::scoped_lock lock(chunk->mutex);
            editOpt->oldVoxelType = chunk->load(x, y, z);
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
    primitive.edits = primitive.generateEdits();
    updateVoxels(primitive.edits);
}

void WorldManager::removePrimitive(const size_t index) {
    ZoneScoped;
    // For each edit, if the current voxel is not the same as edit.voxelType, don't change it (someone else edited on top there).
    // In this case, we also need to update that primitive's oldVoxelType to our oldVoxelType
    // The case is:
    // A placed -> B placed on top of A -> A removed -> B's oldVoxelTypes need to be updated

    // Other case: if the current voxel is the same, then we were the last edit, change it back to oldVoxelType

    // 1) Get all the edits that overlap with another primitive (use naive slow method at first here)
    // 2) For every overlapping edit such that its oldVoxelType is equal to our voxel type (i.e., we were the most recent edit before it):
    //    a) Set its oldVoxelType to our oldVoxelType (set it to what came before us)

    Primitive& primitive = *primitives[index];

    // Fix overlapping edits
    {
        ZoneScopedN("Fix overlapping edits");
        for (size_t i = 0; i < primitives.size(); ++i) {
            if (i == index) continue;  // skip ourselves

            // Find all edits in the other primitive whose positions are the same as one of our edits we will be removing
            for (auto& [otherPos, otherEditOpt] : primitives[i]->edits) {
                if (!otherEditOpt.has_value()) continue;  // skip the position if no edit is associated with it

                const auto it = primitive.edits.find(otherPos);
                if (it != primitive.edits.end()) {
                    const auto& editOpt = it->second;
                    if (editOpt.has_value()) {
                        // Position is in the primitive and an edit exists there
                        if (otherEditOpt->oldVoxelType == editOpt->voxelType) {
                            // We were the most recent edit: set the oldVoxelType to what came before us
                            otherEditOpt->oldVoxelType = editOpt->oldVoxelType;
                        }
                    }
                }
            }
        }
    }

    Primitive::EditMap revertEdits;
    {
        ZoneScopedN("Create revertEdits");
        for (const auto& [pos, editOpt] : primitive.edits) {
            if (!editOpt.has_value()) continue;

            int currentVoxelType = 0;
            Chunk* chunk = getChunk(pos.x >> ChunkSizeShift, pos.z >> ChunkSizeShift);
            if (!chunk) {
                continue;
            }
            {
                std::scoped_lock lock(chunk->mutex);
                currentVoxelType = chunk->load(pos.x & (ChunkSize - 1), pos.y, pos.z & (ChunkSize - 1));
            }

            // If the voxel has been changed since we last edited it, don't change it
            if (currentVoxelType != editOpt->voxelType) {
                // Someone else edited this voxel; leave it
                continue;
            }

            // Change back to the old voxel type
            revertEdits.emplace(pos, Edit{editOpt->oldVoxelType, 0});
        }
    }

    {
        ZoneScopedN("Update voxels");
        updateVoxels(revertEdits);
    }

    // Now delete the primitive from the primitives vector
    {
        ZoneScopedN("Erase primitive");
        primitives.erase(primitives.begin() + index);
    }
}

void WorldManager::movePrimitive(const size_t index, const glm::ivec3& newOrigin) {
    ZoneScoped;
    // For each edit, if the current voxel is not the same as edit.oldVoxelType, don't change it (someone else edited on top there)
    // If the current voxel is the same, then we were the last edit, change it back to oldVoxelType

    Primitive& primitive = *primitives[index];

    primitive.origin = newOrigin;
    Primitive::EditMap newEdits = primitive.generateEdits();

    // The edits we need to remove are the ones in the old map that are not in the new map
    Primitive::EditMap toRemove;
    {
        ZoneScopedN("Create toRemove");
        for (const auto& [pos, oldEditOpt] : primitive.edits) {
            const auto it = newEdits.find(pos);
            if (it == newEdits.end()) {
                // Old position not present in new map: need to add the edit
                toRemove.emplace(pos, oldEditOpt);
                continue;
            }

            auto& newEditOpt = it->second;
            if (!oldEditOpt.has_value() && !newEditOpt.has_value()) {
                // Both new and old have no edit at that position, so we don't need to remove the old edit
                continue;
            }

            if (oldEditOpt.has_value() && newEditOpt.has_value() && oldEditOpt->voxelType == newEditOpt->voxelType) {
                // Both new and old have an edit at the position, and the voxel types are the same, so we don't need to remove the old edit
                continue;
            }

            // In all other cases, the new and old have different edits
            //  - old is nullopt, new isn't
            //  - new is nullopt, old isn't
            //  - neither are nullopt but voxelTypes differ
            toRemove.emplace(pos, oldEditOpt);
        }
    }

    // The edits we need to add are the ones in the new map that are not in the old map
    Primitive::EditMap toAdd;
    {
        ZoneScopedN("Create toAdd");
        for (const auto& [pos, newEditOpt] : newEdits) {
            const auto it = primitive.edits.find(pos);
            if (it == primitive.edits.end()) {
                // New position not present in old map: need to add the edit
                toAdd.emplace(pos, newEditOpt);
                continue;
            }

            auto& oldEditOpt = it->second;
            if (!oldEditOpt.has_value() && !newEditOpt.has_value()) {
                // Both new and old have no edit at that position, so we don't need to add the new edit
                continue;
            }

            if (oldEditOpt.has_value() && newEditOpt.has_value() && oldEditOpt->voxelType == newEditOpt->voxelType) {
                // Both new and old have an edit at the position, and the voxel types are the same, so we don't need to add the new edit
                continue;
            }

            // In all other cases, the new and old have different edits
            toAdd.emplace(pos, newEditOpt);
        }
    }

    // Fix overlapping edits
    {
        ZoneScopedN("Fix overlapping edits");
        for (size_t i = 0; i < primitives.size(); ++i) {
            if (i == index) continue;  // skip ourselves

            // Skip if bounding boxes don't overlap
            const glm::ivec3 otherMin = primitives[i]->start + primitives[i]->origin;
            const glm::ivec3 otherMax = primitives[i]->end + primitives[i]->origin;
            const glm::ivec3 thisMin = primitive.start + primitive.origin;
            const glm::ivec3 thisMax = primitive.end + primitive.origin;

            if (otherMax.x < thisMin.x || otherMin.x > thisMax.x ||
                otherMax.y < thisMin.y || otherMin.y > thisMax.y ||
                otherMax.z < thisMin.z || otherMin.z > thisMax.z) {
                continue;
            }

            // Find all edits in the other primitive whose positions are the same as one of our edits we will be removing
            for (auto& [otherPos, otherEditOpt] : primitives[i]->edits) {
                if (!otherEditOpt.has_value()) continue;  // skip the position if no edit is associated with it

                const auto it = primitive.edits.find(otherPos);
                if (it != primitive.edits.end()) {
                    const auto& editOpt = it->second;
                    if (editOpt.has_value()) {
                        // Position is in the primitive and an edit exists there
                        if (otherEditOpt->oldVoxelType == editOpt->voxelType) {
                            // We were the most recent edit: set the oldVoxelType to what came before us
                            otherEditOpt->oldVoxelType = editOpt->oldVoxelType;
                        }
                    }
                }
            }
        }
    }

    // First, revert old edits
    {
        ZoneScopedN("Revert old edits");
        Primitive::EditMap revertEdits;
        for (const auto& [pos, editOpt] : toRemove) {
            // If editOpt is nullopt, there is nothing to change back to, so continue
            if (!editOpt.has_value()) continue;

            int currentVoxelType = 0;
            Chunk* chunk = getChunk(pos.x >> ChunkSizeShift, pos.z >> ChunkSizeShift);
            if (!chunk) {
                continue;
            }
            {
                std::scoped_lock lock(chunk->mutex);
                currentVoxelType = chunk->load(pos.x & (ChunkSize - 1), pos.y, pos.z & (ChunkSize - 1));
            }

            // If the voxel has been changed since we last edited it, don't change it
            if (currentVoxelType != editOpt->voxelType) {
                // Someone else edited this voxel; leave it
                continue;
            }

            // Change back to the old voxel type
            revertEdits.emplace(pos, Edit{editOpt->oldVoxelType, 0});
        }
        updateVoxels(revertEdits);
    }

    // Now, apply new edits
    {
        ZoneScopedN("Apply new edits");
        updateVoxels(toAdd);
    }

    // Update the primitive's edits
    {
        ZoneScopedN("Update primitive edits (remove)");
        for (const auto &pos: toRemove | std::views::keys) {
            primitive.edits.erase(pos);
        }
    }

    {
        ZoneScopedN("Update primitive edits (add)");
        for (const auto& [pos, editOpt] : toAdd) {
            primitive.edits.emplace(pos, editOpt);
        }
    }
}

void WorldManager::cleanup() {
    threadPool.stop();
}
