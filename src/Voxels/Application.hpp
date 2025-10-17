#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

class Application {
public:
    virtual ~Application() = default;

    void run();

protected:
    virtual bool init();
    virtual bool load();
    void loop();
    virtual void update();
    virtual void render();
    virtual void processInput();
    virtual void cleanup();

    GLFWwindow* windowHandle = nullptr;

    int windowWidth = 2560 * 0.8;
    int windowHeight = 1600 * 0.8;

    float lastX = 0.0f;
    float lastY = 0.0f;
    bool firstMouse = true;

    float deltaTime = 0.0f;
    float lastFrame = 0;
};
