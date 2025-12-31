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

struct Plane : Primitive {
    enum class Axis { X, Y, Z };

    Plane(const int voxelType, const glm::ivec3& origin, const glm::ivec3& start, const glm::ivec3& end, const Axis axis)
        : start(start), end(end), axis(axis) {
        this->voxelType = voxelType;
        this->origin = origin;
    }

    glm::ivec3 start;
    glm::ivec3 end;
    Axis axis;

    std::vector<Edit> generateEdits() override {
        std::vector<Edit> edits;
        std::unordered_set<Edit, EditHash> uniqueEdits;

        if (axis == Axis::Y) {
            // Plane parallel to Y axis - trace line in XZ plane, extrude along Y
            int dx = end.x - start.x;
            int dz = end.z - start.z;
            int steps = std::max(std::abs(dx), std::abs(dz));

            if (steps == 0) steps = 1;

            int minY = std::min(start.y, end.y);
            int maxY = std::max(start.y, end.y);

            // Bresenham line tracing
            for (int step = 0; step <= steps; ++step) {
                float t = static_cast<float>(step) / steps;
                int x = static_cast<int>(std::round(start.x + t * dx));
                int z = static_cast<int>(std::round(start.z + t * dz));

                // For each point on the line, create voxels along the Y axis
                for (int y = std::max(0, minY); y <= std::min(ChunkHeight - 1, maxY); ++y) {
                    const int wx = origin.x + x;
                    const int wy = origin.y + y;
                    const int wz = origin.z + z;

                    Edit edit{
                        .cx = wx >> ChunkSizeShift,
                        .cz = wz >> ChunkSizeShift,
                        .x = (wx % ChunkSize + ChunkSize) % ChunkSize,
                        .y = wy,
                        .z = (wz % ChunkSize + ChunkSize) % ChunkSize,
                        .voxelType = voxelType
                    };

                    if (uniqueEdits.insert(edit).second) {
                        edits.push_back(edit);
                    }
                }
            }
        } else if (axis == Axis::X) {
            // Plane parallel to X axis - trace line in YZ plane, extrude along X
            int dy = end.y - start.y;
            int dz = end.z - start.z;
            int steps = std::max(std::abs(dy), std::abs(dz));

            if (steps == 0) steps = 1;

            int minX = std::min(start.x, end.x);
            int maxX = std::max(start.x, end.x);

            for (int step = 0; step <= steps; ++step) {
                float t = static_cast<float>(step) / steps;
                int y = static_cast<int>(std::round(start.y + t * dy));
                int z = static_cast<int>(std::round(start.z + t * dz));

                if (y < 0 || y >= ChunkHeight) continue;

                // For each point on the line, create voxels along the X axis
                for (int x = minX; x <= maxX; ++x) {
                    const int wx = origin.x + x;
                    const int wy = origin.y + y;
                    const int wz = origin.z + z;

                    Edit edit{
                        .cx = wx >> ChunkSizeShift,
                        .cz = wz >> ChunkSizeShift,
                        .x = (wx % ChunkSize + ChunkSize) % ChunkSize,
                        .y = wy,
                        .z = (wz % ChunkSize + ChunkSize) % ChunkSize,
                        .voxelType = voxelType
                    };

                    if (uniqueEdits.insert(edit).second) {
                        edits.push_back(edit);
                    }
                }
            }
        } else { // Axis::Z
            // Plane parallel to Z axis - trace line in XY plane, extrude along Z
            int dx = end.x - start.x;
            int dy = end.y - start.y;
            int steps = std::max(std::abs(dx), std::abs(dy));

            if (steps == 0) steps = 1;

            int minZ = std::min(start.z, end.z);
            int maxZ = std::max(start.z, end.z);

            for (int step = 0; step <= steps; ++step) {
                float t = static_cast<float>(step) / steps;
                int x = static_cast<int>(std::round(start.x + t * dx));
                int y = static_cast<int>(std::round(start.y + t * dy));

                if (y < 0 || y >= ChunkHeight) continue;

                // For each point on the line, create voxels along the Z axis
                for (int z = minZ; z <= maxZ; ++z) {
                    const int wx = origin.x + x;
                    const int wy = origin.y + y;
                    const int wz = origin.z + z;

                    Edit edit{
                        .cx = wx >> ChunkSizeShift,
                        .cz = wz >> ChunkSizeShift,
                        .x = (wx % ChunkSize + ChunkSize) % ChunkSize,
                        .y = wy,
                        .z = (wz % ChunkSize + ChunkSize) % ChunkSize,
                        .voxelType = voxelType
                    };

                    if (uniqueEdits.insert(edit).second) {
                        edits.push_back(edit);
                    }
                }
            }
        }

        return edits;
    }
};
