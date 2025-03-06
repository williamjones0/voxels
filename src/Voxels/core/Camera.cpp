#include "Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(glm::vec3 position)
    : transform(position) {
    update();
}

void Camera::update() {
    // front = transform.transformDirection(glm::vec3(0.0f, 0.0f, -1.0f));

    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}

glm::mat4 Camera::calculateViewMatrix() const {
    glm::mat4 rotationMatrix = glm::mat4_cast(transform.rotation);
    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), -transform.position);

    return rotationMatrix * translationMatrix;
}
