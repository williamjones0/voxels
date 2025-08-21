#pragma once

#include <functional>
#include <unordered_set>

enum class Action {
    StartMoveForward,
    StartMoveBackward,
    StartMoveLeft,
    StartMoveRight,
    StartMoveUp,
    StartMoveDown,

    StopMoveForward,
    StopMoveBackward,
    StopMoveLeft,
    StopMoveRight,
    StopMoveUp,
    StopMoveDown,

    StartJump,
    StopJump,

    StartCrouch,
    StopCrouch,

    EnableFastMovement,
    DisableFastMovement,

    Break,
    Place,

    // Application actions
    Exit,
    ToggleWireframe,
    SaveLevel,
};

struct BoundKey {
    int keycode;
    int action; // {GLFW_PRESS, GLFW_RELEASE} TODO: typedef?
    bool isMouse = false;

    bool operator==(const BoundKey& other) const {
        return (keycode == other.keycode &&
                action == other.action &&
                isMouse == other.isMouse);
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

public: // Jeff
    static std::unordered_map<BoundKey, Action, BoundKeyHash> bindings;
    static std::unordered_map<Action, ActionCallback> actionCallbacks;

    static std::vector<Action> actionQueue;

    static void registerCallback(Action action, const ActionCallback& callback);

private:
    static double lastMouseX;
    static double lastMouseY;

private:
    static void addAction(BoundKey key);
};
