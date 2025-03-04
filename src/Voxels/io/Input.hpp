#pragma once

#include <unordered_set>

enum class Controls {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN,
    JUMP,
    CROUCH
};

class Input {
public:
    static void key_callback(int key, int scancode, int action, int mods);
    static void mouse_button_callback(int button, int action, int mods);
    static void cursor_position_callback(double xposIn, double yposIn);
    static void scroll_callback(double xoffset, double yoffset);

    static std::unordered_set<int> keys;
    static std::unordered_set<int> lastFrameKeys;
    static std::unordered_set<int> buttons;
    static std::unordered_set<int> lastFrameButtons;

    static bool isKeyDown(int key);
    static bool isButtonDown(int button);
    static bool isKeyPress(int key);
    static bool isButtonPress(int button);
    static void update();

    static double mouseX;
    static double mouseY;
    static double mouseDeltaX;
    static double mouseDeltaY;
    static double scrollX;
    static double scrollY;

private:
    static double lastMouseX;
    static double lastMouseY;
};
