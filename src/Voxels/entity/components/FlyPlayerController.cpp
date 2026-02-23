#include "FlyPlayerController.hpp"

#include "../../io/Input.hpp"
#include "../Entity.hpp"
#include "Transform.hpp"

#include "GLFW/glfw3.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

FlyPlayerController::FlyPlayerController() {
    Input::bindings.insert({{GLFW_KEY_W, GLFW_PRESS}, {ActionType::MoveForward, ActionStateType::Start}});
    Input::bindings.insert({{GLFW_KEY_S, GLFW_PRESS}, {ActionType::MoveBackward, ActionStateType::Start}});
    Input::bindings.insert({{GLFW_KEY_A, GLFW_PRESS}, {ActionType::MoveLeft, ActionStateType::Start}});
    Input::bindings.insert({{GLFW_KEY_D, GLFW_PRESS}, {ActionType::MoveRight, ActionStateType::Start}});
    Input::bindings.insert({{GLFW_KEY_SPACE, GLFW_PRESS}, {ActionType::MoveUp, ActionStateType::Start}});
    Input::bindings.insert({{GLFW_KEY_LEFT_SHIFT, GLFW_PRESS}, {ActionType::MoveDown, ActionStateType::Start}});
    Input::bindings.insert({{GLFW_KEY_LEFT_CONTROL, GLFW_PRESS}, {ActionType::FastMovement, ActionStateType::Start}});

    Input::bindings.insert({{GLFW_KEY_W, GLFW_RELEASE}, {ActionType::MoveForward, ActionStateType::Stop}});
    Input::bindings.insert({{GLFW_KEY_S, GLFW_RELEASE}, {ActionType::MoveBackward, ActionStateType::Stop}});
    Input::bindings.insert({{GLFW_KEY_A, GLFW_RELEASE}, {ActionType::MoveLeft, ActionStateType::Stop}});
    Input::bindings.insert({{GLFW_KEY_D, GLFW_RELEASE}, {ActionType::MoveRight, ActionStateType::Stop}});
    Input::bindings.insert({{GLFW_KEY_SPACE, GLFW_RELEASE}, {ActionType::MoveUp, ActionStateType::Stop}});
    Input::bindings.insert({{GLFW_KEY_LEFT_SHIFT, GLFW_RELEASE}, {ActionType::MoveDown, ActionStateType::Stop}});
    Input::bindings.insert({{GLFW_KEY_LEFT_CONTROL, GLFW_RELEASE}, {ActionType::FastMovement, ActionStateType::Stop}});

    Input::registerCallback({ActionType::MoveForward, ActionStateType::Start}, [this] { movements.insert(Forward); });
    Input::registerCallback({ActionType::MoveBackward, ActionStateType::Start}, [this] { movements.insert(Backward); });
    Input::registerCallback({ActionType::MoveLeft, ActionStateType::Start}, [this] { movements.insert(Left); });
    Input::registerCallback({ActionType::MoveRight, ActionStateType::Start}, [this] { movements.insert(Right); });
    Input::registerCallback({ActionType::MoveUp, ActionStateType::Start}, [this] { movements.insert(Up); });
    Input::registerCallback({ActionType::MoveDown, ActionStateType::Start}, [this] { movements.insert(Down); });

    Input::registerCallback({ActionType::MoveForward, ActionStateType::Stop}, [this] { movements.erase(Forward); });
    Input::registerCallback({ActionType::MoveBackward, ActionStateType::Stop}, [this] { movements.erase(Backward); });
    Input::registerCallback({ActionType::MoveLeft, ActionStateType::Stop}, [this] { movements.erase(Left); });
    Input::registerCallback({ActionType::MoveRight, ActionStateType::Stop}, [this] { movements.erase(Right); });
    Input::registerCallback({ActionType::MoveUp, ActionStateType::Stop}, [this] { movements.erase(Up); });
    Input::registerCallback({ActionType::MoveDown, ActionStateType::Stop}, [this] { movements.erase(Down); });

    Input::registerCallback({ActionType::FastMovement, ActionStateType::Start}, [this] { movementSpeed = 100.0f; });
    Input::registerCallback({ActionType::FastMovement, ActionStateType::Stop}, [this] { movementSpeed = 10.0f; });

    Input::requeueCurrentActions();
}

void FlyPlayerController::update(float deltaTime) {
    Entity* player = getEntity();
    Transform* transform = player->get<Transform>();
    glm::vec3 front = getFront(transform->angles);
    glm::vec3 right = getRight(transform->angles);
    constexpr glm::vec3 WorldUp(0, 1, 0);

    // Keyboard
    float vel = movementSpeed * deltaTime;

    auto positionDelta = glm::vec3(0, 0, 0);

    for (auto movement : movements) {
        switch (movement) {
            case Forward:
                positionDelta += front;
                break;
            case Backward:
                positionDelta -= front;
                break;
            case Left:
                positionDelta -= right;
                break;
            case Right:
                positionDelta += right;
                break;
            case Up:
                positionDelta += WorldUp;
                break;
            case Down:
                positionDelta -= WorldUp;
                break;
        }
    }

    player->get<Kinematics>()->velocity = positionDelta * movementSpeed;

    transform->position += positionDelta * vel;

    // Mouse movement
    transform->angles.y += glm::radians(Input::mouseDeltaX * XSensitivity);
    transform->angles.x += glm::radians(Input::mouseDeltaY * YSensitivity);

    transform->angles.x = glm::clamp(transform->angles.x, glm::radians(-89.9f), glm::radians(89.9f));
}

FlyPlayerController::~FlyPlayerController() {
    Input::bindings.erase({GLFW_KEY_W, GLFW_PRESS});
    Input::bindings.erase({GLFW_KEY_S, GLFW_PRESS});
    Input::bindings.erase({GLFW_KEY_A, GLFW_PRESS});
    Input::bindings.erase({GLFW_KEY_D, GLFW_PRESS});
    Input::bindings.erase({GLFW_KEY_SPACE, GLFW_PRESS});
    Input::bindings.erase({GLFW_KEY_LEFT_SHIFT, GLFW_PRESS});

    Input::bindings.erase({GLFW_KEY_W, GLFW_RELEASE});
    Input::bindings.erase({GLFW_KEY_S, GLFW_RELEASE});
    Input::bindings.erase({GLFW_KEY_A, GLFW_RELEASE});
    Input::bindings.erase({GLFW_KEY_D, GLFW_RELEASE});
    Input::bindings.erase({GLFW_KEY_SPACE, GLFW_RELEASE});
    Input::bindings.erase({GLFW_KEY_LEFT_SHIFT, GLFW_RELEASE});

    Input::bindings.erase({GLFW_KEY_LEFT_CONTROL, GLFW_PRESS});
    Input::bindings.erase({GLFW_KEY_LEFT_CONTROL, GLFW_RELEASE});

    Input::eraseCallback({ActionType::MoveForward, ActionStateType::Start});
    Input::eraseCallback({ActionType::MoveBackward, ActionStateType::Start});
    Input::eraseCallback({ActionType::MoveLeft, ActionStateType::Start});
    Input::eraseCallback({ActionType::MoveRight, ActionStateType::Start});
    Input::eraseCallback({ActionType::MoveUp, ActionStateType::Start});
    Input::eraseCallback({ActionType::MoveDown, ActionStateType::Start});

    Input::eraseCallback({ActionType::MoveForward, ActionStateType::Stop});
    Input::eraseCallback({ActionType::MoveBackward, ActionStateType::Stop});
    Input::eraseCallback({ActionType::MoveLeft, ActionStateType::Stop});
    Input::eraseCallback({ActionType::MoveRight, ActionStateType::Stop});
    Input::eraseCallback({ActionType::MoveUp, ActionStateType::Stop});
    Input::eraseCallback({ActionType::MoveDown, ActionStateType::Stop});

    Input::eraseCallback({ActionType::FastMovement, ActionStateType::Start});
    Input::eraseCallback({ActionType::FastMovement, ActionStateType::Stop});
}
