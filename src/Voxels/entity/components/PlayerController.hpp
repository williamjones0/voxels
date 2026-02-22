#pragma once

#include "../Entity.hpp"

class PlayerController : public Component {
public:
    ~PlayerController() override = default;

    virtual void update(float dt) = 0;
};
