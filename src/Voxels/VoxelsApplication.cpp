#include "VoxelsApplication.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <tracy/Tracy.hpp>
#include <tracy/TracyOpenGL.hpp>

#include <iostream>

#include "entity/components/CameraProperties.hpp"
#include "entity/components/CharacterController.hpp"
#include "entity/components/FlyPlayerController.hpp"
#include "entity/components/Q1PlayerController.hpp"
#include "entity/components/Q3PlayerController.hpp"
#include "entity/components/Transform.hpp"
#include "io/Input.hpp"
#include "world/VertexFormat.hpp"

constexpr float Pi = 3.14159265359f;

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

    player = std::make_unique<Entity>();
    player->add<Transform>(glm::vec3(8.0f, 80.0f, 8.0f));
    player->add<Kinematics>();
    if (noclip) {
        player->add<FlyPlayerController>();
    } else {
        player->add<Q1PlayerController>();
        player->add<CharacterController>(worldManager, true);
    }

    camera = std::make_unique<Entity>();
    camera->add<CameraProperties>();

    shader = Shader("vert.glsl", "frag.glsl");
    drawCommandProgram = Shader("drawcmd_comp.glsl");

    worldManager.createChunk(0, 0);

    glCreateVertexArrays(1, &dummyVAO);

    glCreateBuffers(1, &chunkDrawCmdBuffer);
    glNamedBufferStorage(chunkDrawCmdBuffer,
                         sizeof(ChunkDrawCommand) * MaxChunks,
                         nullptr,
                         GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, chunkDrawCmdBuffer);

    glCreateBuffers(1, &chunkDataBuffer);
    glNamedBufferData(chunkDataBuffer,
                         sizeof(ChunkData) * worldManager.chunkData.size(),
                         static_cast<const void*>(worldManager.chunkData.data()),
                         GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, chunkDataBuffer);

    glCreateBuffers(1, &commandCountBuffer);
    glNamedBufferStorage(commandCountBuffer,
                         sizeof(unsigned int),
                         nullptr,
                         GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, commandCountBuffer);
    glBindBuffer(GL_PARAMETER_BUFFER, commandCountBuffer);

    glCreateBuffers(1, &verticesBuffer);
    glNamedBufferStorage(verticesBuffer,
                      sizeof(uint32_t) * InitialVertexBufferSize,
                      nullptr,
                      GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, verticesBuffer);

    worldManager.updateVerticesBuffer(verticesBuffer, chunkDataBuffer);

    shader.use();
    shader.setInt("chunkSizeShift", ChunkSizeShift);
    shader.setInt("windowWidth", windowWidth);
    shader.setInt("windowHeight", windowHeight);

    shader.setUInt("xBits", VertexFormat::XBits);
    shader.setUInt("yBits", VertexFormat::YBits);
    shader.setUInt("zBits", VertexFormat::ZBits);
    shader.setUInt("colourBits", VertexFormat::ColourBits);
    shader.setUInt("normalBits", VertexFormat::NormalBits);
    shader.setUInt("aoBits", VertexFormat::AOBits);
    shader.setUInt("lightBits", VertexFormat::LightBits);

    shader.setUInt("xShift", VertexFormat::XShift);
    shader.setUInt("yShift", VertexFormat::YShift);
    shader.setUInt("zShift", VertexFormat::ZShift);
    shader.setUInt("colourShift", VertexFormat::ColourShift);
    shader.setUInt("normalShift", VertexFormat::NormalShift);
    shader.setUInt("aoShift", VertexFormat::AOShift);
    shader.setUInt("lightShift", VertexFormat::LightShift);

    shader.setUInt("xMask", VertexFormat::XMask);
    shader.setUInt("yMask", VertexFormat::YMask);
    shader.setUInt("zMask", VertexFormat::ZMask);
    shader.setUInt("colourMask", VertexFormat::ColourMask);
    shader.setUInt("normalMask", VertexFormat::NormalMask);
    shader.setUInt("aoMask", VertexFormat::AOMask);
    shader.setUInt("lightMask", VertexFormat::LightMask);

    shader.setInt("atlas", 0);

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
        if (const auto result = worldManager.raycast(player->get<Transform>()->position, getFront(player->get<Transform>()->angles), 16)) {
            worldManager.propagateTorchLight((result->cx << ChunkSizeShift) + result->x, result->y, (result->cz << ChunkSizeShift) + result->z, 15);
            // worldManager.updateVoxel(*result, false);
        }
    });

    Input::registerCallback({ActionType::Place, ActionStateType::None}, [this] {
        if (const auto result = worldManager.raycast(player->get<Transform>()->position, getFront(player->get<Transform>()->angles), 16)) {
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
            player->remove<Q1PlayerController>();
            player->remove<CharacterController>();
            player->add<FlyPlayerController>();
        } else {
            player->remove<FlyPlayerController>();
            player->add<Q1PlayerController>();
            player->add<CharacterController>(worldManager);
        }
    });

    for (size_t i = 0; i < worldManager.palette.size(); ++i) {
        Input::registerCallback({ActionType::SelectPaletteIndex, ActionStateType::None, static_cast<int>(i)}, [this, i] {
            worldManager.palette.index = i;
        });
    }
}

void VoxelsApplication::setupUI() {
    uiManager.registerStats(worldManager, *player, deltaTime);
    uiManager.registerController(*player);
    uiManager.registerCamera(*camera);
    uiManager.registerDemo();
    uiManager.registerPalette(worldManager.palette);
    uiManager.registerPrimitives(worldManager, *player);
    uiManager.registerLookAt(worldManager, *player);
}

void VoxelsApplication::update() {
    ZoneScoped;

    Application::update();

    while (worldManager.updateFrontierChunks(player->get<Transform>()->position)) {}

    // If any chunks have finished generating, update their voxel field
    worldManager.updateGeneratedChunks();

    worldManager.chunkTasksCount = 0;

    worldManager.updateVerticesBuffer(verticesBuffer, chunkDataBuffer);

    player->get<PlayerController>()->update(deltaTime);

    uiManager.beginFrame();

    const glm::vec3 playerPosition = player->get<Transform>()->position;
    const glm::vec3 velocity = player->get<Kinematics>()->velocity;
    const float currentSpeed = glm::length(glm::vec3{velocity.x, 0, velocity.z});

    // std::cout << "Frame time: " << deltaTime << "\t FPS: " << (1.0f / deltaTime) << std::endl;
    const std::string title = "Voxels | FPS: " + std::to_string(static_cast<int>(1.0f / deltaTime)) +
                        " | X: " + std::to_string(playerPosition.x) +
                        ", Y: " + std::to_string(playerPosition.y) +
                        ", Z: " + std::to_string(playerPosition.z) +
                        " | speed: " + std::to_string(currentSpeed) +
                        " | vel: " + glm::to_string(velocity) +
                        " | front: " + glm::to_string(getFront(player->get<Transform>()->angles));

    glfwSetWindowTitle(windowHandle, title.c_str());
}

void VoxelsApplication::render() {
    ZoneScoped;

    uiManager.drawWindows();

    if (firstFrame) {
        // Remove title bar highlight on first frame
        uiManager.clearWindowFocus();
        firstFrame = false;
    }

    const glm::mat4 projection = glm::perspective(camera->get<CameraProperties>()->FOV * Pi / 180,
                                            static_cast<float>(windowWidth) / static_cast<float>(windowHeight), 0.1f, 5000.0f);
    const glm::mat4 view = calculateViewMatrix(player->get<Transform>()->position, player->get<Transform>()->angles);

    glm::mat4 projectionT = glm::transpose(projection * view);
    glm::vec4 frustum[6];
    frustum[0] = projectionT[3] + projectionT[0];  // x + w < 0
    frustum[1] = projectionT[3] - projectionT[0];  // x - w > 0
    frustum[2] = projectionT[3] + projectionT[1];  // y + w < 0
    frustum[3] = projectionT[3] - projectionT[1];  // y - w > 0
    frustum[4] = projectionT[3] + projectionT[2];  // z + w < 0
    frustum[5] = projectionT[3] - projectionT[2];  // z - w > 0

    // Clear command count buffer
    glClearNamedBufferData(commandCountBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

    // Generate draw commands
    drawCommandProgram.use();
    drawCommandProgram.setVec4Array("frustum", frustum, 6);
    drawCommandProgram.setInt("CHUNK_SIZE", ChunkSize);

    glDispatchCompute(MaxChunks / 1, 1, 1);
    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

    // Render
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//    // Input latency testing
//    if (background) {
//        glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
//    } else {
//        glClearColor(0.8f, 0.8f, 0.7f, 1.0f);
//    }
//    background = !background;

    shader.use();
    shader.setMat4("view", view);
    shader.setMat4("projection", projection);

    glBindVertexArray(dummyVAO);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, chunkDrawCmdBuffer);
    glMultiDrawArraysIndirectCount(GL_TRIANGLES, nullptr, 0, MaxChunks, sizeof(ChunkDrawCommand));

    uiManager.render();
}

void VoxelsApplication::processInput() {
    Input::update();
}

void VoxelsApplication::cleanup() {
    glDeleteBuffers(1, &chunkDrawCmdBuffer);
    glDeleteBuffers(1, &chunkDataBuffer);
    glDeleteBuffers(1, &commandCountBuffer);
    glDeleteBuffers(1, &verticesBuffer);

    worldManager.cleanup();
    uiManager.cleanup();

    Application::cleanup();
}


size_t VoxelsApplication::enlargeVerticesBuffer(const size_t currentCapacity) {
    const size_t newCapacity = currentCapacity * 2;

    GLuint newBuffer;
    glCreateBuffers(1, &newBuffer);
    glNamedBufferStorage(newBuffer,
                         sizeof(uint32_t) * newCapacity,
                         nullptr,
                         GL_DYNAMIC_STORAGE_BIT);

    glCopyNamedBufferSubData(verticesBuffer, newBuffer, 0, 0, sizeof(uint32_t) * currentCapacity);

    glDeleteBuffers(1, &verticesBuffer);
    verticesBuffer = newBuffer;

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, verticesBuffer);

    std::cout << "Enlarged vertices buffer to " << newCapacity << " elements." << std::endl;
    return newCapacity;
}
