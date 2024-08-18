#include "Chunk.hpp"

#include <glad/glad.h>
#include <glm/ext/matrix_transform.hpp>

#include <iostream>
#include <chrono>
#include <fstream>
#include <sstream>

#include "Mesher.hpp"
#include "../util/PerlinNoise.hpp"
#include "../util/Util.hpp"
#include "../util/Flags.h"

#define EPSILON 0.000001

Chunk::Chunk(void) : cx(0), cz(0), minY(CHUNK_HEIGHT), maxY(0), VAO(0), dataVBO(0), numVertices(0), firstIndex(0), model(glm::mat4(1.0f)), voxels((CHUNK_SIZE + 2) * (CHUNK_SIZE + 2) * (CHUNK_HEIGHT + 2)) {}

Chunk::Chunk(int cx, int cz) : cx(cx), cz(cz), minY(CHUNK_HEIGHT), maxY(0), VAO(0), dataVBO(0), numVertices(0), firstIndex(0), model(glm::mat4(1.0f)), voxels((CHUNK_SIZE + 2) * (CHUNK_SIZE + 2) * (CHUNK_HEIGHT + 2)) {}

Chunk::Chunk(const Chunk &other) : cx(other.cx), cz(other.cz), minY(other.minY), maxY(other.maxY), VAO(other.VAO), dataVBO(other.dataVBO), numVertices(other.numVertices), firstIndex(other.firstIndex), model(other.model), voxels(other.voxels) {}

Chunk &Chunk::operator=(const Chunk &other) {
    if (this != &other) {
        cx = other.cx;
        cz = other.cz;
        minY = other.minY;
        maxY = other.maxY;
        VAO = other.VAO;
        dataVBO = other.dataVBO;
        numVertices = other.numVertices;
        firstIndex = other.firstIndex;
        model = other.model;
        voxels = other.voxels;
    }
    return *this;
}

Chunk::Chunk(Chunk &&other) noexcept : cx(other.cx), cz(other.cz), minY(other.minY), maxY(other.maxY), VAO(other.VAO), dataVBO(other.dataVBO), numVertices(other.numVertices), firstIndex(other.firstIndex), model(std::move(other.model)), voxels(std::move(other.voxels)) {
    other.VAO = 0;
    other.dataVBO = 0;
    other.numVertices = 0;
    other.firstIndex = 0;
}

Chunk &Chunk::operator=(Chunk &&other) noexcept {
    if (this != &other) {
        cx = other.cx;
        cz = other.cz;
        minY = other.minY;
        maxY = other.maxY;
        VAO = other.VAO;
        dataVBO = other.dataVBO;
        numVertices = other.numVertices;
        firstIndex = other.firstIndex;
        model = std::move(other.model);
        voxels = std::move(other.voxels);

        other.VAO = 0;
        other.dataVBO = 0;
        other.numVertices = 0;
        other.firstIndex = 0;
    }
    return *this;
}

void Chunk::store(int x, int y, int z, char v) {
    voxels[getVoxelIndex(x + 1, y + 1, z + 1, CHUNK_SIZE + 2)] = v;
}

int Chunk::load(int x, int y, int z) {
    return voxels[getVoxelIndex(x + 1, y + 1, z + 1, CHUNK_SIZE + 2)];
}

#ifdef VERTEX_PACKING
void Chunk::init(std::vector<uint32_t> &data) {
#else
void Chunk::init(std::vector<float> &data) {
#endif
    model = glm::translate(glm::mat4(1.0f), glm::vec3(cx * CHUNK_SIZE, 0, cz * CHUNK_SIZE));
    voxels = std::vector<int>((CHUNK_SIZE + 2) * (CHUNK_SIZE + 2) * (CHUNK_HEIGHT + 2));
}

void Chunk::generateVoxels2D() {
    unsigned int seed = 123456u;
    const siv::PerlinNoise::seed_type s = seed;
    const siv::PerlinNoise perlin{ s };

    int heightMap[CHUNK_SIZE + 2][CHUNK_SIZE + 2];
    heightMap[0][0] = clamp(perlin.octave2D_01((cx * CHUNK_SIZE * 0.01), (cz * CHUNK_SIZE * 0.01), 4), 0.0, 1.0 - EPSILON) * CHUNK_HEIGHT;

    for (int z = -1; z < CHUNK_SIZE + 1; ++z) {
        for (int x = -1; x < CHUNK_SIZE + 1; ++x) {
            int noise_x = cx * CHUNK_SIZE + x + 1;
            int noise_z = cz * CHUNK_SIZE + z + 1;

            // Calculate adjacent noise values in the +x and +z directions
            const double noise10 = clamp(perlin.octave2D_01(((noise_x + 1) * 0.01), (noise_z * 0.01), 4), 0.0, 1.0 - EPSILON);
            const double noise01 = clamp(perlin.octave2D_01((noise_x * 0.01), ((noise_z + 1) * 0.01), 4), 0.0, 1.0 - EPSILON);

            if (x != CHUNK_SIZE) {
                heightMap[x + 2][z + 1] = noise10 * CHUNK_HEIGHT;
            }
            if (z != CHUNK_SIZE) {
                heightMap[x + 1][z + 2] = noise01 * CHUNK_HEIGHT;
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

            voxels[getVoxelIndex(x + 1, CHUNK_HEIGHT / 2, z + 1, CHUNK_SIZE + 2)] = 1;

            maxY = std::max(maxY, CHUNK_HEIGHT / 2);
        }
    }

    minY = std::max(0, minY - 1);
    maxY = std::min(CHUNK_HEIGHT, maxY + 1);
}

void Chunk::generateVoxels3D() {
    unsigned int seed = 123456u;
    const siv::PerlinNoise::seed_type s = seed;
    const siv::PerlinNoise perlin{ s };

    for (int y = -1; y < CHUNK_HEIGHT + 1; ++y) {
        for (int z = -1; z < CHUNK_SIZE + 1; ++z) {
            for (int x = -1; x < CHUNK_SIZE + 1; ++x) {
                float noise_x = cx * CHUNK_SIZE + x + 1;
                float noise_y = y + 1;
                float noise_z = cz * CHUNK_SIZE + z + 1;

                float noise = perlin.octave3D_01(noise_x * 0.01, noise_y * 0.01, noise_z * 0.01, 4);

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

void Chunk::generateVoxels(std::string path) {
    std::ifstream infile(path);

    std::string line;
    int y = 0;
    int gz = 0;
    while (std::getline(infile, line)) {
        if (line.empty()) {
            y++;
            gz = 0;
            continue;
        }

        if (gz >= cz * CHUNK_SIZE - 1 && gz < (cz + 1) * CHUNK_SIZE + 1) {
            std::istringstream iss(line);
            int value;
            int gx = 0;
            while (iss >> value) {
                if (gx >= cx * CHUNK_SIZE - 1 && gx < (cx + 1) * CHUNK_SIZE + 1) {
                    int x = gx - cx * CHUNK_SIZE;
                    int z = gz - cz * CHUNK_SIZE;
                    voxels[getVoxelIndex(x + 1, y + 1, z + 1, CHUNK_SIZE + 2)] = value;
                }
                gx++;
            }
        }
        gz++;
    }

    minY = 0;
    maxY = std::min(CHUNK_HEIGHT, y + 2);

    infile.close();
}
