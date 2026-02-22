#include "FlyPlayerController.hpp"

#include "../../io/Input.hpp"
#include "../Entity.hpp"
#include "Transform.hpp"

#include "GLFW/glfw3.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>

void FlyPlayerController::load() {
    Input::bindings.insert({{GLFW_KEY_W, GLFW_PRESS}, {ActionType::MoveForward, ActionStateType::Start}});
    Input::bindings.insert({{GLFW_KEY_S, GLFW_PRESS}, {ActionType::MoveBackward, ActionStateType::Start}});
    Input::bindings.insert({{GLFW_KEY_A, GLFW_PRESS}, {ActionType::MoveLeft, ActionStateType::Start}});
    Input::bindings.insert({{GLFW_KEY_D, GLFW_PRESS}, {ActionType::MoveRight, ActionStateType::Start}});
    Input::bindings.insert({{GLFW_KEY_SPACE, GLFW_PRESS}, {ActionType::MoveUp, ActionStateType::Start}});
    Input::bindings.insert({{GLFW_KEY_LEFT_SHIFT, GLFW_PRESS}, {ActionType::MoveDown, ActionStateType::Start}});

    Input::bindings.insert({{GLFW_KEY_W, GLFW_RELEASE}, {ActionType::MoveForward, ActionStateType::Stop}});
    Input::bindings.insert({{GLFW_KEY_S, GLFW_RELEASE}, {ActionType::MoveBackward, ActionStateType::Stop}});
    Input::bindings.insert({{GLFW_KEY_A, GLFW_RELEASE}, {ActionType::MoveLeft, ActionStateType::Stop}});
    Input::bindings.insert({{GLFW_KEY_D, GLFW_RELEASE}, {ActionType::MoveRight, ActionStateType::Stop}});
    Input::bindings.insert({{GLFW_KEY_SPACE, GLFW_RELEASE}, {ActionType::MoveUp, ActionStateType::Stop}});
    Input::bindings.insert({{GLFW_KEY_LEFT_SHIFT, GLFW_RELEASE}, {ActionType::MoveDown, ActionStateType::Stop}});

    Input::bindings.insert({{GLFW_KEY_LEFT_CONTROL, GLFW_PRESS}, {ActionType::FastMovement, ActionStateType::Start}});
    Input::bindings.insert({{GLFW_MOUSE_BUTTON_4, GLFW_PRESS, true}, {ActionType::FastMovement, ActionStateType::Start}});

    Input::bindings.insert({{GLFW_KEY_LEFT_CONTROL, GLFW_RELEASE}, {ActionType::FastMovement, ActionStateType::Stop}});
    Input::bindings.insert({{GLFW_MOUSE_BUTTON_4, GLFW_RELEASE, true}, {ActionType::FastMovement, ActionStateType::Stop}});

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

    Input::registerCallback({ActionType::FastMovement, ActionStateType::Start}, [this] { movementSpeed = 1000.0f; });
    Input::registerCallback({ActionType::FastMovement, ActionStateType::Stop}, [this] { movementSpeed = 10.0f; });
}

void FlyPlayerController::update(float deltaTime) {
    Entity* player = getEntity();
    Transform* transform = player->get<Transform>();
    glm::vec3 front = getFront(transform->rotation);
    glm::vec3 right = getRight(transform->rotation);
    constexpr glm::vec3 WorldUp(0, 1, 0);

    // TODO: Calculate camera front for movement direction
    auto camFront = glm::vec3(0, 0, -1);
    camFront = glm::rotateX(camFront, -static_cast<float>(xRot));
    camFront = glm::rotateY(camFront, -static_cast<float>(yRot));

    glm::vec3 camRight = glm::normalize(glm::cross(camFront, WorldUp));

    // std::cout << "transform->front:\t" << glm::to_string(front) << "\n";
    // std::cout << "camera front:\t\t" << glm::to_string(camFront) << "\n\n";
    //
    // std::cout << "transform->right:\t" << glm::to_string(right) << "\n";
    // std::cout << "camera right:\t\t" << glm::to_string(camRight) << "\n\n\n\n";

    // Keyboard
    float vel = movementSpeed * deltaTime;

    auto positionDelta = glm::vec3(0, 0, 0);

    for (auto movement : movements) {
        switch (movement) {
            case Forward:
                positionDelta += camFront;
                break;
            case Backward:
                positionDelta -= camFront;
                break;
            case Left:
                positionDelta -= camRight;
                break;
            case Right:
                positionDelta += camRight;
                break;
            case Up:
                positionDelta += WorldUp;
                break;
            case Down:
                positionDelta -= WorldUp;
                break;
        }
    }

    transform->position += positionDelta * vel;

    // Mouse movement
    yRot += glm::radians(Input::mouseDeltaX * XSensitivity);
    xRot += glm::radians(Input::mouseDeltaY * YSensitivity);

    xRot = glm::clamp(xRot, glm::radians(-89.9), glm::radians(89.9));

    glm::quat yawQuat   = glm::angleAxis(static_cast<float>(yRot), glm::vec3(0, 1, 0));
    glm::quat pitchQuat = glm::angleAxis(static_cast<float>(xRot), glm::vec3(1, 0, 0));

    transform->rotation = pitchQuat * yawQuat;
}
