#pragma once

#include <glm/glm.hpp>

#include "PlayerController.hpp"

class Q1PlayerController : public PlayerController {
public:
    Q1PlayerController() {
        load();
    }

    void load();
    void update(float dt) override;

private:
    class MovementSettings {
    public:
        float maxSpeed;
        float acceleration;
        float deceleration;

        MovementSettings(const float maxSpeed, const float acceleration, const float deceleration)
            : maxSpeed(maxSpeed), acceleration(acceleration), deceleration(deceleration) {}
    };

    float deltaTime = 0;

    bool autoBunnyHop = true;
    bool jumpQueued = false;

    glm::vec3 moveInput{};

    void airMove();
    void applyFriction() const;
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
