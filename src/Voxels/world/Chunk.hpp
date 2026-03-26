#pragma once

#include <atomic>
#include <mutex>
#include <vector>

constexpr int ChunkSizeShift = 4;
constexpr int ChunkHeightShift = 7;
constexpr int ChunkSize = 1 << ChunkSizeShift;
constexpr int ChunkHeight = 1 << ChunkHeightShift;

constexpr int VoxelsSize = (ChunkSize + 2) * (ChunkSize + 2) * ChunkHeight;

enum class GenerationType {
    None,
    Flat,
    Perlin2D,
    Perlin3D
};

constexpr int EmptyVoxel = 0;

class Chunk {
public:
    struct GenerationResult {
        std::shared_ptr<Chunk> chunk;
        std::vector<int> voxelField = std::vector(VoxelsSize, 0);
        int minY{};
        int maxY{};
    };

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
    int debug = 0;

    std::vector<int> voxels{};
    void store(int x, int y, int z, int v);
    int load(int x, int y, int z) const;

    static void storeInto(std::vector<int>& field, int& minY, int& maxY, int x, int y, int z, int v);  // TODO: maybe a better way to do this

    static GenerationResult generateFlat();
    static GenerationResult generateVoxels2D(int cx, int cz);
    static GenerationResult generateVoxels3D(int cx, int cz);

    static size_t getVoxelIndex(size_t x, size_t y, size_t z);
};
