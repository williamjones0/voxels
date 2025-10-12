#pragma once

#include "Chunk.hpp"
#include "../core/FreeListAllocator.hpp"
#include "../core/ThreadPool.hpp"
#include "../core/Camera.hpp"
#include "Level.hpp"

#include <glad/glad.h>

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>

struct ChunkData {
    int cx;
    int cz;
    int minY;
    int maxY;
    unsigned int numVertices;
    unsigned int firstIndex;
    unsigned int _pad0;
    unsigned int _pad1;
};

constexpr int INITIAL_VERTEX_BUFFER_SIZE = 1 << 20;
constexpr int MAX_CHUNK_TASKS = 32;

constexpr int MAX_RENDER_DISTANCE_CHUNKS = 8;
constexpr int MAX_RENDER_DISTANCE_METRES = MAX_RENDER_DISTANCE_CHUNKS << CHUNK_SIZE_SHIFT;
constexpr int MAX_CHUNKS = (2 * MAX_RENDER_DISTANCE_CHUNKS + 1) * (2 * MAX_RENDER_DISTANCE_CHUNKS + 1);

class WorldManager {
public:
    WorldManager(Camera &camera, std::function<size_t(size_t)> outOfCapacityCallback, GenerationType generationType = GenerationType::Perlin2D,
                          const std::filesystem::path &levelFile = "data/levels/level0.txt");

    bool updateFrontierChunks();
    void destroyFrontierChunks();
    bool ensureChunkIfVisible(int cx, int cz);
    Chunk *ensureChunk(int cx, int cz);
    Chunk *createChunk(int cx, int cz);
    void addFrontier(Chunk *chunk);
    void updateFrontierNeighbour(Chunk *frontier, int cx, int cz);
    bool createNewFrontierChunks();
    int onFrontierChunkRemoved(Chunk *frontierChunk);
    int onFrontierChunkRemoved(int cx, int cz, double distance);
    bool chunkInRenderDistance(int cx, int cz) const;
    double squaredDistanceToChunk(int cx, int cz) const;
    static size_t key(int i, int j);

    void updateVerticesBuffer(GLuint &verticesBuffer, GLuint &chunkDataBuffer);
    std::optional<Chunk *> getChunk(int cx, int cz);

    void queueMeshChunk(Chunk *chunk);

    void save();

    int load(int x, int y, int z);

    void cleanup();

    GenerationType generationType;
    const std::filesystem::path levelFile;

    Level level;

    std::vector<Chunk *> chunks;
    std::vector<Chunk *> frontierChunks;
    std::vector<Chunk *> newlyCreatedChunks;
    std::unordered_map<size_t, Chunk *> chunkByCoords;
    std::vector<ChunkData> chunkData;

    std::atomic<int> chunkTasksCount = 0;

    std::mutex cvMutexNewlyCreatedChunks;

    FreeListAllocator allocator;
    ThreadPool threadPool;

private:
    Camera &camera;
    std::vector<uint32_t> dummyVerticesBuffer;

    std::function<size_t(size_t)> outOfCapacityCallback;
};
