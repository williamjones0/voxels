#include "Util.hpp"

#include <algorithm>
#include <iostream>

int getVoxelIndex(int x, int y, int z, int size) {
    return y * size * size + z * size + x;
}

float clamp(float x, float low, float high) {
    return std::max(low, std::min(x, high));
}

bool inBounds(int i, int j, int size) {
    return (0 <= i && i < size) && (0 <= j && j < size);
}

bool inBounds(int x, int y, int z, int size, int height) {
    return (0 <= x && x < size)
           && (0 <= y && y < height)
           && (0 <= z && z < size);
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
