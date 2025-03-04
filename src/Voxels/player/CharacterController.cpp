#include "CharacterController.hpp"

#include <cmath>

void CharacterController::move(glm::vec3 velocity) {
    isGrounded = false;
    handleCollisions(velocity);
}

void CharacterController::handleCollisions(glm::vec3 velocity) {
    std::vector<Contact> contacts;
    collisionDetection(velocity, contacts);
    collisionResponse(velocity, contacts);
}

void CharacterController::collisionDetection(glm::vec3 v, std::vector<Contact> &contacts) {
    float dx = v.x;
    float dy = v.y;
    float dz = v.z;

    int minX = static_cast<int>(std::floor(transform.position.x - PLAYER_WIDTH + (dx < 0 ? dx : 0)));
    int maxX = static_cast<int>(std::floor(transform.position.x + PLAYER_WIDTH + (dx > 0 ? dx : 0)));
    int minY = static_cast<int>(std::floor(transform.position.y - PLAYER_EYE_HEIGHT + (dy < 0 ? dy : 0)));
    int maxY = static_cast<int>(std::floor(transform.position.y + PLAYER_HEIGHT - PLAYER_EYE_HEIGHT + (dy > 0 ? dy : 0)));
    int minZ = static_cast<int>(std::floor(transform.position.z - PLAYER_WIDTH + (dz < 0 ? dz : 0)));
    int maxZ = static_cast<int>(std::floor(transform.position.z + PLAYER_WIDTH + (dz > 0 ? dz : 0)));

    // Loop over all voxels that could possibly collide with the player
    for (int y = std::min(CHUNK_HEIGHT - 1, maxY); y >= 0 && y >= minY; y--) {
        for (int z = minZ; z <= maxZ; z++) {
            for (int x = minX; x <= maxX; x++) {
                if (worldManager.load(x, y, z) == 0) {
                    continue;
                }

                intersectSweptAabbAabb(x, y, z, static_cast<float>(transform.position.x - x), static_cast<float>(transform.position.y - y), static_cast<float>(transform.position.z - z), dx, dy, dz, contacts);
            }
        }
    }
}

void CharacterController::intersectSweptAabbAabb(int x, int y, int z, float px, float py, float pz, float dx, float dy,
                                                 float dz, std::vector<Contact> &contacts) {
    // https://www.gamedev.net/tutorials/programming/general-and-gameplay-programming/swept-aabb-collision-detection-and-response-r3084/
    constexpr float NEGATIVE_INFINITY = -std::numeric_limits<float>::infinity();
    constexpr float POSITIVE_INFINITY = std::numeric_limits<float>::infinity();

    float pxmax = px + PLAYER_WIDTH, pxmin = px - PLAYER_WIDTH, pymax = py + PLAYER_HEIGHT - PLAYER_EYE_HEIGHT, pymin = py - PLAYER_EYE_HEIGHT,
            pzmax = pz + PLAYER_WIDTH, pzmin = pz - PLAYER_WIDTH;
    float xInvEntry = dx > 0 ? -pxmax : 1 - pxmin, xInvExit = dx > 0 ? 1 - pxmin : -pxmax;
    bool xNotValid = dx == 0 || worldManager.load(x + (dx > 0 ? -1 : 1), y, z) != EMPTY_VOXEL;
    float xEntry = xNotValid ? NEGATIVE_INFINITY : xInvEntry / dx, xExit = xNotValid ? POSITIVE_INFINITY : xInvExit / dx;
    float yInvEntry = dy > 0 ? -pymax : 1 - pymin, yInvExit = dy > 0 ? 1 - pymin : -pymax;
    bool yNotValid = dy == 0 || worldManager.load(x, y + (dy > 0 ? -1 : 1), z) != EMPTY_VOXEL;
    float yEntry = yNotValid ? NEGATIVE_INFINITY : yInvEntry / dy, yExit = yNotValid ? POSITIVE_INFINITY : yInvExit / dy;
    float zInvEntry = dz > 0 ? -pzmax : 1 - pzmin, zInvExit = dz > 0 ? 1 - pzmin : -pzmax;
    bool zNotValid = dz == 0 || worldManager.load(x, y, z + (dz > 0 ? -1 : 1)) != EMPTY_VOXEL;
    float zEntry = zNotValid ? NEGATIVE_INFINITY : zInvEntry / dz, zExit = zNotValid ? POSITIVE_INFINITY : zInvExit / dz;
    float tEntry = std::max(std::max(xEntry, yEntry), zEntry), tExit = std::min(std::min(xExit, yExit), zExit);
    if (tEntry < -.5f || tEntry > tExit) {
        return;
    }
    Contact c = Contact(tEntry, x, y, z);
    if (xEntry == tEntry) {
        c.nx = dx > 0 ? -1 : 1;
    } else if (yEntry == tEntry) {
        c.ny = dy > 0 ? -1 : 1;
    } else {
        c.nz = dz > 0 ? -1 : 1;
    }
    contacts.push_back(c);
}

void CharacterController::collisionResponse(glm::vec3 v, std::vector<Contact> &contacts) {
    std::sort(contacts.begin(), contacts.end());

    int minX = INT_MIN, maxX = INT_MAX, minY = INT_MIN, maxY = INT_MAX, minZ = INT_MIN, maxZ = INT_MAX;
    float elapsedTime = 0;
    float dx = v.x, dy = v.y, dz = v.z;
    for (auto contact : contacts) {
        if (contact.x <= minX || contact.y <= minY || contact.z <= minZ || contact.x >= maxX || contact.y >= maxY || contact.z >= maxZ)
            continue;
        float t = contact.t - elapsedTime;
        transform.position += glm::vec3(dx * t, dy * t, dz * t);
        elapsedTime += t;
        if (contact.nx != 0) {
            minX = dx < 0 ? std::max(minX, contact.x) : minX;
            maxX = dx < 0 ? maxX : std::min(maxX, contact.x);
            v.x = 0.0f;
            dx = 0.0f;
        } else if (contact.ny != 0) {
            isGrounded = dy < 0;
            minY = dy < 0 ? std::max(minY, contact.y) : contact.y - (int) PLAYER_HEIGHT;
            maxY = dy < 0 ? contact.y + (int) std::ceil(PLAYER_HEIGHT) + 1 : std::min(maxY, contact.y);
            v.y = 0.0f;
            dy = 0.0f;
        } else if (contact.nz != 0) {
            minZ = dz < 0 ? std::max(minZ, contact.z) : minZ;
            maxZ = dz < 0 ? maxZ : std::min(maxZ, contact.z);
            v.z = 0.0f;
            dz = 0.0f;
        }
    }
    float trem = 1.0f - elapsedTime;
    transform.position += glm::vec3(dx * trem, dy * trem, dz * trem);
}
