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

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void glfw_error_callback(int error, const char* description);
void processInput(GLFWwindow* window);
template <glm::length_t C, glm::length_t R, typename T>
void putMatrix(std::vector<T> &buffer, glm::mat<C, R, T> m);
bool chunkNotInFrustum(Chunk chunk);
bool culledXY(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);
void updateDrawCommandBuffer(std::vector<DrawArraysIndirectCommand>& commands, std::vector<Chunk>& chunks, GLuint drawCmdBuffer, void* drawCmdBufferAddress);
glm::mat4 mulPerspectiveAffine(glm::mat4 proj, glm::mat4 view);

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

    const int NUM_AXIS_CHUNKS = WORLD_SIZE / CHUNK_SIZE;
    const int NUM_CHUNKS = NUM_AXIS_CHUNKS * NUM_AXIS_CHUNKS;
    std::vector<Chunk> chunks;
    for (int cx = 0; cx < NUM_AXIS_CHUNKS; ++cx) {
        for (int cz = 0; cz < NUM_AXIS_CHUNKS; ++cz) {
            Chunk chunk = Chunk(cx, cz);

            auto startTime = std::chrono::high_resolution_clock::now();
            chunk.init(worldMesh.data);
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            std::cout << "Chunk " << cz + cx * NUM_AXIS_CHUNKS << " took " << duration.count() << "us to init" << std::endl << std::endl;
            chunks.push_back(chunk);
        }
    }

    std::cout << "totalMesherTime: " << totalMesherTime << std::endl;

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
        putMatrix(cmb, chunks[i].model);
    }

    GLuint drawCmdBuffer;
    glCreateBuffers(1, &drawCmdBuffer);

    glNamedBufferStorage(drawCmdBuffer,
        sizeof(DrawArraysIndirectCommand) * commands.size(),
        (const void*)commands.data(),
        GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);

    void *drawCmdBufferAddress = glMapNamedBufferRange(drawCmdBuffer,
        0,
        sizeof(DrawArraysIndirectCommand) * commands.size(),
        GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);

    GLuint chunkModelBuffer;
    glCreateBuffers(1, &chunkModelBuffer);

    glNamedBufferStorage(chunkModelBuffer,
        sizeof(float) * cmb.size(),
        (const void*)cmb.data(),
        GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, chunkModelBuffer);

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

        if (doFrustumCulling) {
            glm::mat4 projectionT = projection * view;

            frustum[0] = projectionT[3] + projectionT[0];  // x + w < 0
            frustum[1] = projectionT[3] - projectionT[0];  // x - w > 0
            frustum[2] = projectionT[3] + projectionT[1];  // y + w < 0
            frustum[3] = projectionT[3] - projectionT[1];  // y - w > 0
            frustum[4] = projectionT[3] + projectionT[2];  // z + w < 0
            frustum[5] = projectionT[3] - projectionT[2];  // z - w > 0

            updateDrawCommandBuffer(commands, chunks, drawCmdBuffer, drawCmdBufferAddress);
        }

        processInput(window);

        glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        shader.setMat4("view", view);
        shader.setMat4("projection", projection);

        glBindVertexArray(worldMesh.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, worldMesh.VBO);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawCmdBuffer);
        glMultiDrawArraysIndirect(GL_TRIANGLES, 0, NUM_CHUNKS, sizeof(DrawArraysIndirectCommand));

        // std::cout << "Frame time: " << deltaTime << "\t FPS: " << (1.0f / deltaTime) << std::endl;
        std::string title = "Voxels | FPS: " + std::to_string((int)(1.0f / deltaTime));
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

bool chunkNotInFrustum(Chunk chunk) {
    float xf = (chunk.cx << CHUNK_SIZE_SHIFT) - (float)floor(camera.position.x);
    float ymin = chunk.minY - (float)floor(camera.position.y), ymax = chunk.maxY - (float)floor(camera.position.y);
    float zf = (chunk.cz << CHUNK_SIZE_SHIFT) - (float)floor(camera.position.z);
    return culledXY(xf, ymin, zf, xf + CHUNK_SIZE, ymax + 1, zf + CHUNK_SIZE);
}

bool culledXY(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
    float nxX = frustum[0].x, nxY = frustum[0].y, nxZ = frustum[0].z, nxW = frustum[0].w;
    float pxX = frustum[1].x, pxY = frustum[1].y, pxZ = frustum[1].z, pxW = frustum[1].w;
    float nyX = frustum[2].x, nyY = frustum[2].y, nyZ = frustum[2].z, nyW = frustum[2].w;
    float pyX = frustum[3].x, pyY = frustum[3].y, pyZ = frustum[3].z, pyW = frustum[3].w;

    return nxX * (nxX < 0 ? minX : maxX) + nxY * (nxY < 0 ? minY : maxY) + nxZ * (nxZ < 0 ? minZ : maxZ) < -nxW
        || pxX * (pxX < 0 ? minX : maxX) + pxY * (pxY < 0 ? minY : maxY) + pxZ * (pxZ < 0 ? minZ : maxZ) < -pxW
        || nyX * (nyX < 0 ? minX : maxX) + nyY * (nyY < 0 ? minY : maxY) + nyZ * (nyZ < 0 ? minZ : maxZ) < -nyW
        || pyX * (pyX < 0 ? minX : maxX) + pyY * (pyY < 0 ? minY : maxY) + pyZ * (pyZ < 0 ? minZ : maxZ) < -pyW;
}

void updateDrawCommandBuffer(std::vector<DrawArraysIndirectCommand> &commands, std::vector<Chunk> &chunks, GLuint drawCmdBuffer, void* drawCmdBufferAddress) {
    for (int i = 0; i < chunks.size(); i++) {
        if (chunkNotInFrustum(chunks[i])) {
            std::cout << "Culling chunk " << i << std::endl;
            commands[i].instanceCount = 0;
        }
        else {
            std::cout << "Drawing chunk " << i << std::endl;
            commands[i].instanceCount = 1;
        }
    }

    size_t copySize = commands.size() * sizeof(DrawArraysIndirectCommand);
    std::memcpy(drawCmdBufferAddress, commands.data(), copySize);
    glFlushMappedNamedBufferRange(drawCmdBuffer,
        0,
        copySize);
}

glm::mat4 mulPerspectiveAffine(glm::mat4 proj, glm::mat4 view) {
    float nm00 = proj[0][0] * view[0][0];
    float nm01 = proj[1][1] * view[0][1];
    float nm02 = proj[2][2] * view[0][2];
    float nm03 = proj[2][3] * view[0][2];
    float nm10 = proj[0][0] * view[1][0];
    float nm11 = proj[1][1] * view[1][1];
    float nm12 = proj[2][2] * view[1][2];
    float nm13 = proj[2][3] * view[1][2];
    float nm20 = proj[0][0] * view[2][0];
    float nm21 = proj[1][1] * view[2][1];
    float nm22 = proj[2][2] * view[2][2];
    float nm23 = proj[2][3] * view[2][2];
    float nm30 = proj[0][0] * view[3][0];
    float nm31 = proj[1][1] * view[3][1];
    float nm32 = proj[2][2] * view[3][2] + proj[3][2];
    float nm33 = proj[2][3] * view[3][2];

    return glm::mat4(
        nm00, nm10, nm20, nm30,
        nm01, nm11, nm21, nm31,
        nm02, nm12, nm22, nm32,
        nm03, nm13, nm23, nm33
    );
}
