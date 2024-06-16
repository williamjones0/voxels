#pragma once

#include <vector>

#define EPSILON 0.000001

void generateTerrain(std::vector<int>& voxels, unsigned int seed, int worldSize, int heightScale);
