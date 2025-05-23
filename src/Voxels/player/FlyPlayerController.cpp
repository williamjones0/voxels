#include "FlyPlayerController.hpp"

#include "../io/Input.hpp"
#include "GLFW/glfw3.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>

void FlyPlayerController::update(float deltaTime) {
    float vel = movementSpeed * deltaTime;

    glm::vec3 positionDelta = glm::vec3(0, 0, 0);

    if (Input::isKeyDown(GLFW_KEY_LEFT_CONTROL) || Input::isButtonDown(GLFW_MOUSE_BUTTON_4)) {
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
