//#include <glad/glad.h>
//#include <GLFW/glfw3.h>
//
//#define GLM_ENABLE_EXPERIMENTAL
//
//#include <glm/glm.hpp>
//#include <glm/gtc/matrix_transform.hpp>
//#include <glm/gtx/string_cast.hpp>
//#include <glm/gtc/type_ptr.hpp>
//
//#include <iostream>
//#include <chrono>
//
//#include "world/Chunk.hpp"
//#include "world/WorldMesh.hpp"
//#include "opengl/Shader.h"
//
//typedef struct {
//    unsigned int count;
//    unsigned int instanceCount;
//    unsigned int firstIndex;
//    unsigned int baseInstance;
//} DrawArraysIndirectCommand;
//
//typedef struct {
//    unsigned int count;
//    unsigned int instanceCount;
//    unsigned int firstIndex;
//    unsigned int baseInstance;
//    unsigned int chunkIndex;
//} ChunkDrawCommand;
//
//typedef struct {
//    glm::mat4 model;
//    int cx;
//    int cz;
//    int minY;
//    int maxY;
//    unsigned int numVertices;
//    unsigned int firstIndex;
//    unsigned int _pad0;
//    unsigned int _pad1;
//} ChunkData;
//
//void framebuffer_size_callback(GLFWwindow* window, int width, int height);
//void glfw_error_callback(int error, const char* description);
//void processInput(GLFWwindow* window);
//
//const unsigned int SCREEN_WIDTH = 800;
//const unsigned int SCREEN_HEIGHT = 600;
//
//float lastX = SCREEN_WIDTH / 2.0f;
//float lastY = SCREEN_HEIGHT / 2.0f;
//bool firstMouse = true;
//
//float deltaTime = 0.0f;
//float lastFrame = 0.0f;
//
//void GLAPIENTRY MessageCallback(
//    GLenum source,
//    GLenum type,
//    GLuint id,
//    GLenum severity,
//    GLsizei length,
//    const GLchar* message,
//    const void* userParam
//) {
//    std::string SEVERITY = "";
//    switch (severity) {
//    case GL_DEBUG_SEVERITY_LOW:
//        SEVERITY = "LOW";
//        break;
//    case GL_DEBUG_SEVERITY_MEDIUM:
//        SEVERITY = "MEDIUM";
//        break;
//    case GL_DEBUG_SEVERITY_HIGH:
//        SEVERITY = "HIGH";
//        break;
//    case GL_DEBUG_SEVERITY_NOTIFICATION:
//        SEVERITY = "NOTIFICATION";
//        break;
//    }
//    fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = %s, message = %s\n",
//        type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "",
//        type, SEVERITY.c_str(), message);
//}
//
//int main() {
//    glfwInit();
//    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
//    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
//    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
//    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
//
//    GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Voxels", NULL, NULL);
//    if (window == NULL) {
//        std::cout << "Failed to create GLFW window" << std::endl;
//        glfwTerminate();
//        return -1;
//    }
//
//    glfwMakeContextCurrent(window);
//    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
//
//    glfwSetErrorCallback(glfw_error_callback);
//
//    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
//
//    glfwSwapInterval(1);
//
//    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
//        std::cout << "Failed to initialize GLAD" << std::endl;
//        return -1;
//    }
//
//    // Enable debug output
//    glEnable(GL_DEBUG_OUTPUT);
//    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
//    glDebugMessageCallback(MessageCallback, 0);
//
//    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
//
//    glEnable(GL_DEPTH_TEST);
//
//    Shader drawCommandShader("../../../../../data/shaders/test_comp.glsl");
//    Shader clearShader("../../../../../data/shaders/clear_comp.glsl");
//
//    WorldMesh worldMesh;
//
//    const int NUM_AXIS_CHUNKS = WORLD_SIZE / CHUNK_SIZE;
//    const int NUM_CHUNKS = NUM_AXIS_CHUNKS * NUM_AXIS_CHUNKS;
//    unsigned int firstIndex = 0;
//    std::vector<Chunk> chunks;
//    for (int cx = 0; cx < NUM_AXIS_CHUNKS; ++cx) {
//        for (int cz = 0; cz < NUM_AXIS_CHUNKS; ++cz) {
//            Chunk chunk = Chunk(cx, cz, firstIndex);
//
//            auto startTime = std::chrono::high_resolution_clock::now();
//            chunk.init(worldMesh.data);
//            auto endTime = std::chrono::high_resolution_clock::now();
//            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
//            std::cout << "Chunk " << cz + cx * NUM_AXIS_CHUNKS << " took " << duration.count() << "us to init" << std::endl << std::endl;
//            chunks.push_back(chunk);
//            firstIndex += chunk.numVertices;
//        }
//    }
//
//    worldMesh.createBuffers();
//
//    std::vector<ChunkData> chunkData;
//
//    for (size_t i = 0; i < NUM_CHUNKS; ++i) {
//        ChunkData cd = {
//            .model = chunks[i].model,
//            .cx = chunks[i].cx,
//            .cz = chunks[i].cz,
//            .minY = chunks[i].minY,
//            .maxY = chunks[i].maxY,
//            .numVertices = chunks[i].numVertices,
//            .firstIndex = chunks[i].firstIndex,
//            ._pad0 = 0,
//            ._pad1 = 0
//        };
//        chunkData.push_back(cd);
//    }
//
//    for (size_t i = 0; i < NUM_CHUNKS; ++i) {
//        std::cout << "Chunk " << i << " cx: " << chunkData[i].cx << " cz: " << chunkData[i].cz << " minY: " << chunkData[i].minY << " maxY: " << chunkData[i].maxY << " numVertices: " << chunkData[i].numVertices << " firstIndex: " << chunkData[i].firstIndex << std::endl;
//    }
//
//    // Chunk draw command shader buffers
//    GLuint chunkDrawCmdBuffer;
//    glCreateBuffers(1, &chunkDrawCmdBuffer);
//
//    glNamedBufferStorage(chunkDrawCmdBuffer,
//        sizeof(ChunkDrawCommand) * NUM_CHUNKS,
//        NULL,
//        GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
//    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, chunkDrawCmdBuffer);
//
//    GLuint chunkDataBuffer;
//    glCreateBuffers(1, &chunkDataBuffer);
//
//    glNamedBufferStorage(chunkDataBuffer,
//        sizeof(ChunkData) * chunkData.size(),
//        (const void*)chunkData.data(),
//        GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
//    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, chunkDataBuffer);
//
//    GLuint commandCountBuffer;
//    glCreateBuffers(1, &commandCountBuffer);
//
//    glNamedBufferStorage(commandCountBuffer,
//        sizeof(unsigned int),
//        NULL,
//        NULL);
//    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, commandCountBuffer);
//    glBindBuffer(GL_PARAMETER_BUFFER, commandCountBuffer);
//
//    void *commandCountBufferAddress = glMapNamedBufferRange(commandCountBuffer,
//        0,
//        sizeof(unsigned int),
//        GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
//
//    while (!glfwWindowShouldClose(window)) {
//        float currentFrame = static_cast<float>(glfwGetTime());
//        deltaTime = currentFrame - lastFrame;
//        lastFrame = currentFrame;
//
//        processInput(window);
//
//        // Clear command count buffer
//        clearShader.use();
//
//        glDispatchCompute(1, 1, 1);
//        glMemoryBarrier(GL_ALL_BARRIER_BITS);
//
//        // Generate draw commands
//        drawCommandShader.use();
//
//        glDispatchCompute(NUM_CHUNKS, 1, 1);
//        glMemoryBarrier(GL_ALL_BARRIER_BITS);
//
//        glfwPollEvents();
//        glfwSwapBuffers(window);
//    }
//
//    glfwTerminate();
//    return 0;
//}
//
//void processInput(GLFWwindow* window) {
//    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
//        glfwSetWindowShouldClose(window, true);
//    }
//}
//
//void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
//    glViewport(0, 0, width, height);
//}
//
//void glfw_error_callback(int error, const char* description) {
//    fprintf(stderr, "GLFW error %d: %s\n", error, description);
//}
