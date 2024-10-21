#pragma once

#include <vector>
#include <cstdint>

class WorldMesh {
public:
    explicit WorldMesh(int numChunks);

    unsigned int VAO;
    unsigned int VBO;

    std::vector<uint32_t> data;

    std::vector<unsigned int> chunkVertexStarts;

    void createBuffers();
};
