#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

#define CHUNK_SIZE 8
#define CHUNK_HEIGHT 64

class Chunk {
public:
	Chunk(int cx, int cz);

	const int cx;
	const int cz;
	int minY;
	int maxY;

	unsigned int VAO;
	unsigned int VBO;
	unsigned int aoVBO;
	unsigned int dataVBO;

	std::vector<int> positions;
	std::vector<float> colours;
	std::vector<int> normals;
	std::vector<int> ao;
	std::vector<uint64_t> data;

    glm::mat4 model;

	std::vector<int> voxels;
	void store(int x, int y, int z, char v);
	char load(int x, int y, int z);
	void init();
	void generateVoxels();
	void render();
};
