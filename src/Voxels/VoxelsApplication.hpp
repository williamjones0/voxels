#pragma once

#include "Application.hpp"

#include "core/Camera.hpp"
#include "opengl/Shader.h"
#include "world/Chunk.hpp"
#include "world/WorldMesh.hpp"

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
    int chunkIndex;
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

    Camera camera;

    bool wireframe = false;

    Shader shader;
    Shader drawCommandProgram;

    std::vector<Chunk> chunks;
    std::vector<Chunk> frontierChunks;
    std::vector<ChunkData> chunkData;
    WorldMesh worldMesh;

    GLuint chunkDrawCmdBuffer;
    GLuint chunkDataBuffer;
    GLuint commandCountBuffer;
    GLuint verticesBuffer;
};
