#include "Q1PlayerController.hpp"

#include "../io/Input.hpp"
#include "GLFW/glfw3.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>

namespace QuakeConstants {
    // Quake/QW/server/sv_phys.c
    constexpr float sv_maxvelocity = 2000;

    constexpr float sv_gravity = 800;
    constexpr float sv_stopspeed = 100;
    constexpr float sv_maxspeed = 320;
    constexpr float sv_accelerate = 10;
    constexpr float sv_airaccelerate = 0.7;
    constexpr float sv_friction = 4;
    constexpr float entgravity = 1.0;

    // Quake/QW/client/cl_input.c
    constexpr float cl_upspeed = 400;
    constexpr float cl_forwardspeed = 400;
    constexpr float cl_backspeed = 400;
    constexpr float cl_sidespeed = 400;

    constexpr float jumpspeed = 270;
}

namespace ConvertedQuakeConstants {
    constexpr float UnitScale = 8.0f / 320.0f;

    constexpr float MaxVelocity = QuakeConstants::sv_maxvelocity * UnitScale;

    constexpr float Gravity = QuakeConstants::sv_gravity * UnitScale;
    constexpr float StopSpeed = QuakeConstants::sv_stopspeed * UnitScale;
    constexpr float MaxSpeed = QuakeConstants::sv_maxspeed * UnitScale;
    constexpr float Accelerate = QuakeConstants::sv_accelerate;
    constexpr float AirAccelerate = QuakeConstants::sv_airaccelerate;
    constexpr float Friction = QuakeConstants::sv_friction;

    constexpr float UpSpeed = QuakeConstants::cl_upspeed * UnitScale;
    constexpr float ForwardSpeed = QuakeConstants::cl_forwardspeed * UnitScale;
    constexpr float BackSpeed = QuakeConstants::cl_backspeed * UnitScale;
    constexpr float SideSpeed = QuakeConstants::cl_sidespeed * UnitScale;

    constexpr float JumpSpeed = QuakeConstants::jumpspeed * UnitScale;
    constexpr float AirAccelerateWishSpeedMax = 30 * UnitScale;
}

void Q1PlayerController::load() {
//    Input::bindings.insert({{GLFW_KEY_W, GLFW_PRESS}, Action::StartMoveForward});
//    Input::bindings.insert({{GLFW_KEY_S, GLFW_PRESS}, Action::StartMoveBackward});
//    Input::bindings.insert({{GLFW_KEY_A, GLFW_PRESS}, Action::StartMoveLeft});
//    Input::bindings.insert({{GLFW_KEY_D, GLFW_PRESS}, Action::StartMoveRight});
//
//    Input::bindings.insert({{GLFW_KEY_W, GLFW_RELEASE}, Action::StopMoveForward});
//    Input::bindings.insert({{GLFW_KEY_S, GLFW_RELEASE}, Action::StopMoveBackward});
//    Input::bindings.insert({{GLFW_KEY_A, GLFW_RELEASE}, Action::StopMoveLeft});
//    Input::bindings.insert({{GLFW_KEY_D, GLFW_RELEASE}, Action::StopMoveRight});
//
//    Input::bindings.insert({{GLFW_KEY_SPACE, GLFW_PRESS}, Action::StartJump});
//    Input::bindings.insert({{GLFW_KEY_SPACE, GLFW_RELEASE}, Action::StopJump});
//
//    Input::registerCallback(Action::StartMoveForward, [this] { --moveInput.z; });
//    Input::registerCallback(Action::StartMoveBackward, [this] { ++moveInput.z; });
//    Input::registerCallback(Action::StartMoveLeft, [this] { --moveInput.x; });
//    Input::registerCallback(Action::StartMoveRight, [this] { ++moveInput.x; });
//
//    Input::registerCallback(Action::StopMoveForward, [this] { ++moveInput.z; });
//    Input::registerCallback(Action::StopMoveBackward, [this] { --moveInput.z; });
//    Input::registerCallback(Action::StopMoveLeft, [this] { ++moveInput.x; });
//    Input::registerCallback(Action::StopMoveRight, [this] { --moveInput.x; });
//
//    Input::registerCallback(Action::StartJump, [this] { queueJump(); });
//    Input::registerCallback(Action::StopJump, [this] { jumpQueued = false; });
}

void Q1PlayerController::update(const float dt) {
    deltaTime = dt;

    // TODO: change this input system to use a controls enum instead of the key itself
    int xMovement = 0;
    if (Input::isKeyDown(GLFW_KEY_A)) {
        xMovement--;
    }
    if (Input::isKeyDown(GLFW_KEY_D)) {
        xMovement++;
    }

    int zMovement = 0;
    if (Input::isKeyDown(GLFW_KEY_W)) {
        zMovement--;
    }
    if (Input::isKeyDown(GLFW_KEY_S)) {
        zMovement++;
    }

    // Mouse movement
    yRot += glm::radians(Input::mouseDeltaX * xSensitivity);
    xRot += glm::radians(Input::mouseDeltaY * ySensitivity);

    if (xRot < glm::radians(-90.0f)) {
        xRot = glm::radians(-90.0f);
    } else if (xRot > glm::radians(90.0f)) {
        xRot = glm::radians(90.0f);
    }

    // The character only rotates on the Y-axis
    character.transform.rotation = glm::angleAxis(static_cast<float>(-yRot), glm::vec3(0, 1, 0));

    const glm::quat xRotQuat = glm::angleAxis(static_cast<float>(xRot), glm::vec3(1, 0, 0));
    const glm::quat yRotQuat = glm::angleAxis(static_cast<float>(yRot), glm::vec3(0, 1, 0));
    camera.transform.rotation = xRotQuat * yRotQuat;

    // TODO: Update camera front
    auto camFront = glm::vec3(0, 0, -1);
    camFront = glm::rotateX(camFront, -static_cast<float>(xRot));
    camFront = glm::rotateY(camFront, -static_cast<float>(yRot));
    camera.front = camFront;

    moveInput = glm::vec3(xMovement, 0, zMovement);

    queueJump();  // Update jump state based on current input

    // Check if we should jump BEFORE friction
    bool jumpingThisFrame = false;
    if (character.isGrounded && jumpQueued) {
        playerVelocity.y = ConvertedQuakeConstants::JumpSpeed;
        jumpQueued = false;  // Consume the jump
        jumpingThisFrame = true;
        character.isGrounded = false;
    }

    if (!jumpingThisFrame) {
        applyFriction();
    }

    airMove();

    character.move(playerVelocity, deltaTime);

    camera.transform.position = character.transform.position + glm::vec3(0, 1.2f, 0);

    currentSpeed = glm::length(glm::vec3(playerVelocity.x, 0, playerVelocity.z));
}

void Q1PlayerController::queueJump() {
    if (autoBunnyHop) {
        // Just queue a jump whenever space is held
        jumpQueued = Input::isKeyDown(GLFW_KEY_SPACE);
        return;
    }

    // Regular mode: only queue jump on transition from not pressed to pressed
    bool jumpPressedThisFrame = Input::isKeyDown(GLFW_KEY_SPACE) && !wasJumpKeyDown;

    if (jumpPressedThisFrame) {
        jumpQueued = true;
    }

    // If jump key is released, clear the queue
    if (!Input::isKeyDown(GLFW_KEY_SPACE)) {
        jumpQueued = false;
    }

    // Store for next frame
    wasJumpKeyDown = Input::isKeyDown(GLFW_KEY_SPACE);
}

void Q1PlayerController::airMove() {
    // Get the input and scale by the side and forward speeds
    auto wishvel = glm::vec3(
        moveInput.x * ConvertedQuakeConstants::SideSpeed,
        0,
        moveInput.z * ConvertedQuakeConstants::ForwardSpeed
    );

    // Rotate the vector in the direction the character is facing
    wishvel = character.transform.transformDirection(wishvel);

    // We don't care about the vertical component
    wishvel.y = 0;

    // Get the intended direction and the magnitude of speed
    glm::vec3 wishdir = wishvel;
    float wishspeed = glm::length(wishdir);
    if (wishdir != glm::vec3(0.0f)) {
        wishdir = glm::normalize(wishdir);
    }

    // Clamp to max speed
    if (wishspeed > ConvertedQuakeConstants::MaxSpeed) {
        wishvel *= ConvertedQuakeConstants::MaxSpeed / wishspeed;
        wishspeed = ConvertedQuakeConstants::MaxSpeed;
    }

    d_wishdir = wishdir;
    d_wishvel = wishvel;
    d_wishspeed = wishspeed;

    // If we are grounded, set our vertical component to zero
    // Then, pass to the appropriate accelerate function
    if (character.isGrounded) {
        playerVelocity.y = 0;
        accelerate(wishdir, wishspeed, ConvertedQuakeConstants::Accelerate);

        if (jumpQueued) {
            playerVelocity.y = ConvertedQuakeConstants::JumpSpeed;
            jumpQueued = false;
        }
    } else {
        airAccelerate(wishdir, wishspeed, ConvertedQuakeConstants::Accelerate);
        playerVelocity.y -= ConvertedQuakeConstants::Gravity * deltaTime;
    }
}

void Q1PlayerController::applyFriction() {
    // If our speed is less than 1, come to a stop
    const float speed = glm::length(playerVelocity);
    if (speed < 1 * ConvertedQuakeConstants::UnitScale) {
        playerVelocity.x = 0;
        playerVelocity.z = 0;
        return;
    }

    // Determines if we are about to drop off of a ledge, using a trace
    // If we are about to move near a ledge, double the friction
    if (character.isGrounded) {}

    // Figure out how much to drop our velocity
    // If our speed is less than some minimum stop speed, then we use the stop speed; otherwise,
    // we continue using our speed. Then, the drop value is this result scaled down by friction and deltaTime
    float drop = 0;
    if (character.isGrounded) {
        const float control = std::max(speed, ConvertedQuakeConstants::StopSpeed);
        drop = control * ConvertedQuakeConstants::Friction * deltaTime;
    }

    // Now scale the velocity
    float newSpeed = speed - drop;
    if (newSpeed < 0) {
        newSpeed = 0;
    }

    if (speed > 0) {
        newSpeed /= speed;
    }

    playerVelocity.x *= newSpeed;
    playerVelocity.z *= newSpeed;
}

void Q1PlayerController::accelerate(const glm::vec3 wishdir, const float wishspeed, const float accel) {
    // Take the magnitude of our current velocity in the direction of wishdir
    const float currentSpeed = glm::dot(playerVelocity, wishdir);

    // Subtract this from wishspeed. If we have a component of our velocity in the direction of
    // wishdir that exceeds the set maximum speed, addSpeed will be 0 or negative
    const float addSpeed = wishspeed - currentSpeed;
    if (addSpeed <= 0) {
        return;
    }

    // Scale wishspeed by an acceleration constant, factoring in deltaTime. Clamp to addSpeed
    float accelSpeed = accel * deltaTime * wishspeed;
    if (accelSpeed > addSpeed) {
        accelSpeed = addSpeed;
    }

    // Scale wishdir by accelSpeed and add it to our velocity
    playerVelocity.x += accelSpeed * wishdir.x;
    playerVelocity.z += accelSpeed * wishdir.z;

    // For imgui debugging
    d_accel_currentSpeed = currentSpeed;
    d_accel_addSpeed = addSpeed;
    d_accel_accelSpeed = accelSpeed;
}

void Q1PlayerController::airAccelerate(const glm::vec3 wishdir, float wishspeed, const float accel) {
    // Same as the regular accelerate function, but clamp wishspeed to 30
    const float original_wishspeed = wishspeed;

    if (wishspeed > ConvertedQuakeConstants::AirAccelerateWishSpeedMax) {
        wishspeed = ConvertedQuakeConstants::AirAccelerateWishSpeedMax;
    }

    const float currentSpeed = glm::dot(playerVelocity, wishdir);
    const float addSpeed = wishspeed - currentSpeed;

    if (addSpeed <= 0) {
        return;
    }

    float accelSpeed = accel * deltaTime * original_wishspeed;  // Note that we use the original wishspeed here
    if (accelSpeed > addSpeed) {
        accelSpeed = addSpeed;
    }

    playerVelocity.x += accelSpeed * wishdir.x;
    playerVelocity.z += accelSpeed * wishdir.z;

    d_airaccel_currentSpeed = currentSpeed;
    d_airaccel_addSpeed = addSpeed;
    d_airaccel_accelSpeed = accelSpeed;
}
