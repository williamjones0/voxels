#pragma once

#include "Chunk.hpp"
#include "Mesher.hpp"
#include "../core/FreeListAllocator.hpp"
#include "../core/ThreadPool.hpp"
#include "../Palette.hpp"
#include "Primitive.hpp"

#include <glad/glad.h>

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
    unsigned int lightmapIndex;
    unsigned int _pad0;
};

struct RaycastResult {
    int cx;
    int cz;
    int x;
    int y;
    int z;
    int face;
};

struct VoxelInfo {
    bool valid;   // true when data is available for this world coordinate
    int type;     // voxel / palette index (0 = empty / air)
    int torchlight;
    int sunlight;
};

constexpr int InitialVertexBufferSize = 1 << 20;
constexpr int MaxChunkTasks = 32;

constexpr int MaxRenderDistanceChunks = 16;
constexpr int MaxRenderDistanceMetres = MaxRenderDistanceChunks << ChunkSizeShift;
constexpr int MaxChunks = (2 * MaxRenderDistanceChunks + 1) * (2 * MaxRenderDistanceChunks + 1);

constexpr int InitialLightmapBufferSize = VoxelsSize * MaxChunks;

class WorldManager {
public:
    WorldManager(
        std::function<size_t(size_t)> vertexBufferOutOfCapacityCallback,
        std::function<size_t(size_t)> lightmapBufferOutOfCapacityCallback,
        GenerationType generationType = GenerationType::Perlin2D,
        std::filesystem::path levelFile = "data/levels/level0.txt"
    );

    bool updateFrontierChunks(glm::vec3 position);
    void destroyFrontierChunks(glm::vec3 position);
    bool ensureChunkIfVisible(glm::vec3 position, int cx, int cz);
    std::shared_ptr<Chunk> ensureChunk(int cx, int cz);
    std::shared_ptr<Chunk> createChunk(int cx, int cz);
    void applyEdits(int cx, int cz, Chunk::GenerationResult& result);
    void addFrontier(const std::shared_ptr<Chunk>& chunk);
    void updateFrontierNeighbour(const std::shared_ptr<Chunk>& frontier, int cx, int cz);
    bool createNewFrontierChunks(glm::vec3 position);
    int onFrontierChunkRemoved(glm::vec3 position, const std::shared_ptr<Chunk>& frontierChunk);
    int onFrontierChunkRemoved(glm::vec3 position, int cx, int cz, double distance);
    bool chunkInRenderDistance(glm::vec3 position, int cx, int cz) const;
    double squaredDistanceToChunk(glm::vec3 position, int cx, int cz) const;
    static size_t key(int i, int j);

    void updateGeneratedChunks(GLuint lightmapBuffer, GLuint chunkDataBuffer);
    void updateVerticesBuffer(GLuint verticesBuffer, GLuint chunkDataBuffer);
    void updateLightmapBuffer(GLuint lightmapBuffer, GLuint chunkDataBuffer, Chunk& chunk);

    std::shared_ptr<Chunk> getChunk(int cx, int cz);
    std::shared_ptr<Chunk> getChunkFromWorld(int x, int z);

    void queueGenerateChunk(std::shared_ptr<Chunk> chunk);
    void queueMeshChunk(std::shared_ptr<Chunk> chunk);

    void saveLevel();
    void loadLevel();

    int load(int x, int y, int z);

    std::optional<RaycastResult> raycast(glm::vec3 origin, glm::vec3 direction, int maxSteps);
    void tryStoreVoxel(int cx, int cz, int x, int y, int z, int place, std::unordered_set<std::shared_ptr<Chunk>>& chunksToMesh);
    void updateVoxel(RaycastResult result, bool place);
    void updateVoxels(Primitive::EditMap& edits);
    void addPrimitive(std::unique_ptr<Primitive> primitive);
    void placePrimitive(Primitive& primitive);
    void removePrimitive(size_t index);
    void movePrimitive(size_t index, const glm::ivec3& newOrigin);

    int getLight(int x, int y, int z, bool isSun);
    void setLight(int x, int y, int z, int val, bool isSun);

    std::unordered_set<std::shared_ptr<Chunk>> propagateLight(std::vector<LightNode> queue, bool isSun);
    std::unordered_set<std::shared_ptr<Chunk>> removeLight(int x, int y, int z, bool isSun);

    VoxelInfo getVoxelInfoAtWorld(int worldX, int worldY, int worldZ) const;

    void cleanup();

    GenerationType generationType;
    const std::filesystem::path levelFile;
    std::optional<std::pair<glm::ivec2, glm::ivec2>> levelChunkBounds;

    Palette palette;

    std::vector<std::unique_ptr<Primitive>> primitives;
    Primitive::UserEditMap userEdits;  // global pos -> voxelType

    std::vector<std::shared_ptr<Chunk>> chunks;
    std::vector<std::shared_ptr<Chunk>> frontierChunks;
    std::unordered_map<size_t, std::shared_ptr<Chunk>> chunkByCoords;
    std::vector<ChunkData> chunkData;

    std::vector<Chunk::GenerationResult> pendingGenerationResults;
    std::vector<Mesher::MeshResult> pendingMeshResults;

    std::atomic<int> chunkTasksCount = 0;

    std::mutex pendingGenerationResultsMutex;
    std::mutex pendingMeshResultsMutex;

    FreeListAllocator vertexBufferAllocator;
    FreeListAllocator lightmapBufferAllocator;
    ThreadPool threadPool;
};
