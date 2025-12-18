#pragma once

#include "Transform.hpp"

#include <glm/glm.hpp>

class Camera {
public:
    Camera() = default;
    explicit Camera(glm::vec3 position);

    void update();
    [[nodiscard]] glm::mat4 calculateViewMatrix() const;

    Transform transform{};

    glm::vec3 front{};
    glm::vec3 up{};
    glm::vec3 right{};
    glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

    float FOV = 110.0f;
};
