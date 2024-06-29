#pragma once

#include <vector>

int getVoxelIndex(int x, int y, int z, int size);
float clamp(float x, float low, float high);
bool inBounds(int i, int j, int size);
bool inBounds(int x, int y, int z, int size, int height);
template<typename T>
void debugPrint(std::vector<T>& vec, int stride);

extern long long int totalMesherTime;
