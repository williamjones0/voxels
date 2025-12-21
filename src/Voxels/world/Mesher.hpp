#pragma once

#include <vector>
#include <cstdint>

#include "Chunk.hpp"

class Mesher {
public:
    struct MeshResult {
        Chunk* chunk;
        std::vector<uint32_t> vertices;
    };

    [[nodiscard]] static MeshResult meshChunk(Chunk* chunk);

private:
    static inline int vertexAO(uint8_t side1, uint8_t side2, uint8_t corner);

    static bool inBounds(int x, int y, int z);

    static int dirToIndex(int i, int j, int k);

    static bool shouldMeshFace(int x, int y, int z, int i, int j, int k, const std::vector<int>& voxels);
};
