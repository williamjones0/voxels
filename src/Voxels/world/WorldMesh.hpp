#pragma once

#include <glad/glad.h>
#include <vector>
#include <iostream>
#include "../util/Flags.h"

class WorldMesh {
public:
    WorldMesh();

    unsigned int VAO;
    unsigned int VBO;

#ifdef VERTEX_PACKING
    std::vector<uint32_t> data;
#else
    std::vector<float> data;
#endif

    void createBuffers();
};
