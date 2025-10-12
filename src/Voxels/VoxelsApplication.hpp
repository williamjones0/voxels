#pragma once

#include "Application.hpp"

#include "core/Camera.hpp"
#include "core/FreeListAllocator.hpp"
#include "opengl/Shader.h"
#include "world/Chunk.hpp"
#include "io/Input.hpp"
#include "world/WorldManager.hpp"
#include "player/Q1PlayerController.hpp"
#include "player/Q3PlayerController.hpp"
#include "player/FlyPlayerController.hpp"

#include <list>

struct ChunkDrawCommand {
    unsigned int count;
    unsigned int instanceCount;
    unsigned int firstIndex;
    unsigned int baseInstance;
    unsigned int chunkIndex;
};

struct RaycastResult {
    int cx;
    int cz;
    int x;
    int y;
    int z;
    int face;
};

class VoxelsApplication final : public Application {
protected:
    bool init() override;
    bool load() override;
    void update() override;
    void render() override;
    void processInput() override;
    void cleanup() override;

private:
    void setupInput();

    std::optional<RaycastResult> raycast();
    void tryStoreVoxel(int cx, int cz, int x, int y, int z, bool place, std::vector<Chunk *> &chunksToMesh);
    void updateVoxel(RaycastResult result, bool place);

    size_t enlargeVerticesBuffer(size_t newCapacity);

    Camera camera = Camera(glm::vec3(8.0f, 400.0f, 8.0f));
    CharacterController characterController = CharacterController(worldManager);
//    Q1PlayerController playerController = Q1PlayerController(camera, characterController);
    Q3PlayerController playerController = Q3PlayerController(camera, characterController);
    // FlyPlayerController playerController = FlyPlayerController(camera);

    bool wireframe = false;

    Shader shader;
    Shader drawCommandProgram;

    WorldManager worldManager = WorldManager(
            camera,
            [this](size_t size) {
                return enlargeVerticesBuffer(size);
            },
            GenerationType::Perlin2D,
            std::filesystem::path(PROJECT_SOURCE_DIR) / "data/levels/level0.json"
    );

    GLuint dummyVAO;
    GLuint chunkDrawCmdBuffer;
    GLuint chunkDataBuffer;
    GLuint commandCountBuffer;
    GLuint verticesBuffer;

    bool background;
};
