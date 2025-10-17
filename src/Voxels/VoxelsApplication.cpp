#include "VoxelsApplication.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <tracy/Tracy.hpp>
#include <tracy/TracyOpenGL.hpp>

#include <iostream>
#include <algorithm>

#include "io/Input.hpp"
#include "world/Mesher.hpp"

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

//    dummyVerticesBuffer = std::vector<uint32_t>(INITIAL_VERTEX_BUFFER_SIZE, 0);

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

    shader.setVec3Array("palette", worldManager.level.colors.data(), worldManager.level.colors.size());

    setupInput();

    return true;
}

void VoxelsApplication::setupInput() {
    Input::bindings.insert({{GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, true}, Action::Break});
    Input::bindings.insert({{GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, true}, Action::Place});

    Input::bindings.insert({{GLFW_KEY_ESCAPE, GLFW_PRESS}, Action::Exit});
    Input::bindings.insert({{GLFW_KEY_T, GLFW_PRESS}, Action::ToggleWireframe});
    Input::bindings.insert({{GLFW_KEY_P, GLFW_PRESS}, Action::SaveLevel});

    // Register action callbacks
    Input::registerCallback(Action::Break, [this] {
        if (const auto result = raycast()) {
            updateVoxel(*result, false);
        }
    });

    Input::registerCallback(Action::Place, [this] {
        if (const auto result = raycast()) {
            updateVoxel(*result, true);
        }
    });

    Input::registerCallback(Action::Exit, [this] {
        glfwSetWindowShouldClose(windowHandle, true);
    });

    Input::registerCallback(Action::ToggleWireframe, [this] {
        wireframe = !wireframe;
        if (wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    });

    Input::registerCallback(Action::SaveLevel, [this] {
        worldManager.save();
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

    Application::cleanup();
}

std::optional<RaycastResult> VoxelsApplication::raycast() {
    constexpr float big = 1e30f;

    const float ox = camera.transform.position.x;
    const float oy = camera.transform.position.y - 1;
    const float oz = camera.transform.position.z;

    const float dx = camera.front.x;
    const float dy = camera.front.y;
    const float dz = camera.front.z;

    int px = static_cast<int>(std::floor(ox));
    int py = static_cast<int>(std::floor(oy));
    int pz = static_cast<int>(std::floor(oz));

    const float dxi = 1.0f / dx;
    const float dyi = 1.0f / dy;
    const float dzi = 1.0f / dz;

    const int sx = dx > 0 ? 1 : -1;
    const int sy = dy > 0 ? 1 : -1;
    const int sz = dz > 0 ? 1 : -1;

    const float dtx = std::min(dxi * sx, big);
    const float dty = std::min(dyi * sy, big);
    const float dtz = std::min(dzi * sz, big);

    float tx = abs((px + std::max(sx, 0) - ox) * dxi);
    float ty = abs((py + std::max(sy, 0) - oy) * dyi);
    float tz = abs((pz + std::max(sz, 0) - oz) * dzi);

    constexpr int maxSteps = 16;

    int cmpx = 0, cmpy = 0, cmpz = 0;

    int faceHit = -1;

    auto step = [](const float edge, const float x) -> int {
        return x < edge ? 0 : 1;
    };

    for (int i = 0; i < maxSteps && py >= 0; i++) {
        if (i > 0 && py < ChunkHeight) {
            const int cx = std::floor(static_cast<float>(px) / ChunkSize);
            const int cz = std::floor(static_cast<float>(pz) / ChunkSize);

            const int localX = (px % ChunkSize + ChunkSize) % ChunkSize;
            const int localZ = (pz % ChunkSize + ChunkSize) % ChunkSize;

            const Chunk* chunk = worldManager.getChunk(cx, cz);
            if (!chunk) {
                std::cout << "Chunk not found at (" << cx << ", " << cz << ")" << std::endl;
                return std::nullopt;
            }

            if (const int v = chunk->load(localX, py, localZ); v != 0) {
//                std::cout << "Voxel hit at " << px << ", " << py << ", " << pz << ", face: " << faceHit << std::endl;
                return RaycastResult {
                        .cx = cx,
                        .cz = cz,
                        .x = localX,
                        .y = py,
                        .z = localZ,
                        .face = faceHit
                };
            }
        }

        // Advance to next voxel
        cmpx = step(tx, tz) * step(tx, ty);
        cmpy = step(ty, tx) * step(ty, tz);
        cmpz = step(tz, ty) * step(tz, tx);

        if (cmpx) {
            faceHit = sx < 0 ? 0 : 1;  // 0: +x, 1: -x
        } else if (cmpy) {
            faceHit = sy < 0 ? 2 : 3;  // 2: +y, 3: -y
        } else if (cmpz) {
            faceHit = sz < 0 ? 4 : 5;  // 4: +z, 5: -z
        }

        tx += dtx * cmpx;
        ty += dty * cmpy;
        tz += dtz * cmpz;

        px += sx * cmpx;
        py += sy * cmpy;
        pz += sz * cmpz;
    }

    return std::nullopt;
}

void VoxelsApplication::tryStoreVoxel(const int cx, const int cz, const int x, const int y, const int z, const bool place, std::vector<Chunk*>& chunksToMesh) {
    Chunk* chunk = worldManager.getChunk(cx, cz);
    if (!chunk) {
        return;
    }

    chunk->store(x, y, z, place ? 1 : 0);
    chunksToMesh.push_back(chunk);
}

void VoxelsApplication::updateVoxel(RaycastResult result, const bool place) {
    auto [cx, cz, x, y, z, face] = result;

    if (place) {
        switch (face) {
            case 0: x == ChunkSize - 1 ? (++cx, x = 0) : ++x; break;
            case 1: x == 0              ? (--cx, x = ChunkSize - 1) : --x; break;
            case 2: if (y < ChunkHeight - 2) ++y; break;
            case 3: if (y > 0) --y; break;
            case 4: z == ChunkSize - 1 ? (++cz, z = 0) : ++z; break;
            case 5: z == 0              ? (--cz, z = ChunkSize - 1) : --z; break;
            default:
                std::cerr << "Invalid face: " << face << std::endl;
                return;
        }
    }

    Chunk* hitChunk = worldManager.getChunk(cx, cz);
    if (!hitChunk) {
        std::cerr << "Chunk not found at (" << cx << ", " << cz << ")" << std::endl;
        return;
    }

    // Change voxel value
    hitChunk->store(x, y, z, place ? 1 : 0);

    std::vector<Chunk*> chunksToMesh;
    chunksToMesh.push_back(hitChunk);

    // If the voxel is on a chunk boundary, update the neighboring chunk(s)
    if (x == 0) {
        tryStoreVoxel(cx - 1, cz, ChunkSize, y, z, place, chunksToMesh);

        if (z == 0) {
            tryStoreVoxel(cx - 1, cz - 1, ChunkSize, y, ChunkSize, place, chunksToMesh);
        } else if (z == ChunkSize - 1) {
            tryStoreVoxel(cx - 1, cz + 1, ChunkSize, y, -1, place, chunksToMesh);
        }
    } else if (x == ChunkSize - 1) {
        tryStoreVoxel(cx + 1, cz, -1, y, z, place, chunksToMesh);

        if (z == 0) {
            tryStoreVoxel(cx + 1, cz - 1, -1, y, ChunkSize, place, chunksToMesh);
        } else if (z == ChunkSize - 1) {
            tryStoreVoxel(cx + 1, cz + 1, -1, y, -1, place, chunksToMesh);
        }
    }

    if (z == 0) {
        tryStoreVoxel(cx, cz - 1, x, y, ChunkSize, place, chunksToMesh);
    } else if (z == ChunkSize - 1) {
        tryStoreVoxel(cx, cz + 1, x, y, -1, place, chunksToMesh);
    }

    for (Chunk* chunk : chunksToMesh) {
        worldManager.queueMeshChunk(chunk);
    }
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
