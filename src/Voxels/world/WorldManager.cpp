#include "WorldManager.hpp"

#include <fstream>
#include <sstream>

#include "Mesher.hpp"
#include "../util/Util.hpp"
#include "tracy/Tracy.hpp"

WorldManager::WorldManager(Camera &camera, std::function<size_t(size_t)> outOfCapacityCallback, GenerationType generationType, const std::filesystem::path &levelFile)
    : camera(camera),
      outOfCapacityCallback(std::move(outOfCapacityCallback)),
      generationType(generationType),
      levelFile(levelFile),
      allocator(FreeListAllocator(
              INITIAL_VERTEX_BUFFER_SIZE,
              4096,
              outOfCapacityCallback
      ))
{
    chunks = std::vector<Chunk>();
    frontierChunks = std::vector<Chunk *>();
    newlyCreatedChunks = std::vector<Chunk *>();
    chunkByCoords = std::unordered_map<size_t, Chunk *>();
    chunkData = std::vector<ChunkData>();

    chunks.reserve(MAX_CHUNKS);
    chunkData.resize(MAX_CHUNKS);

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

    std::sort(frontierChunks.begin(), frontierChunks.end(), [this](Chunk *a, Chunk *b) {
        return squaredDistanceToChunk(a->cx, a->cz) < squaredDistanceToChunk(b->cx, b->cz);
    });

    for (size_t i = 0, s = frontierChunks.size(); i < s; i++) {
        Chunk *chunk = frontierChunks[i];

        if (chunkTasksCount >= MAX_CHUNK_TASKS) {
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
            Chunk *chunk = frontierChunks[i];
            // If the chunk's still being initialised, don't destroy it yet since this will invalidate references
            // It will be destroyed later on anyway
            if (chunkInRenderDistance(chunk->cx, chunk->cz) || chunk->initialising) {
                continue;
            }

            // Promote neighbours to frontier if necessary and destroy chunk
            int numPromoted = onFrontierChunkRemoved(chunk);
            frontierChunks.erase(frontierChunks.begin() + i);
            chunkByCoords.erase(key(chunk->cx, chunk->cz));

            // This should only happen if the chunk has already passed through newlyCreatedChunks
            if (chunk->ready) {
                ZoneScopedN("Deallocate chunk vertices");
                allocator.deallocate(chunk->firstIndex, chunk->numVertices);
            }

            chunk->destroyed = true;
            chunkData[chunk->index].numVertices = 0;  // Don't render the chunk any more
            s += numPromoted - 1;
            i--;
        }
    }
}

bool WorldManager::ensureChunkIfVisible(int cx, int cz) {
    int maxChunkX = (int) std::ceil((double) level.maxX / CHUNK_SIZE) - 1;
    int maxChunkZ = (int) std::ceil((double) level.maxZ / CHUNK_SIZE) - 1;

    if (!chunkInRenderDistance(cx, cz) ||
        (generationType == GenerationType::LevelLoad) && (cx < 0 || cx > maxChunkX || cz < 0 || cz > maxChunkZ)) {
        return false;
    }

    return ensureChunk(cx, cz) != nullptr;
}

Chunk *WorldManager::ensureChunk(int cx, int cz) {
    if (chunkByCoords.find(key(cx, cz)) != chunkByCoords.end()) {
        return nullptr;
    }

    return createChunk(cx, cz);
}

Chunk *WorldManager::createChunk(int cx, int cz) {
    ZoneScoped;

    // Find the first free slot in the chunks vector
    size_t index = 0;
    while (index < chunks.size() && !chunks[index].destroyed) {
        index++;
    }

    if (index < chunks.size()) {
        chunks[index] = Chunk(cx, cz);
    } else {
        chunks.emplace_back(cx, cz);
    }

    Chunk &chunk = chunks[index];
    chunk.index = index;
    chunkByCoords[key(cx, cz)] = &chunk;
    addFrontier(&chunk);
    chunkData[index] = {
            .cx = cx,
            .cz = cz,
            .minY = CHUNK_HEIGHT,
            .maxY = 0,
            .numVertices = 0,
            .firstIndex = 0,
            ._pad0 = 0,
            ._pad1 = 0,
    };

    chunkTasksCount++;

    threadPool.queueTask([&chunk, this]() {
        ZoneScoped;

        chunk.init();
        chunk.generate(generationType, level);

        Mesher::meshChunk(chunk, generationType);

        chunk.initialising = false;

        chunk.debug = 1;

        // If newlyCreatedChunks is currently being iterated through, we need to lock the mutex
        // Additionally, only one thread should be in this critical section at the same time
        {
            std::unique_lock<std::mutex> lock(cvMutexNewlyCreatedChunks);
            cvNewlyCreatedChunks.wait(lock, [this]() { return newlyCreatedChunksReady; });

            newlyCreatedChunksReady = false;

            chunk.debug = 2;
            newlyCreatedChunks.push_back(&chunk);

            newlyCreatedChunksReady = true;
            cvNewlyCreatedChunks.notify_one();  // only need to wake up one thread
        }
    });

    return &chunk;
}

void WorldManager::addFrontier(Chunk *chunk) {
    frontierChunks.push_back(chunk);
    int cx = chunk->cx;
    int cz = chunk->cz;

    updateFrontierNeighbour(chunk, cx - 1, cz);
    updateFrontierNeighbour(chunk, cx + 1, cz);
    updateFrontierNeighbour(chunk, cx, cz - 1);
    updateFrontierNeighbour(chunk, cx, cz + 1);
}

void WorldManager::updateFrontierNeighbour(Chunk *frontier, int cx, int cz) {
    if (chunkByCoords.find(key(cx, cz)) == chunkByCoords.end()) {
        return;
    }

    Chunk *neighbour = chunkByCoords[key(cx, cz)];
    neighbour->neighbours++;
    frontier->neighbours++;
    if (neighbour->neighbours == 4) {
        frontierChunks.erase(std::remove(frontierChunks.begin(), frontierChunks.end(), neighbour), frontierChunks.end());
    }
}

int WorldManager::onFrontierChunkRemoved(Chunk *frontierChunk) {
    int cx = frontierChunk->cx;
    int cz = frontierChunk->cz;
    double d = squaredDistanceToChunk(cx, cz);
    return onFrontierChunkRemoved(cx - 1, cz, d) +
           onFrontierChunkRemoved(cx + 1, cz, d) +
           onFrontierChunkRemoved(cx, cz - 1, d) +
           onFrontierChunkRemoved(cx, cz + 1, d);
}

int WorldManager::onFrontierChunkRemoved(int cx, int cz, double distance) {
    if (chunkByCoords.find(key(cx, cz)) == chunkByCoords.end()) {
        return 0;
    }

    Chunk *chunk = chunkByCoords[key(cx, cz)];
    chunk->neighbours--;
    if (!(std::find(frontierChunks.begin(), frontierChunks.end(), chunk) != frontierChunks.end()) &&
        (chunkInRenderDistance(cx, cz) ||
         squaredDistanceToChunk(cx, cz) < distance)) {
        frontierChunks.push_back(chunk);
        return 1;
    }
    return 0;
}

bool WorldManager::chunkInRenderDistance(int cx, int cz) const {
    return squaredDistanceToChunk(cx, cz) < MAX_RENDER_DISTANCE_METRES * MAX_RENDER_DISTANCE_METRES;
}

double WorldManager::squaredDistanceToChunk(int cx, int cz) const {
    double dx = camera.transform.position.x - (cx + 0.5) * CHUNK_SIZE;
    double dz = camera.transform.position.z - (cz + 0.5) * CHUNK_SIZE;
    return dx * dx + dz * dz;
}

size_t WorldManager::key(int i, int j) {
    return (size_t) i << 32 | (unsigned int) j;
}

void WorldManager::updateVerticesBuffer(GLuint &verticesBuffer, GLuint &chunkDataBuffer) {
    ZoneScoped;

    {
        std::unique_lock<std::mutex> lock(cvMutexNewlyCreatedChunks);
        cvNewlyCreatedChunks.wait(lock, [this]() { return newlyCreatedChunksReady; });

        newlyCreatedChunksReady = false;

        for (Chunk *chunk: newlyCreatedChunks) {
            // If the chunk was already destroyed in destroyFrontierChunks, we don't want to allocate, so just skip it
            if (chunk->destroyed) {
                continue;
            }

            // If the chunk wasn't created yet (TODO: somehow?)
            if (chunk->debug == 0) {
                continue;
            }

            auto region = allocator.allocate(chunk->numVertices);

            chunk->firstIndex = region.offset;

            glNamedBufferSubData(verticesBuffer,
                                 region.offset * sizeof(uint32_t),
                                 chunk->numVertices * sizeof(uint32_t),
                                 (const void *) chunk->vertices.data());

            chunk->vertices.clear();

            // Update chunk data
            ChunkData cd = {
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

            chunk->ready = true;
            chunk->debug = 3;
        }

        // Update chunk data buffer
        glNamedBufferData(chunkDataBuffer, sizeof(ChunkData) * chunkData.size(),
                          (const void *) chunkData.data(), GL_DYNAMIC_DRAW);

        // Clear newly created chunks
        newlyCreatedChunks.clear();

        newlyCreatedChunksReady = true;
        cvNewlyCreatedChunks.notify_one();  // only need to wake up one thread
    }
}

std::optional<Chunk *> WorldManager::getChunk(int cx, int cz) {
    auto it = chunkByCoords.find(key(cx, cz));
    if (it != chunkByCoords.end()) {
        return it->second;
    }
    return std::nullopt;
}

void WorldManager::queueMeshChunk(Chunk *chunk) {
    threadPool.queueTask([chunk, this]() {
        // Don't allocate while other threads are allocating
        {
            std::unique_lock<std::mutex> lock(cvMutexAllocator);
            cvAllocator.wait(lock, [this]() { return allocator.ready; });
            allocator.deallocate(chunk->firstIndex, chunk->numVertices);
        }

        chunk->initialising = true;
        Mesher::meshChunk(*chunk, generationType);
        chunk->initialising = false;

        // If newlyCreatedChunks is currently being iterated through, we need to lock the mutex
        {
            std::unique_lock<std::mutex> lock(cvMutexNewlyCreatedChunks);
            cvNewlyCreatedChunks.wait(lock, [this]() { return newlyCreatedChunksReady; });
            newlyCreatedChunks.push_back(chunk);
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

int WorldManager::load(int x, int y, int z) {
    int cx = x >> CHUNK_SIZE_SHIFT;
    int cz = z >> CHUNK_SIZE_SHIFT;

    Chunk *chunk = chunkByCoords[key(cx, cz)];
    if (chunk == nullptr || !chunk->ready) {
        return 0;
    }

    int lx = x - (cx << CHUNK_SIZE_SHIFT);
    int lz = z - (cz << CHUNK_SIZE_SHIFT);

    return chunk->voxels[getVoxelIndex(lx + 1, y + 1, lz + 1, CHUNK_SIZE + 2)];
}

void WorldManager::cleanup() {
    threadPool.stop();
}
