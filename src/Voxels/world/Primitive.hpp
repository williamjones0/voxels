#pragma once

#include <unordered_set>
#include <vector>

struct Edit {
    int voxelType;
    int oldVoxelType;
};

struct IVec3Hash {
    std::size_t operator()(const glm::ivec3& v) const noexcept {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1) ^ (std::hash<int>()(v.z) << 2);
    }
};

struct Primitive {
    virtual ~Primitive() = default;

    using EditMap = std::unordered_map<glm::ivec3, std::optional<Edit>, IVec3Hash>;
    [[nodiscard]] virtual EditMap generateEdits() = 0;
    virtual bool isPosInside(const glm::ivec3& pos) const = 0;

    EditMap edits;  // global pos -> optional Edit
    std::unordered_map<glm::ivec3, int, IVec3Hash> userEdits;  // local pos -> voxelType
    int voxelType{};
    glm::ivec3 origin{};
};

struct Cuboid : Primitive {
    Cuboid(const int voxelType, const glm::ivec3& start, const glm::ivec3& end) {
        this->start = glm::min(start, end);
        this->end = glm::max(start, end);
        this->voxelType = voxelType;
    }

    glm::ivec3 start;
    glm::ivec3 end;

    EditMap generateEdits() override {
        EditMap editMap;
        for (int x = start.x; x <= end.x; ++x) {
            for (int y = std::max(0, start.y); y <= std::min(ChunkHeight, end.y); ++y) {
                for (int z = start.z; z <= end.z; ++z) {
                    const int wx = origin.x + x;
                    const int wy = origin.y + y;
                    const int wz = origin.z + z;

                    const auto it = userEdits.find(glm::ivec3(x, y, z));
                    const int voxelType = it != userEdits.end() ? it->second : this->voxelType;
                    if (voxelType == 0) {
                        // There still needs to be an entry for this position, but there is just no edit there
                        editMap[{wx, wy, wz}] = std::nullopt;
                        continue;
                    }

                    editMap[{wx, wy, wz}] = {voxelType, 0};
                }
            }
        }

        return editMap;
    }

    bool isPosInside(const glm::ivec3 &pos) const override {
        return pos.x >= origin.x + start.x && pos.x <= origin.x + end.x &&
               pos.y >= origin.y + start.y && pos.y <= origin.y + end.y &&
               pos.z >= origin.z + start.z && pos.z <= origin.z + end.z;
    }
};

struct Sphere : Primitive {
    explicit Sphere(const int voxelType, const glm::ivec3& origin, const int r) : radius(r) {
        this->voxelType = voxelType;
        this->origin = origin;
    }

    int radius;

    EditMap generateEdits() override {
        EditMap editMap;

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

                        const auto it = userEdits.find(glm::ivec3(x, y, z));
                        const int voxelType = it != userEdits.end() ? it->second : this->voxelType;
                        if (voxelType == 0) {
                            editMap[{wx, wy, wz}] = std::nullopt;
                            continue;
                        }

                        editMap[{wx, wy, wz}] = Edit{voxelType, 0};
                    }
                }
            }
        }

        return editMap;
    }

    bool isPosInside(const glm::ivec3 &pos) const override {
        const glm::ivec3 relPos = pos - origin;
        return relPos.x * relPos.x + relPos.y * relPos.y + relPos.z * relPos.z <= radius * radius;
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

    EditMap generateEdits() override {
        EditMap editMap;

        for (int y = -height / 2; y <= height / 2; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                for (int z = -radius; z <= radius; ++z) {
                    if (x * x + z * z <= radius * radius) {
                        const int wx = origin.x + x;
                        const int wy = origin.y + y;
                        const int wz = origin.z + z;

                        const auto it = userEdits.find(glm::ivec3(x, y, z));
                        const int voxelType = it != userEdits.end() ? it->second : this->voxelType;
                        if (voxelType == 0) {
                            editMap[{wx, wy, wz}] = std::nullopt;
                            continue;
                        }

                        editMap[{wx, wy, wz}] = Edit{voxelType, 0};
                    }
                }
            }
        }

        return editMap;
    }

    bool isPosInside(const glm::ivec3 &pos) const override {
        const glm::ivec3 relPos = pos - origin;
        return relPos.y >= -height / 2 && relPos.y <= height / 2 &&
               relPos.x * relPos.x + relPos.z * relPos.z <= radius * radius;
    }
};

struct Plane : Primitive {
    enum class Axis { X, Y, Z };

    Plane(const int voxelType, const glm::ivec3& start, const glm::ivec3& end, const Axis axis) : axis(axis) {
        this->start = glm::min(start, end);
        this->end = glm::max(start, end);
        this->voxelType = voxelType;
    }

    glm::ivec3 start;
    glm::ivec3 end;
    Axis axis;

    EditMap generateEdits() override {
        EditMap editMap;

        if (axis == Axis::Y) {
            // Plane parallel to Y axis - trace line in XZ plane, extrude along Y
            int dx = end.x - start.x;
            int dz = end.z - start.z;
            int steps = std::max(std::abs(dx), std::abs(dz));

            if (steps == 0) steps = 1;

            // Bresenham line tracing
            for (int step = 0; step <= steps; ++step) {
                float t = static_cast<float>(step) / steps;
                int x = static_cast<int>(std::round(start.x + t * dx));
                int z = static_cast<int>(std::round(start.z + t * dz));

                // For each point on the line, create voxels along the Y axis
                for (int y = std::max(0, start.y); y <= std::min(ChunkHeight - 1, end.y); ++y) {
                    const int wx = origin.x + x;
                    const int wy = origin.y + y;
                    const int wz = origin.z + z;

                    const auto it = userEdits.find(glm::ivec3(x, y, z));
                    const int voxelType = it != userEdits.end() ? it->second : this->voxelType;
                    if (voxelType == 0) {
                        editMap[{wx, wy, wz}] = std::nullopt;
                        continue;
                    }

                    editMap[{wx, wy, wz}] = Edit{voxelType, 0};
                }
            }
        } else if (axis == Axis::X) {
            // Plane parallel to X axis - trace line in YZ plane, extrude along X
            int dy = end.y - start.y;
            int dz = end.z - start.z;
            int steps = std::max(std::abs(dy), std::abs(dz));

            if (steps == 0) steps = 1;

            for (int step = 0; step <= steps; ++step) {
                float t = static_cast<float>(step) / steps;
                int y = static_cast<int>(std::round(start.y + t * dy));
                int z = static_cast<int>(std::round(start.z + t * dz));

                if (y < 0 || y >= ChunkHeight) continue;

                // For each point on the line, create voxels along the X axis
                for (int x = start.x; x <= end.x; ++x) {
                    const int wx = origin.x + x;
                    const int wy = origin.y + y;
                    const int wz = origin.z + z;

                    const auto it = userEdits.find(glm::ivec3(x, y, z));
                    const int voxelType = it != userEdits.end() ? it->second : this->voxelType;
                    if (voxelType == 0) {
                        editMap[{wx, wy, wz}] = std::nullopt;
                        continue;
                    }

                    editMap[{wx, wy, wz}] = Edit{voxelType, 0};
                }
            }
        } else { // Axis::Z
            // Plane parallel to Z axis - trace line in XY plane, extrude along Z
            int dx = end.x - start.x;
            int dy = end.y - start.y;
            int steps = std::max(std::abs(dx), std::abs(dy));

            if (steps == 0) steps = 1;

            for (int step = 0; step <= steps; ++step) {
                float t = static_cast<float>(step) / steps;
                int x = static_cast<int>(std::round(start.x + t * dx));
                int y = static_cast<int>(std::round(start.y + t * dy));

                if (y < 0 || y >= ChunkHeight) continue;

                // For each point on the line, create voxels along the Z axis
                for (int z = start.z; z <= start.z; ++z) {
                    const int wx = origin.x + x;
                    const int wy = origin.y + y;
                    const int wz = origin.z + z;

                    const auto it = userEdits.find(glm::ivec3(x, y, z));
                    const int voxelType = it != userEdits.end() ? it->second : this->voxelType;
                    if (voxelType == 0) {
                        editMap[{wx, wy, wz}] = std::nullopt;
                        continue;
                    }

                    editMap[{wx, wy, wz}] = Edit{voxelType, 0};
                }
            }
        }

        return editMap;
    }

    bool isPosInside(const glm::ivec3 &pos) const override {
        if (axis == Axis::Y) {
            return pos.x >= origin.x + start.x && pos.x <= origin.x + end.x &&
                   pos.y >= origin.y + start.y && pos.y <= origin.y + end.y &&
                   pos.z >= origin.z + start.z && pos.z <= origin.z + end.z;
        } else if (axis == Axis::X) {
            return pos.x >= origin.x + start.x && pos.x <= origin.x + end.x &&
                   pos.y >= origin.y + start.y && pos.y <= origin.y + end.y &&
                   pos.z >= origin.z + start.z && pos.z <= origin.z + end.z;
        } else { // Axis::Z
            return pos.x >= origin.x + start.x && pos.x <= origin.x + end.x &&
                   pos.y >= origin.y + start.y && pos.y <= origin.y + end.y &&
                   pos.z >= origin.z + start.z && pos.z <= origin.z + end.z;
        }
    }
};
