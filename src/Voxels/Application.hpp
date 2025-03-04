#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <unordered_set>

#include "core/ThreadPool.hpp"

class Application {
public:
    void run();

protected:
    virtual bool init();
    virtual bool load();
    void loop();
    virtual void update();
    virtual void render();
    virtual void processInput();
    virtual void cleanup();

    GLFWwindow *windowHandle;

    const int windowWidth = 2560 * 0.8;
    const int windowHeight = 1600 * 0.8;

    float lastX;
    float lastY;
    bool firstMouse = true;

    float deltaTime;
    float lastFrame = 0;
};
