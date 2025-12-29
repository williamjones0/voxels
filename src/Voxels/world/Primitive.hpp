#pragma once

#include <unordered_set>
#include <vector>

struct Edit {
    int cx;
    int cz;
    int x;
    int y;
    int z;
    int voxelType;
    int oldVoxelType;

    bool operator==(const Edit& other) const {
        return cx == other.cx &&
               cz == other.cz &&
               x == other.x &&
               y == other.y &&
               z == other.z &&
               voxelType == other.voxelType;
    }
};

struct EditHash {
    size_t operator()(const Edit& edit) const {
        return std::hash<int>()(edit.cx) ^
               std::hash<int>()(edit.cz) << 1 ^
               std::hash<int>()(edit.x) << 2 ^
               std::hash<int>()(edit.y) << 3 ^
               std::hash<int>()(edit.z) << 4 ^
               std::hash<int>()(edit.voxelType) << 5;
    }
};

struct Primitive {
    virtual ~Primitive() = default;

    [[nodiscard]] virtual std::vector<Edit> generateEdits() = 0;

    std::vector<Edit> edits;
    int voxelType{};
    glm::ivec3 origin{};
};

struct Cuboid : Primitive {
    Cuboid(const int voxelType, const glm::ivec3& origin, const glm::ivec3& start, const glm::ivec3& end) : start(start), end(end) {
        this->voxelType = voxelType;
        this->origin = origin;
    }

    glm::ivec3 start;
    glm::ivec3 end;

    std::vector<Edit> generateEdits() override {
        std::vector<Edit> edits;
        for (int x = start.x; x <= end.x; ++x) {
            for (int y = std::max(0, start.y); y <= std::min(ChunkHeight, end.y); ++y) {
                for (int z = start.z; z <= end.z; ++z) {
                    const int wx = origin.x + x;
                    const int wy = origin.y + y;
                    const int wz = origin.z + z;
                    edits.push_back(Edit{
                        .cx = wx >> ChunkSizeShift,
                        .cz = wz >> ChunkSizeShift,
                        .x = (wx % ChunkSize + ChunkSize) % ChunkSize,
                        .y = wy,
                        .z = (wz % ChunkSize + ChunkSize) % ChunkSize,
                        .voxelType = voxelType
                    });
                }
            }
        }

        return edits;
    }
};

struct Sphere : Primitive {
    explicit Sphere(const int voxelType, const glm::ivec3& origin, const int r) : radius(r) {
        this->voxelType = voxelType;
        this->origin = origin;
    }

    int radius;

    std::vector<Edit> generateEdits() override {
        std::vector<Edit> edits;

        // oy + y >= 0
        // y >= -oy

        // oy + y < ChunkHeight
        // y < ChunkHeight - oy

        for (int x = -radius; x <= radius; ++x) {
            for (int y = std::max(-origin.y, -radius); y <= std::min(ChunkHeight - origin.y, radius); ++y) {
                for (int z = -radius; z <= radius; ++z) {
                    if (x * x + y * y + z * z <= radius * radius) {
                        const int wx = origin.x + x;
                        const int wy = origin.y + y;
                        const int wz = origin.z + z;
                        edits.push_back(Edit{
                            .cx = wx >> ChunkSizeShift,
                            .cz = wz >> ChunkSizeShift,
                            .x = (wx % ChunkSize + ChunkSize) % ChunkSize,
                            .y = wy,
                            .z = (wz % ChunkSize + ChunkSize) % ChunkSize,
                            .voxelType = voxelType
                        });
                    }
                }
            }
        }

        return edits;
    }
};

struct Cylinder : Primitive {
    Cylinder(const int voxelType, const glm::ivec3& origin, const int r, const int h)
        : radius(r), height(h) {
        this->voxelType = voxelType;
        this->origin = origin;
    }

    int radius;
    int height;

    std::vector<Edit> generateEdits() override {
        std::vector<Edit> edits;

        for (int y = -height / 2; y <= height / 2; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                for (int z = -radius; z <= radius; ++z) {
                    if (x * x + z * z <= radius * radius) {
                        const int wx = origin.x + x;
                        const int wy = origin.y + y;
                        const int wz = origin.z + z;
                        edits.push_back(Edit{
                            .cx = wx >> ChunkSizeShift,
                            .cz = wz >> ChunkSizeShift,
                            .x = (wx % ChunkSize + ChunkSize) % ChunkSize,
                            .y = wy,
                            .z = (wz % ChunkSize + ChunkSize) % ChunkSize,
                            .voxelType = voxelType
                        });
                    }
                }
            }
        }

        return edits;
    }
};
