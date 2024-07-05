#include "Camera.hpp"

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>

void rotationX(glm::quat &q, float angle);
void rotateY(glm::quat &q, float angle);

Camera::Camera(glm::vec3 position, float yaw, float pitch) : position(position), yaw(yaw), pitch(pitch) {
    updateCameraVectors();
}

void Camera::update(float deltaTime) {
    position += velocity * deltaTime;
    velocity *= deceleration;

    tmpq = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    float yawRads = glm::radians(yaw);
    float pitchRads = glm::radians(pitch);

    //tmpq = glm::rotate(tmpq, yawRads, xDir);
    //tmpq = glm::rotate(tmpq, pitchRads, yDir);

    std::cout << "yawRads: " << yawRads << ", pitchRads: " << pitchRads << "\n";

    rotationX(tmpq, yawRads);
    std::cout << "tmpq: " << glm::to_string(tmpq) << "\n";

    rotateY(tmpq, pitchRads);
    std::cout << "tmpq: " << glm::to_string(tmpq) << "\n";

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
    dangx += yoffset;
    dangy += xoffset;

    angx += dangx * 0.002f;
    angy += dangy * 0.002f;
    dangx *= 0.0994f;
    dangy *= 0.0994f;

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

void rotationX(glm::quat &q, float angle) {
    float sin = glm::sin(angle / 2);
    float cos = glm::cos(angle / 2);
    q.w = cos;
    q.x = sin;
    q.y = 0;
    q.z = 0;
}

void rotateY(glm::quat &q, float angle) {
    float sin = glm::sin(angle / 2);
    float cos = glm::cos(angle / 2);
    float w = q.w * cos - q.y * sin;
    float x = q.x * cos - q.z * sin;
    float y = q.w * sin + q.y * cos;
    float z = q.x * sin + q.z * cos;
    q.w = w;
    q.x = x;
    q.y = y;
    q.z = z;
}
