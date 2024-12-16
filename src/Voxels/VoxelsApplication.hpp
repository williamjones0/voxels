#pragma once

#include "Application.hpp"

#include "core/Camera.hpp"
#include "core/FreeListAllocator.hpp"
#include "opengl/Shader.h"
#include "world/Chunk.hpp"

#include <list>

typedef struct {
    unsigned int count;
    unsigned int instanceCount;
    unsigned int firstIndex;
    unsigned int baseInstance;
    unsigned int chunkIndex;
} ChunkDrawCommand;

typedef struct {
    int cx;
    int cz;
    int minY;
    int maxY;
    unsigned int numVertices;
    unsigned int firstIndex;
    unsigned int _pad0;
    unsigned int _pad1;
} ChunkData;

typedef struct {
    Chunk *chunk;
    int x;
    int y;
    int z;
    int face;
} RaycastResult;

class VoxelsApplication final : public Application {
public:
    void key_callback(int key, int scancode, int action, int mods) override;
    void mouse_button_callback(int button, int action, int mods) override;
    void cursor_position_callback(double xposIn, double yposIn) override;
    void scroll_callback(double xoffset, double yoffset) override;

protected:
    bool init() override;
    bool load() override;
    void update() override;
    void render() override;
    void processInput() override;
    void cleanup() override;

private:
    RaycastResult raycast();
    void updateVoxel(RaycastResult result, bool place);
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
    bool chunkInRenderDistance(int cx, int cz);
    double squaredDistanceToChunk(int cx, int cz) const;
    static size_t key(int i, int j);

    void updateVerticesBuffer();

    Camera camera;

    bool wireframe = false;

    Shader shader;
    Shader drawCommandProgram;

    std::list<Chunk> chunks;
    std::vector<Chunk *> frontierChunks;
    std::vector<Chunk *> newlyCreatedChunks;
    std::unordered_map<size_t, Chunk *> chunkByCoords;
    std::vector<ChunkData> chunkData;

    std::atomic<int> chunkTasksCount = 0;
    std::condition_variable cv;
    std::condition_variable cvNewlyCreatedChunks;
    std::mutex cvMutex;
    bool chunksReady = false;
    bool newlyCreatedChunksReady = false;

    FreeListAllocator allocator;
    std::vector<uint32_t> dummyVerticesBuffer;

    GLuint dummyVAO;
    GLuint chunkDrawCmdBuffer;
    GLuint chunkDataBuffer;
    GLuint commandCountBuffer;
    GLuint verticesBuffer;
};
