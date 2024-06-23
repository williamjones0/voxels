#include <world/Chunk.hpp>

#include <glad/glad.h>

#include <util/PerlinNoise.hpp>
#include <world/Mesher.hpp>
#include <util/Util.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <iostream>

#define EPSILON 0.000001

Chunk::Chunk(int cx, int cz) : cx(cx), cz(cz) {};

void Chunk::store(int x, int y, int z, char v) {
    voxels[getVoxelIndex(x, y, z, CHUNK_SIZE)] = v;
}

char Chunk::load(int x, int y, int z) {
    return voxels[getVoxelIndex(x, y, z, CHUNK_SIZE)];
}

void Chunk::init() {
    model = glm::translate(glm::mat4(1.0f), glm::vec3(cx * CHUNK_SIZE, 0, cz * CHUNK_SIZE));

    voxels = std::vector<int>((CHUNK_SIZE + 2) * (CHUNK_SIZE + 2) * CHUNK_HEIGHT);
    generateVoxels();

    meshChunk(this, 32);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &dataVBO);
    glBindBuffer(GL_ARRAY_BUFFER, dataVBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(uint64_t), &data[0], GL_STATIC_DRAW);
    glVertexAttribLPointer(0, 1, GL_DOUBLE, sizeof(uint64_t), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);
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

void Chunk::render() {
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, data.size() * 3);
    glBindVertexArray(0);
}
