#include "Chunk.hpp"

#include <glad/glad.h>
#include <glm/ext/matrix_transform.hpp>

#include <iostream>
#include <chrono>

#include "Mesher.hpp"
#include "GreedyMesher.hpp"
#include "../util/PerlinNoise.hpp"
#include "../util/Util.hpp"
#include "../util/Flags.h"

#define EPSILON 0.000001

Chunk::Chunk(int cx, int cz, unsigned int firstIndex) : cx(cx), cz(cz), firstIndex(firstIndex) {
    minY = CHUNK_HEIGHT;
    maxY = 0;
    VAO = 0;
    dataVBO = 0;
    numVertices = 0;
    model = glm::mat4(1.0f);
    voxels = std::vector<int>((CHUNK_SIZE + 2) * (CHUNK_SIZE + 2) * (CHUNK_HEIGHT + 2));
};

void Chunk::store(int x, int y, int z, char v) {
    voxels[getVoxelIndex(x, y, z, CHUNK_SIZE)] = v;
}

char Chunk::load(int x, int y, int z) {
    return voxels[getVoxelIndex(x, y, z, CHUNK_SIZE)];
}

#ifdef VERTEX_PACKING
void Chunk::init(WorldMesh &worldMesh) {
#else
void Chunk::init(std::vector<float> &data) {
#endif
    auto start = std::chrono::high_resolution_clock::now();
    model = glm::translate(glm::mat4(1.0f), glm::vec3(cx * CHUNK_SIZE, 0, cz * CHUNK_SIZE));

    voxels = std::vector<int>((CHUNK_SIZE + 2) * (CHUNK_SIZE + 2) * (CHUNK_HEIGHT + 2));
    generateVoxels();

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "generateVoxels took " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "us\n";

    start = std::chrono::high_resolution_clock::now();
    //meshChunk(this, WORLD_SIZE, data);
    GreedyMesher mesher = GreedyMesher(minY, maxY, CHUNK_SIZE, CHUNK_SIZE, this, worldMesh);
    mesher.mesh();

    end = std::chrono::high_resolution_clock::now();
    std::cout << "meshChunk took " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "us\n";
}

void Chunk::generateVoxels() {
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

            int y = 2; // heightMap[x + 1][z + 1];
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

            for (int y0 = -1; y0 <= y; ++y0) {
                int index = getVoxelIndex(x + 1, y0 + 1, z + 1, CHUNK_SIZE + 2);

                int voxelType;
                if (y0 == y - 1) {
                    voxelType = 1;
                } else if (y0 < lowestVisibleHeight) {
                    voxelType = 3;
                } else {
                    voxelType = 2;
                }
                voxels[index] = voxelType;
            }
        }
    }
}
