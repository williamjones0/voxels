#pragma once

#include "Application.hpp"

#include "core/Camera.hpp"
#include "core/FreeListAllocator.hpp"
#include "opengl/Shader.h"
#include "world/Chunk.hpp"
#include "world/WorldManager.hpp"
#include "player/Q1PlayerController.hpp"
#include "player/Q3PlayerController.hpp"
#include "player/FlyPlayerController.hpp"
#include "ui/UIManager.hpp"

struct ChunkDrawCommand {
    unsigned int count;
    unsigned int instanceCount;
    unsigned int firstIndex;
    unsigned int baseInstance;
    unsigned int chunkIndex;
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
    void setupUI();

    size_t enlargeVerticesBuffer(size_t currentCapacity);

    Camera camera = Camera(glm::vec3(8.0f, 100.0f, 8.0f));
    CharacterController characterController = CharacterController(worldManager);
    // Q1PlayerController playerController = Q1PlayerController(camera, characterController);
    // Q3PlayerController playerController = Q3PlayerController(camera, characterController);
    FlyPlayerController playerController = FlyPlayerController(camera);

    bool wireframe = false;

    Shader shader{};
    Shader drawCommandProgram{};

    WorldManager worldManager = WorldManager(
        camera,
        [this](const size_t size) {
            return enlargeVerticesBuffer(size);
        },
        GenerationType::None,
        std::filesystem::path(PROJECT_SOURCE_DIR) / "data/levels/new_level2.json"
    );

    UIManager uiManager;

    GLuint dummyVAO = 0;
    GLuint chunkDrawCmdBuffer = 0;
    GLuint chunkDataBuffer = 0;
    GLuint commandCountBuffer = 0;
    GLuint verticesBuffer = 0;

    bool background = false;
    bool firstFrame = true;
};
