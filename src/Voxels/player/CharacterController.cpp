#include "CharacterController.hpp"

#include <cmath>

void CharacterController::move(glm::vec3& velocity, const float dt) {
    isGrounded = false;
    handleCollisions(velocity, dt);
}

void CharacterController::handleCollisions(glm::vec3& velocity, const float dt) {
    std::vector<Contact> contacts;
    collisionDetection(velocity, dt, contacts);
    collisionResponse(velocity, dt, contacts);
}

void CharacterController::collisionDetection(const glm::vec3& velocity, const float dt, std::vector<Contact>& contacts) const {
    const glm::vec3 v = velocity * dt;

    const float dx = v.x;
    const float dy = v.y;
    const float dz = v.z;

    const int minX = static_cast<int>(std::floor(transform.position.x - PLAYER_WIDTH + (dx < 0 ? dx : 0)));
    const int maxX = static_cast<int>(std::floor(transform.position.x + PLAYER_WIDTH + (dx > 0 ? dx : 0)));
    const int minY = static_cast<int>(std::floor(transform.position.y - PLAYER_EYE_HEIGHT + (dy < 0 ? dy : 0)));
    const int maxY = static_cast<int>(std::floor(transform.position.y + PLAYER_HEIGHT - PLAYER_EYE_HEIGHT + (dy > 0 ? dy : 0)));
    const int minZ = static_cast<int>(std::floor(transform.position.z - PLAYER_WIDTH + (dz < 0 ? dz : 0)));
    const int maxZ = static_cast<int>(std::floor(transform.position.z + PLAYER_WIDTH + (dz > 0 ? dz : 0)));

    // Loop over all voxels that could possibly collide with the player
    for (int y = std::min(ChunkHeight - 1, maxY); y >= 0 && y >= minY; y--) {
        for (int z = minZ; z <= maxZ; z++) {
            for (int x = minX; x <= maxX; x++) {
                if (worldManager.load(x, y, z) == 0) {
                    continue;
                }

                intersectSweptAabbAabb(x, y, z, transform.position.x - static_cast<float>(x), transform.position.y - static_cast<float>(y), transform.position.z - static_cast<float>(z), dx, dy, dz, contacts);
            }
        }
    }
}

void CharacterController::intersectSweptAabbAabb(const int x, const int y, const int z, const float px, const float py, const float pz,
                                                 const float dx, const float dy, const float dz, std::vector<Contact>& contacts) const {
    // https://www.gamedev.net/tutorials/programming/general-and-gameplay-programming/swept-aabb-collision-detection-and-response-r3084/
    constexpr float negativeInfinity = -std::numeric_limits<float>::infinity();
    constexpr float positiveInfinity = std::numeric_limits<float>::infinity();

    const float pxmax = px + PLAYER_WIDTH, pxmin = px - PLAYER_WIDTH, pymax = py + PLAYER_HEIGHT - PLAYER_EYE_HEIGHT, pymin = py - PLAYER_EYE_HEIGHT,
                pzmax = pz + PLAYER_WIDTH, pzmin = pz - PLAYER_WIDTH;
    const float xInvEntry = dx > 0 ? -pxmax : 1 - pxmin, xInvExit = dx > 0 ? 1 - pxmin : -pxmax;
    const bool xNotValid = dx == 0 || worldManager.load(x + (dx > 0 ? -1 : 1), y, z) != EmptyVoxel;
    const float xEntry = xNotValid ? negativeInfinity : xInvEntry / dx, xExit = xNotValid ? positiveInfinity : xInvExit / dx;
    const float yInvEntry = dy > 0 ? -pymax : 1 - pymin, yInvExit = dy > 0 ? 1 - pymin : -pymax;
    const bool yNotValid = dy == 0 || worldManager.load(x, y + (dy > 0 ? -1 : 1), z) != EmptyVoxel;
    const float yEntry = yNotValid ? negativeInfinity : yInvEntry / dy, yExit = yNotValid ? positiveInfinity : yInvExit / dy;
    const float zInvEntry = dz > 0 ? -pzmax : 1 - pzmin, zInvExit = dz > 0 ? 1 - pzmin : -pzmax;
    const bool zNotValid = dz == 0 || worldManager.load(x, y, z + (dz > 0 ? -1 : 1)) != EmptyVoxel;
    const float zEntry = zNotValid ? negativeInfinity : zInvEntry / dz, zExit = zNotValid ? positiveInfinity : zInvExit / dz;
    const float tEntry = std::max(std::max(xEntry, yEntry), zEntry), tExit = std::min(std::min(xExit, yExit), zExit);
    if (tEntry < -.5f || tEntry > tExit) {
        return;
    }
    auto c = Contact(tEntry, x, y, z);
    if (xEntry == tEntry) {
        c.nx = dx > 0 ? -1 : 1;
    } else if (yEntry == tEntry) {
        c.ny = dy > 0 ? -1 : 1;
    } else {
        c.nz = dz > 0 ? -1 : 1;
    }
    contacts.push_back(c);
}

void CharacterController::collisionResponse(glm::vec3& velocity, const float dt, std::vector<Contact>& contacts) {
    const glm::vec3 v = velocity * dt;
    std::sort(contacts.begin(), contacts.end());

    int minX = INT_MIN, maxX = INT_MAX, minY = INT_MIN, maxY = INT_MAX, minZ = INT_MIN, maxZ = INT_MAX;
    float elapsedTime = 0;
    float dx = v.x, dy = v.y, dz = v.z;
    bool goUp = false;
    for (auto contact : contacts) {
        if (contact.x <= minX || contact.y <= minY || contact.z <= minZ || contact.x >= maxX || contact.y >= maxY || contact.z >= maxZ)
            continue;
        const float t = contact.t - elapsedTime;
        transform.position += glm::vec3(dx * t, dy * t, dz * t);
        elapsedTime += t;
        if (contact.nx != 0) {
            minX = dx < 0 ? std::max(minX, contact.x) : minX;
            maxX = dx < 0 ? maxX : std::min(maxX, contact.x);
            if (worldManager.load(contact.x, contact.y + 1, contact.z) == EmptyVoxel) {
                goUp = true;
            } else {
                velocity.x = 0.0f;
                dx = 0.0f;
            }
        } else if (contact.ny != 0) {
            isGrounded = dy < 0;
            minY = dy < 0 ? std::max(minY, contact.y) : contact.y - static_cast<int>(PLAYER_HEIGHT);
            maxY = dy < 0 ? contact.y + static_cast<int>(std::ceil(PLAYER_HEIGHT)) + 1 : std::min(maxY, contact.y);
            velocity.y = 0.0f;
            dy = 0.0f;
        } else if (contact.nz != 0) {
            minZ = dz < 0 ? std::max(minZ, contact.z) : minZ;
            maxZ = dz < 0 ? maxZ : std::min(maxZ, contact.z);
            if (worldManager.load(contact.x, contact.y + 1, contact.z) == EmptyVoxel) {
                goUp = true;
            } else {
                velocity.z = 0.0f;
                dz = 0.0f;
            }
        }
    }
    const float trem = 1.0f - elapsedTime;
    transform.position += glm::vec3(dx * trem, dy * trem, dz * trem);
    if (goUp) {
        transform.position.y -= PLAYER_EYE_HEIGHT;
        transform.position.y += 1.0f;
        transform.position.y = std::floor(transform.position.y) + 0.01f;
        transform.position.y += PLAYER_EYE_HEIGHT;
        isGrounded = true;
    }
}
