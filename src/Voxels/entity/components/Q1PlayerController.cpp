#include "Q1PlayerController.hpp"

#include "../../io/Input.hpp"
#include "../Entity.hpp"
#include "CharacterController.hpp"
#include "Transform.hpp"

#include "GLFW/glfw3.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/string_cast.hpp>

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

Q1PlayerController::Q1PlayerController() {
    Input::bindings.insert({{GLFW_KEY_W, GLFW_PRESS}, {ActionType::MoveForward, ActionStateType::Start}});
    Input::bindings.insert({{GLFW_KEY_S, GLFW_PRESS}, {ActionType::MoveBackward, ActionStateType::Start}});
    Input::bindings.insert({{GLFW_KEY_A, GLFW_PRESS}, {ActionType::MoveLeft, ActionStateType::Start}});
    Input::bindings.insert({{GLFW_KEY_D, GLFW_PRESS}, {ActionType::MoveRight, ActionStateType::Start}});
    Input::bindings.insert({{GLFW_KEY_SPACE, GLFW_PRESS}, {ActionType::MoveUp, ActionStateType::Start}});

    Input::bindings.insert({{GLFW_KEY_W, GLFW_RELEASE}, {ActionType::MoveForward, ActionStateType::Stop}});
    Input::bindings.insert({{GLFW_KEY_S, GLFW_RELEASE}, {ActionType::MoveBackward, ActionStateType::Stop}});
    Input::bindings.insert({{GLFW_KEY_A, GLFW_RELEASE}, {ActionType::MoveLeft, ActionStateType::Stop}});
    Input::bindings.insert({{GLFW_KEY_D, GLFW_RELEASE}, {ActionType::MoveRight, ActionStateType::Stop}});
    Input::bindings.insert({{GLFW_KEY_SPACE, GLFW_RELEASE}, {ActionType::MoveUp, ActionStateType::Stop}});

    Input::registerCallback({ActionType::MoveForward, ActionStateType::Start}, [this] { --moveInput.z; });
    Input::registerCallback({ActionType::MoveBackward, ActionStateType::Start}, [this] { ++moveInput.z; });
    Input::registerCallback({ActionType::MoveLeft, ActionStateType::Start}, [this] { --moveInput.x; });
    Input::registerCallback({ActionType::MoveRight, ActionStateType::Start}, [this] { ++moveInput.x; });
    Input::registerCallback({ActionType::MoveUp, ActionStateType::Start}, [this] { jumpQueued = true; });

    Input::registerCallback({ActionType::MoveForward, ActionStateType::Stop}, [this] { ++moveInput.z; });
    Input::registerCallback({ActionType::MoveBackward, ActionStateType::Stop}, [this] { --moveInput.z; });
    Input::registerCallback({ActionType::MoveLeft, ActionStateType::Stop}, [this] { ++moveInput.x; });
    Input::registerCallback({ActionType::MoveRight, ActionStateType::Stop}, [this] { --moveInput.x; });
    Input::registerCallback({ActionType::MoveUp, ActionStateType::Stop}, [this] { jumpQueued = false; });

    Input::requeueCurrentActions();
}

void Q1PlayerController::update(const float dt) {
    deltaTime = dt;

    Entity* player = getEntity();
    Transform* transform = player->get<Transform>();
    CharacterController* character = player->get<CharacterController>();
    Kinematics* kinematics = player->get<Kinematics>();
    glm::vec3& playerVelocity = kinematics->velocity;

    // Mouse movement
    transform->angles.y += glm::radians(Input::mouseDeltaX * XSensitivity);  // yaw
    transform->angles.x += glm::radians(Input::mouseDeltaY * YSensitivity);  // pitch

    transform->angles.x = glm::clamp(transform->angles.x, glm::radians(-90.0f), glm::radians(90.0f));

    // Check if we should jump BEFORE friction
    bool jumpingThisFrame = false;
    if (character->isGrounded && jumpQueued) {
        playerVelocity.y = ConvertedQuakeConstants::JumpSpeed;
        if (!autoBunnyHop) jumpQueued = false;  // Consume the jump
        jumpingThisFrame = true;
        character->isGrounded = false;
    }

    if (!jumpingThisFrame) {
        applyFriction();
    }

    airMove();

    character->move(playerVelocity, deltaTime);
}

void Q1PlayerController::airMove() {
    Entity* player = getEntity();
    Transform* transform = player->get<Transform>();
    CharacterController* character = player->get<CharacterController>();
    Kinematics* kinematics = player->get<Kinematics>();
    glm::vec3& playerVelocity = kinematics->velocity;

    // Get the input and scale by the side and forward speeds
    auto wishvel = glm::vec3(
        moveInput.x * ConvertedQuakeConstants::SideSpeed,
        0,
        moveInput.z * ConvertedQuakeConstants::ForwardSpeed
    );

    // Rotate the vector in the direction the character is facing
    wishvel = transformDirection(wishvel, {0, transform->angles.y, 0});

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
    if (character->isGrounded) {
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

void Q1PlayerController::applyFriction() const {
    Entity* player = getEntity();
    const CharacterController* character = player->get<CharacterController>();
    Kinematics* kinematics = player->get<Kinematics>();
    glm::vec3& playerVelocity = kinematics->velocity;

    // If our speed is less than 1, come to a stop
    const float speed = glm::length(playerVelocity);
    if (speed < 1 * ConvertedQuakeConstants::UnitScale) {
        playerVelocity.x = 0;
        playerVelocity.z = 0;
        return;
    }

    // Determines if we are about to drop off of a ledge, using a trace
    // If we are about to move near a ledge, double the friction
    if (character->isGrounded) {}

    // Figure out how much to drop our velocity
    // If our speed is less than some minimum stop speed, then we use the stop speed; otherwise,
    // we continue using our speed. Then, the drop value is this result scaled down by friction and deltaTime
    float drop = 0;
    if (character->isGrounded) {
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
    Entity* player = getEntity();
    Kinematics* kinematics = player->get<Kinematics>();
    glm::vec3& playerVelocity = kinematics->velocity;

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
    Entity* player = getEntity();
    Kinematics* kinematics = player->get<Kinematics>();
    glm::vec3& playerVelocity = kinematics->velocity;

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

Q1PlayerController::~Q1PlayerController() {
    Input::bindings.erase({GLFW_KEY_W, GLFW_PRESS});
    Input::bindings.erase({GLFW_KEY_S, GLFW_PRESS});
    Input::bindings.erase({GLFW_KEY_A, GLFW_PRESS});
    Input::bindings.erase({GLFW_KEY_D, GLFW_PRESS});

    Input::bindings.erase({GLFW_KEY_W, GLFW_RELEASE});
    Input::bindings.erase({GLFW_KEY_S, GLFW_RELEASE});
    Input::bindings.erase({GLFW_KEY_A, GLFW_RELEASE});
    Input::bindings.erase({GLFW_KEY_D, GLFW_RELEASE});

    Input::bindings.erase({GLFW_KEY_SPACE, GLFW_PRESS});
    Input::bindings.erase({GLFW_KEY_SPACE, GLFW_RELEASE});

    Input::eraseCallback({ActionType::MoveForward, ActionStateType::Start});
    Input::eraseCallback({ActionType::MoveBackward, ActionStateType::Start});
    Input::eraseCallback({ActionType::MoveLeft, ActionStateType::Start});
    Input::eraseCallback({ActionType::MoveRight, ActionStateType::Start});

    Input::eraseCallback({ActionType::MoveForward, ActionStateType::Stop});
    Input::eraseCallback({ActionType::MoveBackward, ActionStateType::Stop});
    Input::eraseCallback({ActionType::MoveLeft, ActionStateType::Stop});
    Input::eraseCallback({ActionType::MoveRight, ActionStateType::Stop});

    Input::eraseCallback({ActionType::MoveUp, ActionStateType::Start});
    Input::eraseCallback({ActionType::MoveUp, ActionStateType::Stop});
}
