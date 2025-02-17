#include "VoxelsApplication.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <algorithm>

#include "world/Mesher.hpp"

constexpr float PI = 3.14159265359f;
constexpr int MAX_RENDER_DISTANCE_CHUNKS = 12;
constexpr int MAX_RENDER_DISTANCE_METRES = MAX_RENDER_DISTANCE_CHUNKS << CHUNK_SIZE_SHIFT;
constexpr int MAX_CHUNKS = (2 * MAX_RENDER_DISTANCE_CHUNKS + 1) * (2 * MAX_RENDER_DISTANCE_CHUNKS + 1);

constexpr int MAX_CHUNK_TASKS = 16;

constexpr int INITIAL_VERTEX_BUFFER_SIZE = 1 << 25;

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

    camera = Camera(glm::vec3(8.0f, 400.0f, 8.0f), 0.0f, -90.0f);

    shader = Shader("vert.glsl", "frag.glsl");
    drawCommandProgram = Shader("drawcmd_comp.glsl");

    chunks = std::list<Chunk>();
    frontierChunks = std::vector<Chunk *>();
    newlyCreatedChunks = std::vector<Chunk *>();
    chunkByCoords = std::unordered_map<size_t, Chunk *>();
    chunkData = std::vector<ChunkData>();

    threadPool.start();

    allocator = FreeListAllocator(INITIAL_VERTEX_BUFFER_SIZE, 4096);
    dummyVerticesBuffer = std::vector<uint32_t>(INITIAL_VERTEX_BUFFER_SIZE, 0);

    createChunk(0, 0);

    glCreateVertexArrays(1, &dummyVAO);

    glCreateBuffers(1, &chunkDrawCmdBuffer);
    glNamedBufferStorage(chunkDrawCmdBuffer,
                         sizeof(ChunkDrawCommand) * MAX_CHUNKS,
                         nullptr,
                         GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, chunkDrawCmdBuffer);

    glCreateBuffers(1, &chunkDataBuffer);
    glNamedBufferData(chunkDataBuffer,
                         sizeof(ChunkData) * chunkData.size(),
                         (const void *) chunkData.data(),
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

    updateVerticesBuffer();

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

    chunksReady = false;

    while (updateFrontierChunks()) {}

    chunkTasksCount = 0;

    // Lock mutex and set the ready flag, then notify all waiting threads
    // We need to wait for all threads to be created before meshing to ensure that no invalid references are created
    {
        std::unique_lock<std::mutex> lock(cvMutexChunks);
        chunksReady = true;
    }
    cvChunks.notify_all();

    updateVerticesBuffer();

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

void VoxelsApplication::updateVerticesBuffer() {
    newlyCreatedChunksReady = false;
    cvNewlyCreatedChunks.notify_all();

    // Sort newly created chunks by chunk.index so that chunkData is updated in the correct order
    std::sort(newlyCreatedChunks.begin(), newlyCreatedChunks.end(), [](Chunk *a, Chunk *b) {
        return a->index < b->index;
    });

    for (Chunk *chunk : newlyCreatedChunks) {
        // If the chunk was already destroyed in destroyFrontierChunks, we don't want to allocate, so just skip it
        if (chunk->dying) {
            continue;
        }

        // If the chunk wasn't created yet (TODO: somehow?)
        if (chunk->debug == 0) {
            continue;
        }

        auto region = allocator.allocate(chunk->numVertices);
        if (!region) {
            throw std::runtime_error("Failed to allocate memory for chunk vertices.");
        }

        chunk->firstIndex = region->offset;

        std::copy(chunk->vertices.begin(), chunk->vertices.end(), dummyVerticesBuffer.begin() + chunk->firstIndex);

        glNamedBufferSubData(verticesBuffer,
                             region->offset * sizeof(uint32_t),
                             chunk->numVertices * sizeof(uint32_t),
                             (const void *) chunk->vertices.data());

        chunk->vertices.clear();

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
        chunkData[chunk->index] = cd;

        if (chunk->numVertices == 0) {
            std::cerr << "Chunk has no vertices!" << std::endl;
        }

        chunk->ready = true;
        chunk->debug = 3;
    }

    // Update chunk data buffer
    glNamedBufferData(chunkDataBuffer, sizeof(ChunkData) * chunkData.size(),
                      (const void *) chunkData.data(), GL_DYNAMIC_DRAW);

    // Clear newly created chunks
    newlyCreatedChunks.clear();

    newlyCreatedChunksReady = true;
    cvNewlyCreatedChunks.notify_all();
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
                    if (result.chunk != nullptr) {
                        updateVoxel(result, false);
                    }
                }
                break;
            case GLFW_MOUSE_BUTTON_RIGHT:
                if (!lastFrameButtons.contains(GLFW_MOUSE_BUTTON_RIGHT)) {
                    RaycastResult result = raycast();
                    if (result.chunk != nullptr) {
                        updateVoxel(result, true);
                    }
                }
                break;
            default:
                break;
        }
    }

    if (buttons.contains(GLFW_MOUSE_BUTTON_4) || keys.contains(GLFW_KEY_LEFT_CONTROL)) {
        camera.movementSpeed = 1000.0f;
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
//    float big = 1e30f;
//
//    float ox = camera.position.x;
//    float oy = camera.position.y - 1;
//    float oz = camera.position.z;
//
//    float dx = camera.front.x;
//    float dy = camera.front.y;
//    float dz = camera.front.z;
//
//    int px = (int) std::floor(ox);
//    int py = (int) std::floor(oy);
//    int pz = (int) std::floor(oz);
//
//    float dxi = 1.0f / dx;
//    float dyi = 1.0f / dy;
//    float dzi = 1.0f / dz;
//
//    int sx = dx > 0 ? 1 : -1;
//    int sy = dy > 0 ? 1 : -1;
//    int sz = dz > 0 ? 1 : -1;
//
//    float dtx = std::min(dxi * sx, big);
//    float dty = std::min(dyi * sy, big);
//    float dtz = std::min(dzi * sz, big);
//
//    float tx = abs((px + std::max(sx, 0) - ox) * dxi);
//    float ty = abs((py + std::max(sy, 0) - oy) * dyi);
//    float tz = abs((pz + std::max(sz, 0) - oz) * dzi);
//
//    int maxSteps = 16;
//
//    int cmpx = 0, cmpy = 0, cmpz = 0;
//
//    int faceHit = -1;
//
//    for (int i = 0; i < maxSteps && py >= 0; i++) {
//        if (i > 0 && py < CHUNK_HEIGHT) {
//            int cx = px / CHUNK_SIZE;
//            int cz = pz / CHUNK_SIZE;
//
//            if (0 <= cx && cx <= (WORLD_SIZE / CHUNK_SIZE - 1) && 0 <= cz && cz <= (WORLD_SIZE / CHUNK_SIZE - 1)) {
//                Chunk *chunk = chunkByCoords[key(cx, cz)];
//                int v = chunk->load(px % CHUNK_SIZE, py, pz % CHUNK_SIZE);
//
//                if (v != 0) {
//                    std::cout << "Voxel hit at " << px << ", " << py << ", " << pz << ", face: " << faceHit << std::endl;
//                    return RaycastResult {
//                            .chunk = chunk,
//                            .x = px % CHUNK_SIZE,
//                            .y = py,
//                            .z = pz % CHUNK_SIZE,
//                            .face = faceHit
//                    };
//                }
//            }
//        }
//
//        // Advance to next voxel
//        cmpx = step(tx, tz) * step(tx, ty);
//        cmpy = step(ty, tx) * step(ty, tz);
//        cmpz = step(tz, ty) * step(tz, tx);
//
//        if (cmpx) {
//            faceHit = (sx < 0) ? 0 : 1;  // 0: +x, 1: -x
//        } else if (cmpy) {
//            faceHit = (sy < 0) ? 2 : 3;  // 2: +y, 3: -y
//        } else if (cmpz) {
//            faceHit = (sz < 0) ? 4 : 5;  // 4: +z, 5: -z
//        }
//
//        tx += dtx * cmpx;
//        ty += dty * cmpy;
//        tz += dtz * cmpz;
//
//        px += sx * cmpx;
//        py += sy * cmpy;
//        pz += sz * cmpz;
//    }

    return RaycastResult {
            .chunk = nullptr,
            .x = -1,
            .y = -1,
            .z = -1,
            .face = -1
    };
}

void VoxelsApplication::updateVoxel(RaycastResult result, bool place) {
//    Chunk *chunk = result.chunk;
//
//    if (place) {
//        if (result.face == 0) {
//            if (result.x == CHUNK_SIZE - 1) {
//                if (chunk->cx == WORLD_SIZE / CHUNK_SIZE - 1) {
//                    return;
//                }
//                chunk = chunkByCoords[key(chunk->cx + 1, chunk->cz)];
//                result.x = 0;
//            } else {
//                result.x++;
//            }
//        } else if (result.face == 1) {
//            if (result.x == 0) {
//                if (chunk->cx == 0) {
//                    return;
//                }
//                chunk = chunkByCoords[key(chunk->cx - 1, chunk->cz)];
//                result.x = CHUNK_SIZE - 1;
//            } else {
//                result.x--;
//            }
//        } else if (result.face == 2) {
//            if (result.y == CHUNK_HEIGHT - 2) {
//                return;
//            }
//            result.y++;
//        } else if (result.face == 3) {
//            if (result.y == 0) {
//                return;
//            }
//            result.y--;
//        } else if (result.face == 4) {
//            if (result.z == CHUNK_SIZE - 1) {
//                if (chunk->cz == WORLD_SIZE / CHUNK_SIZE - 1) {
//                    return;
//                }
//                chunk = chunkByCoords[key(chunk->cx, chunk->cz + 1)];
//                result.z = 0;
//            } else {
//                result.z++;
//            }
//        } else if (result.face == 5) {
//            if (result.z == 0) {
//                if (chunk->cz == 0) {
//                    return;
//                }
//                chunk = chunkByCoords[key(chunk->cx, chunk->cz - 1)];
//                result.z = CHUNK_SIZE - 1;
//            } else {
//                result.z--;
//            }
//        }
//    }
//
//    // Change voxel value
//    chunk->store(result.x, result.y, result.z, place ? 1 : 0);
//
//    // Update minY and maxY
//    chunk->minY = std::min(chunk->minY, result.y);
//    chunk->maxY = std::max(chunk->maxY, result.y + 2);
//
//    std::vector<Chunk *> chunksToMesh;
//    chunksToMesh.push_back(chunk);
//
//    // If the voxel is on a chunk boundary, update the neighboring chunk(s)
//    if (result.x == 0 && chunk->cx > 0) {
//        Chunk *negXChunk = chunkByCoords[key(chunk->cx - 1, chunk->cz)];
//        negXChunk->store(CHUNK_SIZE, result.y, result.z, place ? 1 : 0);
//        chunksToMesh.push_back(negXChunk);
//
//        if (result.z == 0 && chunk->cz > 0) {
//            Chunk *negXNegZChunk = chunkByCoords[key(chunk->cx - 1, chunk->cz - 1)];
//            negXNegZChunk->store(CHUNK_SIZE, result.y, CHUNK_SIZE, place ? 1 : 0);
//            chunksToMesh.push_back(negXNegZChunk);
//        } else if (result.z == CHUNK_SIZE - 1 && chunk->cz < WORLD_SIZE / CHUNK_SIZE - 1) {
//            Chunk *negXPosZChunk = chunkByCoords[key(chunk->cx - 1, chunk->cz + 1)];
//            negXPosZChunk->store(CHUNK_SIZE, result.y, -1, place ? 1 : 0);
//            chunksToMesh.push_back(negXPosZChunk);
//        }
//    } else if (result.x == CHUNK_SIZE - 1 && chunk->cx < WORLD_SIZE / CHUNK_SIZE - 1) {
//        Chunk *posXChunk = chunkByCoords[key(chunk->cx + 1, chunk->cz)];
//        posXChunk->store(-1, result.y, result.z, place ? 1 : 0);
//        chunksToMesh.push_back(posXChunk);
//
//        if (result.z == 0 && chunk->cz > 0) {
//            Chunk *posXNegZChunk = chunkByCoords[key(chunk->cx + 1, chunk->cz - 1)];
//            posXNegZChunk->store(-1, result.y, CHUNK_SIZE, place ? 1 : 0);
//            chunksToMesh.push_back(posXNegZChunk);
//        } else if (result.z == CHUNK_SIZE - 1 && chunk->cz < WORLD_SIZE / CHUNK_SIZE - 1) {
//            Chunk *posXPosZChunk = chunkByCoords[key(chunk->cx + 1, chunk->cz + 1)];
//            posXPosZChunk->store(-1, result.y, -1, place ? 1 : 0);
//            chunksToMesh.push_back(posXPosZChunk);
//        }
//    }
//
//    if (result.z == 0 && chunk->cz > 0) {
//        Chunk *negZChunk = chunkByCoords[key(chunk->cx, chunk->cz - 1)];
//        negZChunk->store(result.x, result.y, CHUNK_SIZE, place ? 1 : 0);
//        chunksToMesh.push_back(negZChunk);
//    } else if (result.z == CHUNK_SIZE - 1 && chunk->cz < WORLD_SIZE / CHUNK_SIZE - 1) {
//        Chunk *posZChunk = chunkByCoords[key(chunk->cx, chunk->cz + 1)];
//        posZChunk->store(result.x, result.y, -1, place ? 1 : 0);
//        chunksToMesh.push_back(posZChunk);
//    }
//
//    // Sort by firstIndex descending
//    std::sort(chunksToMesh.begin(), chunksToMesh.end(), [](Chunk *a, Chunk *b) {
//        return a->firstIndex > b->firstIndex;
//    });
//
//    // Erase old vertex data and move chunks to the end of the vectors to preserve order
//    for (Chunk *chunk : chunksToMesh) {
//        worldMesh.data.erase(worldMesh.data.begin() + chunk->firstIndex,
//                             worldMesh.data.begin() + chunk->firstIndex + chunk->numVertices);
//        std::rotate(chunkData.begin() + chunk->index, chunkData.begin() + chunk->index + 1, chunkData.end());
//        std::rotate(chunks.begin() + chunk->index, chunks.begin() + chunk->index + 1, chunks.end());
//    }
//
//    // Mesh chunks
//    for (Chunk *chunk: chunksToMesh) {
//        Mesher::meshChunk(*chunk);
//        worldMesh.data.insert(worldMesh.data.end(), chunk->vertices.begin(), chunk->vertices.end());
//    }
//
//    // Update firstIndex and chunkData for all chunks
//    int firstIndex = 0;
//    for (int i = 0; i < chunks.size(); ++i) {
//        Chunk &chunk = chunks[i];
//        chunk.firstIndex = firstIndex;
//        chunkData[i] = {
//                .cx = chunk.cx,
//                .cz = chunk.cz,
//                .minY = chunk.minY,
//                .maxY = chunk.maxY,
//                .numVertices = chunk.numVertices,
//                .firstIndex = chunk.firstIndex,
//                ._pad0 = 0,
//                ._pad1 = 0,
//        };
//        firstIndex += chunk.numVertices;
//    }
//
//    // Update vertex buffer
//    glNamedBufferData(verticesBuffer, sizeof(uint32_t) * worldMesh.data.size(),
//                      (const void *) worldMesh.data.data(), GL_DYNAMIC_DRAW);
//
//    // Update all chunk data
//    void *chunkDataBufferPtr = glMapNamedBuffer(chunkDataBuffer, GL_WRITE_ONLY);
//
//    if (chunkDataBufferPtr) {
//        // Perform the memory copy
//        std::memcpy(chunkDataBufferPtr, chunkData.data(), sizeof(ChunkData) * chunkData.size());
//
//        // Unmap the buffer after the operation
//        if (!glUnmapNamedBuffer(chunkDataBuffer)) {
//            std::cerr << "Failed to unmap buffer!" << std::endl;
//        }
//    } else {
//        std::cerr << "Failed to map buffer!" << std::endl;
//    }
}

bool VoxelsApplication::updateFrontierChunks() {
    destroyFrontierChunks();
    return createNewFrontierChunks();
}

bool VoxelsApplication::createNewFrontierChunks() {
    std::sort(frontierChunks.begin(), frontierChunks.end(), [this](Chunk *a, Chunk *b) {
        return squaredDistanceToChunk(a->cx, a->cz) < squaredDistanceToChunk(b->cx, b->cz);
    });

    for (size_t i = 0, s = frontierChunks.size(); i < s; i++) {
        Chunk *chunk = frontierChunks[i];

        if (chunkTasksCount >= MAX_CHUNK_TASKS) {
            break;
        }

        if (ensureChunkIfVisible(chunk->cx - 1, chunk->cz) ||
            ensureChunkIfVisible(chunk->cx + 1, chunk->cz) ||
            ensureChunkIfVisible(chunk->cx, chunk->cz - 1) ||
            ensureChunkIfVisible(chunk->cx, chunk->cz + 1)) {
            return true;
        }
    }

    return false;
}

void VoxelsApplication::destroyFrontierChunks() {
    std::vector<Chunk *> toDestroy;

    for (size_t i = 0, s = frontierChunks.size(); i < s; i++) {
        Chunk *chunk = frontierChunks[i];
        // If the chunk's still being initialised, don't destroy it yet since this will invalidate references
        // It will be destroyed later on anyway
        if (chunkInRenderDistance(chunk->cx, chunk->cz) || chunk->initialising) {
            continue;
        }

        // Promote neighbours to frontier if necessary and destroy chunk
        int numPromoted = onFrontierChunkRemoved(chunk);
        frontierChunks.erase(frontierChunks.begin() + i);
        chunkByCoords.erase(key(chunk->cx, chunk->cz));

        // This should only happen if the chunk has already passed through newlyCreatedChunks
        if (chunk->ready) {
            allocator.deallocate(chunk->firstIndex, chunk->numVertices);
        }

        toDestroy.push_back(chunk);
        chunk->dying = true;
        s += numPromoted - 1;
        i--;
    }

    // Ensure that we can erase properly
    std::sort(toDestroy.begin(), toDestroy.end(), [](Chunk *a, Chunk *b) {
        return a->index > b->index;
    });

    // Erase the relevant data from the buffers, remove the chunks from the chunks vector
    // and update frontierChunks and chunkByCoords indices based on the removed chunks
    for (Chunk *chunk : toDestroy) {
        chunkData.erase(chunkData.begin() + chunk->index);

        chunks.remove(*chunk);
    }

    // Update chunk indexes in case chunks were removed
    for (auto it = chunks.begin(); it != chunks.end(); ++it) {
        it->index = std::distance(chunks.begin(), it);
    }
}

bool VoxelsApplication::ensureChunkIfVisible(int cx, int cz) {
    if (!chunkInRenderDistance(cx, cz)) {
        return false;
    }

    return ensureChunk(cx, cz) != nullptr;
}

Chunk *VoxelsApplication::ensureChunk(int cx, int cz) {
    if (chunkByCoords.find(key(cx, cz)) != chunkByCoords.end()) {
        return nullptr;
    }

    return createChunk(cx, cz);
}

Chunk *VoxelsApplication::createChunk(int cx, int cz) {
    chunks.emplace_back(cx, cz);
    Chunk &chunk = chunks.back();
    size_t chunkIndex = chunks.size() - 1;
    chunk.index = chunkIndex;
    chunkByCoords[key(cx, cz)] = &chunk;
    addFrontier(&chunk);
    chunkData.push_back({
        .cx = cx,
        .cz = cz,
        .minY = CHUNK_HEIGHT,
        .maxY = 0,
        .numVertices = 0,
        .firstIndex = 0,
        ._pad0 = 0,
        ._pad1 = 0,
    });

    chunkTasksCount++;

    threadPool.queueTask([&chunk, this]() {
        {
            std::unique_lock<std::mutex> lock(cvMutexChunks);
            cvChunks.wait(lock, [this]() { return chunksReady; });
        }

        chunk.init();
        chunk.generateVoxels2D();

        Mesher::meshChunk(chunk);

        chunk.initialising = false;

        chunk.debug = 1;

        // If newlyCreatedChunks is currently being iterated through, we need to lock the mutex
        {
            std::unique_lock<std::mutex> lock(cvMutexNewlyCreatedChunks);
            cvNewlyCreatedChunks.wait(lock, [this]() { return newlyCreatedChunksReady; });
        }
        chunk.debug = 2;
        newlyCreatedChunks.push_back(&chunk);
    });

    return &chunk;
}

void VoxelsApplication::addFrontier(Chunk *chunk) {
    frontierChunks.push_back(chunk);
    int cx = chunk->cx;
    int cz = chunk->cz;

    updateFrontierNeighbour(chunk, cx - 1, cz);
    updateFrontierNeighbour(chunk, cx + 1, cz);
    updateFrontierNeighbour(chunk, cx, cz - 1);
    updateFrontierNeighbour(chunk, cx, cz + 1);
}

void VoxelsApplication::updateFrontierNeighbour(Chunk *frontier, int cx, int cz) {
    if (chunkByCoords.find(key(cx, cz)) == chunkByCoords.end()) {
        return;
    }

    Chunk *neighbour = chunkByCoords[key(cx, cz)];
    neighbour->neighbours++;
    frontier->neighbours++;
    if (neighbour->neighbours == 4) {
        frontierChunks.erase(std::remove(frontierChunks.begin(), frontierChunks.end(), neighbour), frontierChunks.end());
    }
}

int VoxelsApplication::onFrontierChunkRemoved(Chunk *frontierChunk) {
    int cx = frontierChunk->cx;
    int cz = frontierChunk->cz;
    double d = squaredDistanceToChunk(cx, cz);
    return onFrontierChunkRemoved(cx - 1, cz, d) +
           onFrontierChunkRemoved(cx + 1, cz, d) +
           onFrontierChunkRemoved(cx, cz - 1, d) +
           onFrontierChunkRemoved(cx, cz + 1, d);
}

int VoxelsApplication::onFrontierChunkRemoved(int cx, int cz, double distance) {
    if (chunkByCoords.find(key(cx, cz)) == chunkByCoords.end()) {
        return 0;
    }

    Chunk *chunk = chunkByCoords[key(cx, cz)];
    chunk->neighbours--;
    if (!(std::find(frontierChunks.begin(), frontierChunks.end(), chunk) != frontierChunks.end()) &&
        (chunkInRenderDistance(cx, cz) ||
        squaredDistanceToChunk(cx, cz) < distance)) {
        frontierChunks.push_back(chunk);
        return 1;
    }
    return 0;
}

bool VoxelsApplication::chunkInRenderDistance(int cx, int cz) {
    return squaredDistanceToChunk(cx, cz) < MAX_RENDER_DISTANCE_METRES * MAX_RENDER_DISTANCE_METRES;
}

double VoxelsApplication::squaredDistanceToChunk(int cx, int cz) const {
    double dx = camera.position.x - (cx + 0.5) * CHUNK_SIZE;
    double dz = camera.position.z - (cz + 0.5) * CHUNK_SIZE;
    return dx * dx + dz * dz;
}

size_t VoxelsApplication::key(int i, int j) {
    return (size_t) i << 32 | (unsigned int) j;
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
