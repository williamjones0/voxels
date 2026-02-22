#pragma once

#include "../Entity.hpp"

struct CameraProperties : Component {
    float FOV = 110.0f;

    float near = 0.1f;
    float far = 5000.0f;
};
