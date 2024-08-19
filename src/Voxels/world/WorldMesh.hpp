#pragma once

#include <vector>

#include "../util/Flags.h"

class WorldMesh {
public:
    WorldMesh(int numChunks);

    unsigned int VAO;
    unsigned int VBO;

#ifdef VERTEX_PACKING
    std::vector<uint32_t> data;
#else
    std::vector<float> data;
#endif

    std::vector<int> chunkVertexStarts;

    void createBuffers();
};
