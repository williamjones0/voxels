#pragma once

#include "Chunk.hpp"
#include "Mesher.hpp"
#include "../core/FreeListAllocator.hpp"
#include "../core/ThreadPool.hpp"
#include "../core/Camera.hpp"
#include "Level.hpp"
#include "VertexFormat.hpp"

#include <glad/glad.h>

#include <atomic>
#include <condition_variable>
#include <filesystem>
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

constexpr int InitialVertexBufferSize = 1 << 20;
constexpr int MaxChunkTasks = 32;

constexpr int MaxRenderDistanceChunks = 32;
constexpr int MaxRenderDistanceMetres = MaxRenderDistanceChunks << ChunkSizeShift;
constexpr int MaxChunks = (2 * MaxRenderDistanceChunks + 1) * (2 * MaxRenderDistanceChunks + 1);

class WorldManager {
public:
    WorldManager(
        Camera& camera,
        std::function<size_t(size_t)> outOfCapacityCallback,
        GenerationType generationType = GenerationType::Perlin2D,
        const std::filesystem::path& levelFile = "data/levels/level0.txt"
    );

    bool updateFrontierChunks();
    void destroyFrontierChunks();
    bool ensureChunkIfVisible(int cx, int cz);
    Chunk* ensureChunk(int cx, int cz);
    Chunk* createChunk(int cx, int cz);
    void addFrontier(Chunk* chunk);
    void updateFrontierNeighbour(Chunk* frontier, int cx, int cz);
    bool createNewFrontierChunks();
    int onFrontierChunkRemoved(const Chunk* frontierChunk);
    int onFrontierChunkRemoved(int cx, int cz, double distance);
    bool chunkInRenderDistance(int cx, int cz) const;
    double squaredDistanceToChunk(int cx, int cz) const;
    static size_t key(int i, int j);

    void updateVerticesBuffer(const GLuint& verticesBuffer, const GLuint& chunkDataBuffer);
    Chunk* getChunk(int cx, int cz);

    void queueMeshChunk(Chunk* chunk);

    void save();

    int load(int x, int y, int z);

    void cleanup();

    GenerationType generationType;
    const std::filesystem::path levelFile;

    Level level;

    std::array<glm::vec3, 1 << VertexFormat::ColourBits> palette;
    size_t paletteIndex = 0;

    std::vector<Chunk*> chunks;
    std::vector<Chunk*> frontierChunks;
    std::vector<Mesher::MeshResult> pendingMeshResults;
    std::unordered_map<size_t, Chunk*> chunkByCoords;
    std::vector<ChunkData> chunkData;

    std::atomic<int> chunkTasksCount = 0;

    std::mutex pendingMeshResultsMutex;

    FreeListAllocator allocator;
    ThreadPool threadPool;

private:
    Camera& camera;

    std::function<size_t(size_t)> outOfCapacityCallback;
};
