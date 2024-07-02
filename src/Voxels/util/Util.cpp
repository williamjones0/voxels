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

long long int totalMesherTime = 0;
