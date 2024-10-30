#include "VoxelsApplication.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <thread>

#include "world/Mesher.hpp"

constexpr float PI = 3.14159265359f;

bool VoxelsApplication::init() {
    if (!Application::init()) {
        return false;
    }

    glfwSetWindowUserPointer(windowHandle, this);

    glfwSetKeyCallback(windowHandle, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
        auto *app = static_cast<VoxelsApplication *>(glfwGetWindowUserPointer(window));
        app->key_callback(key, scancode, action, mods);
    });
    glfwSetMouseButtonCallback(windowHandle, [](GLFWwindow *window, int button, int action, int mods) {
        auto *app = static_cast<VoxelsApplication *>(glfwGetWindowUserPointer(window));
        app->mouse_button_callback(button, action, mods);
    });
    glfwSetCursorPosCallback(windowHandle, [](GLFWwindow *window, double xpos, double ypos) {
        auto *app = static_cast<VoxelsApplication *>(glfwGetWindowUserPointer(window));
        app->cursor_position_callback(xpos, ypos);
    });
    glfwSetScrollCallback(windowHandle, [](GLFWwindow *window, double xoffset, double yoffset) {
        auto *app = static_cast<VoxelsApplication *>(glfwGetWindowUserPointer(window));
        app->scroll_callback(xoffset, yoffset);
    });

    return true;
}

bool VoxelsApplication::load() {
    if (!Application::load()) {
        return false;
    }

    camera = Camera(glm::vec3(50.0f, 80.0f, 50.0f), 0.0f, 0.0f);

    shader = Shader("vert.glsl", "frag.glsl");
    drawCommandProgram = Shader("drawcmd_comp.glsl");

    chunks = std::vector<Chunk>(NUM_CHUNKS);

    worldMesh = WorldMesh(NUM_CHUNKS);

    unsigned int numThreads = std::thread::hardware_concurrency();
    std::cout << "std::thread::hardware_concurrency(): " << std::thread::hardware_concurrency() << std::endl;
    std::cout << "Number of threads: " << numThreads << std::endl;

    int numChunksPerThread = NUM_CHUNKS / numThreads;
    int numThreadsWithOneMoreChunk = NUM_CHUNKS - (numChunksPerThread * numThreads);

    auto startTime = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> threads;
    std::vector<std::vector<uint32_t>> chunkVertices(numThreads);
    for (int threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
        threads.emplace_back([threadIndex, numChunksPerThread, numThreadsWithOneMoreChunk, &chunkVertices, &chunks = chunks, &worldMesh = worldMesh]() {
            int start = std::min(threadIndex, numThreadsWithOneMoreChunk) * (numChunksPerThread + 1) + std::max(0, threadIndex - numThreadsWithOneMoreChunk) * numChunksPerThread;
            int end = start + numChunksPerThread;
            if (threadIndex < numThreadsWithOneMoreChunk) {
                end++;
            }

            for (int i = start; i < end; ++i) {
                int cx = i / NUM_AXIS_CHUNKS;
                int cz = i % NUM_AXIS_CHUNKS;
                Chunk chunk = Chunk(cx, cz);

                auto startTime = std::chrono::high_resolution_clock::now();
                chunk.init();
                chunk.generateVoxels2D();
                Mesher::meshChunk(&chunk, WORLD_SIZE, worldMesh, chunkVertices[threadIndex]);
                auto endTime = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
                // std::cout << "Chunk " << cz + cx * NUM_AXIS_CHUNKS << " took " << duration.count() << "us to init" << std::endl << std::endl;
                chunks[i] = chunk;
            }
        });
    }

    for (int i = 0; i < numThreads; ++i) {
        threads[i].join();
    }

    for (int i = 0; i < numThreads; ++i) {
        worldMesh.data.insert(worldMesh.data.end(), chunkVertices[i].begin(), chunkVertices[i].end());
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    auto durationSeconds = static_cast<float>(duration.count()) / 1000000.0f;
    std::cout << "Meshing all " << NUM_CHUNKS << " chunks took " << durationSeconds << "s" << std::endl;

    unsigned int firstIndex = 0;
    for (int i = 0; i < NUM_CHUNKS; ++i) {
        chunks[i].firstIndex = firstIndex;
        worldMesh.chunkVertexStarts[i] = firstIndex;
        firstIndex += chunks[i].numVertices;
    }

    std::cout << "totalMesherTime: " << Mesher::totalMesherTime << std::endl;

    worldMesh.createBuffers();

    chunkData = std::vector<ChunkData>();

    for (size_t i = 0; i < NUM_CHUNKS; ++i) {
        ChunkData cd = {
                .cx = chunks[i].cx,
                .cz = chunks[i].cz,
                .minY = chunks[i].minY,
                .maxY = chunks[i].maxY,
                .numVertices = chunks[i].numVertices,
                .firstIndex = chunks[i].firstIndex,
                ._pad0 = 0,
                ._pad1 = 0,
        };
        chunkData.push_back(cd);
    }

    glCreateBuffers(1, &chunkDrawCmdBuffer);
    glNamedBufferStorage(chunkDrawCmdBuffer,
                         sizeof(ChunkDrawCommand) * NUM_CHUNKS,
                         nullptr,
                         NULL);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, chunkDrawCmdBuffer);

    glCreateBuffers(1, &chunkDataBuffer);
    glNamedBufferStorage(chunkDataBuffer,
                         sizeof(ChunkData) * chunkData.size(),
                         (const void *) chunkData.data(),
                         GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, chunkDataBuffer);

    glCreateBuffers(1, &commandCountBuffer);
    glNamedBufferStorage(commandCountBuffer,
                         sizeof(unsigned int),
                         nullptr,
                         NULL);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, commandCountBuffer);
    glBindBuffer(GL_PARAMETER_BUFFER, commandCountBuffer);

    glCreateBuffers(1, &verticesBuffer);
    glNamedBufferData(verticesBuffer,
                      sizeof(uint32_t) * worldMesh.data.size(),
                      (const void *) worldMesh.data.data(),
                      GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, verticesBuffer);

    shader.use();
    shader.setInt("chunkSizeShift", CHUNK_SIZE_SHIFT);
    shader.setInt("chunkHeightShift", CHUNK_HEIGHT_SHIFT);
    shader.setInt("windowWidth", windowWidth);
    shader.setInt("windowHeight", windowHeight);

    return true;
}

void VoxelsApplication::update() {
    Application::update();

    camera.update(deltaTime);

//    while (updateFrontierChunks()) {}

    // std::cout << "Frame time: " << deltaTime << "\t FPS: " << (1.0f / deltaTime) << std::endl;
    std::string title = "Voxels | FPS: " + std::to_string((int) (1.0f / deltaTime)) +
                        " | X: " + std::to_string(camera.position.x) +
                        ", Y: " + std::to_string(camera.position.y) +
                        ", Z: " + std::to_string(camera.position.z) +
                        " | front: " + glm::to_string(camera.front);

    glfwSetWindowTitle(windowHandle, title.c_str());
}

void VoxelsApplication::render() {
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

    glDispatchCompute(NUM_CHUNKS / 1, 1, 1);
    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

    // Render
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader.use();
    shader.setMat4("view", view);
    shader.setMat4("projection", projection);

    glBindVertexArray(worldMesh.VAO);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, chunkDrawCmdBuffer);
    glMultiDrawArraysIndirectCount(GL_TRIANGLES, nullptr, 0, NUM_CHUNKS, sizeof(ChunkDrawCommand));
}

void VoxelsApplication::processInput() {
    for (int key : keys) {
        switch (key) {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(windowHandle, true);
                break;

            case GLFW_KEY_W:
                camera.processKeyboard(FORWARD, deltaTime);
                break;
            case GLFW_KEY_S:
                camera.processKeyboard(BACKWARD, deltaTime);
                break;
            case GLFW_KEY_A:
                camera.processKeyboard(LEFT, deltaTime);
                break;
            case GLFW_KEY_D:
                camera.processKeyboard(RIGHT, deltaTime);
                break;
            case GLFW_KEY_SPACE:
                camera.processKeyboard(UP, deltaTime);
                break;
            case GLFW_KEY_LEFT_SHIFT:
                camera.processKeyboard(DOWN, deltaTime);
                break;

            case GLFW_KEY_T:
                if (!lastFrameKeys.contains(GLFW_KEY_T)) {
                    if (wireframe) {
                        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                    } else {
                        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    }
                    wireframe = !wireframe;
                }
                break;

            default:
                break;
        }
    }

    for (int button : buttons) {
        switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT:
                if (!lastFrameButtons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
                    RaycastResult result = raycast();
                    if (result.chunkIndex != -1) {
                        updateVoxel(result, false);
                    }
                }
                break;
            case GLFW_MOUSE_BUTTON_RIGHT:
                if (!lastFrameButtons.contains(GLFW_MOUSE_BUTTON_RIGHT)) {
                    RaycastResult result = raycast();
                    if (result.chunkIndex != -1) {
                        updateVoxel(result, true);
                    }
                }
                break;
            default:
                break;
        }
    }

    if (buttons.contains(GLFW_MOUSE_BUTTON_4) || keys.contains(GLFW_KEY_LEFT_CONTROL)) {
        camera.movementSpeed = 100.0f;
    } else {
        camera.movementSpeed = 10.0f;
    }

    Application::processInput();
}

void VoxelsApplication::cleanup() {
    glDeleteBuffers(1, &chunkDrawCmdBuffer);
    glDeleteBuffers(1, &chunkDataBuffer);
    glDeleteBuffers(1, &commandCountBuffer);
    glDeleteBuffers(1, &verticesBuffer);

    Application::cleanup();
}

int step(float edge, float x) {
    return x < edge ? 0 : 1;
}

RaycastResult VoxelsApplication::raycast() {
    float big = 1e30f;

    float ox = camera.position.x;
    float oy = camera.position.y - 1;
    float oz = camera.position.z;

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
            int cx = px / CHUNK_SIZE;
            int cz = pz / CHUNK_SIZE;

            if (0 <= cx && cx <= (WORLD_SIZE / CHUNK_SIZE - 1) && 0 <= cz && cz <= (WORLD_SIZE / CHUNK_SIZE - 1)) {
                Chunk &chunk = chunks[cz + cx * (WORLD_SIZE / CHUNK_SIZE)];
                int v = chunk.load(px % CHUNK_SIZE, py, pz % CHUNK_SIZE);

                if (v != 0) {
                    std::cout << "Voxel hit at " << px << ", " << py << ", " << pz << ", face: " << faceHit << std::endl;
                    return RaycastResult {
                            .chunkIndex = cz + cx * (WORLD_SIZE / CHUNK_SIZE),
                            .x = px % CHUNK_SIZE,
                            .y = py,
                            .z = pz % CHUNK_SIZE,
                            .face = faceHit
                    };
                }
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

    return RaycastResult {
            .chunkIndex = -1,
            .x = -1,
            .y = -1,
            .z = -1,
            .face = -1
    };
}

void VoxelsApplication::updateVoxel(RaycastResult result, bool place) {
    Chunk *chunk = &chunks[result.chunkIndex];

    if (place) {
        if (result.face == 0) {
            if (result.x == CHUNK_SIZE - 1) {
                if (chunk->cx == WORLD_SIZE / CHUNK_SIZE - 1) {
                    return;
                }
                chunk = &chunks[chunk->cz + (chunk->cx + 1) * (WORLD_SIZE / CHUNK_SIZE)];
                result.x = 0;
                result.chunkIndex += WORLD_SIZE / CHUNK_SIZE;
            } else {
                result.x++;
            }
        } else if (result.face == 1) {
            if (result.x == 0) {
                if (chunk->cx == 0) {
                    return;
                }
                chunk = &chunks[chunk->cz + (chunk->cx - 1) * (WORLD_SIZE / CHUNK_SIZE)];
                result.x = CHUNK_SIZE - 1;
                result.chunkIndex -= WORLD_SIZE / CHUNK_SIZE;
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
                if (chunk->cz == WORLD_SIZE / CHUNK_SIZE - 1) {
                    return;
                }
                chunk = &chunks[chunk->cz + 1 + chunk->cx * (WORLD_SIZE / CHUNK_SIZE)];
                result.z = 0;
                result.chunkIndex++;
            } else {
                result.z++;
            }
        } else if (result.face == 5) {
            if (result.z == 0) {
                if (chunk->cz == 0) {
                    return;
                }
                chunk = &chunks[chunk->cz - 1 + chunk->cx * (WORLD_SIZE / CHUNK_SIZE)];
                result.z = CHUNK_SIZE - 1;
                result.chunkIndex--;
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
    if (result.x == 0 && chunk->cx > 0) {
        Chunk &negXChunk = chunks[chunk->cz + (chunk->cx - 1) * (WORLD_SIZE / CHUNK_SIZE)];
        negXChunk.store(CHUNK_SIZE, result.y, result.z, place ? 1 : 0);
        negXChunk.minY = std::min(negXChunk.minY, result.y);
        negXChunk.maxY = std::max(negXChunk.maxY, result.y + 2);
        chunksToMesh.push_back(&negXChunk);

        if (result.z == 0 && chunk->cz > 0) {
            Chunk &negXNegZChunk = chunks[chunk->cz - 1 + (chunk->cx - 1) * (WORLD_SIZE / CHUNK_SIZE)];
            negXNegZChunk.store(CHUNK_SIZE, result.y, CHUNK_SIZE, place ? 1 : 0);
            negXNegZChunk.minY = std::min(negXNegZChunk.minY, result.y);
            negXNegZChunk.maxY = std::max(negXNegZChunk.maxY, result.y + 2);
            chunksToMesh.push_back(&negXNegZChunk);
        } else if (result.z == CHUNK_SIZE - 1 && chunk->cz < WORLD_SIZE / CHUNK_SIZE - 1) {
            Chunk &negXPosZChunk = chunks[chunk->cz + 1 + (chunk->cx - 1) * (WORLD_SIZE / CHUNK_SIZE)];
            negXPosZChunk.store(CHUNK_SIZE, result.y, -1, place ? 1 : 0);
            negXPosZChunk.minY = std::min(negXPosZChunk.minY, result.y);
            negXPosZChunk.maxY = std::max(negXPosZChunk.maxY, result.y + 2);
            chunksToMesh.push_back(&negXPosZChunk);
        }
    } else if (result.x == CHUNK_SIZE - 1 && chunk->cx < WORLD_SIZE / CHUNK_SIZE - 1) {
        Chunk &posXChunk = chunks[chunk->cz + (chunk->cx + 1) * (WORLD_SIZE / CHUNK_SIZE)];
        posXChunk.store(-1, result.y, result.z, place ? 1 : 0);
        posXChunk.minY = std::min(posXChunk.minY, result.y);
        posXChunk.maxY = std::max(posXChunk.maxY, result.y + 2);
        chunksToMesh.push_back(&posXChunk);

        if (result.z == 0 && chunk->cz > 0) {
            Chunk &posXNegZChunk = chunks[chunk->cz - 1 + (chunk->cx + 1) * (WORLD_SIZE / CHUNK_SIZE)];
            posXNegZChunk.store(-1, result.y, CHUNK_SIZE, place ? 1 : 0);
            posXNegZChunk.minY = std::min(posXNegZChunk.minY, result.y);
            posXNegZChunk.maxY = std::max(posXNegZChunk.maxY, result.y + 2);
            chunksToMesh.push_back(&posXNegZChunk);
        } else if (result.z == CHUNK_SIZE - 1 && chunk->cz < WORLD_SIZE / CHUNK_SIZE - 1) {
            Chunk &posXPosZChunk = chunks[chunk->cz + 1 + (chunk->cx + 1) * (WORLD_SIZE / CHUNK_SIZE)];
            posXPosZChunk.store(-1, result.y, -1, place ? 1 : 0);
            posXPosZChunk.minY = std::min(posXPosZChunk.minY, result.y);
            posXPosZChunk.maxY = std::max(posXPosZChunk.maxY, result.y + 2);
            chunksToMesh.push_back(&posXPosZChunk);
        }
    }

    if (result.z == 0 && chunk->cz > 0) {
        Chunk &negZChunk = chunks[chunk->cz - 1 + chunk->cx * (WORLD_SIZE / CHUNK_SIZE)];
        negZChunk.store(result.x, result.y, CHUNK_SIZE, place ? 1 : 0);
        negZChunk.minY = std::min(negZChunk.minY, result.y);
        negZChunk.maxY = std::max(negZChunk.maxY, result.y + 2);
        chunksToMesh.push_back(&negZChunk);
    } else if (result.z == CHUNK_SIZE - 1 && chunk->cz < WORLD_SIZE / CHUNK_SIZE - 1) {
        Chunk &posZChunk = chunks[chunk->cz + 1 + chunk->cx * (WORLD_SIZE / CHUNK_SIZE)];
        posZChunk.store(result.x, result.y, -1, place ? 1 : 0);
        posZChunk.minY = std::min(posZChunk.minY, result.y);
        posZChunk.maxY = std::max(posZChunk.maxY, result.y + 2);
        chunksToMesh.push_back(&posZChunk);
    }

    // Mesh chunks
    for (Chunk *c: chunksToMesh) {
        Mesher::meshChunk(c, WORLD_SIZE, worldMesh, worldMesh.data, true);
    }

    // Update firstIndex for all chunks
    int firstIndex = 0;
    for (int i = 0; i < (WORLD_SIZE / CHUNK_SIZE) * (WORLD_SIZE / CHUNK_SIZE); ++i) {
        chunks[i].firstIndex = firstIndex;
        firstIndex += chunks[i].numVertices;
    }

    // Update vertex buffer
    glNamedBufferData(verticesBuffer, sizeof(uint32_t) * worldMesh.data.size(),
                      (const void *) worldMesh.data.data(), GL_DYNAMIC_DRAW);

    // Update chunk data
    ChunkData cd = {
            .cx = chunk->cx,
            .cz = chunk->cz,
            .minY = chunk->minY,
            .maxY = chunk->maxY,
            .numVertices = chunk->numVertices,
            .firstIndex = chunk->firstIndex,
            ._pad0 = 0,
            ._pad1 = 0,
    };

    chunkData[result.chunkIndex] = cd;

    // Update other chunks' firstIndex and numVertices
    for (int i = 0; i < (WORLD_SIZE / CHUNK_SIZE) * (WORLD_SIZE / CHUNK_SIZE); ++i) {
        chunkData[i].firstIndex = chunks[i].firstIndex;
        chunkData[i].numVertices = chunks[i].numVertices;
    }

    // Update all chunk data
    void *chunkDataBufferPtr = glMapNamedBuffer(chunkDataBuffer, GL_WRITE_ONLY);

    if (chunkDataBufferPtr) {
        // Perform the memory copy
        std::memcpy(chunkDataBufferPtr, chunkData.data(), sizeof(ChunkData) * chunkData.size());

        // Unmap the buffer after the operation
        if (!glUnmapNamedBuffer(chunkDataBuffer)) {
            std::cerr << "Failed to unmap buffer!" << std::endl;
        }
    } else {
        std::cerr << "Failed to map buffer!" << std::endl;
    }
}

void VoxelsApplication::key_callback(int key, int scancode, int action, int mods) {
    Application::key_callback(key, scancode, action, mods);
}

void VoxelsApplication::mouse_button_callback(int button, int action, int mods) {
    Application::mouse_button_callback(button, action, mods);
}

void VoxelsApplication::cursor_position_callback(double xposIn, double yposIn) {
    auto xpos = static_cast<float>(xposIn);
    auto ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    camera.processMouse(xoffset, yoffset, 0);
}

void VoxelsApplication::scroll_callback(double xoffset, double yoffset) {
    camera.processMouse(0, 0, yoffset);
}
