#include "Util.hpp"

int getVoxelIndex(int x, int y, int z, int size) {
    return y * size * size + z * size + x;
}
