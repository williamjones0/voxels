#include "Input.hpp"

#include <GLFW/glfw3.h>

#include <functional>

double Input::mouseX = 0;
double Input::mouseY = 0;
double Input::mouseDeltaX = 0;
double Input::mouseDeltaY = 0;
double Input::scrollX = 0;
double Input::scrollY = 0;
double Input::lastMouseX = 0;
double Input::lastMouseY = 0;

std::unordered_map<BoundKey, Action, BoundKeyHash> Input::bindings = {};
std::unordered_map<Action, ActionCallback> Input::actionCallbacks = {};
std::vector<Action> Input::actionQueue = {};

std::unordered_set<int> Input::keys = {};

void Input::addAction(BoundKey key) {
    for (const auto& [boundKey, boundAction] : bindings) {
        if (boundKey == key) {
            actionQueue.push_back(boundAction);
        }
    }
}

void Input::key_callback(int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        keys.insert(key);
    } else if (action == GLFW_RELEASE) {
        keys.erase(key);
    }

    addAction({ key, action, false });
}

void Input::mouse_button_callback(int button, int action, int mods) {
    addAction({ button, action, true });
}

void Input::cursor_position_callback(double xposIn, double yposIn) {
    mouseX = xposIn;
    mouseY = yposIn;
}

void Input::scroll_callback(double xoffset, double yoffset) {
    scrollX += xoffset;
    scrollY += yoffset;
}

void Input::update() {
    for (const auto& action : actionQueue) {
        if (actionCallbacks.contains(action)) {
            actionCallbacks[action]();
        }
    }
    actionQueue.clear();

    scrollX = 0;
    scrollY = 0;

    mouseDeltaX = mouseX - lastMouseX;
    mouseDeltaY = mouseY - lastMouseY;

    lastMouseX = mouseX;
    lastMouseY = mouseY;
}

void Input::registerCallback(Action action, const ActionCallback& callback) {
    actionCallbacks[action] = callback;
}

bool Input::isKeyDown(int key) {
    return keys.contains(key);
}
