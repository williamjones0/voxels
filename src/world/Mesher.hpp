#pragma once

#include <vector>
#include <cstdint>

static inline int vertexAO(uint8_t side1, uint8_t side2, uint8_t corner);

bool inBounds(int x, int y, int z, int size, int height);

int dirToIndex(int i, int j, int k);

uint64_t createVertex(int x, int y, int z, int colour, int normal, int ao);

void mesh(
    std::vector<int>& world,
    std::vector<float>& world_colours,
    std::vector<int>& world_normals,
    std::vector<int>& world_ao
);

void mesh(
    std::vector<int>& world,
    std::vector<float>& world_colours,
    std::vector<int>& world_normals,
    std::vector<int>& world_ao
);

void meshVoxels(
    std::vector<int>& voxels,
    std::vector<int>& world,
    std::vector<float>& world_colours,
    std::vector<int>& world_normals,
    std::vector<int>& world_ao,
    std::vector<uint64_t>& world_data,
    int worldSize,
    int heightScale
);
