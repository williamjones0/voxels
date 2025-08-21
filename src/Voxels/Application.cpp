#include "Application.hpp"
#include "tracy/Tracy.hpp"

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

    const auto screenWidth = videoMode->width;
    const auto screenHeight = videoMode->height;

    bool fullscreen = false;
    double scalar = fullscreen ? 1 : 0.8;

    windowWidth = static_cast<int>(screenWidth * scalar);
    windowHeight = static_cast<int>(screenHeight * scalar);

    windowHandle = glfwCreateWindow(windowWidth, windowHeight, "Voxels", fullscreen ? glfwGetPrimaryMonitor() : nullptr, nullptr);
    if (windowHandle == nullptr) {
        std::cout << "Failed to create GLFW windowHandle" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwSetWindowPos(windowHandle, (screenWidth - windowWidth) / 2, (screenHeight - windowHeight) / 2);

    glfwMakeContextCurrent(windowHandle);
    glfwSetFramebufferSizeCallback(windowHandle, [](GLFWwindow *window, int width, int height) {
        glViewport(0, 0, width, height);
    });

    glfwSetErrorCallback([](int error, const char *description) {
        fprintf(stderr, "GLFW error %d: %s\n", error, description);
    });

    glfwSetInputMode(windowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (glfwRawMouseMotionSupported()) {
        std::cout << "Raw mouse motion is supported" << std::endl;
        glfwSetInputMode(windowHandle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

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
    ZoneScoped;

    {
        ZoneScopedN("Process Input");
        processInput();
    }
    {
        ZoneScopedN("Update");
        update();
    }
    {
        ZoneScopedN("Render");
        render();
    }
    {
        ZoneScopedN("Poll Events");
        glfwPollEvents();
    }
    {
        ZoneScopedN("Swap Buffers");
        glfwSwapBuffers(windowHandle);
    }

    FrameMark;
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
