#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <chrono>

#include "opengl/Shader.h"
#include "core/Camera.hpp"
#include "world/Mesher.hpp"
#include "world/Chunk.hpp"
#include "world/WorldMesh.hpp"
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
void processInput(GLFWwindow* window);
template <glm::length_t C, glm::length_t R, typename T>
void putMatrix(std::vector<T> &buffer, glm::mat<C, R, T> m);

const unsigned int SCREEN_WIDTH = 1920;
const unsigned int SCREEN_HEIGHT = 1080;

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

Camera camera(glm::vec3(-5.0f, 20.0f, 0.0f), 0.0f, 0.0f);
float lastX = SCREEN_WIDTH / 2.0f;
float lastY = SCREEN_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

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
    Shader clearShader("../../../../../data/shaders/clear_comp.glsl");

    WorldMesh worldMesh;

    const int NUM_AXIS_CHUNKS = WORLD_SIZE / CHUNK_SIZE;
    const int NUM_CHUNKS = NUM_AXIS_CHUNKS * NUM_AXIS_CHUNKS;
    unsigned int firstIndex = 0;
    std::vector<Chunk> chunks;
    for (int cx = 0; cx < NUM_AXIS_CHUNKS; ++cx) {
        for (int cz = 0; cz < NUM_AXIS_CHUNKS; ++cz) {
            Chunk chunk = Chunk(cx, cz, firstIndex);

            auto startTime = std::chrono::high_resolution_clock::now();
            chunk.init(worldMesh.data);
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            std::cout << "Chunk " << cz + cx * NUM_AXIS_CHUNKS << " took " << duration.count() << "us to init" << std::endl << std::endl;
            chunks.push_back(chunk);
            firstIndex += chunk.numVertices;
        }
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
        NULL);
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

    glNamedBufferStorage(verticesBuffer,
        sizeof(uint32_t) * worldMesh.data.size(),
        (const void*)worldMesh.data.data(),
        GL_DYNAMIC_STORAGE_BIT);
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

        processInput(window);

        // Clear command count buffer
        clearShader.use();
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

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

        // std::cout << "Frame time: " << deltaTime << "\t FPS: " << (1.0f / deltaTime) << std::endl;
        std::string title = "Voxels | FPS: " + std::to_string((int)(1.0f / deltaTime)) +
            " | X: " + std::to_string(camera.position.x) +
            ", Y: " + std::to_string(camera.position.y) +
            ", Z: " + std::to_string(camera.position.z);

        glfwSetWindowTitle(window, title.c_str());

        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window) {
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

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
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
