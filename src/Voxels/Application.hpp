#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <unordered_set>

class Application {
public:
    void run();

    virtual void key_callback(int key, int scancode, int action, int mods);
    virtual void mouse_button_callback(int button, int action, int mods);
    virtual void cursor_position_callback(double xposIn, double yposIn);
    virtual void scroll_callback(double xoffset, double yoffset);

protected:
    virtual bool init();
    virtual bool load();
    void loop();
    virtual void update();
    virtual void render();
    virtual void processInput();
    virtual void cleanup();

    GLFWwindow *windowHandle;

    int windowWidth;
    int windowHeight;

    float lastX;
    float lastY;
    bool firstMouse = true;

    std::unordered_set<int> keys;
    std::unordered_set<int> lastFrameKeys;
    std::unordered_set<int> buttons;
    std::unordered_set<int> lastFrameButtons;

    float deltaTime;
    float lastFrame;
};
