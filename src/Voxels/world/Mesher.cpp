#include "Mesher.hpp"

#include "../util/PerlinNoise.hpp"
#include "../util/Util.hpp"

constexpr int FRONT_FACE = 0;
constexpr int BACK_FACE = 18;
constexpr int LEFT_FACE = 36;
constexpr int RIGHT_FACE = 54;
constexpr int BOTTOM_FACE = 72;
constexpr int TOP_FACE = 90;

constexpr int VERTICES_LENGTH = 108;

constexpr int FRONT_NORMAL = 0;
constexpr int BACK_NORMAL = 1;
constexpr int LEFT_NORMAL = 2;
constexpr int RIGHT_NORMAL = 3;
constexpr int BOTTOM_NORMAL = 4;
constexpr int TOP_NORMAL = 5;

namespace {
    constexpr int cube_vertices[] = {
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

    constexpr int flipped_cube_vertices[] = {
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

inline int Mesher::vertexAO(uint8_t side1, uint8_t side2, uint8_t corner) {
    if (side1 == 1 || side2 == 1) corner = 0;
    return (side1 && side2) ? 0 : (3 - (side1 + side2 + corner));
}

bool Mesher::inBounds(int x, int y, int z) {
    constexpr int size = CHUNK_SIZE + 2;
    constexpr int height = CHUNK_HEIGHT;
    return (0 <= x && x < size)
        && (0 <= y && y < height)
        && (0 <= z && z < size);
}

int Mesher::dirToIndex(int i, int j, int k) {
    return (i + 1) * 9 + (j + 1) * 3 + k + 1;
}

void Mesher::meshChunk(Chunk *chunk, GenerationType generationType) {
    std::vector<int> &voxels = chunk->voxels;
    std::vector<int> positions;
    std::vector<int> colours;
    std::vector<int> normals;
    std::vector<int> ao;

    for (int y = chunk->minY; y < chunk->maxY; ++y) {
        for (int z = 1; z < CHUNK_SIZE + 1; ++z) {
            for (int x = 1; x < CHUNK_SIZE + 1; ++x) {
                int voxel = voxels[getVoxelIndex(x, y, z, CHUNK_SIZE + 2)];
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
                                && voxels[getVoxelIndex(x + i, y + j, z + k, CHUNK_SIZE + 2)] != 0;
                            ++index;
                        }
                    }
                }

                std::array<int, 24> voxelAo{};

                // Top
                voxelAo[0] = (vertexAO(presence[dirToIndex(0, +1, -1)], presence[dirToIndex(-1, +1, 0)],
                                       presence[dirToIndex(-1, +1, -1)]));  // bottom left   a00
                voxelAo[1] = (vertexAO(presence[dirToIndex(0, +1, -1)], presence[dirToIndex(+1, +1, 0)],
                                       presence[dirToIndex(+1, +1, -1)]));  // bottom right  a10
                voxelAo[2] = (vertexAO(presence[dirToIndex(0, +1, +1)], presence[dirToIndex(+1, +1, 0)],
                                       presence[dirToIndex(+1, +1, +1)]));  // top right     a11
                voxelAo[3] = (vertexAO(presence[dirToIndex(0, +1, +1)], presence[dirToIndex(-1, +1, 0)],
                                       presence[dirToIndex(-1, +1, +1)]));  // top left      a01

                // Bottom
                voxelAo[4] = (vertexAO(presence[dirToIndex(0, -1, -1)], presence[dirToIndex(-1, -1, 0)],
                                       presence[dirToIndex(-1, -1, -1)]));  // bottom left
                voxelAo[5] = (vertexAO(presence[dirToIndex(0, -1, -1)], presence[dirToIndex(+1, -1, 0)],
                                       presence[dirToIndex(+1, -1, -1)]));  // bottom right
                voxelAo[6] = (vertexAO(presence[dirToIndex(0, -1, +1)], presence[dirToIndex(+1, -1, 0)],
                                       presence[dirToIndex(+1, -1, +1)]));  // top right
                voxelAo[7] = (vertexAO(presence[dirToIndex(0, -1, +1)], presence[dirToIndex(-1, -1, 0)],
                                       presence[dirToIndex(-1, -1, +1)]));  // top left

                // Left
                voxelAo[8] = (vertexAO(presence[dirToIndex(-1, 0, +1)], presence[dirToIndex(-1, -1, 0)],
                                       presence[dirToIndex(-1, -1, +1)]));  // bottom left
                voxelAo[9] = (vertexAO(presence[dirToIndex(-1, 0, -1)], presence[dirToIndex(-1, -1, 0)],
                                       presence[dirToIndex(-1, -1, -1)]));  // bottom right
                voxelAo[10] = (vertexAO(presence[dirToIndex(-1, 0, -1)], presence[dirToIndex(-1, +1, 0)],
                                        presence[dirToIndex(-1, +1, -1)]));  // top right
                voxelAo[11] = (vertexAO(presence[dirToIndex(-1, 0, +1)], presence[dirToIndex(-1, +1, 0)],
                                        presence[dirToIndex(-1, +1, +1)]));  // top left

                // Right
                voxelAo[12] = (vertexAO(presence[dirToIndex(+1, 0, -1)], presence[dirToIndex(+1, -1, 0)],
                                        presence[dirToIndex(+1, -1, -1)]));  // bottom left
                voxelAo[13] = (vertexAO(presence[dirToIndex(+1, 0, +1)], presence[dirToIndex(+1, -1, 0)],
                                        presence[dirToIndex(+1, -1, +1)]));  // bottom right
                voxelAo[14] = (vertexAO(presence[dirToIndex(+1, 0, +1)], presence[dirToIndex(+1, +1, 0)],
                                        presence[dirToIndex(+1, +1, +1)]));  // top right
                voxelAo[15] = (vertexAO(presence[dirToIndex(+1, 0, -1)], presence[dirToIndex(+1, +1, 0)],
                                        presence[dirToIndex(+1, +1, -1)]));  // top left

                // Front
                voxelAo[16] = (vertexAO(presence[dirToIndex(-1, 0, -1)], presence[dirToIndex(0, -1, -1)],
                                        presence[dirToIndex(-1, -1, -1)]));  // bottom left
                voxelAo[17] = (vertexAO(presence[dirToIndex(+1, 0, -1)], presence[dirToIndex(0, -1, -1)],
                                        presence[dirToIndex(+1, -1, -1)]));  // bottom right
                voxelAo[18] = (vertexAO(presence[dirToIndex(+1, 0, -1)], presence[dirToIndex(0, +1, -1)],
                                        presence[dirToIndex(+1, +1, -1)]));  // top right
                voxelAo[19] = (vertexAO(presence[dirToIndex(-1, 0, -1)], presence[dirToIndex(0, +1, -1)],
                                        presence[dirToIndex(-1, +1, -1)]));  // top left

                // Back
                voxelAo[20] = (vertexAO(presence[dirToIndex(+1, 0, +1)], presence[dirToIndex(0, -1, +1)],
                                        presence[dirToIndex(+1, -1, +1)]));  // bottom left
                voxelAo[21] = (vertexAO(presence[dirToIndex(-1, 0, +1)], presence[dirToIndex(0, -1, +1)],
                                        presence[dirToIndex(-1, -1, +1)]));  // bottom right
                voxelAo[22] = (vertexAO(presence[dirToIndex(-1, 0, +1)], presence[dirToIndex(0, +1, +1)],
                                        presence[dirToIndex(-1, +1, +1)]));  // top right
                voxelAo[23] = (vertexAO(presence[dirToIndex(+1, 0, +1)], presence[dirToIndex(0, +1, +1)],
                                        presence[dirToIndex(+1, +1, +1)]));  // top left

                // Top
                if (shouldMeshFace(x, y, z, 0, 1, 0, voxels)) {
                    if (voxelAo[0] + voxelAo[2] <= voxelAo[3] + voxelAo[1]) {
                        // Flip
                        ao.push_back(voxelAo[0]);
                        ao.push_back(voxelAo[1]);
                        ao.push_back(voxelAo[3]);
                        ao.push_back(voxelAo[3]);
                        ao.push_back(voxelAo[1]);
                        ao.push_back(voxelAo[2]);
                    } else {
                        ao.push_back(voxelAo[0]);
                        ao.push_back(voxelAo[1]);
                        ao.push_back(voxelAo[2]);
                        ao.push_back(voxelAo[2]);
                        ao.push_back(voxelAo[3]);
                        ao.push_back(voxelAo[0]);
                    }
                }

                // Bottom
                if (shouldMeshFace(x, y, z, 0, -1, 0, voxels)) {
                    if (voxelAo[4] + voxelAo[6] > voxelAo[7] + voxelAo[5]) {
                        ao.push_back(voxelAo[4]);
                        ao.push_back(voxelAo[5]);
                        ao.push_back(voxelAo[7]);
                        ao.push_back(voxelAo[7]);
                        ao.push_back(voxelAo[5]);
                        ao.push_back(voxelAo[6]);
                    } else {
                        ao.push_back(voxelAo[4]);
                        ao.push_back(voxelAo[5]);
                        ao.push_back(voxelAo[6]);
                        ao.push_back(voxelAo[6]);
                        ao.push_back(voxelAo[7]);
                        ao.push_back(voxelAo[4]);
                    }
                }

                // Left
                if (shouldMeshFace(x, y, z, -1, 0, 0, voxels)) {
                    if (voxelAo[8] + voxelAo[10] > voxelAo[11] + voxelAo[9]) {
                        ao.push_back(voxelAo[11]);
                        ao.push_back(voxelAo[10]);
                        ao.push_back(voxelAo[8]);
                        ao.push_back(voxelAo[8]);
                        ao.push_back(voxelAo[10]);
                        ao.push_back(voxelAo[9]);
                    } else {
                        ao.push_back(voxelAo[11]);
                        ao.push_back(voxelAo[10]);
                        ao.push_back(voxelAo[9]);
                        ao.push_back(voxelAo[9]);
                        ao.push_back(voxelAo[8]);
                        ao.push_back(voxelAo[11]);
                    }
                }

                // Right
                if (shouldMeshFace(x, y, z, 1, 0, 0, voxels)) {
                    if (voxelAo[12] + voxelAo[14] <= voxelAo[15] + voxelAo[13]) {
                        ao.push_back(voxelAo[14]);
                        ao.push_back(voxelAo[15]);
                        ao.push_back(voxelAo[13]);
                        ao.push_back(voxelAo[13]);
                        ao.push_back(voxelAo[15]);
                        ao.push_back(voxelAo[12]);
                    } else {
                        ao.push_back(voxelAo[14]);
                        ao.push_back(voxelAo[15]);
                        ao.push_back(voxelAo[12]);
                        ao.push_back(voxelAo[12]);
                        ao.push_back(voxelAo[13]);
                        ao.push_back(voxelAo[14]);
                    }
                }

                // Front
                if (shouldMeshFace(x, y, z, 0, 0, -1, voxels)) {
                    if (voxelAo[16] + voxelAo[18] <= voxelAo[19] + voxelAo[17]) {
                        ao.push_back(voxelAo[16]);
                        ao.push_back(voxelAo[17]);
                        ao.push_back(voxelAo[19]);
                        ao.push_back(voxelAo[19]);
                        ao.push_back(voxelAo[17]);
                        ao.push_back(voxelAo[18]);
                    } else {
                        ao.push_back(voxelAo[16]);
                        ao.push_back(voxelAo[17]);
                        ao.push_back(voxelAo[18]);
                        ao.push_back(voxelAo[18]);
                        ao.push_back(voxelAo[19]);
                        ao.push_back(voxelAo[16]);
                    }
                }

                // Back
                if (shouldMeshFace(x, y, z, 0, 0, 1, voxels)) {
                    if (voxelAo[20] + voxelAo[22] > voxelAo[23] + voxelAo[21]) {
                        ao.push_back(voxelAo[21]);
                        ao.push_back(voxelAo[20]);
                        ao.push_back(voxelAo[22]);
                        ao.push_back(voxelAo[22]);
                        ao.push_back(voxelAo[20]);
                        ao.push_back(voxelAo[23]);
                    } else {
                        ao.push_back(voxelAo[21]);
                        ao.push_back(voxelAo[20]);
                        ao.push_back(voxelAo[23]);
                        ao.push_back(voxelAo[23]);
                        ao.push_back(voxelAo[22]);
                        ao.push_back(voxelAo[21]);
                    }
                }

                // Add vertices
                int translated_vertices[VERTICES_LENGTH];
                for (int k = 0; k < 36; ++k) {
                    translated_vertices[3 * k] = cube_vertices[3 * k] + x - 1;
                    translated_vertices[3 * k + 1] = cube_vertices[3 * k + 1] + y;
                    translated_vertices[3 * k + 2] = cube_vertices[3 * k + 2] + z - 1;
                }

                int translated_flipped_vertices[VERTICES_LENGTH];
                for (int k = 0; k < 36; ++k) {
                    translated_flipped_vertices[3 * k] = flipped_cube_vertices[3 * k] + x - 1;
                    translated_flipped_vertices[3 * k + 1] = flipped_cube_vertices[3 * k + 1] + y;
                    translated_flipped_vertices[3 * k + 2] = flipped_cube_vertices[3 * k + 2] + z - 1;
                }

                // Top face
                if (shouldMeshFace(x, y, z, 0, 1, 0, voxels)) {
                    if (voxelAo[0] + voxelAo[2] <= voxelAo[3] + voxelAo[1]) {
                        positions.insert(positions.end(), &translated_flipped_vertices[TOP_FACE],
                                         &translated_flipped_vertices[TOP_FACE + 18]);
                    } else {
                        positions.insert(positions.end(), &translated_vertices[TOP_FACE],
                                         &translated_vertices[TOP_FACE + 18]);
                    }
                    for (int i = 0; i < 6; i++) {
                        normals.push_back(TOP_NORMAL);
                    }
                    colours.push_back(voxel);
                }

                // Bottom
                if (shouldMeshFace(x, y, z, 0, -1, 0, voxels)) {
                    if (voxelAo[4] + voxelAo[6] > voxelAo[7] + voxelAo[5]) {
                        positions.insert(positions.end(), &translated_flipped_vertices[BOTTOM_FACE],
                                         &translated_flipped_vertices[BOTTOM_FACE + 18]);
                    } else {
                        positions.insert(positions.end(), &translated_vertices[BOTTOM_FACE],
                                         &translated_vertices[BOTTOM_FACE + 18]);
                    }
                    for (int i = 0; i < 6; i++) {
                        normals.push_back(BOTTOM_NORMAL);
                    }
                    colours.push_back(voxel);
                }

                // Left
                if (shouldMeshFace(x, y, z, -1, 0, 0, voxels)) {
                    if (voxelAo[8] + voxelAo[10] > voxelAo[11] + voxelAo[9]) {
                        positions.insert(positions.end(), &translated_flipped_vertices[LEFT_FACE],
                                         &translated_flipped_vertices[LEFT_FACE + 18]);
                    } else {
                        positions.insert(positions.end(), &translated_vertices[LEFT_FACE],
                                         &translated_vertices[LEFT_FACE + 18]);
                    }
                    for (int i = 0; i < 6; i++) {
                        normals.push_back(LEFT_NORMAL);
                    }
                    colours.push_back(voxel);
                }

                // Right
                if (shouldMeshFace(x, y, z, 1, 0, 0, voxels)) {
                    if (voxelAo[12] + voxelAo[14] <= voxelAo[15] + voxelAo[13]) {
                        positions.insert(positions.end(), &translated_flipped_vertices[RIGHT_FACE],
                                         &translated_flipped_vertices[RIGHT_FACE + 18]);
                    } else {
                        positions.insert(positions.end(), &translated_vertices[RIGHT_FACE],
                                         &translated_vertices[RIGHT_FACE + 18]);
                    }
                    for (int i = 0; i < 6; i++) {
                        normals.push_back(RIGHT_NORMAL);
                    }
                    colours.push_back(voxel);
                }

                // Front
                if (shouldMeshFace(x, y, z, 0, 0, -1, voxels)) {
                    if (voxelAo[16] + voxelAo[18] <= voxelAo[19] + voxelAo[17]) {
                        positions.insert(positions.end(), &translated_flipped_vertices[FRONT_FACE],
                                         &translated_flipped_vertices[FRONT_FACE + 18]);
                    } else {
                        positions.insert(positions.end(), &translated_vertices[FRONT_FACE],
                                         &translated_vertices[FRONT_FACE + 18]);
                    }
                    for (int i = 0; i < 6; i++) {
                        normals.push_back(FRONT_NORMAL);
                    }
                    colours.push_back(voxel);
                }

                // Back
                if (shouldMeshFace(x, y, z, 0, 0, 1, voxels)) {
                    if (voxelAo[20] + voxelAo[22] > voxelAo[23] + voxelAo[21]) {
                        positions.insert(positions.end(), &translated_flipped_vertices[BACK_FACE],
                                         &translated_flipped_vertices[BACK_FACE + 18]);
                    } else {
                        positions.insert(positions.end(), &translated_vertices[BACK_FACE],
                                         &translated_vertices[BACK_FACE + 18]);
                    }
                    for (int i = 0; i < 6; i++) {
                        normals.push_back(BACK_NORMAL);
                    }
                    colours.push_back(voxel);
                }
            }
        }
    }

    chunk->vertices.reserve(positions.size() / 3);
    for (int i = 0; i < positions.size() / 3; ++i) {
        uint32_t vertex =
                (uint32_t)positions[3 * i] |
                ((uint32_t)positions[3 * i + 1] << VertexFormat::Y_SHIFT) |
                ((uint32_t)positions[3 * i + 2] << VertexFormat::Z_SHIFT) |
                ((uint32_t)colours[i / 6] << VertexFormat::COLOUR_SHIFT) |
                ((uint32_t)normals[i] << VertexFormat::NORMAL_SHIFT) |
                ((uint32_t)ao[i] << VertexFormat::AO_SHIFT);

        chunk->vertices.push_back(vertex);
    }

    chunk->numVertices = chunk->vertices.size();
}

bool Mesher::shouldMeshFace(int x, int y, int z, int i, int j, int k, std::vector<int> &voxels) {
    bool adjInBounds = y + j >= 0 && y + j < CHUNK_HEIGHT + 1;

    bool isAdjVoxelEmpty = true;
    if (adjInBounds) {
        isAdjVoxelEmpty = voxels[getVoxelIndex(x + i, y + j, z + k, CHUNK_SIZE + 2)] == 0;
    }

    return adjInBounds && isAdjVoxelEmpty;
}
