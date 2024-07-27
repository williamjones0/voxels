#pragma once

#include <vector>

#include "../util/Flags.h"

class WorldMesh {
public:
    WorldMesh();

    unsigned int VAO;
    unsigned int ptVBO;
	unsigned int saVBO;

    std::vector<uint32_t> positionsAndTypes;
	std::vector<uint32_t> sidesAndAoFactors;

    std::vector<int> indices;

    void createBuffers();
};
