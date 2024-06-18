#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <util/PerlinNoise.hpp>

#include <iostream>
#include <opengl/Shader.h>
#include <core/Camera.hpp>
#include <world/Mesher.hpp>
#include <world/Noise.hpp>
#include <world/Chunk.hpp>

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

#define CHUNK_SIZE_SHIFT 4

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

    Shader shader("../res/shaders/vert.glsl", "../res/shaders/frag.glsl");

    const int WORLD_SIZE = 32;

    const int NUM_AXIS_CHUNKS = WORLD_SIZE / CHUNK_SIZE;
    const int NUM_CHUNKS = NUM_AXIS_CHUNKS * NUM_AXIS_CHUNKS;
    std::vector<Chunk> chunks;
    for (int cx = 0; cx < NUM_AXIS_CHUNKS; ++cx) {
        for (int cz = 0; cz < NUM_AXIS_CHUNKS; ++cz) {
            Chunk chunk = Chunk(cx, cz);
            chunk.init();
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

//            std::cout << std::endl;
        }
    }

    std::vector<int> world;
    std::vector<float> world_colours;
    std::vector<int> world_normals;
    std::vector<int> world_ao;
    std::vector<uint64_t> world_data;

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

        for (int i = 0; i < NUM_CHUNKS; ++i) {
            Chunk chunk = chunks[i];
            shader.setMat4("model", chunk.model);
            chunk.render();
        }

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
