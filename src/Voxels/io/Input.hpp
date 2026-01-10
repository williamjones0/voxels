#pragma once

#include <functional>
#include <unordered_set>

enum class ActionType {
    // States
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,

    Jump,

    Crouch,

    FastMovement,

    // Actions
    Break,
    Place,

    Exit,
    ToggleWireframe,
    SaveLevel,
    LoadLevel,

    ToggleUIMode,

    SelectPaletteIndex,
};

enum class ActionStateType {
    Start,
    Stop,
    None  // For actions without state (e.g., Break, Place)
};

struct Action {
    ActionType type;
    ActionStateType stateType;
    int payload = 0;  // Optional; used by actions like SelectPaletteIndex

    bool operator==(const Action& other) const {
        return type == other.type && stateType == other.stateType && payload == other.payload;
    }
};

struct ActionHash {
    std::size_t operator()(const Action& action) const {
        return std::hash<int>()(static_cast<int>(action.type)) ^ std::hash<int>()(static_cast<int>(action.stateType)) ^ std::hash<int>()(action.payload);
    }
};

struct BoundKey {
    int keycode;
    int action; // {GLFW_PRESS, GLFW_RELEASE} TODO: typedef?
    bool isMouse = false;

    bool operator==(const BoundKey& other) const {
        return keycode == other.keycode &&
               action == other.action &&
               isMouse == other.isMouse;
    }
};

struct BoundKeyHash {
    std::size_t operator()(const BoundKey& key) const {
        return std::hash<int>()(key.keycode) ^ std::hash<int>()(key.action) ^ std::hash<bool>()(key.isMouse);
    }
};

using ActionCallback = std::function<void()>;

class Input {
public:
    static void key_callback(int key, int scancode, int action, int mods);
    static void mouse_button_callback(int button, int action, int mods);
    static void cursor_position_callback(double xposIn, double yposIn);
    static void scroll_callback(double xoffset, double yoffset);

    static std::unordered_set<int> keys;
    static bool isKeyDown(int key);

    static void update();

    static double mouseX;
    static double mouseY;
    static double mouseDeltaX;
    static double mouseDeltaY;
    static double scrollX;
    static double scrollY;

    static std::unordered_map<BoundKey, Action, BoundKeyHash> bindings;
    static std::unordered_map<Action, ActionCallback, ActionHash> actionCallbacks;

    static std::vector<Action> actionQueue;  // Actions pending this frame

    // For handling entering/exiting UI mode
    static std::unordered_set<ActionType> currentActions;  // Actions that have been started but not yet stopped
    static std::vector<Action> actionsToBeCleared;
    static void clearCurrentActions();

    static void registerCallback(Action action, ActionCallback callback);

    static int uiToggleKey;
    static bool uiMode;

private:
    static double lastMouseX;
    static double lastMouseY;

    static void addAction(BoundKey key);
};
