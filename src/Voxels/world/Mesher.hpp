#pragma once

#include <vector>
#include <cstdint>

#include "Chunk.hpp"

class Mesher {
public:
    static void meshChunk(Chunk* chunk, GenerationType generationType);

private:
    static inline int vertexAO(uint8_t side1, uint8_t side2, uint8_t corner);

    static bool inBounds(int x, int y, int z);

    static int dirToIndex(int i, int j, int k);

    static bool shouldMeshFace(int x, int y, int z, int i, int j, int k, const std::vector<int>& voxels);
};

namespace VertexFormat {
    // Number of bits used per field. Must sum to less than or equal to 32 bits
    constexpr uint32_t XBits = ChunkSizeShift + 1;
    constexpr uint32_t YBits = ChunkHeightShift + 1;
    constexpr uint32_t ZBits = ChunkSizeShift + 1;
    constexpr uint32_t ColourBits = 3;
    constexpr uint32_t NormalBits = 3;
    constexpr uint32_t AOBits = 2;

    // Shift amounts for each field
    constexpr uint32_t XShift = 0;
    constexpr uint32_t YShift = XBits;
    constexpr uint32_t ZShift = YShift + YBits;
    constexpr uint32_t ColourShift = ZShift + ZBits;
    constexpr uint32_t NormalShift = ColourShift + ColourBits;
    constexpr uint32_t AOShift = NormalShift + NormalBits;

    // Masks for each field
    constexpr uint32_t XMask = (1u << XBits) - 1;
    constexpr uint32_t YMask = (1u << YBits) - 1;
    constexpr uint32_t ZMask = (1u << ZBits) - 1;
    constexpr uint32_t ColourMask = (1u << ColourBits) - 1;
    constexpr uint32_t NormalMask = (1u << NormalBits) - 1;
    constexpr uint32_t AOMask = (1u << AOBits) - 1;
}
