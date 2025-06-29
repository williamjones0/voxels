#include "Chunk.hpp"

#include <iostream>

#include "../util/PerlinNoise.hpp"
#include "../util/Util.hpp"

constexpr float EPSILON = 0.000001;

constexpr unsigned int seed = 123456u;
constexpr siv::PerlinNoise::seed_type s = seed;
const siv::PerlinNoise perlin{ s };

Chunk::Chunk(int cx, int cz)
  : cx(cx)
  , cz(cz)
  , minY(CHUNK_HEIGHT)
  , maxY(0)
  , neighbours(0)
  , VAO(0)
  , dataVBO(0)
  , vertices(0)
  , index(0)
  , numVertices(0)
  , firstIndex(-1)
{}

void Chunk::store(int x, int y, int z, char v) {
    voxels[getVoxelIndex(x + 1, y + 1, z + 1, CHUNK_SIZE + 2)] = v;
    minY = std::min(minY, y);
    maxY = std::max(maxY, y + 2);
}

int Chunk::load(int x, int y, int z) {
    return voxels[getVoxelIndex(x + 1, y + 1, z + 1, CHUNK_SIZE + 2)];
}

void Chunk::init() {
    voxels = std::vector<int>(VOXELS_SIZE, 0);
}

void Chunk::generate(GenerationType type, const Level& level) {
    switch (type) {
        case GenerationType::Flat:
            generateFlat();
            break;
        case GenerationType::Perlin2D:
            generateVoxels2D();
            break;
        case GenerationType::Perlin3D:
            generateVoxels3D();
            break;
        case GenerationType::LevelLoad:
            generateVoxels(level);
            break;
    }
}

void Chunk::generateFlat() {
    for (int y = 0; y < CHUNK_HEIGHT; ++y) {
        for (int z = -1; z < CHUNK_SIZE + 1; ++z) {
            for (int x = -1; x < CHUNK_SIZE + 1; ++x) {
                int index = getVoxelIndex(x + 1, y + 1, z + 1, CHUNK_SIZE + 2);

                if (y == CHUNK_HEIGHT / 2) {
                    voxels[index] = 1;
                } else if (y < CHUNK_HEIGHT / 2) {
                    voxels[index] = 2;
                }
            }
        }
    }

    minY = 0;
    maxY = CHUNK_HEIGHT / 2 + 2;
}

double terrainNoise(int x, int z) {
    return clamp(perlin.octave2D_01(x * 0.01, z * 0.01, 1), 0.0, 1.0 - EPSILON);
}

void Chunk::generateVoxels2D() {
    int heightMap[CHUNK_SIZE + 2][CHUNK_SIZE + 2];
    heightMap[0][0] = static_cast<int>(terrainNoise(cx * CHUNK_SIZE, cz * CHUNK_SIZE) * CHUNK_HEIGHT);

    for (int z = -1; z < CHUNK_SIZE + 1; ++z) {
        for (int x = -1; x < CHUNK_SIZE + 1; ++x) {
            int noise_x = cx * CHUNK_SIZE + x + 1;
            int noise_z = cz * CHUNK_SIZE + z + 1;

            // Calculate adjacent noise values in the +x and +z directions
            const double noise10 = terrainNoise(noise_x + 1, noise_z);
            const double noise01 = terrainNoise(noise_x, noise_z + 1);

            if (x != CHUNK_SIZE) {
                heightMap[x + 2][z + 1] = static_cast<int>(noise10 * CHUNK_HEIGHT);
            }
            if (z != CHUNK_SIZE) {
                heightMap[x + 1][z + 2] = static_cast<int>(noise01 * CHUNK_HEIGHT);
            }

            int y = heightMap[x + 1][z + 1];
            y = std::min(std::max(0, y), CHUNK_HEIGHT - 1);

            minY = std::min(y, minY);
            maxY = std::max(y, maxY);

            // Lowest visible height is the minimum of the current height and the adjacent heights
            int lowestVisibleHeight = y;
            if (x != -1) {
                lowestVisibleHeight = std::min(lowestVisibleHeight, heightMap[x][z + 1]);
            }
            if (x != CHUNK_SIZE) {
                lowestVisibleHeight = std::min(lowestVisibleHeight, heightMap[x + 2][z + 1]);
            }
            if (z != -1) {
                lowestVisibleHeight = std::min(lowestVisibleHeight, heightMap[x + 1][z]);
            }
            if (z != CHUNK_SIZE) {
                lowestVisibleHeight = std::min(lowestVisibleHeight, heightMap[x + 1][z + 2]);
            }

            for (int y0 = 0; y0 < y; ++y0) {
                int index = getVoxelIndex(x + 1, y0, z + 1, CHUNK_SIZE + 2);

                int voxelType;
                if (y0 == y - 1) {
                    voxelType = 1;
                } else if (y0 < lowestVisibleHeight) {
                    voxelType = 2;
                } else {
                    voxelType = 2;
                }
                voxels[index] = voxelType;
            }
        }
    }

    minY = std::max(0, minY - 1);
    maxY = std::min(CHUNK_HEIGHT, maxY + 1);
}

void Chunk::generateVoxels3D() {
    for (int y = -1; y < CHUNK_HEIGHT + 1; ++y) {
        for (int z = -1; z < CHUNK_SIZE + 1; ++z) {
            for (int x = -1; x < CHUNK_SIZE + 1; ++x) {
                auto noise_x = static_cast<float>(cx * CHUNK_SIZE + x + 1);
                auto noise_y = static_cast<float>(y + 1);
                auto noise_z = static_cast<float>(cz * CHUNK_SIZE + z + 1);

                double noise = perlin.octave3D_01(noise_x * 0.01, noise_y * 0.01, noise_z * 0.01, 4);

                if (noise > 0.5) {
                    voxels[getVoxelIndex(x + 1, y + 1, z + 1, CHUNK_SIZE + 2)] = 1;
                    minY = std::min(y, minY);
                    maxY = std::max(y, maxY);
                }
            }
        }
    }

    minY = std::max(0, minY - 1);
    maxY = std::min(CHUNK_HEIGHT, maxY + 2);
}

void Chunk::generateVoxels(const Level& level) {
    for (int y = 0; y < CHUNK_HEIGHT; ++y) {
        for (int z = -1; z < CHUNK_SIZE + 1; ++z) {
            for (int x = -1; x < CHUNK_SIZE + 1; ++x) {
                int gx = cx * CHUNK_SIZE + x;
                int gz = cz * CHUNK_SIZE + z;

                if (gx < 0 || gz < 0 || gx >= level.maxX || gz >= level.maxZ || y >= level.maxY) {
                    continue; // Out of bounds
                }

                int value = level.data[y * level.maxX * level.maxZ + gz * level.maxX + gx];

                voxels[getVoxelIndex(x + 1, y + 1, z + 1, CHUNK_SIZE + 2)] = value;

                if (value != EMPTY_VOXEL) {
                    minY = std::min(y, minY);
                    maxY = std::max(y, maxY);
                }
            }
        }
    }

    maxY = std::min(CHUNK_HEIGHT, maxY + 2);
}
