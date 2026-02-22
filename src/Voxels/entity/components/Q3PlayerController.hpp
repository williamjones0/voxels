#pragma once

#include <glm/glm.hpp>

#include "PlayerController.hpp"

class Q3PlayerController : public PlayerController {
public:
    Q3PlayerController() {
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

    float friction = 6;
    float gravity = 20;
    float jumpForce = 8;
    bool autoBunnyHop = false;
    float m_airControl = 0.3f;
    MovementSettings groundSettings = MovementSettings(7, 14, 10);
    MovementSettings airSettings = MovementSettings(7, 2, 2);
    MovementSettings strafeSettings = MovementSettings(1, 50, 50);

    glm::vec3 moveDirectionNorm{};

    bool jumpQueued = false;

    float playerFriction = 0;

    glm::vec3 moveInput{};

    void queueJump();
    void airMove();
    void airControl(glm::vec3 targetDir, float targetSpeed);
    void groundMove();
    void applyFriction(float t);
    void accelerate(glm::vec3 targetDir, float targetSpeed, float accel) const;
};
