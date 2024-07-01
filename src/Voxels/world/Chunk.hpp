#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include "../util/Flags.h"

class Chunk {
public:
    Chunk(int cx, int cz);

    const int cx;
    const int cz;
    int minY;
    int maxY;

    unsigned int VAO;
    unsigned int dataVBO;

    unsigned int numVertices;

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
