#include "Input.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <functional>

double Input::mouseX = 0;
double Input::mouseY = 0;
double Input::mouseDeltaX = 0;
double Input::mouseDeltaY = 0;
double Input::scrollX = 0;
double Input::scrollY = 0;
double Input::lastMouseX = 0;
double Input::lastMouseY = 0;

std::unordered_map<BoundKey, Action, BoundKeyHash> Input::bindings{};
std::unordered_map<Action, ActionCallback> Input::actionCallbacks{};
std::vector<Action> Input::actionQueue{};

std::unordered_set<int> Input::keys{};

int Input::uiToggleKey = GLFW_KEY_H;
bool Input::uiMode = false;

void Input::addAction(const BoundKey key) {
    for (const auto& [boundKey, boundAction] : bindings) {
        if (boundKey == key) {
            actionQueue.push_back(boundAction);
        }
    }
}

void Input::key_callback(const int key, const int scancode, const int action, const int mods) {
    if (ImGui::GetIO().WantTextInput || (uiMode && key != uiToggleKey)) {
        return;
    }

    if (action == GLFW_PRESS) {
        keys.insert(key);
    } else if (action == GLFW_RELEASE) {
        keys.erase(key);
    }

    addAction({ key, action, false });
}

void Input::mouse_button_callback(const int button, const int action, const int mods) {
    if (uiMode) {
        return;
    }
    addAction({ button, action, true });
}

void Input::cursor_position_callback(const double xposIn, const double yposIn) {
    if (uiMode) {
        // Avoids camera jumping when leaving UI
        lastMouseX = xposIn;
        lastMouseY = yposIn;
        mouseX = xposIn;
        mouseY = yposIn;
        mouseDeltaX = 0;
        mouseDeltaY = 0;
        return;
    }
    mouseX = xposIn;
    mouseY = yposIn;
}

void Input::scroll_callback(const double xoffset, const double yoffset) {
    if (uiMode) {
        return;
    }
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

void Input::registerCallback(const Action action, const ActionCallback& callback) {
    actionCallbacks[action] = callback;
}

bool Input::isKeyDown(const int key) {
    return keys.contains(key);
}
