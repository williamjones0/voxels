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
std::unordered_map<Action, ActionCallback, ActionHash> Input::actionCallbacks{};
std::vector<Action> Input::actionQueue{};
std::unordered_set<ActionType> Input::currentActions{};
std::vector<Action> Input::actionsToBeCleared{};

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
    for (const auto& action : actionsToBeCleared) {
        actionQueue.push_back(action);
    }
    actionsToBeCleared.clear();

    for (const auto& action : actionQueue) {
        if (actionCallbacks.contains(action)) {
            // Update set of current actions
            if (action.stateType == ActionStateType::Start) {
                currentActions.insert(action.type);
            } else if (action.stateType == ActionStateType::Stop) {
                // If the action type is not in the set of current actions, then we should not call the callback.
                // This only happens if the user was in UI mode holding a key, and then exits UI mode,
                // then releases the key. The start action would not be sent, but the stop action would be sent.

                if (!currentActions.contains(action.type)) {
                    continue;
                }
                currentActions.erase(action.type);
            }

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

void Input::clearCurrentActions() {
    for (const auto actionType : currentActions) {
        actionsToBeCleared.push_back({actionType, ActionStateType::Stop});
    }
}

void Input::registerCallback(const Action action, const ActionCallback& callback) {
    actionCallbacks[action] = callback;
}

bool Input::isKeyDown(const int key) {
    return keys.contains(key);
}
