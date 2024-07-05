#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

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
glm::mat4 mulPerspectiveAffine(glm::mat4 proj, glm::mat<4, 3, float> view);
void translateMat4x3(glm::mat<4, 3, float>& m, float x, float y, float z);
void updateFrustumPlanes(glm::mat4 m);
glm::mat<4, 3, float> rotation(glm::quat quat);

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

bool doFrustumCulling = true;
float nxX, nxY, nxZ, nxW, pxX, pxY, pxZ, pxW, nyX, nyY, nyZ, nyW, pyX, pyY, pyZ, pyW;

Camera camera(glm::vec3(5.2f, 150.3f, 10.4f), 160.0f, -75.0f);
float lastX = SCREEN_WIDTH / 2.0f;
float lastY = SCREEN_HEIGHT / 2.0f;
bool firstMouse = true;

float angx, angy, dangx, dangy;

int visibleChunks = 0;

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

        std::cout << "view: " << std::endl;
        std::cout << view << std::endl;

        //glm::mat<4, 3, float> vMat = rotation(camera.tmpq);
        //translateMat4x3(vMat,
        //    (float) -(camera.position.x - floor(camera.position.x)),
        //    (float) -(camera.position.y - floor(camera.position.y)),
        //    (float) -(camera.position.z - floor(camera.position.z))
        //);

        glm::mat<4, 3, float> vMat = glm::mat<4, 3, float>(view);

        glm::mat4 mvpMat = mulPerspectiveAffine(projection, vMat);
        if (doFrustumCulling) {

            std::cout << "tmpq: " << std::endl;
            std::cout << camera.tmpq << std::endl;
            std::cout << "vMat: " << std::endl;
            std::cout << vMat << std::endl;
            std::cout << "pMat: " << std::endl;
            std::cout << projection << std::endl;
            std::cout << "mvpMat: " << std::endl;
            std::cout << mvpMat << std::endl;

            updateFrustumPlanes(mvpMat);

            updateDrawCommandBuffer(commands, chunks, drawCmdBuffer, drawCmdBufferAddress);
        }

        processInput(window);

        glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        glm::mat4 vMat4 = glm::mat4(
            vMat[0][0], vMat[0][1], vMat[0][2], 0,
            vMat[1][0], vMat[1][1], vMat[1][2], 0,
            vMat[2][0], vMat[2][1], vMat[2][2], 0,
            vMat[3][0], vMat[3][1], vMat[3][2], 0
        );

        shader.setMat4("mvp", mvpMat);
        shader.setMat4("view", view);
        shader.setMat4("projection", projection);

        glBindVertexArray(worldMesh.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, worldMesh.VBO);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawCmdBuffer);
        glMultiDrawArraysIndirect(GL_TRIANGLES, 0, NUM_CHUNKS, sizeof(DrawArraysIndirectCommand));

        // std::cout << "Frame time: " << deltaTime << "\t FPS: " << (1.0f / deltaTime) << std::endl;
        std::string title = "Voxels | FPS: " + std::to_string((int)(1.0f / deltaTime)) +
            " | X: " + std::to_string(camera.position.x) +
            " | Y: " + std::to_string(camera.position.y) +
            " | Z: " + std::to_string(camera.position.z) +
            " | yaw: " + std::to_string(camera.yaw * PI / 180) +
            " | pitch: " + std::to_string(camera.pitch * PI / 180) +
            " | chunks: " + std::to_string(visibleChunks);

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
        float deltaX = xpos - lastX;
        float deltaY = ypos - lastY;
        dangx += deltaY;
        dangy += deltaX;

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
    return nxX * (nxX < 0 ? minX : maxX) + nxY * (nxY < 0 ? minY : maxY) + nxZ * (nxZ < 0 ? minZ : maxZ) < -nxW
        || pxX * (pxX < 0 ? minX : maxX) + pxY * (pxY < 0 ? minY : maxY) + pxZ * (pxZ < 0 ? minZ : maxZ) < -pxW
        || nyX * (nyX < 0 ? minX : maxX) + nyY * (nyY < 0 ? minY : maxY) + nyZ * (nyZ < 0 ? minZ : maxZ) < -nyW
        || pyX * (pyX < 0 ? minX : maxX) + pyY * (pyY < 0 ? minY : maxY) + pyZ * (pyZ < 0 ? minZ : maxZ) < -pyW;
}

void updateDrawCommandBuffer(std::vector<DrawArraysIndirectCommand> &commands, std::vector<Chunk> &chunks, GLuint drawCmdBuffer, void* drawCmdBufferAddress) {
    visibleChunks = 0;
    for (int i = 0; i < chunks.size(); i++) {
        if (chunkNotInFrustum(chunks[i])) {
            // std::cout << "Culling chunk " << i << std::endl;
            commands[i].instanceCount = 0;
        }
        else {
            // std::cout << "Drawing chunk " << i << std::endl;
            commands[i].instanceCount = 1;
            visibleChunks++;
        }
    }

    size_t copySize = commands.size() * sizeof(DrawArraysIndirectCommand);
    std::memcpy(drawCmdBufferAddress, commands.data(), copySize);
    glFlushMappedNamedBufferRange(drawCmdBuffer,
        0,
        copySize);
}

glm::mat4 mulPerspectiveAffine(glm::mat4 proj, glm::mat<4, 3, float> view) {
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
        nm00, nm01, nm02, nm03,
        nm10, nm11, nm12, nm13,
        nm20, nm21, nm22, nm23,
        nm30, nm31, nm32, nm33
    );
}

void translateMat4x3(glm::mat<4, 3, float>& m, float x, float y, float z) {
    m[3][0] = m[0][0] * x + m[1][0] * y + m[2][0] * z + m[3][0];
    m[3][1] = m[0][1] * x + m[1][1] * y + m[2][1] * z + m[3][1];
    m[3][2] = m[0][2] * x + m[1][2] * y + m[2][2] * z + m[3][2];
}

void updateFrustumPlanes(glm::mat4 m) {
    nxX = m[0][3] + m[0][0];
    nxY = m[1][3] + m[1][0];
    nxZ = m[2][3] + m[2][0];
    nxW = m[3][3] + m[3][0];
    pxX = m[0][3] - m[0][0];
    pxY = m[1][3] - m[1][0];
    pxZ = m[2][3] - m[2][0];
    pxW = m[3][3] - m[3][0];
    nyX = m[0][3] + m[0][1];
    nyY = m[1][3] + m[1][1];
    nyZ = m[2][3] + m[2][1];
    nyW = m[3][3] + m[3][1];
    pyX = m[0][3] - m[0][1];
    pyY = m[1][3] - m[1][1];
    pyZ = m[2][3] - m[2][1];
    pyW = m[3][3] - m[3][1];

    std::cout << "nxX: " << nxX << ", nxY: " << nxY << ", nxZ: " << nxZ << ", nxW: " << nxW << std::endl;
    std::cout << "pxX: " << pxX << ", pxY: " << pxY << ", pxZ: " << pxZ << ", pxW: " << pxW << std::endl;
    std::cout << "nyX: " << nyX << ", nyY: " << nyY << ", nyZ: " << nyZ << ", nyW: " << nyW << std::endl;
    std::cout << "pyX: " << pyX << ", pyY: " << pyY << ", pyZ: " << pyZ << ", pyW: " << pyW << std::endl;
    std::cout << std::endl;
}

glm::mat<4, 3, float> rotation(glm::quat quat) {
    float w2 = quat.w * quat.w;
    float x2 = quat.x * quat.x;
    float y2 = quat.y * quat.y;
    float z2 = quat.z * quat.z;
    float zw = quat.z * quat.w;
    float xy = quat.x * quat.y;
    float xz = quat.x * quat.z;
    float yw = quat.y * quat.w;
    float yz = quat.y * quat.z;
    float xw = quat.x * quat.w;

    float m00 = w2 + x2 - z2 - y2;
    float m01 = xy + zw + zw + xy;
    float m02 = xz - yw + xz - yw;
    float m10 = -zw + xy - zw + xy;
    float m11 = y2 - z2 + w2 - x2;
    float m12 = yz + yz + xw + xw;
    float m20 = yw + xz + xz + yw;
    float m21 = yz + yz - xw - xw;
    float m22 = z2 - y2 - x2 + w2;

    return glm::mat<4, 3, float>(
        m00, m01, m02,
        m10, m11, m12,
        m20, m21, m22,
        0, 0, 0
    );
}
