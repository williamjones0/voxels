#pragma once

#include <vector>
#include <cstdint>

#include "Chunk.hpp"

class Mesher {
public:
    static void meshChunk(Chunk &chunk, GenerationType generationType);

private:
    static inline int vertexAO(uint8_t side1, uint8_t side2, uint8_t corner);

    static bool inBounds(int x, int y, int z);

    static int dirToIndex(int i, int j, int k);

    static bool shouldMeshFace(int x, int y, int z, int i, int j, int k, std::vector<int> &voxels);
};

namespace VertexFormat {
    // Number of bits used per field. Must sum to less than or equal to 32 bits
    constexpr uint32_t X_BITS = CHUNK_SIZE_SHIFT + 1;
    constexpr uint32_t Y_BITS = CHUNK_HEIGHT_SHIFT + 1;
    constexpr uint32_t Z_BITS = CHUNK_SIZE_SHIFT + 1;
    constexpr uint32_t COLOUR_BITS = 3;
    constexpr uint32_t NORMAL_BITS = 3;
    constexpr uint32_t AO_BITS = 2;

    // Shift amounts for each field
    constexpr uint32_t X_SHIFT = 0;
    constexpr uint32_t Y_SHIFT = X_BITS;
    constexpr uint32_t Z_SHIFT = Y_SHIFT + Y_BITS;
    constexpr uint32_t COLOUR_SHIFT = Z_SHIFT + Z_BITS;
    constexpr uint32_t NORMAL_SHIFT = COLOUR_SHIFT + COLOUR_BITS;
    constexpr uint32_t AO_SHIFT = NORMAL_SHIFT + NORMAL_BITS;

    // Masks for each field
    constexpr uint32_t X_MASK = (1u << X_BITS) - 1;
    constexpr uint32_t Y_MASK = (1u << Y_BITS) - 1;
    constexpr uint32_t Z_MASK = (1u << Z_BITS) - 1;
    constexpr uint32_t COLOUR_MASK = (1u << COLOUR_BITS) - 1;
    constexpr uint32_t NORMAL_MASK = (1u << NORMAL_BITS) - 1;
    constexpr uint32_t AO_MASK = (1u << AO_BITS) - 1;
}
