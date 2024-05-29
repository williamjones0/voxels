#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <util/PerlinNoise.hpp>

#include <iostream>
#include <opengl/Shader.h>
#include <core/Camera.hpp>
#include <world/Mesher.hpp>
#include <world/Noise.hpp>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void glfw_error_callback(int error, const char* description);
void processInput(GLFWwindow* window);

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
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

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
    glDebugMessageCallback(MessageCallback, 0);

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    glEnable(GL_DEPTH_TEST);
    
    Shader shader("../../../res/shaders/vert.glsl", "../../../res/shaders/frag.glsl");

    //float vertices[] = {
    //    // positions         // colors
    //     0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,   // bottom right
    //    -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,   // bottom left
    //     0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f    // top 
    //};
    //unsigned int indices[] = {
    //    0, 1, 2
    //};

    const int WORLD_SIZE = 32;
    const int HEIGHT_SCALE = 32;

    std::vector<int> voxels(WORLD_SIZE * WORLD_SIZE * HEIGHT_SCALE);
    //std::vector<int> voxels = {
    //    1, 1, 1,
    //    1, 1, 1,
    //    1, 1, 1,

    //    0, 0, 0,
    //    0, 1, 0,
    //    0, 0, 0,

    //    0, 0, 0,
    //    0, 0, 0,
    //    0, 0, 0
    //};
    std::fill(voxels.begin(), voxels.end(), 0);
    generateTerrain(voxels, 123456u, WORLD_SIZE, HEIGHT_SCALE);

    std::vector<int> world;
    std::vector<float> world_colours;
    std::vector<int> world_normals;
    std::vector<int> world_ao;
    std::vector<uint64_t> world_data;

    // mesh(world, world_colours, world_normals, world_ao);

    meshVoxels(voxels, world, world_colours, world_normals, world_ao, world_data, WORLD_SIZE, HEIGHT_SCALE);

    std::cout << "world size: " << world.size() << std::endl;
    std::cout << "colours size: " << world_colours.size() << std::endl;
    std::cout << "normals size: " << world_normals.size() << std::endl;
    std::cout << "ao size: " << world_ao.size() << std::endl;

    for (int i = 0; i < world.size() / 3; ++i) {
        uint64_t colour;
        if (world_colours[3 * i] == 0.278f) {
            colour = 0;
        }
        else {
            colour = 1;
        }

        uint64_t data =
            (uint64_t)world[3 * i] |
            ((uint64_t)world[3 * i + 1] << 11) |
            ((uint64_t)world[3 * i + 2] << 22) |
            (colour << 33) |
            ((uint64_t)world_normals[i] << 34) |
            ((uint64_t)world_ao[i] << 37);

        world_data.push_back(data);
    }

    std::cout << "data size: " << world_data.size() << std::endl;

    unsigned int VBO;
    glGenBuffers(1, &VBO);

    unsigned int VAO;
    glGenVertexArrays(1, &VAO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // glBufferData(GL_ARRAY_BUFFER, VERTICES_LENGTH * WORLD_SIZE * WORLD_SIZE * sizeof(float), chunk_vertices, GL_STATIC_DRAW);
    // glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBufferData(GL_ARRAY_BUFFER, world.size() * sizeof(int), &world[0], GL_STATIC_DRAW);

    glVertexAttribPointer(1, 3, GL_INT, GL_FALSE, 3 * sizeof(int), (void*)0);
    glEnableVertexAttribArray(1);

    unsigned int aoVBO;
    glGenBuffers(1, &aoVBO);
    glBindBuffer(GL_ARRAY_BUFFER, aoVBO);
    glBufferData(GL_ARRAY_BUFFER, world_ao.size() * sizeof(int), &world_ao[0], GL_STATIC_DRAW);
    glVertexAttribPointer(2, 1, GL_INT, GL_FALSE, sizeof(int), (void*)0);
    glEnableVertexAttribArray(2);

    //unsigned int EBO;
    //glGenBuffers(1, &EBO);

    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    //glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    //unsigned int coloursVBO;
    //glGenBuffers(1, &coloursVBO);

    //glBindBuffer(GL_ARRAY_BUFFER, coloursVBO);
    //glBufferData(GL_ARRAY_BUFFER, world_colours.size() * sizeof(float), &world_colours[0], GL_STATIC_DRAW);

    //glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    //glEnableVertexAttribArray(1);

    //unsigned int normalsVBO;
    //glGenBuffers(1, &normalsVBO);

    //glBindBuffer(GL_ARRAY_BUFFER, normalsVBO);
    //glBufferData(GL_ARRAY_BUFFER, world_normals.size() * sizeof(int), &world_normals[0], GL_STATIC_DRAW);

    //glVertexAttribIPointer(2, 1, GL_INT, sizeof(int), (void*)0);
    //glEnableVertexAttribArray(2);

    unsigned int dataVBO;
    glGenBuffers(1, &dataVBO);

    glBindBuffer(GL_ARRAY_BUFFER, dataVBO);
    glBufferData(GL_ARRAY_BUFFER, world_data.size() * sizeof(uint64_t), &world_data[0], GL_STATIC_DRAW);

    glVertexAttribLPointer(0, 1, GL_DOUBLE, sizeof(uint64_t), (void*)0);
    glEnableVertexAttribArray(0);

    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        camera.update(deltaTime);
        processInput(window);

        glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 model = glm::mat4(1.0f);
        // model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.5f, 1.0f, 0.0f));
        glm::mat4 projection = glm::perspective((float)(camera.FOV * PI / 180), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 5000.0f);

        shader.use();

        shader.setMat4("model", model);
        shader.setMat4("view", camera.calculateViewMatrix());
        shader.setMat4("projection", projection);

        glBindVertexArray(VAO);
        // glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
        // glDrawArrays(GL_TRIANGLES, 0, (VERTICES_LENGTH * WORLD_SIZE * WORLD_SIZE));
        // glDrawArrays(GL_TRIANGLES, 0, 36);
        glDrawArrays(GL_TRIANGLES, 0, world.size());

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
