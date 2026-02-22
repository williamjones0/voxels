#pragma once

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>

#include "../Entity.hpp"

struct Transform : Component {
    explicit Transform(
        const glm::vec3 position = glm::vec3(),
        const glm::vec3 angles = glm::vec3(),
        const glm::vec3 scale = glm::vec3(1.0f)
    ) : position(position), angles(angles), scale(scale) {}

    glm::vec3 position;
    glm::vec3 angles;  // {pitch, yaw, roll}
    glm::vec3 scale;
};

inline glm::vec3 transformDirection(const glm::vec3 direction, const glm::vec3 angles) {
    glm::vec3 out = direction;
    out = glm::rotateX(out, -angles.x);
    out = glm::rotateY(out, -angles.y);
    out = glm::rotateZ(out, -angles.z);
    return out;
}

inline glm::vec3 getFront(const glm::vec3 angles) {
    return transformDirection(glm::vec3(0, 0, -1), angles);
}

inline glm::vec3 getRight(const glm::vec3 angles) {
    return transformDirection(glm::vec3(1, 0, 0), angles);
}

inline glm::mat4 calculateViewMatrix(const glm::vec3 position, const glm::vec3 angles) {
    const glm::mat4 rotationMatrix =
        glm::rotate(glm::mat4(1.0f), angles.z, glm::vec3(0, 0, 1)) *
        glm::rotate(glm::mat4(1.0f), angles.x, glm::vec3(1, 0, 0)) *
        glm::rotate(glm::mat4(1.0f), angles.y, glm::vec3(0, 1, 0));

    const glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), -position);

    return rotationMatrix * translationMatrix;
}

struct Kinematics : Component {
    glm::vec3 velocity{};
    // float acceleration;
};
