#include "FlyPlayerController.hpp"

#include "../io/Input.hpp"
#include "GLFW/glfw3.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>

void FlyPlayerController::load() {
//    Input::bindings.insert({{GLFW_KEY_W, GLFW_PRESS}, Action::StartMoveForward});
//    Input::bindings.insert({{GLFW_KEY_S, GLFW_PRESS}, Action::StartMoveBackward});
//    Input::bindings.insert({{GLFW_KEY_A, GLFW_PRESS}, Action::StartMoveLeft});
//    Input::bindings.insert({{GLFW_KEY_D, GLFW_PRESS}, Action::StartMoveRight});
//
//    Input::bindings.insert({{GLFW_KEY_W, GLFW_RELEASE}, Action::StopMoveForward});
//    Input::bindings.insert({{GLFW_KEY_S, GLFW_RELEASE}, Action::StopMoveBackward});
//    Input::bindings.insert({{GLFW_KEY_A, GLFW_RELEASE}, Action::StopMoveLeft});
//    Input::bindings.insert({{GLFW_KEY_D, GLFW_RELEASE}, Action::StopMoveRight});
//
//    Input::bindings.insert({{GLFW_KEY_SPACE, GLFW_PRESS}, Action::StartJump});
//    Input::bindings.insert({{GLFW_KEY_SPACE, GLFW_RELEASE}, Action::StopJump});
//
//    Input::bindings.insert({{GLFW_KEY_LEFT_SHIFT, GLFW_PRESS}, Action::StartCrouch});
//    Input::bindings.insert({{GLFW_KEY_LEFT_SHIFT, GLFW_RELEASE}, Action::StopCrouch});
//
//    Input::bindings.insert({{GLFW_KEY_LEFT_CONTROL, GLFW_PRESS}, Action::EnableFastMovement});
//    Input::bindings.insert({{GLFW_MOUSE_BUTTON_4, GLFW_PRESS}, Action::EnableFastMovement});
//    Input::bindings.insert({{GLFW_KEY_LEFT_CONTROL, GLFW_RELEASE}, Action::DisableFastMovement});
//    Input::bindings.insert({{GLFW_MOUSE_BUTTON_4, GLFW_RELEASE}, Action::DisableFastMovement});
//
//    Input::registerCallback(Action::StartMoveForward, [this] { direction += camera.front; });
//    Input::registerCallback(Action::StartMoveBackward, [this] { direction -= camera.front; });
//    Input::registerCallback(Action::StartMoveLeft, [this] { direction -= camera.right; });
//    Input::registerCallback(Action::StartMoveRight, [this] { direction += camera.right; });
//
//    Input::registerCallback(Action::StopMoveForward, [this] { direction -= camera.front; });
//    Input::registerCallback(Action::StopMoveBackward, [this] { direction += camera.front; });
//    Input::registerCallback(Action::StopMoveLeft, [this] { direction += camera.right; });
//    Input::registerCallback(Action::StopMoveRight, [this] { direction -= camera.right; });
//
//    Input::registerCallback(Action::StartJump, [this] { direction += camera.worldUp; });
//    Input::registerCallback(Action::StopJump, [this] { direction -= camera.worldUp; });
//
//    Input::registerCallback(Action::StartCrouch, [this] { direction -= camera.worldUp; });
//    Input::registerCallback(Action::StopCrouch, [this] { direction += camera.worldUp; });
//
//    Input::registerCallback(Action::EnableFastMovement, [this] { movementSpeed = 1000.0f; });
//    Input::registerCallback(Action::DisableFastMovement, [this] { movementSpeed = 10.0f; });
}

void FlyPlayerController::update(float deltaTime) {
    float vel = movementSpeed * deltaTime;

    glm::vec3 positionDelta = glm::vec3(0, 0, 0);

//    camera.transform.position += direction * vel;
//    direction = {};

    if (Input::isKeyDown(GLFW_KEY_LEFT_CONTROL)) {
        movementSpeed = 1000.0f;
    } else {
        movementSpeed = 10.0f;
    }

    if (Input::isKeyDown(GLFW_KEY_W)) {
        camera.transform.position += camera.front * vel;
    }
    if (Input::isKeyDown(GLFW_KEY_S)) {
        camera.transform.position -= camera.front * vel;
    }
    if (Input::isKeyDown(GLFW_KEY_A)) {
        camera.transform.position -= camera.right * vel;
    }
    if (Input::isKeyDown(GLFW_KEY_D)) {
        camera.transform.position += camera.right * vel;
    }
    if (Input::isKeyDown(GLFW_KEY_SPACE)) {
        camera.transform.position += camera.worldUp * vel;
    }
    if (Input::isKeyDown(GLFW_KEY_LEFT_SHIFT)) {
        camera.transform.position -= camera.worldUp * vel;
    }

    // Mouse movement
    yRot += glm::radians(Input::mouseDeltaX * xSensitivity);
    xRot += glm::radians(Input::mouseDeltaY * ySensitivity);

    if (xRot < glm::radians(-89.9f)) {
        xRot = glm::radians(-89.9f);
    } else if (xRot > glm::radians(89.9f)) {
        xRot = glm::radians(89.9f);
    }

    glm::quat xRotQuat = glm::angleAxis((float)xRot, glm::vec3(1, 0, 0));
    glm::quat yRotQuat = glm::angleAxis((float)yRot, glm::vec3(0, 1, 0));
    camera.transform.rotation = xRotQuat * yRotQuat;

    // TODO: Update camera front
    glm::vec3 camFront = glm::vec3(0, 0, -1);
    camFront = glm::rotateX(camFront, -(float)xRot);
    camFront = glm::rotateY(camFront, -(float)yRot);
    camera.front = camFront;
}
