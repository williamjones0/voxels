#include "Input.hpp"

#include <GLFW/glfw3.h>

std::unordered_set<int> Input::keys;
std::unordered_set<int> Input::lastFrameKeys;
std::unordered_set<int> Input::buttons;
std::unordered_set<int> Input::lastFrameButtons;

double Input::mouseX = 0;
double Input::mouseY = 0;
double Input::mouseDeltaX = 0;
double Input::mouseDeltaY = 0;
double Input::scrollX = 0;
double Input::scrollY = 0;
double Input::lastMouseX = 0;
double Input::lastMouseY = 0;

void Input::key_callback(int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        keys.insert(key);
    } else if (action == GLFW_RELEASE) {
        keys.erase(key);
    }
}

void Input::mouse_button_callback(int button, int action, int mods) {
    if (action == GLFW_PRESS) {
        buttons.insert(button);
    } else if (action == GLFW_RELEASE) {
        buttons.erase(button);
    }
}

void Input::cursor_position_callback(double xposIn, double yposIn) {
    mouseX = xposIn;
    mouseY = yposIn;
}

void Input::scroll_callback(double xoffset, double yoffset) {
    scrollX += xoffset;
    scrollY += yoffset;
}

bool Input::isKeyDown(int key) {
    return keys.contains(key);
}

bool Input::isButtonDown(int button) {
    return buttons.contains(button);
}

bool Input::isKeyPress(int key) {
    return keys.contains(key) && !lastFrameKeys.contains(key);
}

bool Input::isButtonPress(int button) {
    return buttons.contains(button) && !lastFrameButtons.contains(button);
}

void Input::update() {
    lastFrameKeys = keys;
    lastFrameButtons = buttons;
    scrollX = 0;
    scrollY = 0;

    mouseDeltaX = mouseX - lastMouseX;
    mouseDeltaY = mouseY - lastMouseY;

    lastMouseX = mouseX;
    lastMouseY = mouseY;
}
