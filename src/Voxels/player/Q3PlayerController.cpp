#include "Q3PlayerController.hpp"

#include "../io/Input.hpp"
#include "GLFW/glfw3.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>

void Q3PlayerController::load() {
    Input::bindings.insert({{GLFW_KEY_W, GLFW_PRESS}, Action::StartMoveForward});
    Input::bindings.insert({{GLFW_KEY_S, GLFW_PRESS}, Action::StartMoveBackward});
    Input::bindings.insert({{GLFW_KEY_A, GLFW_PRESS}, Action::StartMoveLeft});
    Input::bindings.insert({{GLFW_KEY_D, GLFW_PRESS}, Action::StartMoveRight});

    Input::bindings.insert({{GLFW_KEY_W, GLFW_RELEASE}, Action::StopMoveForward});
    Input::bindings.insert({{GLFW_KEY_S, GLFW_RELEASE}, Action::StopMoveBackward});
    Input::bindings.insert({{GLFW_KEY_A, GLFW_RELEASE}, Action::StopMoveLeft});
    Input::bindings.insert({{GLFW_KEY_D, GLFW_RELEASE}, Action::StopMoveRight});

    Input::bindings.insert({{GLFW_KEY_SPACE, GLFW_PRESS}, Action::StartJump});
    Input::bindings.insert({{GLFW_KEY_SPACE, GLFW_RELEASE}, Action::StopJump});

    Input::registerCallback(Action::StartMoveForward, [this] { --moveInput.z; });
    Input::registerCallback(Action::StartMoveBackward, [this] { ++moveInput.z; });
    Input::registerCallback(Action::StartMoveLeft, [this] { --moveInput.x; });
    Input::registerCallback(Action::StartMoveRight, [this] { ++moveInput.x; });

    Input::registerCallback(Action::StopMoveForward, [this] { ++moveInput.z; });
    Input::registerCallback(Action::StopMoveBackward, [this] { --moveInput.z; });
    Input::registerCallback(Action::StopMoveLeft, [this] { ++moveInput.x; });
    Input::registerCallback(Action::StopMoveRight, [this] { --moveInput.x; });

    Input::registerCallback(Action::StartJump, [this] { jumpQueued = true; });
    Input::registerCallback(Action::StopJump, [this] { jumpQueued = false; });
}

void Q3PlayerController::update(float dt) {
    deltaTime = dt;

    // TODO: fix this input system
    int xMovement = 0;
//    if (Input::isKeyDown(GLFW_KEY_A)) {
//        xMovement--;
//    }
//    if (Input::isKeyDown(GLFW_KEY_D)) {
//        xMovement++;
//    }

    int zMovement = 0;
//    if (Input::isKeyDown(GLFW_KEY_W)) {
//        zMovement--;
//    }
//    if (Input::isKeyDown(GLFW_KEY_S)) {
//        zMovement++;
//    }

    // moveInput = glm::vec3(xMovement, 0, zMovement);
    queueJump();

    if (character.isGrounded) {
        groundMove();
    } else {
        airMove();
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

    // TODO: The character's position looks like it should include the 1.7f PLAYER_EYE_HEIGHT
    // but it only appears to be 0.7f
    camera.transform.position = character.transform.position + glm::vec3(0, 1.0f, 0);

    speed = glm::length(glm::vec3(playerVelocity.x, 0, playerVelocity.z));
}

void Q3PlayerController::queueJump() {
    if (autoBunnyHop) {
        jumpQueued = true;
        return;
    }
}

void Q3PlayerController::airMove() {
    float accel;

    glm::vec3 wishdir = glm::vec3(moveInput.x, 0, moveInput.z);
    wishdir = character.transform.transformDirection(wishdir);

    float wishspeed = glm::length(wishdir);
    wishspeed *= airSettings.maxSpeed;

    if (wishspeed > 0) {
        wishdir = glm::normalize(wishdir);
    }
    moveDirectionNorm = wishdir;

    // CPM air control
    float wishspeed2 = wishspeed;
    if (glm::dot(playerVelocity, wishdir) < 0) {
        accel = airSettings.deceleration;
    } else {
        accel = airSettings.acceleration;
    }

    // If the player is ONLY strafing left or right
    if (moveInput.z == 0 && moveInput.x != 0) {
        if (wishspeed > strafeSettings.maxSpeed) {
            wishspeed = strafeSettings.maxSpeed;
        }

        accel = strafeSettings.acceleration;
    }

    accelerate(wishdir, wishspeed, accel);
    if (m_airControl > 0) {
        airControl(wishdir, wishspeed2);
    }

    // Apply gravity
    playerVelocity.y -= gravity * deltaTime;
}

void Q3PlayerController::airControl(glm::vec3 targetDir, float targetSpeed) {
    // Only control air movement when moving forward or backward
    if (abs(moveInput.z) < 0.001 || abs(targetSpeed) < 0.001) {
        return;
    }

    float zSpeed = playerVelocity.y;
    playerVelocity.y = 0;

    float speed = glm::length(playerVelocity);
    if (speed < 0.001) {
        return;
    }
    playerVelocity = glm::normalize(playerVelocity);

    float dot = glm::dot(playerVelocity, targetDir);
    float k = 32;
    k *= m_airControl * dot * dot * deltaTime;

    // Change direction while slowing down
    if (dot > 0) {
        playerVelocity.x = playerVelocity.x * speed + targetDir.x * k;
        playerVelocity.y = playerVelocity.y * speed + targetDir.y * k;
        playerVelocity.z = playerVelocity.z * speed + targetDir.z * k;

        playerVelocity = glm::normalize(playerVelocity);
        moveDirectionNorm = playerVelocity;
    }

    playerVelocity.x *= speed;
    playerVelocity.y = zSpeed; // Note this line
    playerVelocity.z *= speed;
}

void Q3PlayerController::groundMove() {
    // Do not apply friction if the player is queueing up the next jump
    if (!jumpQueued) {
        applyFriction(1.0f);
    } else {
        applyFriction(0);
    }

    glm::vec3 wishdir = glm::vec3(moveInput.x, 0, moveInput.z);
    wishdir = character.transform.transformDirection(wishdir);
    if (glm::length(wishdir) > 0) {
        wishdir = glm::normalize(wishdir);
    }
    moveDirectionNorm = wishdir;

    float wishspeed = glm::length(wishdir);
    wishspeed *= groundSettings.maxSpeed;

    accelerate(wishdir, wishspeed, groundSettings.acceleration);

    // Reset the gravity velocity
    playerVelocity.y = -gravity * deltaTime;

    if (jumpQueued) {
        playerVelocity.y = jumpForce;
        jumpQueued = false;
    }
}

void Q3PlayerController::applyFriction(float t) {
    glm::vec3 vec = playerVelocity;
    vec.y = 0;
    float speed = glm::length(vec);
    float drop = 0;

    // Only apply friction when grounded
    if (character.isGrounded) {
        float control = (speed < groundSettings.deceleration) ? groundSettings.deceleration : speed;
        drop = control * friction * deltaTime * t;
    }

    float newSpeed = speed - drop;
    playerFriction = newSpeed;
    if (newSpeed < 0) {
        newSpeed = 0;
    }

    if (speed > 0) {
        newSpeed /= speed;
    }

    playerVelocity.x *= newSpeed;
    playerVelocity.z *= newSpeed;
}

void Q3PlayerController::accelerate(glm::vec3 targetDir, float targetSpeed, float accel) {
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
