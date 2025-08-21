#pragma once

#include "../core/Camera.hpp"
#include "CharacterController.hpp"

class Q1PlayerController {
public:
    Q1PlayerController(Camera &camera, CharacterController &character)
            : camera(camera), character(character) {}

    void load();
    void update(float deltaTime);

    float speed = 0;
    glm::vec3 playerVelocity = glm::vec3(0, 0, 0);

private:
    class MovementSettings {
    public:
        float maxSpeed;
        float acceleration;
        float deceleration;

        MovementSettings(float maxSpeed, float acceleration, float deceleration)
                : maxSpeed(maxSpeed), acceleration(acceleration), deceleration(deceleration) {}
    };

    Camera &camera;

    float deltaTime = 0;

    const float unitScale = 7.0f / 320.0f;

    float friction = 6;
    float gravity = 20;
    float jumpForce = 8;
    bool autoBunnyHop = false;
    float m_airControl = 0.3f;
    MovementSettings groundSettings = MovementSettings(7, 14, 10);
    MovementSettings airSettings = MovementSettings(7, 2, 2);
    MovementSettings strafeSettings = MovementSettings(1, 50, 50);
    float stopSpeed = 100.0f * unitScale;

    CharacterController &character;
    glm::vec3 moveDirectionNorm = glm::vec3(0, 0, 0);

    bool jumpQueued = false;

    float playerFriction = 0;

    glm::vec3 moveInput = glm::vec3(0, 0, 0);

    double xRot = 0.0f;
    double yRot = 0.0f;
    float xSensitivity = 0.02f;
    float ySensitivity = 0.02f;

    void queueJump();
    void airMove();
    void groundMove();
    void flyMove();
    void applyFriction();
    void accelerate(glm::vec3 targetDir, float targetSpeed, float accel);
    void airAccelerate(glm::vec3 targetDir, float targetSpeed, float accel);
};
