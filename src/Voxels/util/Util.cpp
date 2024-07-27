#include "Util.hpp"

#include <algorithm>
#include <iostream>

int getVoxelIndex(int x, int y, int z, int size) {
    return y * size * size + z * size + x;
}

float clamp(float x, float low, float high) {
    return std::max(low, std::min(x, high));
}

template<typename T>
void debugPrint(std::vector<T>& vec, int stride) {
    for (int i = 0; i < vec.size(); ++i) {
        std::cout << vec[i] << " ";
        if (i % stride == stride - 1) {
            std::cout << std::endl;
        }
    }
}

int bitCount(int i) {
    i = i - ((i >> 1) & 0x55555555);
    i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
    i = (i + (i >> 4)) & 0x0f0f0f0f;
    i = i + (i >> 8);
    i = i + (i >>16);
    return i & 0x3f;
}

long long int totalMesherTime = 0;
