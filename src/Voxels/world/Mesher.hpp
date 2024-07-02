#pragma once

#include <vector>
#include <cstdint>

#include "Chunk.hpp"
#include "../util/Flags.h"

static inline int vertexAO(uint8_t side1, uint8_t side2, uint8_t corner);

bool inBounds(int x, int y, int z, int size, int height);

int dirToIndex(int i, int j, int k);

#ifdef VERTEX_PACKING
void meshChunk(Chunk *chunk, int worldSize, std::vector<uint32_t> &data);
#else
void meshChunk(Chunk *chunk, int worldSize, std::vector<float> &data);
#endif

bool boundsCheck(int x, int y, int z, int i, int j, int k, int worldSize, std::vector<int> &voxels);
