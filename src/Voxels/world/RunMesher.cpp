#include "RunMesher.hpp"

#include "VertexFormat.hpp"

constexpr int FrontNormal = 0;
constexpr int BackNormal = 1;
constexpr int LeftNormal = 2;
constexpr int RightNormal = 3;
constexpr int BottomNormal = 4;
constexpr int TopNormal = 5;

RunMesher::MeshResult RunMesher::meshChunk() {
    // Y-axis - start from the bottom and search up
    for (size_t j = chunk->minY; j < chunk->maxY; ++j) {  // y
        for (size_t k = 1; k < ChunkSize + 1; ++k) {      // z
            for (size_t i = 1; i < ChunkSize + 1; ++i) {  // x
                const size_t access = Chunk::getVoxelIndex(i, j, k);
                const int voxel = chunk->voxels[access];

                if (voxel == 0 || (generationType == GenerationType::Perlin2D && voxel == 3)) {
                    continue;
                }

                createRun(voxel, i, j, k, access);
            }
        }
    }

    return {
        .chunk = chunk,
        .vertices = std::move(vertices)
    };
}

void RunMesher::createRun(const int voxel, const size_t i, const size_t j, const size_t k, const size_t access) {
    // Left face (-X)
    if (!visitedXN[access] && shouldMeshFace(i - 1, j, k)) {
        int length = 0;

        for (int q = j; q < ChunkHeight; ++q) {
            const size_t chunkAccess = Chunk::getVoxelIndex(i, q, k);

            // If we reach a different block or an empty block, end the run
            if (differentBlock(chunkAccess, voxel)) {
                break;
            }

            visitedXN[chunkAccess] = true;

            ++length;
        }

        if (length > 0) {
            appendQuad(
                glm::ivec3(i, length + j, k + 1),
                glm::ivec3(i, length + j, k),
                glm::ivec3(i, j, k + 1),
                glm::ivec3(i, j, k),
                LeftNormal,
                voxel
            );
        }
    }

    // Right face (+X)
    if (!visitedXP[access] && shouldMeshFace(i + 1, j, k)) {
        int length = 0;
        for (int q = j; q < ChunkHeight; ++q) {
            const size_t chunkAccess = Chunk::getVoxelIndex(i, q, k);

            if (differentBlock(chunkAccess, voxel)) {
                break;
            }

            visitedXP[chunkAccess] = true;

            ++length;
        }

        if (length > 0) {
            appendQuad(
                glm::ivec3(i + 1, j, k),
                glm::ivec3(i + 1, length + j, k),
                glm::ivec3(i + 1, j, k + 1),
                glm::ivec3(i + 1, length + j, k + 1),
                RightNormal,
                voxel
            );
        }
    }

    // Back face (-Z)
    if (!visitedZN[access] && shouldMeshFace(i, j, k - 1)) {
        int length = 0;
        for (int q = j; q < ChunkHeight; ++q) {
            const size_t chunkAccess = Chunk::getVoxelIndex(i, q, k);

            if (differentBlock(chunkAccess, voxel)) {
                break;
            }

            visitedZN[chunkAccess] = true;

            ++length;
        }

        if (length > 0) {
            appendQuad(
                glm::ivec3(i + 1, length + j, k),
                glm::ivec3(i, length + j, k),
                glm::ivec3(i + 1, j, k),
                glm::ivec3(i, j, k),
                BackNormal,
                voxel
            );
        }
    }

    // Front face (+Z)
    if (!visitedZP[access] && shouldMeshFace(i, j, k + 1)) {
        int length = 0;
        for (int q = j; q < ChunkHeight; ++q) {
            const size_t chunkAccess = Chunk::getVoxelIndex(i, q, k);

            if (differentBlock(chunkAccess, voxel)) {
                break;
            }

            visitedZP[chunkAccess] = true;

            ++length;
        }

        if (length > 0) {
            appendQuad(
                glm::ivec3(i, j, k + 1),
                glm::ivec3(i + 1, j, k + 1),
                glm::ivec3(i, length + j, k + 1),
                glm::ivec3(i + 1, length + j, k + 1),
                FrontNormal,
                voxel
            );
        }
    }

    // Bottom face (-Y)
    if (!visitedYN[access] && shouldMeshFace(i, j - 1, k)) {
        int length = 0;
        for (int q = i; q < ChunkSize + 1; ++q) {
            const size_t chunkAccess = Chunk::getVoxelIndex(q, j, k);

            if (differentBlock(chunkAccess, voxel)) {
                break;
            }

            visitedYN[chunkAccess] = true;

            ++length;
        }

        if (length > 0) {
            appendQuad(
                glm::ivec3(i, j, k),
                glm::ivec3(i + length, j, k),
                glm::ivec3(i, j, k + 1),
                glm::ivec3(i + length, j, k + 1),
                BottomNormal,
                voxel
            );
        }
    }

    // Top face (+Y)
    if (!visitedYP[access] && shouldMeshFace(i, j + 1, k)) {
        int length = 0;
        for (int q = i; q < ChunkSize + 1; ++q) {
            const size_t chunkAccess = Chunk::getVoxelIndex(q, j, k);

            if (differentBlock(chunkAccess, voxel)) {
                break;
            }

            visitedYP[chunkAccess] = true;

            ++length;
        }

        if (length > 0) {
            appendQuad(
                glm::ivec3(i + length, j + 1, k + 1),
                glm::ivec3(i + length, j + 1, k),
                glm::ivec3(i, j + 1, k + 1),
                glm::ivec3(i, j + 1, k),
                TopNormal,
                voxel
            );
        }
    }
}

bool RunMesher::shouldMeshFace(const int i, const int j, const int k) const {
    const bool adjInBounds = j >= 0 && j < ChunkHeight + 1;

    bool isAdjVoxelEmpty = true;
    if (adjInBounds) {
        isAdjVoxelEmpty = chunk->voxels[Chunk::getVoxelIndex(i, j, k)] == 0;
    }

    return adjInBounds && isAdjVoxelEmpty;
}

bool RunMesher::differentBlock(size_t access, int voxel) const {
    if (access >= chunk->voxels.size()) {
        return true;
    }

    const int otherVoxel = chunk->voxels[access];
    return otherVoxel != voxel || otherVoxel == 0 || (generationType == GenerationType::Perlin2D && otherVoxel == 3);
}

void RunMesher::appendQuad(
    const glm::ivec3& tl,
    const glm::ivec3& tr,
    const glm::ivec3& bl,
    const glm::ivec3& br,
    const int normal,
    const int voxel
) {
    // Each vertex in a quad shares the same colour and normal
    const uint32_t shared = static_cast<uint32_t>(normal) << VertexFormat::NormalShift
                          | static_cast<uint32_t>(voxel) << VertexFormat::ColourShift
                          | 0b11 << VertexFormat::AOShift; // Full ambient occlusion for now

    // Top left vertex
    const uint32_t tlv = combinePosition(tl, shared);

    // Top right vertex
    const uint32_t trv = combinePosition(tr, shared);

    // Bottom left vertex
    const uint32_t blv = combinePosition(bl, shared);

    // Bottom right vertex
    const uint32_t brv = combinePosition(br, shared);

    vertices.push_back(tlv);
    vertices.push_back(blv);
    vertices.push_back(trv);
    vertices.push_back(trv);
    vertices.push_back(blv);
    vertices.push_back(brv);
}

uint32_t RunMesher::combinePosition(const glm::ivec3 &pos, const uint32_t shared) {
    return shared |
           static_cast<uint32_t>(pos.x) << VertexFormat::XShift |
           static_cast<uint32_t>(pos.y) << VertexFormat::YShift |
           static_cast<uint32_t>(pos.z) << VertexFormat::ZShift;
}
