#include "UIManager.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <nfd.hpp>

#include <memory>

#include "../entity/components/CameraProperties.hpp"
#include "../entity/components/Transform.hpp"
#include "../io/Input.hpp"

void UIManager::load(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();
}

void UIManager::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    if (!Input::uiMode)
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    else
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;

    ImGui::NewFrame();
}

void UIManager::render() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void UIManager::drawWindows() {
    for (Window& window : windows) {
        if (!window.visible) continue;

        window.fn();
    }
}

void UIManager::cleanup() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UIManager::registerWindow(const std::string& title, const WindowFn& fn, const bool visible) {
    windows.push_back(Window{title, fn, visible});
}

void UIManager::registerStats(const WorldManager& worldManager, const Entity& player, float deltaTime) {
    registerWindow("Stats", [&worldManager, &player, deltaTime] {
        ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)",
                    player.get<Transform>()->position.x,
                    player.get<Transform>()->position.y,
                    player.get<Transform>()->position.z);
        ImGui::Text("Chunks Loaded: %llu", worldManager.chunks.size());
        ImGui::Text("FPS: %.2f", 1.0f / deltaTime);

        ImGui::Text("WantCaptureKeyboard: %s", ImGui::GetIO().WantCaptureKeyboard ? "true" : "false");

        // Text input test
        static char levelName[128] = "";
        ImGui::InputText("Level Name", levelName, IM_ARRAYSIZE(levelName));

        ImGui::End();
    });
}

void UIManager::registerController(const Entity& player) {
    registerWindow("Controller", [&player] {
        const auto* playerController = player.get<Q1PlayerController>();
        const auto* kinematics = player.get<Kinematics>();

        if (!playerController) {
            return;
        }

        // Display Q1PlayerController fields
        ImGui::Begin("Controller", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        // player velocity
        const float currentSpeed = std::sqrt(
            player.get<Kinematics>()->velocity.x * player.get<Kinematics>()->velocity.x +
            player.get<Kinematics>()->velocity.z * player.get<Kinematics>()->velocity.z
        );
        ImGui::Text("Player Velocity: (%.2f, %.2f, %.2f)",
                    kinematics->velocity.x,
                    kinematics->velocity.y,
                    kinematics->velocity.z);
        ImGui::Text("Current Speed: %.2f", currentSpeed);

        ImGui::Text("wishdir: (%.2f, %.2f, %.2f)",
                    playerController->d_wishdir.x,
                    playerController->d_wishdir.y,
                    playerController->d_wishdir.z);
        ImGui::Text("wishvel: (%.2f, %.2f, %.2f)",
                    playerController->d_wishvel.x,
                    playerController->d_wishvel.y,
                    playerController->d_wishvel.z);
        ImGui::Text("wishspeed: %.2f", playerController->d_wishspeed);

        constexpr float scaleDown = 1.0f / 4.0f;
        glm::vec2 vel = glm::vec2(kinematics->velocity.x, -kinematics->velocity.z) * scaleDown;
        glm::vec2 wish = glm::vec2(
            playerController->d_wishdir.x * playerController->d_wishspeed,
            -playerController->d_wishdir.z * playerController->d_wishspeed
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
        ImGui::Text("currentSpeed: %.2f", playerController->d_accel_currentSpeed);
        ImGui::Text("addSpeed: %.2f", playerController->d_accel_addSpeed);
        ImGui::Text("accelSpeed: %.2f", playerController->d_accel_accelSpeed);

        // air accelerate
        ImGui::Separator();
        ImGui::Text("Air Accelerate");
        ImGui::Text("currentSpeed: %.2f", playerController->d_airaccel_currentSpeed);
        ImGui::Text("addSpeed: %.2f", playerController->d_airaccel_addSpeed);
        ImGui::Text("accelSpeed: %.2f", playerController->d_airaccel_accelSpeed);

        // DEBUG CURRENT ACTIONS
        ImGui::Separator();
        ImGui::Text("Current Actions:");
        for (ActionType action : Input::currentActions) {
            ImGui::Text("- %d", static_cast<int>(action));
        }

        ImGui::End();
    });
}

void UIManager::registerCamera(Entity& camera) {
    registerWindow("Camera", [&camera] {
        // FOV slider
        ImGui::Begin("Camera", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        float FOV = camera.get<CameraProperties>()->FOV;
        ImGui::SliderFloat("FOV", &FOV, 30.0f, 120.0f);
        camera.get<CameraProperties>()->FOV = FOV;
        ImGui::End();
    });
}

void UIManager::registerDemo() {
    registerWindow("Demo", [] {
        ImGui::ShowDemoWindow();
    });
}

void UIManager::registerPalette(Palette& palette) {
    registerWindow("Palette", [this, &palette] {
        ImGui::Begin("Palette", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        for (size_t i = 0; i < palette.size(); ++i) {
            auto& entry = palette.getEntry(i);

            constexpr int rowHeight = 20;
            constexpr int previewWidth = 20;

            ImGui::PushID(static_cast<int>(i));

            ImGui::Text("%i", static_cast<int>(i));
            ImGui::SameLine();

            if (ImGui::Button("Edit", ImVec2(50, rowHeight))) {
                ImGui::OpenPopup("PaletteEntryPopup");
            }

            ImGui::SameLine();

            // Colour preview
            ImGui::ColorButton("##colorPreview",
                ImVec4(entry.colour.r, entry.colour.g, entry.colour.b, 1.0f),
                ImGuiColorEditFlags_NoTooltip,
                ImVec2(previewWidth, rowHeight)
            );

            ImGui::SameLine();

            ImVec2 size(previewWidth, rowHeight);

            // Texture preview
            if (entry.useTexture) {
                ImGui::Image(
                    reinterpret_cast<ImTextureID>(static_cast<intptr_t>(palette.getAtlasTextureID())),
                    size,
                    ImVec2(entry.uvOffset.x, entry.uvOffset.y),
                    ImVec2(entry.uvOffset.x + entry.uvScale.x,
                           entry.uvOffset.y + entry.uvScale.y)
                );
            } else {
                ImGui::Dummy(size);
            }

            if (ImGui::BeginPopup("PaletteEntryPopup")) {
                drawPaletteEntryEditor(palette, entry);
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        ImGui::End();
    });
}

void UIManager::registerPrimitives(WorldManager& worldManager, const Entity& player) {
    registerWindow("Primitives", [&worldManager, &player] {
        ImGui::Begin("Primitives", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text("Palette index:");
        ImGui::SameLine();
        const size_t min = 0u;
        const size_t max = worldManager.palette.size() - 1;
        ImGui::SliderScalar("##paletteIndex", ImGuiDataType_U32, &worldManager.palette.index, &min, &max);

        static int primitiveType = 0;

        static glm::ivec3 cuboidStartPos{};
        static glm::ivec3 cuboidEndPos{};

        static int sphRadius = 1.0f;

        static int cylRadius = 1.0f;
        static int cylHeight = 1.0f;

        static glm::ivec3 planeStartPos{};
        static glm::ivec3 planeEndPos{};
        static Plane::Axis planeAxis = Plane::Axis::Y;

        ImGui::Text("Primitive type:");
        ImGui::SameLine();
        ImGui::Combo("##primitiveType", &primitiveType, "None\0Cuboid\0Sphere\0Cylinder\0Plane\0");
        switch (primitiveType) {
            case 0: {
                // None
                break;
            }
            case 1: {
                // Cuboid
                ImGui::Text("Start:");
                ImGui::SameLine();
                ImGui::DragInt3("##cuboidStartPos", reinterpret_cast<int*>(&cuboidStartPos), 0.1f, 0, 0, "%.2f");
                ImGui::SameLine();
                if (ImGui::Button("CamPos##start")) {
                    cuboidStartPos = glm::ivec3(player.get<Transform>()->position);
                }

                ImGui::Text("End:");
                ImGui::SameLine();
                ImGui::DragInt3("##cuboidEndPos", reinterpret_cast<int*>(&cuboidEndPos), 0.1f, 0, 0, "%.2f");
                ImGui::SameLine();
                if (ImGui::Button("CamPos##end")) {
                    cuboidEndPos = glm::ivec3(player.get<Transform>()->position);
                }
                break;
            }
            case 2: {
                // Sphere
                ImGui::Text("Radius:");
                ImGui::SameLine();
                ImGui::DragInt("##radius", &sphRadius, 0.1f, 1, std::numeric_limits<int>::max(), "%.2f");
                break;
            }
            case 3: {
                // Cylinder
                ImGui::Text("Radius:");
                ImGui::SameLine();
                ImGui::DragInt("##cylRadius", &cylRadius, 0.1f, 1, std::numeric_limits<int>::max(), "%.2f");
                ImGui::Text("Height:");
                ImGui::SameLine();
                ImGui::DragInt("##height", &cylHeight, 0.1f, 1, std::numeric_limits<int>::max(), "%.2f");
                break;
            }
            case 4: {
                // Plane
                ImGui::Text("Start:");
                ImGui::SameLine();
                ImGui::DragInt3("##planeStartPos", reinterpret_cast<int*>(&planeStartPos), 0.1f, 0, 0, "%.2f");
                ImGui::SameLine();
                if (ImGui::Button("CamPos##start")) {
                    planeStartPos = glm::ivec3(player.get<Transform>()->position);
                }

                ImGui::Text("End:");
                ImGui::SameLine();
                ImGui::DragInt3("##planeEndPos", reinterpret_cast<int*>(&planeEndPos), 0.1f, 0, 0, "%.2f");
                ImGui::SameLine();
                if (ImGui::Button("CamPos##end")) {
                    planeEndPos = glm::ivec3(player.get<Transform>()->position);
                }

                ImGui::Text("Axis:");
                ImGui::SameLine();
                ImGui::Combo("##planeAxis", reinterpret_cast<int*>(&planeAxis), "X\0Y\0Z\0");

                break;
            }
            default: {
                ImGui::Text("Unknown primitive type.");
                break;
            }
        }

        if (primitiveType != 0) {
            static glm::ivec3 origin{};
            // Cuboids and planes should not have an origin selection
            if (primitiveType != 1 && primitiveType != 4) {
                ImGui::Text("Origin:");
                ImGui::SameLine();
                ImGui::DragInt3("##origin", reinterpret_cast<int*>(&origin), 0.1f, 0, 0, "%.2f");

                ImGui::SameLine();
                if (ImGui::Button("CamPos##origin")) {
                    origin = player.get<Transform>()->position;
                }

                ImGui::SameLine();
                if (ImGui::Button("RayPos")) {
                    if (const auto result = worldManager.raycast(player.get<Transform>()->position, getFront(player.get<Transform>()->angles), 16)) {
                        const int wx = (result->cx << ChunkSizeShift) + result->x;
                        const int wy = result->y;
                        const int wz = (result->cz << ChunkSizeShift) + result->z;
                        origin = {wx, wy, wz};
                    }
                }
            }

            if (ImGui::Button("Place")) {
                switch (primitiveType) {
                    case 1: {
                        worldManager.addPrimitive(std::make_unique<Cuboid>(worldManager.palette.index + 1, cuboidStartPos, cuboidEndPos));
                        break;
                    }
                    case 2: {
                        worldManager.addPrimitive(std::make_unique<Sphere>(worldManager.palette.index + 1, origin, sphRadius));
                        break;
                    }
                    case 3: {
                        worldManager.addPrimitive(std::make_unique<Cylinder>(worldManager.palette.index + 1, origin, cylRadius, cylHeight));
                        break;
                    }
                    case 4: {
                        worldManager.addPrimitive(std::make_unique<Plane>(worldManager.palette.index + 1, planeStartPos, planeEndPos, planeAxis));
                        break;
                    }
                    default: break;
                }
            }
        }

        // List of primitives with a position slider
        ImGui::Separator();
        ImGui::Text("Primitives:");
        for (size_t i = 0; i < worldManager.primitives.size(); ++i) {
            ImGui::Text("%zu: %s at ", i,
                        dynamic_cast<Cuboid*>(worldManager.primitives[i].get()) ? "Cuboid" :
                        dynamic_cast<Sphere*>(worldManager.primitives[i].get()) ? "Sphere" :
                        dynamic_cast<Cylinder*>(worldManager.primitives[i].get()) ? "Cylinder" :
                        dynamic_cast<Plane*>(worldManager.primitives[i].get()) ? "Plane" :
                        "Unknown");
            ImGui::SameLine();
            if (ImGui::DragInt3(("##primitivePos" + std::to_string(i)).c_str(),
                              reinterpret_cast<int*>(&(worldManager.primitives[i]->origin)),
                              0.1f, 0, 0, "%.2f")) {
                worldManager.movePrimitive(i, worldManager.primitives[i]->origin);
            }
            ImGui::Text("Palette index:");
            if (ImGui::SliderScalar(("##primitivePaletteIndex" + std::to_string(i)).c_str(), ImGuiDataType_U32, &(worldManager.primitives[i]->voxelType), &min, &max)) {
                worldManager.movePrimitive(i, worldManager.primitives[i]->origin);  // TODO: hacky
            }

            ImGui::SameLine();
            if (ImGui::Button(("Remove##" + std::to_string(i)).c_str())) {
                worldManager.removePrimitive(i);
                --i;  // Adjust index since we removed an element
            }
        }

        ImGui::End();
    });
}

void UIManager::registerLookAt(WorldManager& worldManager, const Entity& player) {
    registerWindow("LookAt", [&worldManager, &player] {
        ImGui::Begin("LookAt", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        if (const auto result = worldManager.raycast(player.get<Transform>()->position, getFront(player.get<Transform>()->angles), 32)) {
            const int wx = (result->cx << ChunkSizeShift) + result->x;
            const int wy = result->y;
            const int wz = (result->cz << ChunkSizeShift) + result->z;

            ImGui::Text("World pos: %d, %d, %d", wx, wy, wz);
            ImGui::Text("Chunk: cx=%d cz=%d local=(%d,%d,%d)", result->cx, result->cz, result->x, result->y, result->z);

            // Current voxel
            {
                const auto info = worldManager.getVoxelInfoAtWorld(wx, wy, wz);
                if (info.valid) {
                    ImGui::Separator();
                    ImGui::Text("Looked voxel:");
                    ImGui::Text("  Type: %d", info.type);
                    ImGui::Text("  Light: (%d, %d)", info.torchlight, info.sunlight);
                    if (info.type > 0 && info.type <= static_cast<int>(worldManager.palette.size())) {
                        const auto& entry = worldManager.palette.getEntry(info.type - 1);
                        ImGui::SameLine();
                        ImGui::ColorButton("##lookat_color", ImVec4(entry.colour.r, entry.colour.g, entry.colour.b, 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(24,24));
                    }
                } else {
                    ImGui::TextColored(ImVec4(1,0.8f,0.2f,1), "Voxel info unavailable (not loaded)");
                }
            }

            // Adjacent voxels
            {
                ImGui::Separator();
                ImGui::Text("Adjacent voxels:");

                struct Dir { const char* name; int dx, dy, dz; };
                const Dir dirs[] = {
                    {"+X",  1,  0,  0},
                    {"-X", -1,  0,  0},
                    {"+Y",  0,  1,  0},
                    {"-Y",  0, -1,  0},
                    {"+Z",  0,  0,  1},
                    {"-Z",  0,  0, -1},
                };

                for (const auto& d : dirs) {
                    const int nx = wx + d.dx;
                    const int ny = wy + d.dy;
                    const int nz = wz + d.dz;

                    const auto ninfo = worldManager.getVoxelInfoAtWorld(nx, ny, nz);
                    if (ninfo.valid) {
                        ImGui::Text("%s (%d,%d,%d): type=%d light=(%d,%d)", d.name, nx, ny, nz, ninfo.type, ninfo.torchlight, ninfo.sunlight);
                        if (ninfo.type > 0 && ninfo.type <= static_cast<int>(worldManager.palette.size())) {
                            const auto& entry = worldManager.palette.getEntry(ninfo.type - 1);
                            ImGui::SameLine();
                            ImGui::ColorButton((std::string("##lookat_neighbor_") + d.name).c_str(),
                                               ImVec4(entry.colour.r, entry.colour.g, entry.colour.b, 1.0f),
                                               ImGuiColorEditFlags_NoTooltip, ImVec2(18,18));
                        }
                    } else {
                        ImGui::TextColored(ImVec4(0.9f,0.6f,0.1f,1), "%s: unavailable", d.name);
                    }
                }
            }
        } else {
            ImGui::Text("Not looking at any voxel within range.");
        }

        ImGui::End();
    });
}

void UIManager::clearWindowFocus() {
    ImGui::SetWindowFocus(nullptr);
}

void UIManager::drawPaletteEntryEditor(Palette& palette, PaletteEntry& entry) {
    if (ImGui::Checkbox("Use Texture", &entry.useTexture)) {
        // Just need to update useTexture on the GPU side
        palette.uploadToGPU();
    }

    if (ImGui::ColorEdit3("Colour", &entry.colour.x)) {
        palette.uploadToGPU();
    }

    ImGui::DragInt("Light", &entry.lightLevel);

    ImGui::Text("Texture:");

    // Preview
    const ImVec2 previewSize(64, 64);
    if (entry.useTexture) {
        ImGui::Image(
            reinterpret_cast<ImTextureID>(static_cast<intptr_t>(palette.getAtlasTextureID())),
            previewSize,
            ImVec2(entry.uvOffset.x, entry.uvOffset.y),
            ImVec2(entry.uvOffset.x + entry.uvScale.x,
                   entry.uvOffset.y + entry.uvScale.y)
        );
    } else {
        ImGui::Dummy(previewSize);
    }

    if (ImGui::Button("Browse...")) {
        NFD::UniquePathU8 path;

        const std::filesystem::path base = std::filesystem::path(PROJECT_SOURCE_DIR) / "data/textures";

        constexpr nfdu8filteritem_t filters[] = {
            { "Image Files", "png,jpg,jpeg" }
        };

        const auto result = NFD::OpenDialog(path, filters, 1, base.string().c_str());

        if (result == NFD_OKAY) {
            const std::filesystem::path selectedPath = path.get();

            const auto rel = std::filesystem::relative(selectedPath, base);

            if (!rel.empty() && rel.native()[0] != '.')  {
                entry.texturePath = selectedPath.string();
            } else {
                return;
            }

            entry.useTexture = true;

            palette.rebuildAtlas();
            palette.uploadToGPU();
        }
    }
}
