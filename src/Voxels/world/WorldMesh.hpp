#pragma once

#include <glad/glad.h>
#include <vector>
#include <iostream>

class WorldMesh {
public:
    WorldMesh();

    unsigned int VAO;
    unsigned int VBO;

    std::vector<uint32_t> data;

    void createBuffers();
};
