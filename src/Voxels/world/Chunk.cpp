#include "Chunk.hpp"

#include "../util/PerlinNoise.hpp"

constexpr float Epsilon = 0.000001;

constexpr unsigned int seed = 123456u;
constexpr siv::PerlinNoise::seed_type s = seed;
const siv::PerlinNoise perlin{ s };

Chunk::Chunk(const int cx, const int cz)
  : cx(cx), cz(cz)
{}

void Chunk::store(const int x, const int y, const int z, const int v) {
    storeInto(voxels, minY, maxY, x, y, z, v);
}

int Chunk::load(const int x, const int y, const int z) const {
    return voxels[getVoxelIndex(x + 1, y, z + 1)];
}

void Chunk::storeInto(std::vector<int>& field, int& minY, int& maxY, const int x, const int y, const int z, const int v) {
    field[getVoxelIndex(x + 1, y, z + 1)] = v;
    minY = std::min(minY, y);
    maxY = std::max(maxY, y + 2);
}

auto Chunk::generateFlat() -> GenerationResult {
    GenerationResult result;

    for (int y = 0; y < ChunkHeight; ++y) {
        for (int z = -1; z < ChunkSize + 1; ++z) {
            for (int x = -1; x < ChunkSize + 1; ++x) {
                if (y == ChunkHeight / 2) {
                    storeInto(result.voxelField, result.minY, result.maxY, x, y, z, 1);
                } else if (y < ChunkHeight / 2) {
                    storeInto(result.voxelField, result.minY, result.maxY, x, y, z, 2);
                }
            }
        }
    }

    result.minY = 0;
    result.maxY = ChunkHeight / 2 + 2;

    return result;
}

double terrainNoise(const int x, const int z) {
    auto clamp = [](auto v, auto low, auto high) {
        return std::max(low, std::min(v, high));
    };

    return clamp(perlin.octave2D_01(x * 0.01, z * 0.01, 1), 0.0, 1.0 - Epsilon);
}

auto Chunk::generateVoxels2D(const int cx, const int cz) -> GenerationResult {
    GenerationResult result;

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

            result.minY = std::min(y, result.minY);
            result.maxY = std::max(y, result.maxY);

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
                int voxelType;
                if (y0 == y - 1) {
                    voxelType = 1;
                } else if (y0 < lowestVisibleHeight) {
                    voxelType = 2;
                } else {
                    voxelType = 2;
                }
                storeInto(result.voxelField, result.minY, result.maxY, x, y0, z, voxelType);
            }
        }
    }

    result.minY = std::max(0, result.minY - 1);
    result.maxY = std::min(ChunkHeight, result.maxY + 1);

    return result;
}

auto Chunk::generateVoxels3D(const int cx, const int cz) -> GenerationResult {
    GenerationResult result;

    for (int y = -1; y < ChunkHeight + 1; ++y) {
        for (int z = -1; z < ChunkSize + 1; ++z) {
            for (int x = -1; x < ChunkSize + 1; ++x) {
                const auto noise_x = static_cast<float>(cx * ChunkSize + x + 1);
                const auto noise_y = static_cast<float>(y + 1);
                const auto noise_z = static_cast<float>(cz * ChunkSize + z + 1);

                if (const double noise = perlin.octave3D_01(noise_x * 0.01, noise_y * 0.01, noise_z * 0.01, 4); noise > 0.5) {
                    storeInto(result.voxelField, result.minY, result.maxY, x, y, z, 1);
                    result.minY = std::min(y, result.minY);
                    result.maxY = std::max(y, result.maxY);
                }
            }
        }
    }

    result.minY = std::max(0, result.minY - 1);
    result.maxY = std::min(ChunkHeight, result.maxY + 2);

    return result;
}

size_t Chunk::getVoxelIndex(const size_t x, const size_t y, const size_t z) {
    return y * (ChunkSize + 2) * (ChunkSize + 2) + z * (ChunkSize + 2) + x;
}
