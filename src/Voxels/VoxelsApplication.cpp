#include "VoxelsApplication.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <tracy/Tracy.hpp>
#include <tracy/TracyOpenGL.hpp>

#include <iostream>
#include <algorithm>

#include "world/Mesher.hpp"

constexpr float PI = 3.14159265359f;

bool VoxelsApplication::init() {
    if (!Application::init()) {
        return false;
    }

    glfwSetWindowUserPointer(windowHandle, this);

    glfwSetKeyCallback(windowHandle, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
        Input::key_callback(key, scancode, action, mods);
    });
    glfwSetMouseButtonCallback(windowHandle, [](GLFWwindow *window, int button, int action, int mods) {
        Input::mouse_button_callback(button, action, mods);
    });
    glfwSetCursorPosCallback(windowHandle, [](GLFWwindow *window, double xpos, double ypos) {
        Input::cursor_position_callback(xpos, ypos);
    });
    glfwSetScrollCallback(windowHandle, [](GLFWwindow *window, double xoffset, double yoffset) {
        Input::scroll_callback(xoffset, yoffset);
    });

    return true;
}

bool VoxelsApplication::load() {
    if (!Application::load()) {
        return false;
    }

    camera = Camera(glm::vec3(8.0f, 400.0f, 8.0f));

    shader = Shader("vert.glsl", "frag.glsl");
    drawCommandProgram = Shader("drawcmd_comp.glsl");

//    dummyVerticesBuffer = std::vector<uint32_t>(INITIAL_VERTEX_BUFFER_SIZE, 0);

    worldManager.createChunk(0, 0);

    glCreateVertexArrays(1, &dummyVAO);

    glCreateBuffers(1, &chunkDrawCmdBuffer);
    glNamedBufferStorage(chunkDrawCmdBuffer,
                         sizeof(ChunkDrawCommand) * MAX_CHUNKS,
                         nullptr,
                         GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, chunkDrawCmdBuffer);

    glCreateBuffers(1, &chunkDataBuffer);
    glNamedBufferData(chunkDataBuffer,
                         sizeof(ChunkData) * worldManager.chunkData.size(),
                         (const void *) worldManager.chunkData.data(),
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
                      sizeof(uint32_t) * INITIAL_VERTEX_BUFFER_SIZE,
                      nullptr,
                      GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, verticesBuffer);

    worldManager.updateVerticesBuffer(verticesBuffer, chunkDataBuffer);

    shader.use();
    shader.setInt("chunkSizeShift", CHUNK_SIZE_SHIFT);
    shader.setInt("windowWidth", windowWidth);
    shader.setInt("windowHeight", windowHeight);

    shader.setUInt("xBits", VertexFormat::X_BITS);
    shader.setUInt("yBits", VertexFormat::Y_BITS);
    shader.setUInt("zBits", VertexFormat::Z_BITS);
    shader.setUInt("colourBits", VertexFormat::COLOUR_BITS);
    shader.setUInt("normalBits", VertexFormat::NORMAL_BITS);
    shader.setUInt("aoBits", VertexFormat::AO_BITS);

    shader.setUInt("xShift", VertexFormat::X_SHIFT);
    shader.setUInt("yShift", VertexFormat::Y_SHIFT);
    shader.setUInt("zShift", VertexFormat::Z_SHIFT);
    shader.setUInt("colourShift", VertexFormat::COLOUR_SHIFT);
    shader.setUInt("normalShift", VertexFormat::NORMAL_SHIFT);
    shader.setUInt("aoShift", VertexFormat::AO_SHIFT);

    shader.setUInt("xMask", VertexFormat::X_MASK);
    shader.setUInt("yMask", VertexFormat::Y_MASK);
    shader.setUInt("zMask", VertexFormat::Z_MASK);
    shader.setUInt("colourMask", VertexFormat::COLOUR_MASK);
    shader.setUInt("normalMask", VertexFormat::NORMAL_MASK);
    shader.setUInt("aoMask", VertexFormat::AO_MASK);

    shader.setVec3Array("palette", worldManager.level.colors.data(), worldManager.level.colors.size());

    return true;
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
    std::string title = "Voxels | FPS: " + std::to_string((int) (1.0f / deltaTime)) +
                        " | X: " + std::to_string(camera.transform.position.x) +
                        ", Y: " + std::to_string(camera.transform.position.y) +
                        ", Z: " + std::to_string(camera.transform.position.z) +
                        " | speed: " + std::to_string(playerController.speed) +
                        " | vel: " + glm::to_string(playerController.playerVelocity) +
                        " | front: " + glm::to_string(camera.front);

    glfwSetWindowTitle(windowHandle, title.c_str());
}

void VoxelsApplication::render() {
    ZoneScoped;

    glm::mat4 projection = glm::perspective((float) (camera.FOV * PI / 180),
                                            (float) windowWidth / (float) windowHeight, 0.1f, 5000.0f);
    glm::mat4 view = camera.calculateViewMatrix();

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
    drawCommandProgram.setInt("CHUNK_SIZE", CHUNK_SIZE);

    glDispatchCompute(MAX_CHUNKS / 1, 1, 1);
    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

    // Render
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader.use();
    shader.setMat4("view", view);
    shader.setMat4("projection", projection);

    glBindVertexArray(dummyVAO);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, chunkDrawCmdBuffer);
    glMultiDrawArraysIndirectCount(GL_TRIANGLES, nullptr, 0, MAX_CHUNKS, sizeof(ChunkDrawCommand));
}

void VoxelsApplication::processInput() {
    if (Input::isKeyDown(GLFW_KEY_ESCAPE)) {
        glfwSetWindowShouldClose(windowHandle, true);
    }

    if (Input::isKeyPress(GLFW_KEY_T)) {
        if (wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }
        wireframe = !wireframe;
    }

    if (Input::isButtonPress(GLFW_MOUSE_BUTTON_LEFT)) {
        auto result = raycast();
        if (result) {
            updateVoxel(*result, false);
        }
    }
    if (Input::isButtonPress(GLFW_MOUSE_BUTTON_RIGHT)) {
        auto result = raycast();
        if (result) {
            updateVoxel(*result, true);
        }
    }

    if (Input::isKeyPress(GLFW_KEY_P)) {
        worldManager.save();
    }

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

int step(float edge, float x) {
    return x < edge ? 0 : 1;
}

std::optional<RaycastResult> VoxelsApplication::raycast() {
    float big = 1e30f;

    float ox = camera.transform.position.x;
    float oy = camera.transform.position.y - 1;
    float oz = camera.transform.position.z;

    float dx = camera.front.x;
    float dy = camera.front.y;
    float dz = camera.front.z;

    int px = (int) std::floor(ox);
    int py = (int) std::floor(oy);
    int pz = (int) std::floor(oz);

    float dxi = 1.0f / dx;
    float dyi = 1.0f / dy;
    float dzi = 1.0f / dz;

    int sx = dx > 0 ? 1 : -1;
    int sy = dy > 0 ? 1 : -1;
    int sz = dz > 0 ? 1 : -1;

    float dtx = std::min(dxi * sx, big);
    float dty = std::min(dyi * sy, big);
    float dtz = std::min(dzi * sz, big);

    float tx = abs((px + std::max(sx, 0) - ox) * dxi);
    float ty = abs((py + std::max(sy, 0) - oy) * dyi);
    float tz = abs((pz + std::max(sz, 0) - oz) * dzi);

    int maxSteps = 16;

    int cmpx = 0, cmpy = 0, cmpz = 0;

    int faceHit = -1;

    for (int i = 0; i < maxSteps && py >= 0; i++) {
        if (i > 0 && py < CHUNK_HEIGHT) {
            int cx = std::floor((float)px / CHUNK_SIZE);
            int cz = std::floor((float)pz / CHUNK_SIZE);

            int localX = (px % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
            int localZ = (pz % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

            auto chunkOpt = worldManager.getChunk(cx, cz);
            if (!chunkOpt) {
                std::cout << "Chunk not found at (" << cx << ", " << cz << ")" << std::endl;
                return std::nullopt;
            }
            Chunk *chunk = chunkOpt.value();
            int v = chunk->load(localX, py, localZ);

            if (v != 0) {
                std::cout << "Voxel hit at " << px << ", " << py << ", " << pz << ", face: " << faceHit << std::endl;
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
            faceHit = (sx < 0) ? 0 : 1;  // 0: +x, 1: -x
        } else if (cmpy) {
            faceHit = (sy < 0) ? 2 : 3;  // 2: +y, 3: -y
        } else if (cmpz) {
            faceHit = (sz < 0) ? 4 : 5;  // 4: +z, 5: -z
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

void VoxelsApplication::tryStoreVoxel(int cx, int cz, int x, int y, int z, bool place, std::vector<Chunk *> &chunksToMesh) {
    auto chunkOpt = worldManager.getChunk(cx, cz);
    if (!chunkOpt) {
        return;
    }

    Chunk *chunk = chunkOpt.value();
    chunk->store(x, y, z, place ? 1 : 0);
    chunksToMesh.push_back(chunk);
}

void VoxelsApplication::updateVoxel(RaycastResult result, bool place) {
    auto [cx, cz, x, y, z, face] = result;

    if (place) {
        switch (face) {
            case 0: (x == CHUNK_SIZE - 1) ? (++cx, x = 0) : ++x; break;
            case 1: (x == 0)              ? (--cx, x = CHUNK_SIZE - 1) : --x; break;
            case 2: if (y < CHUNK_HEIGHT - 2) ++y; break;
            case 3: if (y > 0) --y; break;
            case 4: (z == CHUNK_SIZE - 1) ? (++cz, z = 0) : ++z; break;
            case 5: (z == 0)              ? (--cz, z = CHUNK_SIZE - 1) : --z; break;
            default:
                std::cerr << "Invalid face: " << face << std::endl;
                return;
        }
    }

    auto chunkOpt = worldManager.getChunk(cx, cz);
    if (!chunkOpt) {
        std::cerr << "Chunk not found at (" << cx << ", " << cz << ")" << std::endl;
        return;
    }

    Chunk *chunk = chunkOpt.value();

    // Change voxel value
    chunk->store(x, y, z, place ? 1 : 0);

    std::vector<Chunk *> chunksToMesh;
    chunksToMesh.push_back(chunk);

    // If the voxel is on a chunk boundary, update the neighboring chunk(s)
    if (x == 0) {
        tryStoreVoxel(cx - 1, cz, CHUNK_SIZE, y, z, place, chunksToMesh);

        if (z == 0) {
            tryStoreVoxel(cx - 1, cz - 1, CHUNK_SIZE, y, CHUNK_SIZE, place, chunksToMesh);
        } else if (z == CHUNK_SIZE - 1) {
            tryStoreVoxel(cx - 1, cz + 1, CHUNK_SIZE, y, -1, place, chunksToMesh);
        }
    } else if (x == CHUNK_SIZE - 1) {
        tryStoreVoxel(cx + 1, cz, -1, y, z, place, chunksToMesh);

        if (z == 0) {
            tryStoreVoxel(cx + 1, cz - 1, -1, y, CHUNK_SIZE, place, chunksToMesh);
        } else if (z == CHUNK_SIZE - 1) {
            tryStoreVoxel(cx + 1, cz + 1, -1, y, -1, place, chunksToMesh);
        }
    }

    if (z == 0) {
        tryStoreVoxel(cx, cz - 1, x, y, CHUNK_SIZE, place, chunksToMesh);
    } else if (z == CHUNK_SIZE - 1) {
        tryStoreVoxel(cx, cz + 1, x, y, -1, place, chunksToMesh);
    }

    for (Chunk *chunk : chunksToMesh) {
        worldManager.queueMeshChunk(chunk);
    }
}

size_t VoxelsApplication::enlargeVerticesBuffer(size_t currentCapacity) {
    size_t newCapacity = currentCapacity * 2;

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
