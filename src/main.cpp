#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <util/PerlinNoise.hpp>

#include <iostream>
#include <opengl/Shader.h>
#include <core/Camera.hpp>

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

    float vertices[] = {
        // Front
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,

        // Back
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,

        // Left
        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,

        // Right
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,

         // Bottom
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,

        // Top
        -0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f
    };

    float colours[] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f,

        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f,

        1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,

        1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,

        0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,

        0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f
    };

    const int WORLD_SIZE = 64;
    const double HEIGHT_SCALE = 128;

    int *chunk = new int[WORLD_SIZE * WORLD_SIZE];

    const siv::PerlinNoise::seed_type seed = 123456u;
    const siv::PerlinNoise perlin{ seed };

    for (int y = 0; y < WORLD_SIZE; ++y) {
        for (int x = 0; x < WORLD_SIZE; ++x) {
            const double noise = perlin.octave2D_01((x * 0.01), (y * 0.01), 4);
            chunk[y * WORLD_SIZE + x] = noise * HEIGHT_SCALE;
        }
    }

    float *chunk_vertices = new float[VERTICES_LENGTH * WORLD_SIZE * WORLD_SIZE];

    for (int i = 0; i < WORLD_SIZE; ++i) {
        for (int j = 0; j < WORLD_SIZE; ++j) {
            float *translated_vertices = new float[VERTICES_LENGTH];
            std::memcpy(translated_vertices, vertices, VERTICES_LENGTH * sizeof(float));
            for (int k = 0; k < 36; ++k) {
                translated_vertices[3 * k] += j;
                translated_vertices[3 * k + 1] += chunk[WORLD_SIZE * i + j];
                translated_vertices[3 * k + 2] += i;
            }

            std::memcpy(chunk_vertices + VERTICES_LENGTH * (WORLD_SIZE * i + j), translated_vertices, VERTICES_LENGTH * sizeof(float));
        }
    }

    std::vector<float> stuff;

    for (int i = 0; i < WORLD_SIZE; ++i) {
        for (int j = 0; j < WORLD_SIZE; ++j) {
            float* translated_vertices = new float[VERTICES_LENGTH];
            std::memcpy(translated_vertices, vertices, VERTICES_LENGTH * sizeof(float));
            for (int k = 0; k < 36; ++k) {
                translated_vertices[3 * k] += j;
                translated_vertices[3 * k + 1] += chunk[WORLD_SIZE * i + j];
                translated_vertices[3 * k + 2] += i;
            }

            int height = chunk[WORLD_SIZE * i + j];

            int x_plus1 = WORLD_SIZE * i + j + 1;
            int x_minus1 = WORLD_SIZE * i + j - 1;
            int z_plus1 = WORLD_SIZE * (i + 1) + j;
            int z_minus1 = WORLD_SIZE * (i - 1) + j;

            int x_plus1h = -1;
            int x_minus1h = -1;
            int z_plus1h = -1;
            int z_minus1h = -1;

            if (0 <= x_plus1 && x_plus1 < WORLD_SIZE * WORLD_SIZE) {
                x_plus1h = chunk[x_plus1];
            }
            if (0 <= x_minus1 && x_minus1 < WORLD_SIZE * WORLD_SIZE) {
                x_minus1h = chunk[x_minus1];
            }
            if (0 <= z_plus1 && z_plus1 < WORLD_SIZE * WORLD_SIZE) {
                z_plus1h = chunk[z_plus1];
            }
            if (0 <= z_minus1 && z_minus1 < WORLD_SIZE * WORLD_SIZE) {
                z_minus1h = chunk[z_minus1];
            }

            // Copy top face
            stuff.insert(stuff.end(), &translated_vertices[TOP_FACE], &translated_vertices[TOP_FACE + 18]);

            if (height != x_plus1h) {
                stuff.insert(stuff.end(), &translated_vertices[RIGHT_FACE], &translated_vertices[RIGHT_FACE + 18]);
            }

            if (height != x_minus1h) {
                stuff.insert(stuff.end(), &translated_vertices[LEFT_FACE], &translated_vertices[LEFT_FACE + 18]);
            }
            if (height != z_plus1h) {
                stuff.insert(stuff.end(), &translated_vertices[BACK_FACE], &translated_vertices[BACK_FACE + 18]);
            }
            if (height != z_minus1h) {
                stuff.insert(stuff.end(), &translated_vertices[FRONT_FACE], &translated_vertices[FRONT_FACE + 18]);
            }
        }
    }

    std::cout << "stuff size:\t" << stuff.size() << std::endl;
    std::cout << "num vertices:\t" << WORLD_SIZE * WORLD_SIZE * VERTICES_LENGTH << std::endl;

    float* world_colours = new float[108 * WORLD_SIZE * WORLD_SIZE];
    for (int i = 0; i < WORLD_SIZE * WORLD_SIZE; ++i) {
        std::memcpy(world_colours + i * 108, colours, 108 * sizeof(float));
    }

    float* world_vertices = &stuff[0];

    unsigned int VBO;
    glGenBuffers(1, &VBO);

    unsigned int VAO;
    glGenVertexArrays(1, &VAO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // glBufferData(GL_ARRAY_BUFFER, VERTICES_LENGTH * WORLD_SIZE * WORLD_SIZE * sizeof(float), chunk_vertices, GL_STATIC_DRAW);
    // glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBufferData(GL_ARRAY_BUFFER, stuff.size() * sizeof(float), world_vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    //unsigned int EBO;
    //glGenBuffers(1, &EBO);

    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    //glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    unsigned int coloursVBO;
    glGenBuffers(1, &coloursVBO);

    glBindBuffer(GL_ARRAY_BUFFER, coloursVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colours) * WORLD_SIZE * WORLD_SIZE, world_colours, GL_STATIC_DRAW);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);

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
        glDrawArrays(GL_TRIANGLES, 0, stuff.size());

        std::cout << "Frame time: " << deltaTime << "\t FPS: " << (1.0f / deltaTime) << std::endl;

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
