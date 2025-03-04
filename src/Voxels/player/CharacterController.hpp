#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "../world/WorldManager.hpp"

class CharacterController {
public:
    explicit CharacterController(WorldManager &worldManager) : worldManager(worldManager) {}

    Transform transform = Transform(glm::vec3(0.0f, 400.0f, 0.0f));

    bool isGrounded = false;

    void move(glm::vec3 velocity);

private:
    class Contact {
    public:
        // Collision normal
        int nx = 0, ny = 0, nz = 0;
        // Global position of collided voxel
        int x, y, z;
        // Collision time
        float t;

        Contact(float t, int x, int y, int z) : t(t), x(x), y(y), z(z) {}

        bool operator<(const Contact &other) const {
            // Resolve first by Y contacts, then by distance
            return ny != other.ny ? ny < other.ny : t < other.t;
        }
    };

    WorldManager &worldManager;

    float PLAYER_WIDTH = 0.4f;
    float PLAYER_EYE_HEIGHT = 1.7f;
    float PLAYER_HEIGHT = 1.8f;

    void handleCollisions(glm::vec3 velocity);
    void collisionDetection(glm::vec3 v, std::vector<Contact> &contacts);
    void intersectSweptAabbAabb(int x, int y, int z, float px, float py, float pz, float dx, float dy, float dz, std::vector<Contact> &contacts);
    void collisionResponse(glm::vec3 velocity, std::vector<Contact> &contacts);
};
