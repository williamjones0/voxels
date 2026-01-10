#pragma once

#include "Level.hpp"

#include <glm/glm.hpp>

#include <atomic>
#include <mutex>
#include <vector>

constexpr int ChunkSizeShift = 4;
constexpr int ChunkHeightShift = 7;
constexpr int ChunkSize = 1 << ChunkSizeShift;
constexpr int ChunkHeight = 1 << ChunkHeightShift;

constexpr int VoxelsSize = (ChunkSize + 2) * (ChunkSize + 2) * ChunkHeight;

enum class GenerationType {
    Flat,
    Perlin2D,
    Perlin3D,
    LevelLoad
};

constexpr int EmptyVoxel = 0;

class Chunk {
public:
    Chunk(int cx, int cz);

    Chunk(const Chunk& chunk) = delete;                // Copy constructor
    Chunk(Chunk&& other) noexcept = delete;            // Move constructor
    Chunk& operator=(const Chunk& other) = delete;     // Copy assignment operator
    Chunk& operator=(Chunk&& other) noexcept = delete; // Move assignment operator

    int cx;
    int cz;
    int minY = ChunkHeight;
    int maxY = 0;

    int neighbours = 0;

    size_t index = std::numeric_limits<size_t>::max();

    unsigned int numVertices = 0;
    unsigned int firstIndex = -1;

    std::atomic_bool bufferRegionAllocated = false;
    std::atomic_bool destroyed = false;
    std::atomic_bool beingMeshed = false;
    int debug = 0;
    std::mutex mutex;

    std::vector<int> voxels{};
    void store(int x, int y, int z, int v);
    int load(int x, int y, int z) const;
    void init();
    void generate(GenerationType type, const Level& level);
    void generateFlat();
    void generateVoxels2D();
    void generateVoxels3D();
    void generateVoxels(const Level& level);

    static size_t getVoxelIndex(size_t x, size_t y, size_t z);
};
