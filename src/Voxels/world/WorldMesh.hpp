#pragma once

#include <vector>
#include <cstdint>

class WorldMesh {
public:
    WorldMesh() = default;
    explicit WorldMesh(int numChunks);

    unsigned int VAO{};
    unsigned int VBO{};

    std::vector<uint32_t> data;
    std::vector<int> voxels;

    std::vector<unsigned int> chunkVertexStarts;

    void createBuffers();
};
