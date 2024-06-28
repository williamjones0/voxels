#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "util/PerlinNoise.hpp"

#include <iostream>
#include "opengl/Shader.h"
#include "core/Camera.hpp"
#include "world/Mesher.hpp"
#include "world/Chunk.hpp"
#include "world/WorldMesh.hpp"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void glfw_error_callback(int error, const char* description);
void processInput(GLFWwindow* window);
void putMatrix4f(std::vector<float> &buffer, glm::mat4 m);

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

#define CHUNK_SIZE_SHIFT 4

typedef struct {
    unsigned int count;
    unsigned int instanceCount;
    unsigned int firstIndex;
    unsigned int baseInstance;
} DrawArraysIndirectCommand;

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

    WorldMesh worldMesh;

    const int WORLD_SIZE = 128;

    const int NUM_AXIS_CHUNKS = WORLD_SIZE / CHUNK_SIZE;
    const int NUM_CHUNKS = NUM_AXIS_CHUNKS * NUM_AXIS_CHUNKS;
    std::vector<Chunk> chunks;
    for (int cx = 0; cx < NUM_AXIS_CHUNKS; ++cx) {
        for (int cz = 0; cz < NUM_AXIS_CHUNKS; ++cz) {
            Chunk chunk = Chunk(cx, cz);
            chunk.init(worldMesh.data);
            chunks.push_back(chunk);

//            std::cout << "Chunk " << cz + cx * NUM_AXIS_CHUNKS << ":\n";
//            std::cout << "positions size: " << chunk.positions.size() << std::endl;
//            std::cout << "colours size: " << chunk.colours.size() << std::endl;
//            std::cout << "normals size: " << chunk.normals.size() << std::endl;
//            std::cout << "ao size: " << chunk.ao.size() << std::endl;
//            std::cout << glm::to_string(chunk.model) << std::endl;

//            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
//                for (int z = 0; z < CHUNK_SIZE; ++z) {
//                    for (int x = 0; x < CHUNK_SIZE; ++x) {
//                        std::cout << chunk.voxels[y * CHUNK_SIZE * CHUNK_SIZE + z * CHUNK_SIZE + x] << " ";
//                    }
//                    std::cout << std::endl;
//                }
//                std::cout << std::endl;
//            }

//            std::cout << std::endl
        }
    }

    worldMesh.createBuffers();

    std::vector<DrawArraysIndirectCommand> commands;
    std::vector<float> cmb;
    unsigned int firstIndex = 0;
    for (size_t i = 0; i < NUM_CHUNKS; ++i) {
        DrawArraysIndirectCommand chunkCmd = {
                .count = static_cast<unsigned int>(chunks[i].numVertices),
                .instanceCount = 1,
                .firstIndex = firstIndex,
                .baseInstance = 0
        };
        commands.push_back(chunkCmd);
        firstIndex += chunks[i].numVertices;

        // Push chunk's model matrix to the vertex buffer
        putMatrix4f(cmb, chunks[i].model);
    }

    GLuint drawCmdBuffer;
    glCreateBuffers(1, &drawCmdBuffer);

    glNamedBufferStorage(drawCmdBuffer,
        sizeof(DrawArraysIndirectCommand) * commands.size(),
        (const void*)commands.data(),
        GL_DYNAMIC_STORAGE_BIT);

    GLuint chunkModelBuffer;
    glCreateBuffers(1, &chunkModelBuffer);

    glNamedBufferStorage(chunkModelBuffer,
        sizeof(float) * cmb.size(),
        (const void*)cmb.data(),
        GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, chunkModelBuffer);

    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        camera.update(deltaTime);
        processInput(window);

        glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective((float)(camera.FOV * PI / 180), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 5000.0f);

        shader.use();

        shader.setMat4("view", camera.calculateViewMatrix());
        shader.setMat4("projection", projection);

        glBindVertexArray(worldMesh.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, worldMesh.VBO);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawCmdBuffer);
        glMultiDrawArraysIndirect(GL_TRIANGLES, 0, NUM_CHUNKS, sizeof(DrawArraysIndirectCommand));

        // std::cout << "Frame time: " << deltaTime << "\t FPS: " << (1.0f / deltaTime) << std::endl;

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

void putMatrix4f(std::vector<float> &buffer, glm::mat4 m) {
    buffer.push_back(m[0][0]);
    buffer.push_back(m[0][1]);
    buffer.push_back(m[0][2]);
    buffer.push_back(m[0][3]);
    buffer.push_back(m[1][0]);
    buffer.push_back(m[1][1]);
    buffer.push_back(m[1][2]);
    buffer.push_back(m[1][3]);
    buffer.push_back(m[2][0]);
    buffer.push_back(m[2][1]);
    buffer.push_back(m[2][2]);
    buffer.push_back(m[2][3]);
    buffer.push_back(m[3][0]);
    buffer.push_back(m[3][1]);
    buffer.push_back(m[3][2]);
    buffer.push_back(m[3][3]);
}
