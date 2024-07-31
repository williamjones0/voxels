#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

#include "../util/Flags.h"

const int WORLD_SIZE = 1024;

const int CHUNK_SIZE_SHIFT = 4;
const int CHUNK_HEIGHT_SHIFT = 7;
const int CHUNK_SIZE = 1 << CHUNK_SIZE_SHIFT;
const int CHUNK_HEIGHT = 1 << CHUNK_HEIGHT_SHIFT;
const int CHUNK_SIZE_MASK = (1 << (CHUNK_SIZE_SHIFT + 1)) - 1;
const int CHUNK_HEIGHT_MASK = (1 << (CHUNK_HEIGHT_SHIFT + 1)) - 1;

class Chunk {
public:
    Chunk(void);
    Chunk(int cx, int cz);
    Chunk(const Chunk &other); // Copy constructor
    Chunk &operator=(const Chunk &other); // Copy assignment operator
    Chunk(Chunk &&other) noexcept; // Move constructor
    Chunk &operator=(Chunk &&other) noexcept; // Move assignment operator

    int cx;
    int cz;
    int minY;
    int maxY;

    unsigned int VAO;
    unsigned int dataVBO;

    unsigned int numVertices;
    unsigned int firstIndex;

    glm::mat4 model;

    std::vector<int> voxels;
    void store(int x, int y, int z, char v);
    char load(int x, int y, int z);
#ifdef VERTEX_PACKING
    void init(std::vector<uint32_t> &data);
#else
    void init(std::vector<float> &data);
#endif
    void generateVoxels();
};
