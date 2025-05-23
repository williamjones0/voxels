#pragma once

#include "../core/Camera.hpp"

class FlyPlayerController {
public:
    explicit FlyPlayerController(Camera &camera)
        : camera(camera) {}

    void update(float deltaTime);

    float speed = 0;
    glm::vec3 playerVelocity = glm::vec3(0, 0, 0);

private:
    Camera &camera;

    glm::vec3 velocity {};

    double xRot = 0.0f;
    double yRot = 0.0f;
    float xSensitivity = 0.02f;
    float ySensitivity = 0.02f;

    float movementSpeed = 10.0f;
};
