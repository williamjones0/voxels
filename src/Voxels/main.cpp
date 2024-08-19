#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <chrono>
#include <set>
#include <thread>

#include "opengl/Shader.h"
#include "core/Camera.hpp"
#include "world/Mesher.hpp"
#include "world/Chunk.hpp"
#include "world/WorldMesh.hpp"
#include "world/Line.h"
#include "util/PerlinNoise.hpp"
#include "util/Util.hpp"
#include "util/Flags.h"

typedef struct {
    unsigned int count;
    unsigned int instanceCount;
    unsigned int firstIndex;
    unsigned int baseInstance;
} DrawArraysIndirectCommand;

typedef struct {
    unsigned int count;
    unsigned int instanceCount;
    unsigned int firstIndex;
    unsigned int baseInstance;
    unsigned int chunkIndex;
} ChunkDrawCommand;

typedef struct {
    glm::mat4 model;
    int cx;
    int cz;
    int minY;
    int maxY;
    unsigned int numVertices;
    unsigned int firstIndex;
    unsigned int _pad0;
    unsigned int _pad1;
} ChunkData;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void glfw_error_callback(int error, const char* description);
void processInput(GLFWwindow *window, std::vector<Chunk> &chunks, std::vector<ChunkData> &chunkData, WorldMesh &worldMesh, GLuint verticesBuffer, GLuint chunkDataBuffer);
template <glm::length_t C, glm::length_t R, typename T>
void putMatrix(std::vector<T> &buffer, glm::mat<C, R, T> m);
float intbound(float s, float ds);
void raycast(float radius, std::vector<Chunk> &chunks, WorldMesh &worldMesh, GLuint verticesBuffer, GLuint chunkDataBuffer);
void raycast_(float radius, std::vector<Chunk> &chunks, std::vector<ChunkData> &chunkData, WorldMesh &worldMesh, GLuint verticesBuffer, GLuint chunkDataBuffer);

const unsigned int SCREEN_WIDTH = 2560 * 0.8;
const unsigned int SCREEN_HEIGHT = 1600 * 0.8;

const double PI = 3.1415926535;

#define FRONT_FACE 0
#define BACK_FACE 18
#define LEFT_FACE 36
#define RIGHT_FACE 54
#define BOTTOM_FACE 72
#define TOP_FACE 90

#define VERTICES_LENGTH 108

glm::vec4 frustum[6];
bool doFrustumCulling = true;
int visibleChunks = 0;

Camera camera(glm::vec3(1.5183858f, 3.0508814f + 1, 3.6004007f), 0.0f, 0.0f);
// Camera camera(glm::vec3(4.2f, 1.5f, 0.6f), 0.0f, 0.0f);
float lastX = SCREEN_WIDTH / 2.0f;
float lastY = SCREEN_HEIGHT / 2.0f;
bool firstMouse = true;

std::set<int> lastFrameKeyPresses;
std::set<int> lastFrameMousePresses;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool wireframe = false;

void GLAPIENTRY MessageCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam
) {
    std::string SEVERITY = "";
    switch (severity) {
    case GL_DEBUG_SEVERITY_LOW:
        SEVERITY = "LOW";
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        SEVERITY = "MEDIUM";
        break;
    case GL_DEBUG_SEVERITY_HIGH:
        SEVERITY = "HIGH";
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        SEVERITY = "NOTIFICATION";
        break;
    }
    fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = %s, message = %s\n",
        type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "",
        type, SEVERITY.c_str(), message);
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Voxels", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwSetErrorCallback(glfw_error_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Enable debug output
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(MessageCallback, 0);

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    glEnable(GL_DEPTH_TEST);

    Shader shader("../../../../../data/shaders/vert.glsl", "../../../../../data/shaders/frag.glsl");
    Shader drawCommandShader("../../../../../data/shaders/drawcmd_comp.glsl");
    Shader lineShader("../../../../../data/shaders/line_vert.glsl", "../../../../../data/shaders/line_frag.glsl");

    const int NUM_AXIS_CHUNKS = WORLD_SIZE / CHUNK_SIZE;
    const int NUM_CHUNKS = NUM_AXIS_CHUNKS * NUM_AXIS_CHUNKS;
    unsigned int firstIndex = 0;
    std::vector<Chunk> chunks(NUM_CHUNKS);

    WorldMesh worldMesh(NUM_CHUNKS);

    //unsigned int numThreads = std::thread::hardware_concurrency();
    //std::cout << "Number of threads: " << numThreads << std::endl;

    //int numChunksPerThread = NUM_CHUNKS / numThreads;
    //int numThreadsWithOneMoreChunk = NUM_CHUNKS - (numChunksPerThread * numThreads);

    //std::vector<std::thread> threads;
    //std::vector<std::vector<uint32_t>> chunkVertices(numThreads);
    //for (int threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
    //    threads.push_back(std::thread([threadIndex, numChunksPerThread, numThreadsWithOneMoreChunk, &chunkVertices, &chunks, &worldMesh]() {
    //        int start = std::min(threadIndex, numThreadsWithOneMoreChunk) * (numChunksPerThread + 1) + std::max(0, threadIndex - numThreadsWithOneMoreChunk) * numChunksPerThread;
    //        int end = start + numChunksPerThread;
    //        if (threadIndex < numThreadsWithOneMoreChunk) {
    //            end++;
    //        }

    //        for (int i = start; i < end; ++i) {
    //            int cx = i / NUM_AXIS_CHUNKS;
    //            int cz = i % NUM_AXIS_CHUNKS;
    //            Chunk chunk = Chunk(cx, cz);

    //            auto startTime = std::chrono::high_resolution_clock::now();
    //            chunk.init(chunkVertices[threadIndex]);
    //            auto endTime = std::chrono::high_resolution_clock::now();
    //            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    //            // std::cout << "Chunk " << cz + cx * NUM_AXIS_CHUNKS << " took " << duration.count() << "us to init" << std::endl << std::endl;
                //chunks[i] = chunk;
    //        }
    //    }));
    //}

    //for (int i = 0; i < numThreads; ++i) {
    //    threads[i].join();
    //}

    //for (int i = 0; i < numThreads; ++i) {
    //    worldMesh.data.insert(worldMesh.data.end(), chunkVertices[i].begin(), chunkVertices[i].end());
    //}

    for (int cx = 0; cx < NUM_AXIS_CHUNKS; ++cx) {
        for (int cz = 0; cz < NUM_AXIS_CHUNKS; ++cz) {
            Chunk chunk = Chunk(cx, cz);

            auto startTime = std::chrono::high_resolution_clock::now();
            chunk.init(worldMesh.data);
            // chunk.generateVoxels("../../../../../data/levels/level1.txt");
            chunk.generateVoxels2D();
            meshChunk(&chunk, WORLD_SIZE, worldMesh);

            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            std::cout << "Chunk " << cz + cx * NUM_AXIS_CHUNKS << " took " << duration.count() << "us to init" << std::endl << std::endl;
            chunks[cz + cx * NUM_AXIS_CHUNKS] = chunk;
        }
    }

    for (int i = 0; i < NUM_CHUNKS; ++i) {
        chunks[i].firstIndex = firstIndex;
        firstIndex += chunks[i].numVertices;
    }

    std::cout << "totalMesherTime: " << totalMesherTime << std::endl;

    worldMesh.createBuffers();

    std::vector<ChunkData> chunkData;

    for (size_t i = 0; i < NUM_CHUNKS; ++i) {
        ChunkData cd = {
            .model = chunks[i].model,
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

    // Chunk draw command shader buffers
    GLuint chunkDrawCmdBuffer;
    glCreateBuffers(1, &chunkDrawCmdBuffer);

    glNamedBufferStorage(chunkDrawCmdBuffer,
        sizeof(ChunkDrawCommand) * NUM_CHUNKS,
        NULL,
        NULL);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, chunkDrawCmdBuffer);

    GLuint chunkDataBuffer;
    glCreateBuffers(1, &chunkDataBuffer);

    glNamedBufferStorage(chunkDataBuffer,
        sizeof(ChunkData) * chunkData.size(),
        (const void*)chunkData.data(),
        GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, chunkDataBuffer);

    GLuint commandCountBuffer;
    glCreateBuffers(1, &commandCountBuffer);

    glNamedBufferStorage(commandCountBuffer,
        sizeof(unsigned int),
        NULL,
        NULL);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, commandCountBuffer);
    glBindBuffer(GL_PARAMETER_BUFFER, commandCountBuffer);

    GLuint verticesBuffer;
    glCreateBuffers(1, &verticesBuffer);

    glNamedBufferData(verticesBuffer,
        sizeof(uint32_t) * worldMesh.data.size(),
        (const void*)worldMesh.data.data(),
        GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, verticesBuffer);

    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    shader.use();
    shader.setInt("chunkSizeShift", CHUNK_SIZE_SHIFT);
    shader.setInt("chunkHeightShift", CHUNK_HEIGHT_SHIFT);

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        camera.update(deltaTime);

        glm::mat4 projection = glm::perspective((float)(camera.FOV * PI / 180), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 5000.0f);
        glm::mat4 view = camera.calculateViewMatrix();

        glm::mat4 projectionT = glm::transpose(projection * view);
        frustum[0] = projectionT[3] + projectionT[0];  // x + w < 0
        frustum[1] = projectionT[3] - projectionT[0];  // x - w > 0
        frustum[2] = projectionT[3] + projectionT[1];  // y + w < 0
        frustum[3] = projectionT[3] - projectionT[1];  // y - w > 0
        frustum[4] = projectionT[3] + projectionT[2];  // z + w < 0
        frustum[5] = projectionT[3] - projectionT[2];  // z - w > 0

        processInput(window, chunks, chunkData, worldMesh, verticesBuffer, chunkDataBuffer);

        // Clear command count buffer
        glClearNamedBufferData(commandCountBuffer, GL_R32UI, GL_RED, GL_UNSIGNED_INT, NULL);

        // Generate draw commands
        drawCommandShader.use();
        drawCommandShader.setVec4Array("frustum", frustum, 6);

        glDispatchCompute(NUM_CHUNKS / 1, 1, 1);
        glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

        // Render
        glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        shader.setMat4("view", view);
        shader.setMat4("projection", projection);

        glBindVertexArray(worldMesh.VAO);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, chunkDrawCmdBuffer);
        glMultiDrawArraysIndirectCount(GL_TRIANGLES, 0, 0, NUM_CHUNKS, sizeof(ChunkDrawCommand));

        Line line(
            glm::vec3(camera.position.x, camera.position.y, camera.position.z),
            glm::vec3(camera.position.x, camera.position.y, camera.position.z) + glm::vec3(camera.front.x, camera.front.y, camera.front.z) * 0.4f
        );

        lineShader.use();

        lineShader.setVec3("color", glm::vec3(1.0f, 0.0f, 0.0f));
        lineShader.setMat4("mvp", projection * view);
        line.draw();

        // std::cout << "Frame time: " << deltaTime << "\t FPS: " << (1.0f / deltaTime) << std::endl;
        std::string title = "Voxels | FPS: " + std::to_string((int)(1.0f / deltaTime)) +
            " | X: " + std::to_string(camera.position.x) +
            ", Y: " + std::to_string(camera.position.y) +
            ", Z: " + std::to_string(camera.position.z) +
            "| front: " + glm::to_string(camera.front);

        glfwSetWindowTitle(window, title.c_str());

        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window, std::vector<Chunk> &chunks, std::vector<ChunkData> &chunkData, WorldMesh &worldMesh, GLuint verticesBuffer, GLuint chunkDataBuffer) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.processKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.processKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.processKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.processKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.processKeyboard(UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera.processKeyboard(DOWN, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
        if (!lastFrameKeyPresses.contains(GLFW_KEY_T)) {
            if (wireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
            else {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            }
            wireframe = !wireframe;
        }

        lastFrameKeyPresses.insert(GLFW_KEY_T);
    }
    else {
        lastFrameKeyPresses.erase(GLFW_KEY_T);
    }

    // Mouse
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        if (!lastFrameMousePresses.contains(GLFW_MOUSE_BUTTON_LEFT)) {
            raycast_(10.0f, chunks, chunkData, worldMesh, verticesBuffer, chunkDataBuffer);
        }

        lastFrameMousePresses.insert(GLFW_MOUSE_BUTTON_LEFT);
    } else {
        lastFrameMousePresses.erase(GLFW_MOUSE_BUTTON_LEFT);
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        camera.movementSpeed = 100.0f;
    }
    else {
        camera.movementSpeed = 10.0f;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

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

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.processMouse(0, 0, static_cast<float>(yoffset));
}

void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

template <glm::length_t C, glm::length_t R, typename T>
void putMatrix(std::vector<T> &buffer, glm::mat<C, R, T> m) {
    for (size_t i = 0; i < C; ++i) {
        for (size_t j = 0; j < R; ++j) {
            buffer.push_back(m[i][j]);
        }
    }
}

float intbound(float s, float ds) {
    // Find the smallest positive t such that s + t*ds is an integer.
    if (ds < 0) {
        return intbound(-s, -ds);
    } else {
        s = fmod(s, 1);
        // problem is now s + t * ds = 1
        return (1 - s) / ds;
    }
}

void raycast(float radius, std::vector<Chunk> &chunks, WorldMesh &worldMesh, GLuint verticesBuffer, GLuint chunkDataBuffer) {
    int x = floor(camera.position.x);
    int y = floor(camera.position.y);
    int z = floor(camera.position.z);

    float dx = camera.front.x;
    float dy = camera.front.y;
    float dz = camera.front.z;

    float stepX = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
    float stepY = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;
    float stepZ = (dz > 0) ? 1 : (dz < 0) ? -1 : 0;

    float tMaxX = (stepX > 0) ? (1 - (camera.position.x - x)) / abs(camera.front.x) : (camera.position.x - x) / abs(camera.front.x); // (x - camera.position.x) / camera.front.x; // intbound(camera.position.x, dx); // (stepX > 0) ? (ceil(x) - x) / dx : (x - floor(x)) / dx;
    float tMaxY = (stepY > 0) ? (1 - (camera.position.y - y)) / abs(camera.front.y) : (camera.position.y - y) / abs(camera.front.y); // (y - camera.position.y) / camera.front.y; // intbound(camera.position.y, dy); // (stepY > 0) ? (ceil(y) - y) / dy : (y - floor(y)) / dy;
    float tMaxZ = (stepZ > 0) ? (1 - (camera.position.z - z)) / abs(camera.front.z) : (camera.position.z - z) / abs(camera.front.z); // (z - camera.position.z) / camera.front.z; // intbound(camera.position.z, dz); // (stepZ > 0) ? (ceil(z) - z) / dz : (z - floor(z)) / dz;

    float tDeltaX = (stepX != 0) ? stepX / dx : 0;
    float tDeltaY = (stepY != 0) ? stepY / dy : 0;
    float tDeltaZ = (stepZ != 0) ? stepZ / dz : 0;

    glm::vec3 face = glm::vec3(0.0f);

    radius /= sqrt(dx * dx + dy * dy + dz * dz);

    while (
        (stepX > 0 ? x < WORLD_SIZE : x >= 0) &&
        (stepY > 0 ? y < CHUNK_HEIGHT : y >= 0) &&
        (stepZ > 0 ? z < WORLD_SIZE : z >= 0)
    ) {
        if (!(x < 0 || y < 0 || z < 0 || x >= WORLD_SIZE || y >= CHUNK_HEIGHT || z >= WORLD_SIZE)) {
            // Check if voxel is solid
            int cx = x / CHUNK_SIZE;
            int cz = z / CHUNK_SIZE;

            Chunk &chunk = chunks[cz + cx * (WORLD_SIZE / CHUNK_SIZE)];
            int v = chunk.load(x % CHUNK_SIZE, y, z % CHUNK_SIZE);
            
            if (v != 0) {
                std::cout << "Voxel hit at " << x << ", " << y << ", " << z << std::endl;
                // Change voxel to air
                chunk.store(x % CHUNK_SIZE, y, z % CHUNK_SIZE, 0);

                // If the voxel is on a chunk boundary, update the neighboring chunk(s)
                if (x % CHUNK_SIZE == 0 && cx > 0) {
                    Chunk &negXChunk = chunks[cz + (cx - 1) * (WORLD_SIZE / CHUNK_SIZE)];
                    negXChunk.store(CHUNK_SIZE - 1, y, z % CHUNK_SIZE, 0);
                }
                else if (x % CHUNK_SIZE == CHUNK_SIZE - 1 && cx < WORLD_SIZE / CHUNK_SIZE) {
                    Chunk &posXChunk = chunks[cz + (cx + 1) * (WORLD_SIZE / CHUNK_SIZE)];
                    chunk.store(0, y, z % CHUNK_SIZE, 0);
                }

                if (z % CHUNK_SIZE == 0 && cz > 0) {
                    Chunk &negZChunk = chunks[cz - 1 + cx * (WORLD_SIZE / CHUNK_SIZE)];
                    negZChunk.store(x % CHUNK_SIZE, y, CHUNK_SIZE - 1, 0);
                } else if (z % CHUNK_SIZE == CHUNK_SIZE - 1 && cz < WORLD_SIZE / CHUNK_SIZE) {
                    Chunk &posZChunk = chunks[cz + 1 + cx * (WORLD_SIZE / CHUNK_SIZE)];
                    posZChunk.store(x % CHUNK_SIZE, y, 0, 0);
                }

                // Update chunk
                // meshChunk(&chunk, WORLD_SIZE, worldMesh, true);

                // TEST mesh all chunks
                worldMesh.data.clear();
                for (int cx = 0; cx < WORLD_SIZE / CHUNK_SIZE; ++cx) {
                    for (int cz = 0; cz < WORLD_SIZE / CHUNK_SIZE; ++cz) {
                        Chunk &chunk = chunks[cz + cx * (WORLD_SIZE / CHUNK_SIZE)];
                        meshChunk(&chunk, WORLD_SIZE, worldMesh);
                    }
                }

                // Update firstIndex for all chunks
                int firstIndex = 0;
                for (int i = 0; i < (WORLD_SIZE / CHUNK_SIZE) * (WORLD_SIZE / CHUNK_SIZE); ++i) {
                    chunks[i].firstIndex = firstIndex;
                    firstIndex += chunks[i].numVertices;
                }

                // Update vertex buffer
                glNamedBufferData(verticesBuffer, sizeof(uint32_t) * worldMesh.data.size(), (const void *)worldMesh.data.data(), GL_DYNAMIC_DRAW);
                // glNamedBufferSubData(verticesBuffer, 0, sizeof(uint32_t) * worldMesh.data.size(), worldMesh.data.data());

                // Update chunk data
                ChunkData cd = {
                    .model = chunk.model,
                    .cx = chunk.cx,
                    .cz = chunk.cz,
                    .minY = chunk.minY,
                    .maxY = chunk.maxY,
                    .numVertices = chunk.numVertices,
                    .firstIndex = chunk.firstIndex,
                    ._pad0 = 0,
                    ._pad1 = 0,
                };

                int index = cz + cx * (WORLD_SIZE / CHUNK_SIZE);
                size_t offset = index * sizeof(ChunkData);

                void *chunkDataBufferPtr = glMapNamedBufferRange(chunkDataBuffer, offset, sizeof(ChunkData), GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);

                if (chunkDataBufferPtr) {
                    // Perform the memory copy
                    std::memcpy(chunkDataBufferPtr, &cd, sizeof(ChunkData));

                    // Unmap the buffer after the operation
                    if (!glUnmapNamedBuffer(chunkDataBuffer)) {
                        std::cerr << "Failed to unmap buffer!" << std::endl;
                    }
                } else {
                    std::cerr << "Failed to map buffer range!" << std::endl;
                }

                break;
            }
        }

        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                if (tMaxX > radius) break;

                x += stepX;
                tMaxX += tDeltaX;

                face[0] = -stepX;
                face[1] = 0;
                face[2] = 0;
            } else {
                if (tMaxZ > radius) break;

                z += stepZ;
                tMaxZ += tDeltaZ;

                face[0] = 0;
                face[1] = 0;
                face[2] = -stepZ;
            }
        } else {
            if (tMaxY < tMaxZ) {
                if (tMaxY > radius) break;

                y += stepY;
                tMaxY += tDeltaY;

                face[0] = 0;
                face[1] = -stepY;
                face[2] = 0;
            } else {
                if (tMaxZ > radius) break;

                z += stepZ;
                tMaxZ += tDeltaZ;

                face[0] = 0;
                face[1] = 0;
                face[2] = -stepZ;
            }
        }
    }
}

int step(float edge, float x) {
    return x < edge ? 0 : 1;
}

void raycast_(float radius, std::vector<Chunk> &chunks, std::vector<ChunkData> &chunkData, WorldMesh &worldMesh, GLuint verticesBuffer, GLuint chunkDataBuffer) {
    float big = 1e30f;

    float ox = camera.position.x;
    float oy = camera.position.y - 1;
    float oz = camera.position.z;

    float dx = camera.front.x;
    float dy = camera.front.y;
    float dz = camera.front.z;

    int px = (int)floor(ox);
    int py = (int)floor(oy);
    int pz = (int)floor(oz);

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

    for (int i = 0; i < maxSteps && py >= 0; i++) {
        if (i > 0 && py < CHUNK_HEIGHT) {
            int cx = px / CHUNK_SIZE;
            int cz = pz / CHUNK_SIZE;

            if (0 <= cx && cx <= (WORLD_SIZE / CHUNK_SIZE - 1) && 0 <= cz && cz <= (WORLD_SIZE / CHUNK_SIZE - 1)) {
                Chunk &chunk = chunks[cz + cx * (WORLD_SIZE / CHUNK_SIZE)];
                int v = chunk.load(px % CHUNK_SIZE, py, pz % CHUNK_SIZE);

                if (v != 0) {
                    std::cout << "Voxel hit at " << px << ", " << py << ", " << pz << std::endl;
                    // Change voxel to air
                    chunk.store(px % CHUNK_SIZE, py, pz % CHUNK_SIZE, 0);

                    // If the voxel is on a chunk boundary, update the neighboring chunk(s)
                    if (px % CHUNK_SIZE == 0 && cx > 0) {
                        Chunk &negXChunk = chunks[cz + (cx - 1) * (WORLD_SIZE / CHUNK_SIZE)];
                        negXChunk.store(CHUNK_SIZE, py, pz % CHUNK_SIZE, 0);
                    } else if (px % CHUNK_SIZE == CHUNK_SIZE - 1 && cx < WORLD_SIZE / CHUNK_SIZE - 1) {
                        Chunk &posXChunk = chunks[cz + (cx + 1) * (WORLD_SIZE / CHUNK_SIZE)];
                        posXChunk.store(-1, py, pz % CHUNK_SIZE, 0);
                    }

                    if (pz % CHUNK_SIZE == 0 && cz > 0) {
                        Chunk &negZChunk = chunks[cz - 1 + cx * (WORLD_SIZE / CHUNK_SIZE)];
                        negZChunk.store(px % CHUNK_SIZE, py, CHUNK_SIZE, 0);
                    } else if (pz % CHUNK_SIZE == CHUNK_SIZE - 1 && cz < WORLD_SIZE / CHUNK_SIZE - 1) {
                        Chunk &posZChunk = chunks[cz + 1 + cx * (WORLD_SIZE / CHUNK_SIZE)];
                        posZChunk.store(px % CHUNK_SIZE, py, -1, 0);
                    }

                    // Update chunk
                    meshChunk(&chunk, WORLD_SIZE, worldMesh, true);

                    //// TEST mesh all chunks
                    //worldMesh.data.clear();
                    //for (int cx = 0; cx < WORLD_SIZE / CHUNK_SIZE; ++cx) {
                    //    for (int cz = 0; cz < WORLD_SIZE / CHUNK_SIZE; ++cz) {
                    //        Chunk &chunk = chunks[cz + cx * (WORLD_SIZE / CHUNK_SIZE)];
                    //        meshChunk(&chunk, WORLD_SIZE, worldMesh);
                    //    }
                    //}

                    // Update firstIndex for all chunks
                    int firstIndex = 0;
                    for (int i = 0; i < (WORLD_SIZE / CHUNK_SIZE) * (WORLD_SIZE / CHUNK_SIZE); ++i) {
                        chunks[i].firstIndex = firstIndex;
                        firstIndex += chunks[i].numVertices;
                    }

                    // Update vertex buffer
                    glNamedBufferData(verticesBuffer, sizeof(uint32_t) * worldMesh.data.size(), (const void *)worldMesh.data.data(), GL_DYNAMIC_DRAW);
                    // glNamedBufferSubData(verticesBuffer, 0, sizeof(uint32_t) * worldMesh.data.size(), worldMesh.data.data());

                    // Update chunk data
                    ChunkData cd = {
                        .model = chunk.model,
                        .cx = chunk.cx,
                        .cz = chunk.cz,
                        .minY = chunk.minY,
                        .maxY = chunk.maxY,
                        .numVertices = chunk.numVertices,
                        .firstIndex = chunk.firstIndex,
                        ._pad0 = 0,
                        ._pad1 = 0,
                    };

                    chunkData[cz + cx * (WORLD_SIZE / CHUNK_SIZE)] = cd;

                    // Update other chunk's firstIndex and numVertices
                    for (int i = 0; i < (WORLD_SIZE / CHUNK_SIZE) * (WORLD_SIZE / CHUNK_SIZE); ++i) {
                        chunkData[i].firstIndex = chunks[i].firstIndex;
                        chunkData[i].numVertices = chunks[i].numVertices;
                    }

                    //// Update single chunk data
     //               int index = cz + cx * (WORLD_SIZE / CHUNK_SIZE);
     //               size_t offset = index * sizeof(ChunkData);

     //               void *chunkDataBufferPtr = glMapNamedBufferRange(chunkDataBuffer, offset, sizeof(ChunkData), GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);

     //               if (chunkDataBufferPtr) {
     //                   // Perform the memory copy
     //                   std::memcpy(chunkDataBufferPtr, &cd, sizeof(ChunkData));

     //                   // Unmap the buffer after the operation
     //                   if (!glUnmapNamedBuffer(chunkDataBuffer)) {
     //                       std::cerr << "Failed to unmap buffer!" << std::endl;
     //                   }
     //               } else {
     //                   std::cerr << "Failed to map buffer range!" << std::endl;
     //               }

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

                    break;
                }
            }
        }

        // Advance to next voxel
        cmpx = step(tx, tz) * step(tx, ty);
        cmpy = step(ty, tx) * step(ty, tz);
        cmpz = step(tz, ty) * step(tz, tx);

        tx += dtx * cmpx;
        ty += dty * cmpy;
        tz += dtz * cmpz;

        px += sx * cmpx;
        py += sy * cmpy;
        pz += sz * cmpz;
    }
}
