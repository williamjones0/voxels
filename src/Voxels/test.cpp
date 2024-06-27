//#include <glad/glad.h>
//#include <GLFW/glfw3.h>
//
//#include <glm/glm.hpp>
//#include <glm/gtc/matrix_transform.hpp>
//
//#include <iostream>
//#include "opengl/Shader.h"
//
//void framebuffer_size_callback(GLFWwindow* window, int width, int height);
//void glfw_error_callback(int error, const char* description);
//void processInput(GLFWwindow* window);
//
//const unsigned int SCREEN_WIDTH = 1920;
//const unsigned int SCREEN_HEIGHT = 1080;
//
//typedef struct {
//    unsigned int count;
//    unsigned int instanceCount;
//    unsigned int firstIndex;
//    unsigned int baseInstance;
//} DrawArraysIndirectCommand;
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
//    glfwSetErrorCallback(glfw_error_callback);
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
//    Shader shader("../../../../../data/shaders/test_vert.glsl",
//        "../../../../../data/shaders/test_frag.glsl");
//
//    float vertices[] = {
//        -0.5f, -0.5f, 0.0f, // left  
//         0.5f, -0.5f, 0.0f, // right 
//         0.0f,  0.5f, 0.0f  // top   
//    };
//
//    const int NUM_TRIANGLES = 10;
//
//    std::vector<float> positions;
//
//    for (int i = 0; i < NUM_TRIANGLES; ++i) {
//        for (int j = 0; j < 9; ++j) {
//            positions.push_back(vertices[j] / 4.0f);
//        }
//    }
//
//    unsigned int VAO;
//    unsigned int VBO;
//
//    glCreateBuffers(1, &VBO);
//    glNamedBufferStorage(VBO, sizeof(float) * positions.size(), positions.data(), GL_DYNAMIC_STORAGE_BIT);
//
//    glCreateVertexArrays(1, &VAO);
//
//    glVertexArrayVertexBuffer(VAO, 0, VBO, 0, sizeof(float) * 3);
//    
//    glEnableVertexArrayAttrib(VAO, 0);
//
//    glVertexArrayAttribFormat(VAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
//
//    glVertexArrayAttribBinding(VAO, 0, 0);
//
//    std::vector<DrawArraysIndirectCommand> commands;
//    unsigned int firstIndex = 0;
//    for (int i = 0; i < NUM_TRIANGLES; ++i) {
//        DrawArraysIndirectCommand triangleCmd = {
//            .count = 3,
//            .instanceCount = 1,
//            .firstIndex = firstIndex,
//            .baseInstance = 0
//        };
//        commands.push_back(triangleCmd);
//        firstIndex += 3;
//    }
//
//    GLuint drawCmdBuffer;
//    glCreateBuffers(1, &drawCmdBuffer);
//
//    glNamedBufferStorage(drawCmdBuffer,
//        sizeof(DrawArraysIndirectCommand) * commands.size(),
//        (const void *)commands.data(),
//        GL_DYNAMIC_STORAGE_BIT);
//
//    std::vector<float> offsets;
//    for (int i = 0; i < NUM_TRIANGLES; ++i) {
//        offsets.push_back(i);
//    }
//
//    GLuint offsetBuffer;
//    glCreateBuffers(1, &offsetBuffer);
//
//    glNamedBufferStorage(offsetBuffer,
//        sizeof(float) * offsets.size(),
//        (const void *)offsets.data(),
//        GL_DYNAMIC_STORAGE_BIT);
//    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, offsetBuffer);
//
//    while (!glfwWindowShouldClose(window)) {
//        processInput(window);
//
//        glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
//        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//
//        shader.use();
//
//        glBindVertexArray(VAO);
//        glBindBuffer(GL_ARRAY_BUFFER, VBO);
//        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawCmdBuffer);
//        glMultiDrawArraysIndirect(GL_TRIANGLES, 0, commands.size(), sizeof(DrawArraysIndirectCommand));
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
