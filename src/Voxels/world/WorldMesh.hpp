#pragma once

#include <vector>
#include <cstdint>

class WorldMesh {
public:
    WorldMesh() = default;

    unsigned int VAO{};
    unsigned int VBO{};

    std::vector<uint32_t> data;

    void createBuffers();
};
