#pragma once

#include <glad/glad.h>
#include <vector>
#include <iostream>

class WorldMesh {
public:
    WorldMesh();

    unsigned int VAO;
    unsigned int dataVBO;

    std::vector<uint64_t> data;

    void createBuffers();
};
