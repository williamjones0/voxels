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

    worldManager.load();

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
    shader.setInt("chunkHeightShift", CHUNK_HEIGHT_SHIFT);
    shader.setInt("windowWidth", windowWidth);
    shader.setInt("windowHeight", windowHeight);

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
                        " | X: " + std::to_string(characterController.transform.position.x) +
                        ", Y: " + std::to_string(characterController.transform.position.y) +
                        ", Z: " + std::to_string(characterController.transform.position.z) +
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

            Chunk *chunk = worldManager.getChunk(cx, cz);
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

void VoxelsApplication::updateVoxel(RaycastResult result, bool place) {
    Chunk *chunk = worldManager.getChunk(result.cx, result.cz);

    if (place) {
        if (result.face == 0) {
            if (result.x == CHUNK_SIZE - 1) {
                chunk = worldManager.getChunk(chunk->cx + 1, chunk->cz);
                result.x = 0;
            } else {
                result.x++;
            }
        } else if (result.face == 1) {
            if (result.x == 0) {
                chunk = worldManager.getChunk(chunk->cx - 1, chunk->cz);
                result.x = CHUNK_SIZE - 1;
            } else {
                result.x--;
            }
        } else if (result.face == 2) {
            if (result.y == CHUNK_HEIGHT - 2) {
                return;
            }
            result.y++;
        } else if (result.face == 3) {
            if (result.y == 0) {
                return;
            }
            result.y--;
        } else if (result.face == 4) {
            if (result.z == CHUNK_SIZE - 1) {
                chunk = worldManager.getChunk(chunk->cx, chunk->cz + 1);
                result.z = 0;
            } else {
                result.z++;
            }
        } else if (result.face == 5) {
            if (result.z == 0) {
                chunk = worldManager.getChunk(chunk->cx, chunk->cz - 1);
                result.z = CHUNK_SIZE - 1;
            } else {
                result.z--;
            }
        }
    }

    // Change voxel value
    chunk->store(result.x, result.y, result.z, place ? 1 : 0);

    // Update minY and maxY
    chunk->minY = std::min(chunk->minY, result.y);
    chunk->maxY = std::max(chunk->maxY, result.y + 2);

    std::vector<Chunk *> chunksToMesh;
    chunksToMesh.push_back(chunk);

    // If the voxel is on a chunk boundary, update the neighboring chunk(s)
    if (result.x == 0) {
        Chunk *negXChunk = worldManager.getChunk(chunk->cx - 1, chunk->cz);
        negXChunk->store(CHUNK_SIZE, result.y, result.z, place ? 1 : 0);
        chunksToMesh.push_back(negXChunk);

        if (result.z == 0) {
            Chunk *negXNegZChunk = worldManager.getChunk(chunk->cx - 1, chunk->cz - 1);
            negXNegZChunk->store(CHUNK_SIZE, result.y, CHUNK_SIZE, place ? 1 : 0);
            chunksToMesh.push_back(negXNegZChunk);
        } else if (result.z == CHUNK_SIZE - 1) {
            Chunk *negXPosZChunk = worldManager.getChunk(chunk->cx - 1, chunk->cz + 1);
            negXPosZChunk->store(CHUNK_SIZE, result.y, -1, place ? 1 : 0);
            chunksToMesh.push_back(negXPosZChunk);
        }
    } else if (result.x == CHUNK_SIZE - 1) {
        Chunk *posXChunk = worldManager.getChunk(chunk->cx + 1, chunk->cz);
        posXChunk->store(-1, result.y, result.z, place ? 1 : 0);
        chunksToMesh.push_back(posXChunk);

        if (result.z == 0) {
            Chunk *posXNegZChunk = worldManager.getChunk(chunk->cx + 1, chunk->cz - 1);
            posXNegZChunk->store(-1, result.y, CHUNK_SIZE, place ? 1 : 0);
            chunksToMesh.push_back(posXNegZChunk);
        } else if (result.z == CHUNK_SIZE - 1) {
            Chunk *posXPosZChunk = worldManager.getChunk(chunk->cx + 1, chunk->cz + 1);
            posXPosZChunk->store(-1, result.y, -1, place ? 1 : 0);
            chunksToMesh.push_back(posXPosZChunk);
        }
    }

    if (result.z == 0) {
        Chunk *negZChunk = worldManager.getChunk(chunk->cx, chunk->cz - 1);
        negZChunk->store(result.x, result.y, CHUNK_SIZE, place ? 1 : 0);
        chunksToMesh.push_back(negZChunk);
    } else if (result.z == CHUNK_SIZE - 1) {
        Chunk *posZChunk = worldManager.getChunk(chunk->cx, chunk->cz + 1);
        posZChunk->store(result.x, result.y, -1, place ? 1 : 0);
        chunksToMesh.push_back(posZChunk);
    }

    for (Chunk *chunk : chunksToMesh) {
        // TODO: make a function in worldManager to queue a chunk mesh task so the thread pool itself isn't exposed
        worldManager.threadPool.queueTask([chunk, this]() {
            worldManager.allocator.deallocate(chunk->firstIndex, chunk->numVertices);

            chunk->initialising = true;
            Mesher::meshChunk(*chunk);
            chunk->initialising = false;

            // If newlyCreatedChunks is currently being iterated through, we need to lock the mutex
            {
                std::unique_lock<std::mutex> lock(worldManager.cvMutexNewlyCreatedChunks);
                worldManager.cvNewlyCreatedChunks.wait(lock, [this]() { return worldManager.newlyCreatedChunksReady; });
            }
            worldManager.newlyCreatedChunks.push_back(chunk);
        });
    }
}
