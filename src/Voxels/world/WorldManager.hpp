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

struct PaletteEntry {
    glm::vec3 colour = glm::vec3(0.0f);

    std::string texturePath;
    bool useTexture = false;

    glm::vec2 uvOffset = glm::vec2(0.0f);
    glm::vec2 uvScale  = glm::vec2(1.0f);
};

struct GPUPaletteEntry {
    glm::vec4 colour;
    glm::vec4 uv;  // xy = offset, zw = scale
    int hasTexture;
    int _pad[3];
};

class TextureAtlas {
public:
    struct Region {
        glm::vec2 offset;
        glm::vec2 scale;
    };

    size_t addTexture(const std::string& path);
    [[nodiscard]] const Region& getRegion(const size_t index) const {
        return regions[index];
    }

    void upload();
    void clear();

    GLuint textureID = 0;

private:
    struct PendingTexture {
        std::string path;
        int width;
        int height;
        std::vector<unsigned char> data;
    };

    std::vector<Region> regions;
    std::vector<PendingTexture> textures;
};

constexpr int InitialVertexBufferSize = 1 << 20;
constexpr int MaxChunkTasks = 32;

constexpr int MaxRenderDistanceChunks = 16;
constexpr int MaxRenderDistanceMetres = MaxRenderDistanceChunks << ChunkSizeShift;
constexpr int MaxChunks = (2 * MaxRenderDistanceChunks + 1) * (2 * MaxRenderDistanceChunks + 1);

class WorldManager {
public:
    explicit WorldManager(
        std::function<size_t(size_t)> outOfCapacityCallback,
        GenerationType generationType = GenerationType::Perlin2D,
        std::filesystem::path levelFile = "data/levels/level0.txt"
    );

    void rebuildAtlas();

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

    void updateGeneratedChunks();
    void updateVerticesBuffer(const GLuint& verticesBuffer, const GLuint& chunkDataBuffer);
    std::shared_ptr<Chunk> getChunk(int cx, int cz);

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

    void cleanup();

    GenerationType generationType;
    const std::filesystem::path levelFile;
    std::optional<std::pair<glm::ivec2, glm::ivec2>> levelChunkBounds;

    TextureAtlas atlas;
    std::array<PaletteEntry, 1 << VertexFormat::ColourBits> palette{};
    size_t paletteIndex = 0;

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

    FreeListAllocator allocator;
    ThreadPool threadPool;

private:
    std::function<size_t(size_t)> outOfCapacityCallback;
};
