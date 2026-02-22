#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../Entity.hpp"

struct Transform : Component {
    explicit Transform(
        const glm::vec3 position = glm::vec3(0.0f),
        const glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        const glm::vec3 scale = glm::vec3(1.0f)
    ) : position(position), rotation(rotation), scale(scale) {}

    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;

    [[nodiscard]] glm::vec3 transformDirection(const glm::vec3 direction) const {
        return rotation * direction;
    }
};

inline glm::vec3 getFront(const glm::quat rotation) {
    return rotation * glm::vec3(0, 0, -1);
}

inline glm::vec3 getRight(const glm::quat rotation) {
    return rotation * glm::vec3(1, 0, 0);
}

inline glm::mat4 calculateViewMatrix(const glm::vec3 position, const glm::quat rotation) {
    const glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
    const glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), -position);

    return rotationMatrix * translationMatrix;
}

struct Kinematics : Component {
    glm::vec3 velocity{};
    // float acceleration;
};
