#include "Mesher.hpp"

#include "VertexFormat.hpp"
#include "../util/PerlinNoise.hpp"
#include "../util/Util.hpp"

constexpr int FaceSize = 18;

constexpr int FrontFace = 0 * FaceSize;
constexpr int BackFace = 1 * FaceSize;
constexpr int LeftFace = 2 * FaceSize;
constexpr int RightFace = 3 * FaceSize;
constexpr int BottomFace = 4 * FaceSize;
constexpr int TopFace = 5 * FaceSize;

constexpr int VerticesLength = 6 * FaceSize;

constexpr int FrontNormal = 0;
constexpr int BackNormal = 1;
constexpr int LeftNormal = 2;
constexpr int RightNormal = 3;
constexpr int BottomNormal = 4;
constexpr int TopNormal = 5;

namespace {
    constexpr int cubeVertices[] = {
            // Front
            0, 0, 0,
            1, 0, 0,
            1, 1, 0,
            1, 1, 0,
            0, 1, 0,
            0, 0, 0,

            // Back
            0, 0, 1,
            1, 0, 1,
            1, 1, 1,
            1, 1, 1,
            0, 1, 1,
            0, 0, 1,

            // Left
            0, 1, 1,
            0, 1, 0,
            0, 0, 0,
            0, 0, 0,
            0, 0, 1,
            0, 1, 1,

            // Right
            1, 1, 1,
            1, 1, 0,
            1, 0, 0,
            1, 0, 0,
            1, 0, 1,
            1, 1, 1,

            // Bottom
            0, 0, 0,
            1, 0, 0,
            1, 0, 1,
            1, 0, 1,
            0, 0, 1,
            0, 0, 0,

            // Top
            0, 1, 0,
            1, 1, 0,
            1, 1, 1,
            1, 1, 1,
            0, 1, 1,
            0, 1, 0
    };

    constexpr int flippedCubeVertices[] = {
            // Front
            0, 0, 0,
            1, 0, 0,
            0, 1, 0,
            0, 1, 0,
            1, 0, 0,
            1, 1, 0,

            // Back
            0, 0, 1,
            1, 0, 1,
            0, 1, 1,
            0, 1, 1,
            1, 0, 1,
            1, 1, 1,

            // Left
            0, 1, 1,
            0, 1, 0,
            0, 0, 1,
            0, 0, 1,
            0, 1, 0,
            0, 0, 0,

            // Right
            1, 1, 1,
            1, 1, 0,
            1, 0, 1,
            1, 0, 1,
            1, 1, 0,
            1, 0, 0,

            // Bottom
            0, 0, 0,
            1, 0, 0,
            0, 0, 1,
            0, 0, 1,
            1, 0, 0,
            1, 0, 1,

            // Top
            0, 1, 0,
            1, 1, 0,
            0, 1, 1,
            0, 1, 1,
            1, 1, 0,
            1, 1, 1,
    };
}

inline int Mesher::vertexAO(const uint8_t side1, const uint8_t side2, uint8_t corner) {
    if (side1 == 1 || side2 == 1) corner = 0;
    return side1 && side2 ? 0 : 3 - (side1 + side2 + corner);
}

bool Mesher::inBounds(const int x, const int y, const int z) {
    constexpr int size = ChunkSize + 2;
    constexpr int height = ChunkHeight;
    return 0 <= x && x < size
        && 0 <= y && y < height
        && 0 <= z && z < size;
}

int Mesher::dirToIndex(const int i, const int j, const int k) {
    return (i + 1) * 9 + (j + 1) * 3 + k + 1;
}

auto Mesher::meshChunk(Chunk* chunk, const GenerationType generationType) -> MeshResult {
    const std::vector<int>& voxels = chunk->voxels;

    std::vector<int> positions;
    std::vector<int> colours;
    std::vector<int> normals;
    std::vector<int> ao;

    for (int y = chunk->minY; y < chunk->maxY; ++y) {
        for (int z = 1; z < ChunkSize + 1; ++z) {
            for (int x = 1; x < ChunkSize + 1; ++x) {
                int voxel = voxels[getVoxelIndex(x, y, z, ChunkSize + 2)];
                if (voxel == 0 || (generationType == GenerationType::Perlin2D && voxel == 3)) {
                    continue;
                }

                // Ambient occlusion (computed first so that quads can be flipped if necessary)
                // (-1, -1, -1) to (1, 1, 1)
                std::array<bool, 27> presence{};
                int index = 0;
                for (int i = -1; i <= 1; ++i) {
                    for (int j = -1; j <= 1; ++j) {
                        for (int k = -1; k <= 1; ++k) {
                            presence[index] = inBounds(x + i, y + j, z + k)
                                && voxels[getVoxelIndex(x + i, y + j, z + k, ChunkSize + 2)] != 0;
                            ++index;
                        }
                    }
                }

                std::array<int, 24> voxelAO{};

                // Top
                voxelAO[0] = vertexAO(presence[dirToIndex(0, +1, -1)], presence[dirToIndex(-1, +1, 0)],
                                      presence[dirToIndex(-1, +1, -1)]);  // bottom left   a00
                voxelAO[1] = vertexAO(presence[dirToIndex(0, +1, -1)], presence[dirToIndex(+1, +1, 0)],
                                      presence[dirToIndex(+1, +1, -1)]);  // bottom right  a10
                voxelAO[2] = vertexAO(presence[dirToIndex(0, +1, +1)], presence[dirToIndex(+1, +1, 0)],
                                      presence[dirToIndex(+1, +1, +1)]);  // top right     a11
                voxelAO[3] = vertexAO(presence[dirToIndex(0, +1, +1)], presence[dirToIndex(-1, +1, 0)],
                                      presence[dirToIndex(-1, +1, +1)]);  // top left      a01

                // Bottom
                voxelAO[4] = vertexAO(presence[dirToIndex(0, -1, -1)], presence[dirToIndex(-1, -1, 0)],
                                      presence[dirToIndex(-1, -1, -1)]);  // bottom left
                voxelAO[5] = vertexAO(presence[dirToIndex(0, -1, -1)], presence[dirToIndex(+1, -1, 0)],
                                      presence[dirToIndex(+1, -1, -1)]);  // bottom right
                voxelAO[6] = vertexAO(presence[dirToIndex(0, -1, +1)], presence[dirToIndex(+1, -1, 0)],
                                      presence[dirToIndex(+1, -1, +1)]);  // top right
                voxelAO[7] = vertexAO(presence[dirToIndex(0, -1, +1)], presence[dirToIndex(-1, -1, 0)],
                                      presence[dirToIndex(-1, -1, +1)]);  // top left

                // Left
                voxelAO[8] = vertexAO(presence[dirToIndex(-1, 0, +1)], presence[dirToIndex(-1, -1, 0)],
                                      presence[dirToIndex(-1, -1, +1)]);  // bottom left
                voxelAO[9] = vertexAO(presence[dirToIndex(-1, 0, -1)], presence[dirToIndex(-1, -1, 0)],
                                       presence[dirToIndex(-1, -1, -1)]);  // bottom right
                voxelAO[10] = vertexAO(presence[dirToIndex(-1, 0, -1)], presence[dirToIndex(-1, +1, 0)],
                                       presence[dirToIndex(-1, +1, -1)]);  // top right
                voxelAO[11] = vertexAO(presence[dirToIndex(-1, 0, +1)], presence[dirToIndex(-1, +1, 0)],
                                       presence[dirToIndex(-1, +1, +1)]);  // top left

                // Right
                voxelAO[12] = vertexAO(presence[dirToIndex(+1, 0, -1)], presence[dirToIndex(+1, -1, 0)],
                                       presence[dirToIndex(+1, -1, -1)]);  // bottom left
                voxelAO[13] = vertexAO(presence[dirToIndex(+1, 0, +1)], presence[dirToIndex(+1, -1, 0)],
                                       presence[dirToIndex(+1, -1, +1)]);  // bottom right
                voxelAO[14] = vertexAO(presence[dirToIndex(+1, 0, +1)], presence[dirToIndex(+1, +1, 0)],
                                       presence[dirToIndex(+1, +1, +1)]);  // top right
                voxelAO[15] = vertexAO(presence[dirToIndex(+1, 0, -1)], presence[dirToIndex(+1, +1, 0)],
                                       presence[dirToIndex(+1, +1, -1)]);  // top left

                // Front
                voxelAO[16] = vertexAO(presence[dirToIndex(-1, 0, -1)], presence[dirToIndex(0, -1, -1)],
                                       presence[dirToIndex(-1, -1, -1)]);  // bottom left
                voxelAO[17] = vertexAO(presence[dirToIndex(+1, 0, -1)], presence[dirToIndex(0, -1, -1)],
                                       presence[dirToIndex(+1, -1, -1)]);  // bottom right
                voxelAO[18] = vertexAO(presence[dirToIndex(+1, 0, -1)], presence[dirToIndex(0, +1, -1)],
                                       presence[dirToIndex(+1, +1, -1)]);  // top right
                voxelAO[19] = vertexAO(presence[dirToIndex(-1, 0, -1)], presence[dirToIndex(0, +1, -1)],
                                        presence[dirToIndex(-1, +1, -1)]);  // top left

                // Back
                voxelAO[20] = vertexAO(presence[dirToIndex(+1, 0, +1)], presence[dirToIndex(0, -1, +1)],
                                       presence[dirToIndex(+1, -1, +1)]);  // bottom left
                voxelAO[21] = vertexAO(presence[dirToIndex(-1, 0, +1)], presence[dirToIndex(0, -1, +1)],
                                       presence[dirToIndex(-1, -1, +1)]);  // bottom right
                voxelAO[22] = vertexAO(presence[dirToIndex(-1, 0, +1)], presence[dirToIndex(0, +1, +1)],
                                       presence[dirToIndex(-1, +1, +1)]);  // top right
                voxelAO[23] = vertexAO(presence[dirToIndex(+1, 0, +1)], presence[dirToIndex(0, +1, +1)],
                                        presence[dirToIndex(+1, +1, +1)]);  // top left

                // Top
                if (shouldMeshFace(x, y, z, 0, 1, 0, voxels)) {
                    if (voxelAO[0] + voxelAO[2] <= voxelAO[3] + voxelAO[1]) {
                        // Flip
                        ao.push_back(voxelAO[0]);
                        ao.push_back(voxelAO[1]);
                        ao.push_back(voxelAO[3]);
                        ao.push_back(voxelAO[3]);
                        ao.push_back(voxelAO[1]);
                        ao.push_back(voxelAO[2]);
                    } else {
                        ao.push_back(voxelAO[0]);
                        ao.push_back(voxelAO[1]);
                        ao.push_back(voxelAO[2]);
                        ao.push_back(voxelAO[2]);
                        ao.push_back(voxelAO[3]);
                        ao.push_back(voxelAO[0]);
                    }
                }

                // Bottom
                if (shouldMeshFace(x, y, z, 0, -1, 0, voxels)) {
                    if (voxelAO[4] + voxelAO[6] > voxelAO[7] + voxelAO[5]) {
                        ao.push_back(voxelAO[4]);
                        ao.push_back(voxelAO[5]);
                        ao.push_back(voxelAO[7]);
                        ao.push_back(voxelAO[7]);
                        ao.push_back(voxelAO[5]);
                        ao.push_back(voxelAO[6]);
                    } else {
                        ao.push_back(voxelAO[4]);
                        ao.push_back(voxelAO[5]);
                        ao.push_back(voxelAO[6]);
                        ao.push_back(voxelAO[6]);
                        ao.push_back(voxelAO[7]);
                        ao.push_back(voxelAO[4]);
                    }
                }

                // Left
                if (shouldMeshFace(x, y, z, -1, 0, 0, voxels)) {
                    if (voxelAO[8] + voxelAO[10] > voxelAO[11] + voxelAO[9]) {
                        ao.push_back(voxelAO[11]);
                        ao.push_back(voxelAO[10]);
                        ao.push_back(voxelAO[8]);
                        ao.push_back(voxelAO[8]);
                        ao.push_back(voxelAO[10]);
                        ao.push_back(voxelAO[9]);
                    } else {
                        ao.push_back(voxelAO[11]);
                        ao.push_back(voxelAO[10]);
                        ao.push_back(voxelAO[9]);
                        ao.push_back(voxelAO[9]);
                        ao.push_back(voxelAO[8]);
                        ao.push_back(voxelAO[11]);
                    }
                }

                // Right
                if (shouldMeshFace(x, y, z, 1, 0, 0, voxels)) {
                    if (voxelAO[12] + voxelAO[14] <= voxelAO[15] + voxelAO[13]) {
                        ao.push_back(voxelAO[14]);
                        ao.push_back(voxelAO[15]);
                        ao.push_back(voxelAO[13]);
                        ao.push_back(voxelAO[13]);
                        ao.push_back(voxelAO[15]);
                        ao.push_back(voxelAO[12]);
                    } else {
                        ao.push_back(voxelAO[14]);
                        ao.push_back(voxelAO[15]);
                        ao.push_back(voxelAO[12]);
                        ao.push_back(voxelAO[12]);
                        ao.push_back(voxelAO[13]);
                        ao.push_back(voxelAO[14]);
                    }
                }

                // Front
                if (shouldMeshFace(x, y, z, 0, 0, -1, voxels)) {
                    if (voxelAO[16] + voxelAO[18] <= voxelAO[19] + voxelAO[17]) {
                        ao.push_back(voxelAO[16]);
                        ao.push_back(voxelAO[17]);
                        ao.push_back(voxelAO[19]);
                        ao.push_back(voxelAO[19]);
                        ao.push_back(voxelAO[17]);
                        ao.push_back(voxelAO[18]);
                    } else {
                        ao.push_back(voxelAO[16]);
                        ao.push_back(voxelAO[17]);
                        ao.push_back(voxelAO[18]);
                        ao.push_back(voxelAO[18]);
                        ao.push_back(voxelAO[19]);
                        ao.push_back(voxelAO[16]);
                    }
                }

                // Back
                if (shouldMeshFace(x, y, z, 0, 0, 1, voxels)) {
                    if (voxelAO[20] + voxelAO[22] > voxelAO[23] + voxelAO[21]) {
                        ao.push_back(voxelAO[21]);
                        ao.push_back(voxelAO[20]);
                        ao.push_back(voxelAO[22]);
                        ao.push_back(voxelAO[22]);
                        ao.push_back(voxelAO[20]);
                        ao.push_back(voxelAO[23]);
                    } else {
                        ao.push_back(voxelAO[21]);
                        ao.push_back(voxelAO[20]);
                        ao.push_back(voxelAO[23]);
                        ao.push_back(voxelAO[23]);
                        ao.push_back(voxelAO[22]);
                        ao.push_back(voxelAO[21]);
                    }
                }

                // Add vertices
                int translated_vertices[VerticesLength];
                for (int k = 0; k < 36; ++k) {
                    translated_vertices[3 * k] = cubeVertices[3 * k] + x - 1;
                    translated_vertices[3 * k + 1] = cubeVertices[3 * k + 1] + y;
                    translated_vertices[3 * k + 2] = cubeVertices[3 * k + 2] + z - 1;
                }

                int translated_flipped_vertices[VerticesLength];
                for (int k = 0; k < 36; ++k) {
                    translated_flipped_vertices[3 * k] = flippedCubeVertices[3 * k] + x - 1;
                    translated_flipped_vertices[3 * k + 1] = flippedCubeVertices[3 * k + 1] + y;
                    translated_flipped_vertices[3 * k + 2] = flippedCubeVertices[3 * k + 2] + z - 1;
                }

                // Top face
                if (shouldMeshFace(x, y, z, 0, 1, 0, voxels)) {
                    if (voxelAO[0] + voxelAO[2] <= voxelAO[3] + voxelAO[1]) {
                        positions.insert(positions.end(), &translated_flipped_vertices[TopFace],
                                         &translated_flipped_vertices[TopFace + 18]);
                    } else {
                        positions.insert(positions.end(), &translated_vertices[TopFace],
                                         &translated_vertices[TopFace + 18]);
                    }
                    for (int i = 0; i < 6; i++) {
                        normals.push_back(TopNormal);
                    }

                    // Subtract 1 because empty voxel is 0, so we don't need a palette slot for it
                    colours.push_back(voxel - 1);
                }

                // Bottom
                if (shouldMeshFace(x, y, z, 0, -1, 0, voxels)) {
                    if (voxelAO[4] + voxelAO[6] > voxelAO[7] + voxelAO[5]) {
                        positions.insert(positions.end(), &translated_flipped_vertices[BottomFace],
                                         &translated_flipped_vertices[BottomFace + 18]);
                    } else {
                        positions.insert(positions.end(), &translated_vertices[BottomFace],
                                         &translated_vertices[BottomFace + 18]);
                    }
                    for (int i = 0; i < 6; i++) {
                        normals.push_back(BottomNormal);
                    }
                    colours.push_back(voxel - 1);
                }

                // Left
                if (shouldMeshFace(x, y, z, -1, 0, 0, voxels)) {
                    if (voxelAO[8] + voxelAO[10] > voxelAO[11] + voxelAO[9]) {
                        positions.insert(positions.end(), &translated_flipped_vertices[LeftFace],
                                         &translated_flipped_vertices[LeftFace + 18]);
                    } else {
                        positions.insert(positions.end(), &translated_vertices[LeftFace],
                                         &translated_vertices[LeftFace + 18]);
                    }
                    for (int i = 0; i < 6; i++) {
                        normals.push_back(LeftNormal);
                    }
                    colours.push_back(voxel - 1);
                }

                // Right
                if (shouldMeshFace(x, y, z, 1, 0, 0, voxels)) {
                    if (voxelAO[12] + voxelAO[14] <= voxelAO[15] + voxelAO[13]) {
                        positions.insert(positions.end(), &translated_flipped_vertices[RightFace],
                                         &translated_flipped_vertices[RightFace + 18]);
                    } else {
                        positions.insert(positions.end(), &translated_vertices[RightFace],
                                         &translated_vertices[RightFace + 18]);
                    }
                    for (int i = 0; i < 6; i++) {
                        normals.push_back(RightNormal);
                    }
                    colours.push_back(voxel - 1);
                }

                // Front
                if (shouldMeshFace(x, y, z, 0, 0, -1, voxels)) {
                    if (voxelAO[16] + voxelAO[18] <= voxelAO[19] + voxelAO[17]) {
                        positions.insert(positions.end(), &translated_flipped_vertices[FrontFace],
                                         &translated_flipped_vertices[FrontFace + 18]);
                    } else {
                        positions.insert(positions.end(), &translated_vertices[FrontFace],
                                         &translated_vertices[FrontFace + 18]);
                    }
                    for (int i = 0; i < 6; i++) {
                        normals.push_back(FrontNormal);
                    }
                    colours.push_back(voxel - 1);
                }

                // Back
                if (shouldMeshFace(x, y, z, 0, 0, 1, voxels)) {
                    if (voxelAO[20] + voxelAO[22] > voxelAO[23] + voxelAO[21]) {
                        positions.insert(positions.end(), &translated_flipped_vertices[BackFace],
                                         &translated_flipped_vertices[BackFace + 18]);
                    } else {
                        positions.insert(positions.end(), &translated_vertices[BackFace],
                                         &translated_vertices[BackFace + 18]);
                    }
                    for (int i = 0; i < 6; i++) {
                        normals.push_back(BackNormal);
                    }
                    colours.push_back(voxel - 1);
                }
            }
        }
    }

    std::vector<uint32_t> vertices;
    vertices.resize(positions.size() / 3);
    for (int i = 0; i < positions.size() / 3; ++i) {
        uint32_t vertex =
                static_cast<uint32_t>(positions[3 * i]) |
                static_cast<uint32_t>(positions[3 * i + 1]) << VertexFormat::YShift |
                static_cast<uint32_t>(positions[3 * i + 2]) << VertexFormat::ZShift |
                static_cast<uint32_t>(colours[i / 6]) << VertexFormat::ColourShift |
                static_cast<uint32_t>(normals[i]) << VertexFormat::NormalShift |
                static_cast<uint32_t>(ao[i]) << VertexFormat::AOShift;

        vertices[i] = vertex;
    }

    return {
        .chunk = chunk,
        .vertices = std::move(vertices)
    };
}

bool Mesher::shouldMeshFace(const int x, const int y, const int z, const int i, const int j, const int k, const std::vector<int>& voxels) {
    const bool adjInBounds = y + j >= 0 && y + j < ChunkHeight + 1;

    bool isAdjVoxelEmpty = true;
    if (adjInBounds) {
        isAdjVoxelEmpty = voxels[getVoxelIndex(x + i, y + j, z + k, ChunkSize + 2)] == 0;
    }

    return adjInBounds && isAdjVoxelEmpty;
}
