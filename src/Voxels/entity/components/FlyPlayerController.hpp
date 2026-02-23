#pragma once

#include <unordered_set>

#include "PlayerController.hpp"

class FlyPlayerController : public PlayerController {
public:
    FlyPlayerController();

    void update(float deltaTime) override;

    ~FlyPlayerController() override;

private:
    enum Movement {
        Forward,
        Backward,
        Left,
        Right,
        Up,
        Down
    };

    std::unordered_set<Movement> movements;

    float movementSpeed = 10.0f;
};
