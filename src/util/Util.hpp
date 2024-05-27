#pragma once

int getVoxelIndex(int x, int y, int z, int size) {
	return y * size * size + z * size + x;
}

float clamp(float x, float low, float high) {
	return std::max(low, std::min(x, high));
}
