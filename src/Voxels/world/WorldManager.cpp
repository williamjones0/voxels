#include "WorldManager.hpp"

#include "Mesher.hpp"
#include "../util/Util.hpp"

WorldManager::WorldManager(Camera &camera) : camera(camera) {
    load();
}

void WorldManager::load() {
    chunks = std::list<Chunk>();
    frontierChunks = std::vector<Chunk *>();
    newlyCreatedChunks = std::vector<Chunk *>();
    chunkByCoords = std::unordered_map<size_t, Chunk *>();
    chunkData = std::vector<ChunkData>();

    threadPool.start();
}

bool WorldManager::updateFrontierChunks() {
    destroyFrontierChunks();
    return createNewFrontierChunks();
}

bool WorldManager::createNewFrontierChunks() {
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
    std::vector<Chunk *> toDestroy;

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
            allocator.deallocate(chunk->firstIndex, chunk->numVertices);
        }

        toDestroy.push_back(chunk);
        chunk->dying = true;
        s += numPromoted - 1;
        i--;
    }

    // Ensure that we can erase properly
    std::sort(toDestroy.begin(), toDestroy.end(), [](Chunk *a, Chunk *b) {
        return a->index > b->index;
    });

    // Erase the relevant data from the buffers, remove the chunks from the chunks vector
    // and update frontierChunks and chunkByCoords indices based on the removed chunks
    for (Chunk *chunk : toDestroy) {
        chunkData.erase(chunkData.begin() + chunk->index);

        chunks.remove(*chunk);
    }

    // Update chunk indexes in case chunks were removed
    for (auto it = chunks.begin(); it != chunks.end(); ++it) {
        it->index = std::distance(chunks.begin(), it);
    }
}

bool WorldManager::ensureChunkIfVisible(int cx, int cz) {
    if (!chunkInRenderDistance(cx, cz)) {
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
    chunks.emplace_back(cx, cz);
    Chunk &chunk = chunks.back();
    size_t chunkIndex = chunks.size() - 1;
    chunk.index = chunkIndex;
    chunkByCoords[key(cx, cz)] = &chunk;
    addFrontier(&chunk);
    chunkData.push_back({
            .cx = cx,
            .cz = cz,
            .minY = CHUNK_HEIGHT,
            .maxY = 0,
            .numVertices = 0,
            .firstIndex = 0,
            ._pad0 = 0,
            ._pad1 = 0,
    });

    chunkTasksCount++;

    threadPool.queueTask([&chunk, this]() {
        {
            std::unique_lock<std::mutex> lock(cvMutexChunks);
            cvChunks.wait(lock, [this]() { return chunksReady; });
        }

        chunk.init();
        chunk.generate(GenerationType::PERLIN_2D);

        Mesher::meshChunk(chunk);

        chunk.initialising = false;

        chunk.debug = 1;

        // If newlyCreatedChunks is currently being iterated through, we need to lock the mutex
        {
            std::unique_lock<std::mutex> lock(cvMutexNewlyCreatedChunks);
            cvNewlyCreatedChunks.wait(lock, [this]() { return newlyCreatedChunksReady; });
        }
        chunk.debug = 2;
        newlyCreatedChunks.push_back(&chunk);
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

void WorldManager::updateVerticesBuffer(GLuint verticesBuffer, GLuint chunkDataBuffer) {
    newlyCreatedChunksReady = false;
    cvNewlyCreatedChunks.notify_all();

    // Sort newly created chunks by chunk.index so that chunkData is updated in the correct order
    std::sort(newlyCreatedChunks.begin(), newlyCreatedChunks.end(), [](Chunk *a, Chunk *b) {
        return a->index < b->index;
    });

    for (Chunk *chunk : newlyCreatedChunks) {
        // If the chunk was already destroyed in destroyFrontierChunks, we don't want to allocate, so just skip it
        if (chunk->dying) {
            continue;
        }

        // If the chunk wasn't created yet (TODO: somehow?)
        if (chunk->debug == 0) {
            continue;
        }

        auto region = allocator.allocate(chunk->numVertices);
        if (!region) {
            throw std::runtime_error("Failed to allocate memory for chunk vertices.");
        }

        chunk->firstIndex = region->offset;

        // std::copy(chunk->vertices.begin(), chunk->vertices.end(), dummyVerticesBuffer.begin() + chunk->firstIndex);

        glNamedBufferSubData(verticesBuffer,
                             region->offset * sizeof(uint32_t),
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
    cvNewlyCreatedChunks.notify_all();
}

Chunk *WorldManager::getChunk(int cx, int cz) {
    return chunkByCoords[key(cx, cz)];
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
