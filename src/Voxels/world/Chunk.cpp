#include "Chunk.hpp"

#include "../util/PerlinNoise.hpp"
#include "../util/Util.hpp"

constexpr float Epsilon = 0.000001;

constexpr unsigned int seed = 123456u;
constexpr siv::PerlinNoise::seed_type s = seed;
const siv::PerlinNoise perlin{ s };

Chunk::Chunk(const int cx, const int cz)
  : cx(cx), cz(cz)
{}

void Chunk::store(const int x, const int y, const int z, const int v) {
    voxels[getVoxelIndex(x + 1, y + 1, z + 1, ChunkSize + 2)] = v;
    minY = std::min(minY, y);
    maxY = std::max(maxY, y + 2);
}

int Chunk::load(const int x, const int y, const int z) const {
    return voxels[getVoxelIndex(x + 1, y + 1, z + 1, ChunkSize + 2)];
}

void Chunk::init() {
    voxels = std::vector(VoxelsSize, 0);
}

void Chunk::generate(const GenerationType type, const Level& level) {
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
    for (int y = 0; y < ChunkHeight; ++y) {
        for (int z = -1; z < ChunkSize + 1; ++z) {
            for (int x = -1; x < ChunkSize + 1; ++x) {
                const int i = getVoxelIndex(x + 1, y + 1, z + 1, ChunkSize + 2);

                if (y == ChunkHeight / 2) {
                    voxels[i] = 1;
                } else if (y < ChunkHeight / 2) {
                    voxels[i] = 2;
                }
            }
        }
    }

    minY = 0;
    maxY = ChunkHeight / 2 + 2;
}

double terrainNoise(const int x, const int z) {
    return clamp(perlin.octave2D_01(x * 0.01, z * 0.01, 1), 0.0, 1.0 - Epsilon);
}

void Chunk::generateVoxels2D() {
    int heightMap[ChunkSize + 2][ChunkSize + 2];
    heightMap[0][0] = static_cast<int>(terrainNoise(cx * ChunkSize, cz * ChunkSize) * ChunkHeight);

    for (int z = -1; z < ChunkSize + 1; ++z) {
        for (int x = -1; x < ChunkSize + 1; ++x) {
            const int noise_x = cx * ChunkSize + x + 1;
            const int noise_z = cz * ChunkSize + z + 1;

            // Calculate adjacent noise values in the +x and +z directions
            const double noise10 = terrainNoise(noise_x + 1, noise_z);
            const double noise01 = terrainNoise(noise_x, noise_z + 1);

            if (x != ChunkSize) {
                heightMap[x + 2][z + 1] = static_cast<int>(noise10 * ChunkHeight);
            }
            if (z != ChunkSize) {
                heightMap[x + 1][z + 2] = static_cast<int>(noise01 * ChunkHeight);
            }

            int y = heightMap[x + 1][z + 1];
            y = std::min(std::max(0, y), ChunkHeight - 1);

            minY = std::min(y, minY);
            maxY = std::max(y, maxY);

            // Lowest visible height is the minimum of the current height and the adjacent heights
            int lowestVisibleHeight = y;
            if (x != -1) {
                lowestVisibleHeight = std::min(lowestVisibleHeight, heightMap[x][z + 1]);
            }
            if (x != ChunkSize) {
                lowestVisibleHeight = std::min(lowestVisibleHeight, heightMap[x + 2][z + 1]);
            }
            if (z != -1) {
                lowestVisibleHeight = std::min(lowestVisibleHeight, heightMap[x + 1][z]);
            }
            if (z != ChunkSize) {
                lowestVisibleHeight = std::min(lowestVisibleHeight, heightMap[x + 1][z + 2]);
            }

            for (int y0 = 0; y0 < y; ++y0) {
                const int i = getVoxelIndex(x + 1, y0, z + 1, ChunkSize + 2);

                int voxelType;
                if (y0 == y - 1) {
                    voxelType = 1;
                } else if (y0 < lowestVisibleHeight) {
                    voxelType = 2;
                } else {
                    voxelType = 2;
                }
                voxels[i] = voxelType;
            }
        }
    }

    minY = std::max(0, minY - 1);
    maxY = std::min(ChunkHeight, maxY + 1);
}

void Chunk::generateVoxels3D() {
    for (int y = -1; y < ChunkHeight + 1; ++y) {
        for (int z = -1; z < ChunkSize + 1; ++z) {
            for (int x = -1; x < ChunkSize + 1; ++x) {
                const auto noise_x = static_cast<float>(cx * ChunkSize + x + 1);
                const auto noise_y = static_cast<float>(y + 1);
                const auto noise_z = static_cast<float>(cz * ChunkSize + z + 1);

                if (const double noise = perlin.octave3D_01(noise_x * 0.01, noise_y * 0.01, noise_z * 0.01, 4); noise > 0.5) {
                    voxels[getVoxelIndex(x + 1, y + 1, z + 1, ChunkSize + 2)] = 1;
                    minY = std::min(y, minY);
                    maxY = std::max(y, maxY);
                }
            }
        }
    }

    minY = std::max(0, minY - 1);
    maxY = std::min(ChunkHeight, maxY + 2);
}

void Chunk::generateVoxels(const Level& level) {
    for (int y = 0; y < ChunkHeight; ++y) {
        for (int z = -1; z < ChunkSize + 1; ++z) {
            for (int x = -1; x < ChunkSize + 1; ++x) {
                const int gx = cx * ChunkSize + x;
                const int gz = cz * ChunkSize + z;

                if (gx < 0 || gz < 0 || gx >= level.maxX || gz >= level.maxZ || y >= level.maxY) {
                    continue; // Out of bounds
                }

                const int value = level.data[y * level.maxX * level.maxZ + gz * level.maxX + gx];

                voxels[getVoxelIndex(x + 1, y + 1, z + 1, ChunkSize + 2)] = value;

                if (value != EmptyVoxel) {
                    minY = std::min(y, minY);
                    maxY = std::max(y, maxY);
                }
            }
        }
    }

    maxY = std::min(ChunkHeight, maxY + 2);
}
