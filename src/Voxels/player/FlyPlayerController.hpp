#pragma once

#include <unordered_set>

#include "../core/Camera.hpp"

class FlyPlayerController {
public:
    explicit FlyPlayerController(Camera& camera)
        : camera(camera) {
        load();
    }

    void load();
    void update(float deltaTime);

    float currentSpeed = 0;
    glm::vec3 playerVelocity = glm::vec3(0, 0, 0);

private:
    enum Movement {
        Forward,
        Backward,
        Left,
        Right,
        Up,
        Down
    };

    std::unordered_set<Movement> movements;

    Camera& camera;

    glm::vec3 velocity{};
    glm::vec3 direction{};

    double xRot = 0.0f;
    double yRot = 0.0f;
    float xSensitivity = 0.02f;
    float ySensitivity = 0.02f;

    float movementSpeed = 10.0f;
};
