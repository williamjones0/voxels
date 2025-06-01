#pragma once

#include "Level.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>
#include <string>

constexpr int CHUNK_SIZE_SHIFT = 4;
constexpr int CHUNK_HEIGHT_SHIFT = 7;
constexpr int CHUNK_SIZE = 1 << CHUNK_SIZE_SHIFT;
constexpr int CHUNK_HEIGHT = 1 << CHUNK_HEIGHT_SHIFT;

constexpr int VOXELS_SIZE = (CHUNK_SIZE + 2) * (CHUNK_SIZE + 2) * (CHUNK_HEIGHT + 2);

enum class GenerationType {
    Flat,
    Perlin2D,
    Perlin3D,
    LevelLoad
};

constexpr int EMPTY_VOXEL = 0;

class Chunk {
public:
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
    bool initialising = true;
    int debug = 0;

    std::vector<int> voxels{};
    void store(int x, int y, int z, char v);
    int load(int x, int y, int z);
    void init();
    void generate(GenerationType type, const Level& level);
    void generateFlat();
    void generateVoxels2D();
    void generateVoxels3D();
    void generateVoxels(const Level& level);
};
