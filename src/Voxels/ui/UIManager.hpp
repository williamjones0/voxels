#pragma once

#include <functional>
#include <string>
#include <vector>
#include <GLFW/glfw3.h>

class UIManager {
public:
    using WindowFn = std::function<void()>;

    void load(GLFWwindow* window);
    void beginFrame();
    void drawWindows();
    void render();
    void cleanup();

    void registerWindow(const std::string& title, const WindowFn &fn, bool visible = true);

private:
    struct Window { std::string title; WindowFn fn; bool visible; };
    std::vector<Window> windows;
};
