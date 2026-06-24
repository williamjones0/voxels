#pragma once

#include "../Entity.hpp"

class PlayerController : public Component {
public:
    PlayerController(Entity& owner) : Component(owner) {}

    ~PlayerController() override = default;

    virtual void update(float dt) = 0;
};
