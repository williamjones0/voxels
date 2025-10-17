#pragma once

#include "../core/Camera.hpp"
#include "CharacterController.hpp"

class Q3PlayerController {
public:
    Q3PlayerController(Camera& camera, CharacterController& character)
        : camera(camera), character(character) {
        load();
    }

    void load();
    void update(float dt);

    float currSpeed = 0;
    glm::vec3 playerVelocity{};

private:
    class MovementSettings {
    public:
        float maxSpeed;
        float acceleration;
        float deceleration;

        MovementSettings(const float maxSpeed, const float acceleration, const float deceleration)
            : maxSpeed(maxSpeed), acceleration(acceleration), deceleration(deceleration) {}
    };

    Camera& camera;

    float deltaTime = 0;

    float friction = 6;
    float gravity = 20;
    float jumpForce = 8;
    bool autoBunnyHop = true;
    float m_airControl = 0.3f;
    MovementSettings groundSettings = MovementSettings(7, 14, 10);
    MovementSettings airSettings = MovementSettings(7, 2, 2);
    MovementSettings strafeSettings = MovementSettings(1, 50, 50);

    CharacterController& character;
    glm::vec3 moveDirectionNorm{};

    bool jumpQueued = false;

    float playerFriction = 0;

    glm::vec3 moveInput{};

    double xRot = 0.0f;
    double yRot = 0.0f;
    float xSensitivity = 0.02f;
    float ySensitivity = 0.02f;

    void queueJump();
    void airMove();
    void airControl(glm::vec3 targetDir, float targetSpeed);
    void groundMove();
    void applyFriction(float t);
    void accelerate(glm::vec3 targetDir, float targetSpeed, float accel);
};
