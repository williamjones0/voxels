#pragma once

#include "../core/Camera.hpp"
#include "CharacterController.hpp"

class Q1PlayerController {
public:
    Q1PlayerController(Camera& camera, CharacterController& character)
        : camera(camera), character(character) {
        load();
    }

    void load();
    void update(float dt);

    float currentSpeed = 0;
    glm::vec3 playerVelocity = glm::vec3(0, 0, 0);

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

    CharacterController& character;

    bool autoBunnyHop = true;

    bool jumpQueued = false;
    bool wasJumpKeyDown = false;

    glm::vec3 moveInput{};

    double xRot = 0.0f;
    double yRot = 0.0f;
    float xSensitivity = 0.016f;
    float ySensitivity = 0.016f;

    void queueJump();
    void airMove();
    void applyFriction();
    void accelerate(glm::vec3 wishdir, float wishspeed, float accel);
    void airAccelerate(glm::vec3 wishdir, float wishspeed, float accel);

public:
    // debugging
    glm::vec3 d_wishdir;
    glm::vec3 d_wishvel;
    float d_wishspeed;

    // in accelerate function
    float d_accel_currentSpeed;
    float d_accel_addSpeed;
    float d_accel_accelSpeed;

    // air accelerate
    float d_airaccel_currentSpeed;
    float d_airaccel_addSpeed;
    float d_airaccel_accelSpeed;
};
