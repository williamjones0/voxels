#include "Q1PlayerController.hpp"

#include "../io/Input.hpp"
#include "GLFW/glfw3.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>

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

void Q1PlayerController::update(float dt) {
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

    moveInput = glm::vec3(xMovement, 0, zMovement);
    queueJump();

    applyFriction();

    airMove();

    // Mouse movement
    yRot += glm::radians(Input::mouseDeltaX * xSensitivity);
    xRot += glm::radians(Input::mouseDeltaY * ySensitivity);

    if (xRot < glm::radians(-90.0f)) {
        xRot = glm::radians(-90.0f);
    } else if (xRot > glm::radians(90.0f)) {
        xRot = glm::radians(90.0f);
    }

    // The character only rotates on the Y-axis
    character.transform.rotation = glm::angleAxis((float)-yRot, glm::vec3(0, 1, 0));

    glm::quat xRotQuat = glm::angleAxis((float)xRot, glm::vec3(1, 0, 0));
    glm::quat yRotQuat = glm::angleAxis((float)yRot, glm::vec3(0, 1, 0));
    camera.transform.rotation = xRotQuat * yRotQuat;

    // TODO: Update camera front
    glm::vec3 camFront = glm::vec3(0, 0, -1);
    camFront = glm::rotateX(camFront, -(float)xRot);
    camFront = glm::rotateY(camFront, -(float)yRot);
    camera.front = camFront;

    character.move(playerVelocity, deltaTime);

    camera.transform.position = character.transform.position + glm::vec3(0, 1.7f, 0);

    speed = glm::length(glm::vec3(playerVelocity.x, 0, playerVelocity.z));
}

void Q1PlayerController::queueJump() {
    if (autoBunnyHop) {
        jumpQueued = Input::isKeyDown(GLFW_KEY_SPACE);
        return;
    }

    if (Input::isKeyDown(GLFW_KEY_SPACE) && !jumpQueued) {
        jumpQueued = true;
    }

    if (!Input::isKeyDown(GLFW_KEY_SPACE)) {
        jumpQueued = false;
    }
}

//void Q1PlayerController::queueJump() {
//    if (autoBunnyHop) {
//        jumpQueued = true;
//        return;
//    }
//
//    if (!jumpQueued) {
//        jumpQueued = true;
//    }
//}

void Q1PlayerController::airMove() {
    glm::vec3 wishdir = glm::vec3(moveInput.x, 0, moveInput.z);
    wishdir = character.transform.transformDirection(wishdir);

    float wishspeed = glm::length(wishdir);
    wishspeed *= airSettings.maxSpeed;

    if (wishspeed > 0) {
        wishdir = glm::normalize(wishdir);
    }
    moveDirectionNorm = wishdir;

    glm::vec3 wishvel = wishdir;

    // Clamp to max speed
    if (wishspeed > airSettings.maxSpeed) {
        wishvel *= airSettings.maxSpeed / wishspeed;
        wishspeed = airSettings.maxSpeed;
    }

    if (character.isGrounded) {
        playerVelocity.y = 0;
        accelerate(wishdir, wishspeed, airSettings.acceleration);
        playerVelocity.y -= gravity * deltaTime;
        groundMove();
    } else {
        airAccelerate(wishdir, wishspeed, airSettings.acceleration);
        playerVelocity.y -= gravity * deltaTime;
        flyMove();
    }
}

void Q1PlayerController::groundMove() {
//    glm::vec3 wishdir = glm::vec3(moveInput.x, 0, moveInput.z);
//    wishdir = character.transform.transformDirection(wishdir);
//    if (glm::length(wishdir) > 0) {
//        wishdir = glm::normalize(wishdir);
//    }
//    moveDirectionNorm = wishdir;
//
//    float wishspeed = glm::length(wishdir);
//    wishspeed *= groundSettings.maxSpeed;
//
//    accelerate(wishdir, wishspeed, groundSettings.acceleration);
//
//    // Reset the gravity velocity
//    playerVelocity.y = -gravity * deltaTime;



    if (jumpQueued) {
        playerVelocity.y = jumpForce;
        jumpQueued = false;
    }
}

void Q1PlayerController::flyMove() {}

void Q1PlayerController::applyFriction() {
    glm::vec3 vel = playerVelocity;

    float speed = glm::length(vel);
    if (speed < 1 * unitScale) {
        playerVelocity.x = 0;
        playerVelocity.z = 0;
        return;
    }

    // something to do with ledges
    if (character.isGrounded) {}

    float drop = 0;
    if (character.isGrounded) {
        float control = speed < stopSpeed ? stopSpeed : speed;
        drop = control * friction * deltaTime;
    }

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

void Q1PlayerController::accelerate(glm::vec3 targetDir, float targetSpeed, float accel) {
    float currentSpeed = glm::dot(playerVelocity, targetDir);
    float addSpeed = targetSpeed - currentSpeed;

    if (addSpeed <= 0) {
        return;
    }

    float accelSpeed = accel * deltaTime * targetSpeed;
    if (accelSpeed > addSpeed) {
        accelSpeed = addSpeed;
    }

    playerVelocity.x += accelSpeed * targetDir.x;
    playerVelocity.z += accelSpeed * targetDir.z;
}

void Q1PlayerController::airAccelerate(glm::vec3 targetDir, float targetSpeed, float accel) {
    if (targetSpeed > 30) {
        targetSpeed = 30;
    }

    float currentSpeed = glm::dot(playerVelocity, targetDir);
    float addSpeed = targetSpeed - currentSpeed;

    if (addSpeed <= 0) {
        return;
    }

    float accelSpeed = accel * deltaTime * targetSpeed;
    if (accelSpeed > addSpeed) {
        accelSpeed = addSpeed;
    }

    playerVelocity.x += accelSpeed * targetDir.x;
    playerVelocity.z += accelSpeed * targetDir.z;
}
