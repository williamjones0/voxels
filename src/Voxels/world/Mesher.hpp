#pragma once

#include <vector>
#include <cstdint>

#include "Chunk.hpp"
#include "WorldMesh.hpp"

class Mesher {
public:
    static void meshChunk(Chunk *chunk, int worldSize, WorldMesh &worldMesh, std::vector<uint32_t> &vertices, bool reMesh = false);

    static long long int totalMesherTime;

private:
    static inline int vertexAO(uint8_t side1, uint8_t side2, uint8_t corner);

    static bool inBounds(int x, int y, int z, int size, int height);

    static int dirToIndex(int i, int j, int k);

    static bool shouldMeshFace(int x, int y, int z, int i, int j, int k, int worldSize, std::vector<int> &voxels);

    static long long int presenceTime;
    static long long int voxelAoTime;
    static long long int aoPushTime;
    static long long int addVertexTime;
};

namespace {
    constexpr int cube_vertices[] = {
            // Front
            0, 0, 0,
            1, 0, 0,
            1, 1, 0,
            1, 1, 0,
            0, 1, 0,
            0, 0, 0,

            // Back
            0, 0, 1,
            1, 0, 1,
            1, 1, 1,
            1, 1, 1,
            0, 1, 1,
            0, 0, 1,

            // Left
            0, 1, 1,
            0, 1, 0,
            0, 0, 0,
            0, 0, 0,
            0, 0, 1,
            0, 1, 1,

            // Right
            1, 1, 1,
            1, 1, 0,
            1, 0, 0,
            1, 0, 0,
            1, 0, 1,
            1, 1, 1,

            // Bottom
            0, 0, 0,
            1, 0, 0,
            1, 0, 1,
            1, 0, 1,
            0, 0, 1,
            0, 0, 0,

            // Top
            0, 1, 0,
            1, 1, 0,
            1, 1, 1,
            1, 1, 1,
            0, 1, 1,
            0, 1, 0
    };

    constexpr int flipped_cube_vertices[] = {
            // Front
            0, 0, 0,
            1, 0, 0,
            0, 1, 0,
            0, 1, 0,
            1, 0, 0,
            1, 1, 0,

            // Back
            0, 0, 1,
            1, 0, 1,
            0, 1, 1,
            0, 1, 1,
            1, 0, 1,
            1, 1, 1,

            // Left
            0, 1, 1,
            0, 1, 0,
            0, 0, 1,
            0, 0, 1,
            0, 1, 0,
            0, 0, 0,

            // Right
            1, 1, 1,
            1, 1, 0,
            1, 0, 1,
            1, 0, 1,
            1, 1, 0,
            1, 0, 0,

            // Bottom
            0, 0, 0,
            1, 0, 0,
            0, 0, 1,
            0, 0, 1,
            1, 0, 0,
            1, 0, 1,

            // Top
            0, 1, 0,
            1, 1, 0,
            0, 1, 1,
            0, 1, 1,
            1, 1, 0,
            1, 1, 1,
    };
}
