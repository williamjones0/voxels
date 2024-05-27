#pragma once

#include <vector>
#include <util/PerlinNoise.hpp>
#include <util/Util.hpp>

const float EPSILON = 0.000001;

void generateTerrain(std::vector<int>& voxels, unsigned int seed, int worldSize, int heightScale) {
    int* chunk = new int[worldSize * worldSize];
    const siv::PerlinNoise::seed_type s = seed;
    const siv::PerlinNoise perlin{ s };

    for (int z = 0; z < worldSize; ++z) {
        for (int x = 0; x < worldSize; ++x) {
            const double noise = clamp(perlin.octave2D_01((x * 0.01), (z * 0.01), 4), 0.0, 1.0 - EPSILON);
            const int height = noise * heightScale;
            chunk[z * worldSize + x] = height;
            int index = getVoxelIndex(x, height, z, worldSize);
            voxels[index] = 1;
        }
    }

    for (int y = 0; y < heightScale - 1; ++y) {
        for (int z = 0; z < worldSize; ++z) {
            for (int x = 0; x < worldSize; ++x) {
                if (voxels[getVoxelIndex(x, y + 1, z, worldSize)] == 1) {
                    voxels[getVoxelIndex(x, y, z, worldSize)] = 2;
                }
            }
        }
    }
}
