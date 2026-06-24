#include "VoxelsApplication.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <tracy/Tracy.hpp>
#include <tracy/TracyOpenGL.hpp>

#include "entity/components/CameraProperties.hpp"
#include "entity/components/CharacterController.hpp"
#include "entity/components/FlyPlayerController.hpp"
#include "entity/components/Q1PlayerController.hpp"
#include "entity/components/Q3PlayerController.hpp"
#include "entity/components/Transform.hpp"
#include "io/Input.hpp"

bool VoxelsApplication::init() {
    if (!Application::init()) {
        return false;
    }

    glfwSetWindowUserPointer(windowHandle, this);

    glfwSetKeyCallback(windowHandle, [](GLFWwindow* window, const int key, const int scancode, const int action, const int mods) {
        Input::key_callback(key, scancode, action, mods);
    });
    glfwSetMouseButtonCallback(windowHandle, [](GLFWwindow* window, const int button, const int action, const int mods) {
        Input::mouse_button_callback(button, action, mods);
    });
    glfwSetCursorPosCallback(windowHandle, [](GLFWwindow* window, const double xpos, const double ypos) {
        Input::cursor_position_callback(xpos, ypos);
    });
    glfwSetScrollCallback(windowHandle, [](GLFWwindow* window, const double xoffset, const double yoffset) {
        Input::scroll_callback(xoffset, yoffset);
    });

    return true;
}

bool VoxelsApplication::load() {
    if (!Application::load()) {
        return false;
    }

    player = Entity();
    player.add<Transform>(glm::vec3(8.0f, 80.0f, 8.0f));
    player.add<Kinematics>();
    if (noclip) {
        player.add<FlyPlayerController>();
    } else {
        player.add<Q1PlayerController>();
        player.add<CharacterController>(worldManager, true);
    }

    camera = Entity();
    camera.add<CameraProperties>();

    worldManager.createChunk(0, 0);

    renderer.load(worldManager);

    worldManager.updateVerticesBuffer(renderer.verticesBuffer, renderer.chunkDataBuffer);

    setupInput();

    uiManager.load(windowHandle);

    setupUI();

    return true;
}

void VoxelsApplication::setupInput() {
    Input::bindings.insert({{GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, true}, {ActionType::Break, ActionStateType::None}});
    Input::bindings.insert({{GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, true}, {ActionType::Place, ActionStateType::None}});

    Input::bindings.insert({{GLFW_KEY_ESCAPE, GLFW_PRESS}, {ActionType::Exit, ActionStateType::None}});
    Input::bindings.insert({{GLFW_KEY_T, GLFW_PRESS}, {ActionType::ToggleWireframe, ActionStateType::None}});
    Input::bindings.insert({{GLFW_KEY_P, GLFW_PRESS}, {ActionType::SaveLevel, ActionStateType::None}});
    Input::bindings.insert({{GLFW_KEY_LEFT_BRACKET, GLFW_PRESS}, {ActionType::LoadLevel, ActionStateType::None}});

    Input::bindings.insert({{Input::uiToggleKey, GLFW_PRESS}, {ActionType::ToggleUIMode, ActionStateType::None}});
    Input::bindings.insert({{GLFW_MOUSE_BUTTON_4, GLFW_PRESS, true}, {ActionType::ToggleNoclip, ActionStateType::None}});

    for (int i = 0; i < worldManager.palette.size(); ++i) {
        Input::bindings.insert({{GLFW_KEY_1 + i, GLFW_PRESS}, {ActionType::SelectPaletteIndex, ActionStateType::None, i}});
    }

    // Register action callbacks
    Input::registerCallback({ActionType::Break, ActionStateType::None}, [this] {
        if (const auto result = worldManager.raycast(player.get<Transform>()->position, getFront(player.get<Transform>()->angles), 16)) {
            worldManager.propagateTorchLight((result->cx << ChunkSizeShift) + result->x, result->y, (result->cz << ChunkSizeShift) + result->z, 15);
            // worldManager.updateVoxel(*result, false);
        }
    });

    Input::registerCallback({ActionType::Place, ActionStateType::None}, [this] {
        if (const auto result = worldManager.raycast(player.get<Transform>()->position, getFront(player.get<Transform>()->angles), 16)) {
            worldManager.updateVoxel(*result, true);
        }
    });

    Input::registerCallback({ActionType::Exit, ActionStateType::None}, [this] {
        glfwSetWindowShouldClose(windowHandle, true);
    });

    Input::registerCallback({ActionType::ToggleWireframe, ActionStateType::None}, [this] {
        wireframe = !wireframe;
        if (wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    });

    Input::registerCallback({ActionType::SaveLevel, ActionStateType::None}, [this] {
        worldManager.saveLevel();
    });

    Input::registerCallback({ActionType::LoadLevel, ActionStateType::None}, [this] {
        worldManager.loadLevel();
        worldManager.palette.uploadToGPU();
    });

    Input::registerCallback({ActionType::ToggleUIMode, ActionStateType::None}, [this] {
        Input::uiMode = !Input::uiMode;

        // If the UI mode is being entered, then for all current actions, call the corresponding stop action
        if (Input::uiMode == true) {
            Input::clearCurrentActions();
        }

        // Remove title bar highlight
        uiManager.clearWindowFocus();

        if (Input::uiMode) {
            glfwSetInputMode(windowHandle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(windowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    });

    Input::registerCallback({ActionType::ToggleNoclip, ActionStateType::None}, [this] {
        noclip = !noclip;

        if (noclip) {
            player.remove<Q1PlayerController>();
            player.remove<CharacterController>();
            player.add<FlyPlayerController>();
        } else {
            player.remove<FlyPlayerController>();
            player.add<Q1PlayerController>();
            player.add<CharacterController>(worldManager);
        }
    });

    for (size_t i = 0; i < worldManager.palette.size(); ++i) {
        Input::registerCallback({ActionType::SelectPaletteIndex, ActionStateType::None, static_cast<int>(i)}, [this, i] {
            worldManager.palette.index = i;
        });
    }
}

void VoxelsApplication::setupUI() {
    uiManager.registerStats(worldManager, player, deltaTime);
    uiManager.registerController(player);
    uiManager.registerCamera(camera);
    uiManager.registerDemo();
    uiManager.registerPalette(worldManager.palette);
    uiManager.registerPrimitives(worldManager, player);
    uiManager.registerLookAt(worldManager, player);
}

void VoxelsApplication::update() {
    ZoneScoped;

    Application::update();

    while (worldManager.updateFrontierChunks(player.get<Transform>()->position)) {}

    // If any chunks have finished generating, update their voxel field
    worldManager.updateGeneratedChunks();

    worldManager.chunkTasksCount = 0;

    worldManager.updateVerticesBuffer(renderer.verticesBuffer, renderer.chunkDataBuffer);

    player.get<PlayerController>()->update(deltaTime);

    const glm::vec3 playerPosition = player.get<Transform>()->position;
    const glm::vec3 velocity = player.get<Kinematics>()->velocity;
    const float currentSpeed = glm::length(glm::vec3{velocity.x, 0, velocity.z});

    // std::cout << "Frame time: " << deltaTime << "\t FPS: " << (1.0f / deltaTime) << std::endl;
    const std::string title = "Voxels | FPS: " + std::to_string(static_cast<int>(1.0f / deltaTime)) +
                        " | X: " + std::to_string(playerPosition.x) +
                        ", Y: " + std::to_string(playerPosition.y) +
                        ", Z: " + std::to_string(playerPosition.z) +
                        " | speed: " + std::to_string(currentSpeed) +
                        " | vel: " + glm::to_string(velocity) +
                        " | front: " + glm::to_string(getFront(player.get<Transform>()->angles));

    glfwSetWindowTitle(windowHandle, title.c_str());
}

void VoxelsApplication::render() {
    ZoneScoped;

    uiManager.beginFrame();
    uiManager.drawWindows();

    if (firstFrame) {
        // Remove title bar highlight on first frame
        uiManager.clearWindowFocus();
        firstFrame = false;
    }

    renderer.render(camera, player);
    uiManager.render();
}

void VoxelsApplication::processInput() {
    Input::update();
}

void VoxelsApplication::cleanup() {
    renderer.cleanup();
    worldManager.cleanup();
    uiManager.cleanup();

    Application::cleanup();
}
