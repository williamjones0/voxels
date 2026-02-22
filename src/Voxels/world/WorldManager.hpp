#pragma once

#include "Chunk.hpp"
#include "Mesher.hpp"
#include "../core/FreeListAllocator.hpp"
#include "../core/ThreadPool.hpp"
#include "VertexFormat.hpp"
#include "Primitive.hpp"

#include <glad/glad.h>

#include <array>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
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

struct RaycastResult {
    int cx;
    int cz;
    int x;
    int y;
    int z;
    int face;
};

constexpr int InitialVertexBufferSize = 1 << 20;
constexpr int MaxChunkTasks = 32;

constexpr int MaxRenderDistanceChunks = 8;
constexpr int MaxRenderDistanceMetres = MaxRenderDistanceChunks << ChunkSizeShift;
constexpr int MaxChunks = (2 * MaxRenderDistanceChunks + 1) * (2 * MaxRenderDistanceChunks + 1);

class WorldManager {
public:
    explicit WorldManager(
        std::function<size_t(size_t)> outOfCapacityCallback,
        GenerationType generationType = GenerationType::Perlin2D,
        const std::filesystem::path& levelFile = "data/levels/level0.txt"
    );

    bool updateFrontierChunks(glm::vec3 position);
    void destroyFrontierChunks(glm::vec3 position);
    bool ensureChunkIfVisible(glm::vec3 position, int cx, int cz);
    Chunk* ensureChunk(int cx, int cz);
    Chunk* createChunk(int cx, int cz);
    void applyEditsToChunk(Chunk* chunk);
    void addFrontier(Chunk* chunk);
    void updateFrontierNeighbour(Chunk* frontier, int cx, int cz);
    bool createNewFrontierChunks(glm::vec3 position);
    int onFrontierChunkRemoved(glm::vec3 position, const Chunk* frontierChunk);
    int onFrontierChunkRemoved(glm::vec3 position, int cx, int cz, double distance);
    bool chunkInRenderDistance(glm::vec3 position, int cx, int cz) const;
    double squaredDistanceToChunk(glm::vec3 position, int cx, int cz) const;
    static size_t key(int i, int j);

    void updateVerticesBuffer(const GLuint& verticesBuffer, const GLuint& chunkDataBuffer);
    Chunk* getChunk(int cx, int cz);

    void queueMeshChunk(Chunk* chunk);

    void saveLevel();
    void loadLevel();

    int load(int x, int y, int z);

    std::optional<RaycastResult> raycast(glm::vec3 origin, glm::vec3 direction, int maxSteps);
    void tryStoreVoxel(int cx, int cz, int x, int y, int z, int place, std::unordered_set<Chunk*>& chunksToMesh);
    void updateVoxel(RaycastResult result, bool place);
    void updateVoxels(Primitive::EditMap& edits);
    void addPrimitive(std::unique_ptr<Primitive> primitive);
    void placePrimitive(Primitive& primitive);
    void removePrimitive(size_t index);
    void movePrimitive(size_t index, const glm::ivec3& newOrigin);

    void cleanup();

    GenerationType generationType;
    const std::filesystem::path levelFile;
    std::optional<std::pair<glm::ivec2, glm::ivec2>> levelChunkBounds;

    std::array<glm::vec3, 1 << VertexFormat::ColourBits> palette{};
    size_t paletteIndex = 0;

    std::vector<std::unique_ptr<Primitive>> primitives;
    Primitive::UserEditMap userEdits;  // global pos -> voxelType

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
    std::function<size_t(size_t)> outOfCapacityCallback;
};
