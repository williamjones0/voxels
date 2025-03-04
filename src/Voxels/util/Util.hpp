#pragma once

#include <vector>
#include <iostream>
#include "../world/Chunk.hpp"

int getVoxelIndex(int x, int y, int z, int size);

template <typename T>
T clamp(T x, T low, T high) {
    return std::max(low, std::min(x, high));
}

template<typename T>
void debugPrint(std::vector<T> &vec, int stride) {
    for (int i = 0; i < vec.size(); ++i) {
        std::cout << vec[i] << " ";
        if (i % stride == stride - 1) {
            std::cout << std::endl;
        }
    }
}
