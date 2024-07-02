#pragma once

#include <glm/glm.hpp>

enum MovementMode {
    CONSTANT,
    SMOOTH
};

enum Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

class Camera {
public:
    Camera(glm::vec3 position, float yaw, float pitch);

    void update(float deltaTime);

    void processKeyboard(Movement direction, float deltaTime);
    void processMouse(float xoffset, float yoffset, float scrollyoffset);

    glm::mat4 calculateViewMatrix();

    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    const glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

    float yaw;
    float pitch;

    float FOV = 60.0f;

    float movementSpeed = 10.0f;
    float mouseSensitivity = 0.02f;
    float deceleration = 0.95f;

    MovementMode movementMode = CONSTANT;

    void updateCameraVectors();
};
