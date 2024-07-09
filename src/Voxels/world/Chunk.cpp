#include "Chunk.hpp"

#include <glad/glad.h>
#include <glm/ext/matrix_transform.hpp>

#include <iostream>
#include <chrono>

#include "Mesher.hpp"
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
    voxels = std::vector<int>((CHUNK_SIZE + 2) * (CHUNK_SIZE + 2) * CHUNK_HEIGHT);
};

void Chunk::store(int x, int y, int z, char v) {
    voxels[getVoxelIndex(x, y, z, CHUNK_SIZE)] = v;
}

char Chunk::load(int x, int y, int z) {
    return voxels[getVoxelIndex(x, y, z, CHUNK_SIZE)];
}

#ifdef VERTEX_PACKING
void Chunk::init(std::vector<uint32_t> &data) {
#else
void Chunk::init(std::vector<float> &data) {
#endif
    auto start = std::chrono::high_resolution_clock::now();
    model = glm::translate(glm::mat4(1.0f), glm::vec3(cx * CHUNK_SIZE, 0, cz * CHUNK_SIZE));

    voxels = std::vector<int>((CHUNK_SIZE + 2) * (CHUNK_SIZE + 2) * CHUNK_HEIGHT);
    generateVoxels();

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "generateVoxels took " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "us\n";

    start = std::chrono::high_resolution_clock::now();
    meshChunk(this, WORLD_SIZE, data);
    end = std::chrono::high_resolution_clock::now();
    std::cout << "meshChunk took " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "us\n";
}

void Chunk::generateVoxels() {
    unsigned int seed = 123456u;
    const siv::PerlinNoise::seed_type s = seed;
    const siv::PerlinNoise perlin{ s };

    for (int z = -1; z < CHUNK_SIZE + 1; ++z) {
        for (int x = -1; x < CHUNK_SIZE + 1; ++x) {
            int noise_x = cx * CHUNK_SIZE + x + 1;
            int noise_z = cz * CHUNK_SIZE + z + 1;
            const double noise = clamp(perlin.octave2D_01((noise_x * 0.01), (noise_z * 0.01), 4), 0.0, 1.0 - EPSILON);
            int y = noise * CHUNK_HEIGHT;
            y = std::min(std::max(0, y), CHUNK_HEIGHT - 1);

            minY = std::min(y, minY);
            maxY = std::max(y, maxY);
            for (int y0 = 0; y0 < y; ++y0) {
                int index = getVoxelIndex(x + 1, y0, z + 1, CHUNK_SIZE + 2);
                voxels[index] = (y0 == y - 1 ? 1 : 2);
            }
        }
    }
}
