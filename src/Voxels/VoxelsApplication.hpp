#pragma once

#include "Application.hpp"

#include "core/Camera.hpp"
#include "core/FreeListAllocator.hpp"
#include "opengl/Shader.h"
#include "world/Chunk.hpp"
#include "io/Input.hpp"
#include "player/Q3PlayerController.hpp"
#include "world/WorldManager.hpp"

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
    int x;
    int y;
    int z;
    int face;
} RaycastResult;

class VoxelsApplication final : public Application {
protected:
    bool init() override;
    bool load() override;
    void update() override;
    void render() override;
    void processInput() override;
    void cleanup() override;

private:
    std::optional<RaycastResult> raycast();
    void updateVoxel(RaycastResult result, bool place);

    Camera camera = Camera(glm::vec3(8.0f, 400.0f, 8.0f));
    CharacterController characterController = CharacterController(worldManager);
    Q3PlayerController playerController = Q3PlayerController(camera, characterController);

    bool wireframe = false;

    Shader shader;
    Shader drawCommandProgram;

    WorldManager worldManager = WorldManager(camera);

    GLuint dummyVAO;
    GLuint chunkDrawCmdBuffer;
    GLuint chunkDataBuffer;
    GLuint commandCountBuffer;
    GLuint verticesBuffer;
};
