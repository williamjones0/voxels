#pragma once

#include "Chunk.hpp"

class RunMesher {
public:
    RunMesher(Chunk* chunk, GenerationType generationType)
        : chunk(chunk), generationType(generationType) {}

    struct MeshResult {
        Chunk* chunk;
        std::vector<uint32_t> vertices;
    };

    MeshResult meshChunk();

private:
    void createRun(int voxel, size_t i, size_t j, size_t k, size_t access);
    bool shouldMeshFace(int i, int j, int k) const;
    bool differentBlock(size_t access, int voxel) const;
    void appendQuad(
        const glm::ivec3& tl,
        const glm::ivec3& tr,
        const glm::ivec3& bl,
        const glm::ivec3& br,
        int normal,
        int voxel
    );

    static uint32_t combinePosition(const glm::ivec3& pos, uint32_t shared);

    Chunk* chunk;
    GenerationType generationType;

    std::vector<bool> visitedXN = std::vector(VoxelsSize, false);
    std::vector<bool> visitedXP = std::vector(VoxelsSize, false);
    std::vector<bool> visitedZN = std::vector(VoxelsSize, false);
    std::vector<bool> visitedZP = std::vector(VoxelsSize, false);
    std::vector<bool> visitedYN = std::vector(VoxelsSize, false);
    std::vector<bool> visitedYP = std::vector(VoxelsSize, false);

    std::vector<uint32_t> vertices;
};
