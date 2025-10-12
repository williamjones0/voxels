#pragma once

#include "Level.hpp"

#include <glm/glm.hpp>

#include <atomic>
#include <cstdint>
#include <mutex>
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

    Chunk(const Chunk &chunk) = delete;                // Copy constructor
    Chunk(Chunk &&other) noexcept = delete;            // Move constructor
    Chunk &operator=(const Chunk &other) = delete;     // Copy assignment operator
    Chunk &operator=(Chunk &&other) noexcept = delete; // Move assignment operator

    int cx;
    int cz;
    int minY;
    int maxY;

    int neighbours;

    std::vector<uint32_t> vertices;

    size_t index;

    unsigned int numVertices;
    unsigned int firstIndex;

    std::atomic_bool onGPU = false;
    std::atomic_bool destroyed = false;
    std::atomic_bool beingMeshed = true;
    int debug = 0;
    std::mutex mutex;

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
