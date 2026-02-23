#pragma once

#include "Application.hpp"

#include "opengl/Shader.h"
#include "world/Chunk.hpp"
#include "world/WorldManager.hpp"
#include "entity/Entity.hpp"
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

    std::unique_ptr<Entity> player;
    std::unique_ptr<Entity> camera;

    bool wireframe = false;
    bool noclip = true;

    Shader shader{};
    Shader drawCommandProgram{};

    WorldManager worldManager = WorldManager(
        [this](const size_t size) {
            return enlargeVerticesBuffer(size);
        },
        GenerationType::Perlin2D,
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
