#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>
#include <string>

constexpr int WORLD_SIZE = 256;

constexpr int CHUNK_SIZE_SHIFT = 4;
constexpr int CHUNK_HEIGHT_SHIFT = 7;
constexpr int CHUNK_SIZE = 1 << CHUNK_SIZE_SHIFT;
constexpr int CHUNK_HEIGHT = 1 << CHUNK_HEIGHT_SHIFT;

constexpr int NUM_AXIS_CHUNKS = WORLD_SIZE / CHUNK_SIZE;
constexpr int NUM_CHUNKS = NUM_AXIS_CHUNKS * NUM_AXIS_CHUNKS;

constexpr int VOXELS_SIZE = (CHUNK_SIZE + 2) * (CHUNK_SIZE + 2) * (CHUNK_HEIGHT + 2);

class Chunk {
public:
    Chunk();
    Chunk(int cx, int cz);

    Chunk(const Chunk &chunk);

    Chunk &operator=(const Chunk &other); // Copy assignment operator
    Chunk(Chunk &&other) noexcept; // Move constructor
    Chunk &operator=(Chunk &&other) noexcept; // Move assignment operator
    bool operator==(const Chunk &other) const; // Comparison operator

    int cx;
    int cz;
    int minY;
    int maxY;

    int neighbours;

    unsigned int VAO;
    unsigned int dataVBO;

    std::vector<uint32_t> vertices;

    size_t index;

    unsigned int numVertices;
    unsigned int firstIndex;

    bool ready = false;
    bool dying = false;
    int meshed = 0;

    std::vector<int> voxels{};
    void store(int x, int y, int z, char v);
    int load(int x, int y, int z);
    void init();
    void generateVoxels2D();
    void generateVoxels3D();
    void generateVoxels(const std::string &path);
};
