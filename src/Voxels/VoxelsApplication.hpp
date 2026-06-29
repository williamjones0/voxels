#pragma once

#include "Application.hpp"

#include "entity/Entity.hpp"
#include "opengl/Renderer.hpp"
#include "ui/UIManager.hpp"
#include "world/Chunk.hpp"
#include "world/WorldManager.hpp"

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

    void saveScreenshot();

    Entity player;
    Entity camera;

    bool wireframe = false;
    bool noclip = true;

    WorldManager worldManager = WorldManager(
        [this](const size_t size) {
            return renderer.enlargeVerticesBuffer(size);
        },
        GenerationType::None,
        std::filesystem::path(PROJECT_SOURCE_DIR) / "data/levels/ztndm4.json"
    );

    UIManager uiManager;

    Renderer renderer{windowWidth, windowHeight};

    bool firstFrame = true;
};
