#include "Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

Camera::Camera(glm::vec3 position, float yaw, float pitch) : position(position), yaw(yaw), pitch(pitch) {
    updateCameraVectors();
}

void Camera::update(float deltaTime) {
    position += velocity * deltaTime;
    velocity *= deceleration;
    updateCameraVectors();
}

void Camera::processKeyboard(Movement direction, float deltaTime) {
    float vel = movementSpeed * deltaTime;

    if (movementMode == CONSTANT) {
        switch (direction) {
        case FORWARD:
            position += front * vel;
            break;
        case BACKWARD:
            position -= front * vel;
            break;
        case LEFT:
            position -= right * vel;
            break;
        case RIGHT:
            position += right * vel;
            break;
        case UP:
            position += worldUp * vel;
            break;
        case DOWN:
            position -= worldUp * vel;
            break;
        }
    }
    else if (movementMode == SMOOTH) {
        switch (direction) {
        case FORWARD:
            velocity += front * vel;
            break;
        case BACKWARD:
            velocity -= front * vel;
            break;
        case LEFT:
            velocity -= right * vel;
            break;
        case RIGHT:
            velocity += right * vel;
            break;
        case UP:
            velocity += worldUp * vel;
            break;
        case DOWN:
            velocity -= worldUp * vel;
            break;
        }
    }
}

void Camera::processMouse(float xoffset, float yoffset, float scrollyoffset) {
    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.9f)
        pitch = 89.9f;
    if (pitch < -89.9f)
        pitch = -89.9f;

    updateCameraVectors();
}

glm::mat4 Camera::calculateViewMatrix() {
    return glm::lookAt(position, position + front, worldUp);
}

void Camera::updateCameraVectors() {
    front = glm::normalize(glm::vec3(
        sin(glm::radians(yaw)) * cos(glm::radians(pitch)),
        sin(glm::radians(pitch)),
        -cos(glm::radians(yaw)) * cos(glm::radians(pitch))
    ));

    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}
