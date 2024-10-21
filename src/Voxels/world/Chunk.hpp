#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>
#include <string>

constexpr int WORLD_SIZE = 1024;

constexpr int CHUNK_SIZE_SHIFT = 4;
constexpr int CHUNK_HEIGHT_SHIFT = 7;
constexpr int CHUNK_SIZE = 1 << CHUNK_SIZE_SHIFT;
constexpr int CHUNK_HEIGHT = 1 << CHUNK_HEIGHT_SHIFT;
constexpr int CHUNK_SIZE_MASK = (1 << (CHUNK_SIZE_SHIFT + 1)) - 1;
constexpr int CHUNK_HEIGHT_MASK = (1 << (CHUNK_HEIGHT_SHIFT + 1)) - 1;

constexpr int NUM_AXIS_CHUNKS = WORLD_SIZE / CHUNK_SIZE;
constexpr int NUM_CHUNKS = NUM_AXIS_CHUNKS * NUM_AXIS_CHUNKS;

class Chunk {
public:
    Chunk();
    Chunk(int cx, int cz);
    Chunk &operator=(const Chunk &other); // Copy assignment operator
    Chunk(Chunk &&other) noexcept; // Move constructor
    Chunk &operator=(Chunk &&other) noexcept; // Move assignment operator

    int cx;
    int cz;
    int minY;
    int maxY;

    unsigned int VAO;
    unsigned int dataVBO;

    unsigned int numVertices;
    unsigned int firstIndex;

    glm::mat4 model;

    std::vector<int> voxels;
    void store(int x, int y, int z, char v);
    int load(int x, int y, int z);
    void init();
    void generateVoxels2D();
    void generateVoxels3D();
    void generateVoxels(const std::string &path);
};
