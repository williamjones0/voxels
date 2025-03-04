#include "Application.hpp"

#include <iostream>

void Application::run() {
    if (!init()) {
        std::cerr << "Failed to initialize application" << std::endl;
        return;
    }

    if (!load()) {
        std::cerr << "Failed to load application" << std::endl;
        return;
    }

    while (!glfwWindowShouldClose(windowHandle)) {
        loop();
    }

    cleanup();
}

bool Application::init() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    const auto primaryMonitor = glfwGetPrimaryMonitor();
    const auto videoMode = glfwGetVideoMode(primaryMonitor);

    windowHandle = glfwCreateWindow(windowWidth, windowHeight, "Voxels", nullptr, nullptr);
    if (windowHandle == nullptr) {
        std::cout << "Failed to create GLFW windowHandle" << std::endl;
        glfwTerminate();
        return false;
    }

    const auto screenWidth = videoMode->width;
    const auto screenHeight = videoMode->height;
    glfwSetWindowPos(windowHandle, (screenWidth - windowWidth) / 2, (screenHeight - windowHeight) / 2);

    glfwMakeContextCurrent(windowHandle);
    glfwSetFramebufferSizeCallback(windowHandle, [](GLFWwindow *window, int width, int height) {
        glViewport(0, 0, width, height);
    });

    glfwSetErrorCallback([](int error, const char *description) {
        fprintf(stderr, "GLFW error %d: %s\n", error, description);
    });

    glfwSetInputMode(windowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    return true;
}

bool Application::load() {
    // Enable debug output
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback([](GLenum source,
                                      GLenum type,
                                      GLuint id,
                                      GLenum severity,
                                      GLsizei length,
                                      const GLchar *message,
                                      const void *userParam
    ) {
        std::string SEVERITY;
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
    }, nullptr);

    glClearColor(0.2f, 0.2f, 0.3f, 1.0f);

    glEnable(GL_DEPTH_TEST);

    return true;
}

void Application::loop() {
    update();
    processInput();
    render();
    glfwPollEvents();
    glfwSwapBuffers(windowHandle);
}

void Application::update() {
    auto currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
}

void Application::render() {}

void Application::processInput() {}

void Application::cleanup() {
    glfwTerminate();
}
