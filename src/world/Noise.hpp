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

    for (int z = 0; z < worldSize; ++z) {
        for (int x = 0; x < worldSize; ++x) {
            int height = chunk[z * worldSize + x];
            int height_diffs[] = { 0, 0, 0, 0 };  // -x, +x, -z, +z
            if (inBounds(x - 1, z, worldSize)) {
                height_diffs[0] = chunk[z * worldSize + x] - chunk[z * worldSize + x - 1];
            }
            if (inBounds(x + 1, z, worldSize)) {
                height_diffs[1] = chunk[z * worldSize + x] - chunk[z * worldSize + x + 1];
            }
            if (inBounds(x, z - 1, worldSize)) {
                height_diffs[2] = chunk[z * worldSize + x] - chunk[(z - 1) * worldSize + x];
            }
            if (inBounds(x, z + 1, worldSize)) {
                height_diffs[3] = chunk[z * worldSize + x] - chunk[(z + 1) * worldSize + x];
            }

            int max_diff = *std::max_element(std::begin(height_diffs), std::end(height_diffs));
            if (max_diff > 1) {
                for (int i = 1; i < max_diff; ++i) {
                    if (inBounds(x, height - i, z, worldSize, heightScale)) {
                        voxels[getVoxelIndex(x, height - i, z, worldSize)] = 2;
                    }
                }
            }
        }
    }
}
