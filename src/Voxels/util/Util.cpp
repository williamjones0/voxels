#include "Util.hpp"

int getVoxelIndex(const int x, const int y, const int z, const int size) {
    return y * size * size + z * size + x;
}
