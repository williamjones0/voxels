#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Transform {
public:
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
