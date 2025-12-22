#include "VoxelsApplication.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <imgui.h>
#include <tracy/Tracy.hpp>
#include <tracy/TracyOpenGL.hpp>

#include <iostream>
#include <algorithm>

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

    camera = Camera(glm::vec3(8.0f, 80.0f, 8.0f));

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

    shader.setUInt("xShift", VertexFormat::XShift);
    shader.setUInt("yShift", VertexFormat::YShift);
    shader.setUInt("zShift", VertexFormat::ZShift);
    shader.setUInt("colourShift", VertexFormat::ColourShift);
    shader.setUInt("normalShift", VertexFormat::NormalShift);
    shader.setUInt("aoShift", VertexFormat::AOShift);

    shader.setUInt("xMask", VertexFormat::XMask);
    shader.setUInt("yMask", VertexFormat::YMask);
    shader.setUInt("zMask", VertexFormat::ZMask);
    shader.setUInt("colourMask", VertexFormat::ColourMask);
    shader.setUInt("normalMask", VertexFormat::NormalMask);
    shader.setUInt("aoMask", VertexFormat::AOMask);

    shader.setVec3Array("palette", worldManager.palette.data(), worldManager.palette.size());

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

    Input::bindings.insert({{Input::uiToggleKey, GLFW_PRESS}, {ActionType::ToggleUIMode, ActionStateType::None}});

    for (int i = 0; i < worldManager.palette.size(); ++i) {
        Input::bindings.insert({{GLFW_KEY_1 + i, GLFW_PRESS}, {ActionType::SelectPaletteIndex, ActionStateType::None, i}});
    }

    // Register action callbacks
    Input::registerCallback({ActionType::Break, ActionStateType::None}, [this] {
        if (const auto result = worldManager.raycast()) {
            worldManager.updateVoxel(*result, false);
        }
    });

    Input::registerCallback({ActionType::Place, ActionStateType::None}, [this] {
        if (const auto result = worldManager.raycast()) {
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
        worldManager.save();
    });

    Input::registerCallback({ActionType::ToggleUIMode, ActionStateType::None}, [this] {
        Input::uiMode = !Input::uiMode;

        // If the UI mode is being entered, then for all current actions, call the corresponding stop action
        if (Input::uiMode == true) {
            Input::clearCurrentActions();
        }

        // Remove title bar highlight
        ImGui::SetWindowFocus(nullptr);

        if (Input::uiMode) {
            glfwSetInputMode(windowHandle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(windowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    });

    for (size_t i = 0; i < worldManager.palette.size(); ++i) {
        Input::registerCallback({ActionType::SelectPaletteIndex, ActionStateType::None, static_cast<int>(i)}, [this, i] {
            worldManager.paletteIndex = i;
        });
    }
}

void VoxelsApplication::setupUI() {
    uiManager.registerWindow("Stats", [this] {
        ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)",
                    camera.transform.position.x,
                    camera.transform.position.y,
                    camera.transform.position.z);
        ImGui::Text("Chunks Loaded: %llu", worldManager.chunks.size());
        ImGui::Text("FPS: %.2f", 1.0f / deltaTime);

        ImGui::Text("WantCaptureKeyboard: %s", ImGui::GetIO().WantCaptureKeyboard ? "true" : "false");

        // Text input test
        static char levelName[128] = "";
        ImGui::InputText("Level Name", levelName, IM_ARRAYSIZE(levelName));

        ImGui::End();
    });

    uiManager.registerWindow("Controller", [this] {
        // Display Q1PlayerController fields
        ImGui::Begin("Controller", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        // player velocity
        ImGui::Text("Player Velocity: (%.2f, %.2f, %.2f)",
                    playerController.playerVelocity.x,
                    playerController.playerVelocity.y,
                    playerController.playerVelocity.z);
        ImGui::Text("Current Speed: %.2f", playerController.currentSpeed);

        ImGui::Text("wishdir: (%.2f, %.2f, %.2f)",
                    playerController.d_wishdir.x,
                    playerController.d_wishdir.y,
                    playerController.d_wishdir.z);
        ImGui::Text("wishvel: (%.2f, %.2f, %.2f)",
                    playerController.d_wishvel.x,
                    playerController.d_wishvel.y,
                    playerController.d_wishvel.z);
        ImGui::Text("wishspeed: %.2f", playerController.d_wishspeed);

        constexpr float scaleDown = 1.0f / 4.0f;
        glm::vec2 vel = glm::vec2(playerController.playerVelocity.x, -playerController.playerVelocity.z) * scaleDown;
        glm::vec2 wish = glm::vec2(
            playerController.d_wishdir.x * playerController.d_wishspeed,
            -playerController.d_wishdir.z * playerController.d_wishspeed
        ) * scaleDown;

        const ImVec2 canvasSize(200, 200);
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Draw background
        drawList->AddRectFilled(
            canvasPos,
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
            IM_COL32(40, 40, 40, 255)
        );

        // Optional border
        drawList->AddRect(
            canvasPos,
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
            IM_COL32(255, 255, 255, 255)
        );

        // Center of the canvas
        ImVec2 origin(
            canvasPos.x + canvasSize.x * 0.5f,
            canvasPos.y + canvasSize.y * 0.5f
        );

        // Scale factor (tweak this)
        float scale = 20.0f;

        // Convert vectors (x,z → screen x,y)
        ImVec2 velEnd(
            origin.x + vel.x * scale,
            origin.y - vel.y * scale
        );

        ImVec2 wishEnd(
            origin.x + wish.x * scale,
            origin.y - wish.y * scale
        );

        // Draw arrows
        auto drawArrow = [&](ImVec2 from, ImVec2 to, ImU32 color)
        {
            drawList->AddLine(from, to, color, 2.0f);

            // Arrowhead
            ImVec2 dir = ImVec2(to.x - from.x, to.y - from.y);
            float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
            if (len < 1e-3f) return;

            dir.x /= len;
            dir.y /= len;

            ImVec2 perp(-dir.y, dir.x);
            float headSize = 8.0f;

            ImVec2 p1 = to;
            ImVec2 p2 = ImVec2(
                to.x - dir.x * headSize + perp.x * headSize * 0.5f,
                to.y - dir.y * headSize + perp.y * headSize * 0.5f
            );
            ImVec2 p3 = ImVec2(
                to.x - dir.x * headSize - perp.x * headSize * 0.5f,
                to.y - dir.y * headSize - perp.y * headSize * 0.5f
            );

            drawList->AddTriangleFilled(p1, p2, p3, color);
        };

        // Colours
        drawArrow(origin, velEnd, IM_COL32(0, 200, 0, 255));     // velocity = green
        drawArrow(origin, wishEnd, IM_COL32(0, 0, 200, 255));    // wishdir = blue

        // Reserve space in layout
        ImGui::InvisibleButton("canvas", canvasSize);


        // accelerate function values, separate this with an imgui separator
        ImGui::Separator();
        ImGui::Text("Accelerate");
        ImGui::Text("currentSpeed: %.2f", playerController.d_accel_currentSpeed);
        ImGui::Text("addSpeed: %.2f", playerController.d_accel_addSpeed);
        ImGui::Text("accelSpeed: %.2f", playerController.d_accel_accelSpeed);

        // air accelerate
        ImGui::Separator();
        ImGui::Text("Air Accelerate");
        ImGui::Text("currentSpeed: %.2f", playerController.d_airaccel_currentSpeed);
        ImGui::Text("addSpeed: %.2f", playerController.d_airaccel_addSpeed);
        ImGui::Text("accelSpeed: %.2f", playerController.d_airaccel_accelSpeed);

        // DEBUG CURRENT ACTIONS
        ImGui::Separator();
        ImGui::Text("Current Actions:");
        for (ActionType action : Input::currentActions) {
            ImGui::Text("- %d", static_cast<int>(action));
        }

        ImGui::End();
    });

    uiManager.registerWindow("Demo", [] {
        ImGui::ShowDemoWindow();
    });

    uiManager.registerWindow("Palette", [this] {
        ImGui::Begin("Palette", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        for (size_t i = 0; i < worldManager.palette.size(); ++i) {
            ImGui::Text("%i", static_cast<int>(i));
            ImGui::SameLine();

            ImGui::ColorEdit3(("##paletteColor" + std::to_string(i)).c_str(),
                              reinterpret_cast<float*>(&worldManager.palette[i]),
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
        }

        ImGui::End();
    });

    uiManager.registerWindow("Primitives", [this] {
        ImGui::Begin("Primitives", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text("Palette index:");
        ImGui::SameLine();
        const size_t min = 0u;
        const size_t max = worldManager.palette.size() - 1;
        ImGui::SliderScalar("##paletteIndex", ImGuiDataType_U32, &worldManager.paletteIndex, &min, &max);

        static int primitiveType = 0;
        static float sizeX = 1.0f;
        static float sizeY = 1.0f;
        static float sizeZ = 1.0f;
        static float radius = 1.0f;
        static float cylRadius = 1.0f;
        static float height = 1.0f;
        ImGui::Text("Primitive type:");
        ImGui::SameLine();
        ImGui::Combo("##primitiveType", &primitiveType, "None\0Cuboid\0Sphere\0Cylinder\0");
        switch (primitiveType) {
            case 0: {
                // None
                break;
            }
            case 1: {
                // Cuboid
                ImGui::Text("Size X:");
                ImGui::SameLine();
                ImGui::DragFloat("##sizeX", &sizeX, 0.1f, 0.1f, 100.0f, "%.2f");
                ImGui::Text("Size Y:");
                ImGui::SameLine();
                ImGui::DragFloat("##sizeY", &sizeY, 0.1f, 0.1f, 100.0f, "%.2f");
                ImGui::Text("Size Z:");
                ImGui::SameLine();
                ImGui::DragFloat("##sizeZ", &sizeZ, 0.1f, 0.1f, 100.0f, "%.2f");
                break;
            }
            case 2: {
                // Sphere
                ImGui::Text("Radius:");
                ImGui::SameLine();
                ImGui::DragFloat("##radius", &radius, 0.1f, 0.1f, 100.0f, "%.2f");
                break;
            }
            case 3: {
                // Cylinder
                ImGui::Text("Radius:");
                ImGui::SameLine();
                ImGui::DragFloat("##cylRadius", &cylRadius, 0.1f, 0.1f, 100.0f, "%.2f");
                ImGui::Text("Height:");
                ImGui::SameLine();
                ImGui::DragFloat("##height", &height, 0.1f, 0.1f, 100.0f, "%.2f");
                break;
            }
            default: {
                ImGui::Text("Unknown primitive type.");
                break;
            }
        }

        if (primitiveType != 0) {
            static int originType = 0;
            ImGui::Text("Origin type:");
            ImGui::SameLine();
            ImGui::Combo("##originType", &originType, "Selected point, centered\0Selected point, bottom\0Custom\0");

            static glm::ivec3 origin{};
            switch (originType) {
                case 0: {
                    // Selected point, centered
                    if (const auto result = worldManager.raycast()) {
                        origin.x = result->cx * ChunkSize + result->x;
                        origin.y = result->y;
                        origin.z = result->cz * ChunkSize + result->z;
                    }
                    break;
                }
                case 1: {
                    // Selected point, bottom
                    if (const auto result = worldManager.raycast()) {
                        origin.x = result->cx * ChunkSize + result->x;
                        origin.y = result->y;
                        origin.z = result->cz * ChunkSize + result->z;
                    }
                    break;
                }
                case 2: {
                    ImGui::DragInt3("##customOrigin", reinterpret_cast<int*>(&origin));
                    break;
                }
                default: {
                    ImGui::Text("Unknown origin type.");
                    break;
                }
            }

            if (ImGui::Button("Place")) {
                switch (primitiveType) {
                    case 1: {
                        Cuboid c{sizeX, sizeY, sizeZ};
                        worldManager.placePrimitive(origin, c);
                        break;
                    }
                    case 2: {
                        Sphere s{radius};
                        worldManager.placePrimitive(origin, s);
                        break;
                    }
                    case 3: {
                        Cylinder cyl{cylRadius, height};
                        worldManager.placePrimitive(origin, cyl);
                        break;
                    }
                    default: break;
                }
             }
        }

        ImGui::End();
    });
}

void VoxelsApplication::update() {
    ZoneScoped;

    Application::update();

    camera.update();

    while (worldManager.updateFrontierChunks()) {}

    worldManager.chunkTasksCount = 0;

    worldManager.updateVerticesBuffer(verticesBuffer, chunkDataBuffer);

    playerController.update(deltaTime);

    // TODO: only really need to do this when a palette colour is changed, but it doesn't really matter
    shader.setVec3Array("palette", worldManager.palette.data(), worldManager.palette.size());

    uiManager.beginFrame();

    // std::cout << "Frame time: " << deltaTime << "\t FPS: " << (1.0f / deltaTime) << std::endl;
    const std::string title = "Voxels | FPS: " + std::to_string(static_cast<int>(1.0f / deltaTime)) +
                        " | X: " + std::to_string(camera.transform.position.x) +
                        ", Y: " + std::to_string(camera.transform.position.y) +
                        ", Z: " + std::to_string(camera.transform.position.z) +
                        " | speed: " + std::to_string(playerController.currentSpeed) +
                        " | vel: " + glm::to_string(playerController.playerVelocity) +
                        " | front: " + glm::to_string(camera.front);

    glfwSetWindowTitle(windowHandle, title.c_str());
}

void VoxelsApplication::render() {
    ZoneScoped;

    uiManager.drawWindows();

    if (firstFrame) {
        // Remove title bar highlight on first frame
        ImGui::SetWindowFocus(nullptr);
        firstFrame = false;
    }

    const glm::mat4 projection = glm::perspective(camera.FOV * Pi / 180,
                                            static_cast<float>(windowWidth) / static_cast<float>(windowHeight), 0.1f, 5000.0f);
    const glm::mat4 view = camera.calculateViewMatrix();

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
