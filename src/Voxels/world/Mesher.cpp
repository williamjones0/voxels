#include "Mesher.hpp"

#include <cstring>
#include <iostream>

#include "../util/PerlinNoise.hpp"
#include "../util/Util.hpp"

#define FRONT_FACE 0
#define BACK_FACE 18
#define LEFT_FACE 36
#define RIGHT_FACE 54
#define BOTTOM_FACE 72
#define TOP_FACE 90

#define VERTICES_LENGTH 108

#define FRONT_NORMAL 0
#define BACK_NORMAL 1
#define LEFT_NORMAL 2
#define RIGHT_NORMAL 3
#define BOTTOM_NORMAL 4
#define TOP_NORMAL 5

static inline int vertexAO(uint8_t side1, uint8_t side2, uint8_t corner) {
    return (side1 && side2) ? 0 : (3 - (side1 + side2 + corner));
}

int dirToIndex(int i, int j, int k) {
    return (i + 1) * 9 + (j + 1) * 3 + k + 1;
}

uint32_t createVertex(int x, int y, int z, int colour, int normal, int ao) {
    return (uint32_t) x |
           ((uint32_t) y << 11) |
           ((uint32_t) z << 22) |
           (colour < 33) |
           ((uint32_t) normal << 34) |
           ((uint32_t) ao << 37);
}

void meshChunk(Chunk *chunk, int worldSize, std::vector<uint32_t> &data) {
    std::vector<int> &voxels = chunk->voxels;
    std::vector<int> positions;
    std::vector<float> colours;
    std::vector<int> normals;
    std::vector<int> ao;

    int vertices[] = {
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

    int flipped_vertices[] = {
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

    float red_face[] = {
            0.6, 0.1, 0.1,
            0.6, 0.1, 0.1,
            0.6, 0.1, 0.1,
            0.6, 0.1, 0.1,
            0.6, 0.1, 0.1,
            0.6, 0.1, 0.1
    };

    float green_face[] = {
            0.278, 0.600, 0.141,
            0.278, 0.600, 0.141,
            0.278, 0.600, 0.141,
            0.278, 0.600, 0.141,
            0.278, 0.600, 0.141,
            0.278, 0.600, 0.141
    };

    for (int y = 0; y < CHUNK_HEIGHT - 1; ++y) {
        for (int z = 1; z < CHUNK_SIZE + 1; ++z) {
            for (int x = 1; x < CHUNK_SIZE + 1; ++x) {
                int voxel = voxels[getVoxelIndex(x, y, z, CHUNK_SIZE + 2)];
                if (voxel != 0) {
                    // Ambient occlusion (computed first so that quads can be flipped if necessary)
                    // (-1, -1, -1) to (1, 1, 1)
                    std::vector<bool> presence;

                    for (int i = -1; i <= 1; ++i) {
                        for (int j = -1; j <= 1; ++j) {
                            for (int k = -1; k <= 1; ++k) {
                                presence.push_back(inBounds(x + i, y + j, z + k, CHUNK_SIZE + 2, CHUNK_HEIGHT)
                                                   && voxels[getVoxelIndex(x + i, y + j, z + k, CHUNK_SIZE + 2)] != 0);
                            }
                        }
                    }

                    std::vector<int> voxelAo;

                    // Top
                    voxelAo.push_back(vertexAO(presence[dirToIndex(0, +1, -1)], presence[dirToIndex(-1, +1, 0)],
                                               presence[dirToIndex(-1, +1, -1)]));  // bottom left   a00
                    voxelAo.push_back(vertexAO(presence[dirToIndex(0, +1, -1)], presence[dirToIndex(+1, +1, 0)],
                                               presence[dirToIndex(+1, +1, -1)]));  // bottom right  a10
                    voxelAo.push_back(vertexAO(presence[dirToIndex(0, +1, +1)], presence[dirToIndex(+1, +1, 0)],
                                               presence[dirToIndex(+1, +1, +1)]));  // top right     a11
                    voxelAo.push_back(vertexAO(presence[dirToIndex(0, +1, +1)], presence[dirToIndex(-1, +1, 0)],
                                               presence[dirToIndex(-1, +1, +1)]));  // top left      a01

                    // Left
                    voxelAo.push_back(vertexAO(presence[dirToIndex(-1, 0, +1)], presence[dirToIndex(-1, -1, 0)],
                                               presence[dirToIndex(-1, -1, +1)]));  // bottom left
                    voxelAo.push_back(vertexAO(presence[dirToIndex(-1, 0, -1)], presence[dirToIndex(-1, -1, 0)],
                                               presence[dirToIndex(-1, -1, -1)]));  // bottom right
                    voxelAo.push_back(vertexAO(presence[dirToIndex(-1, 0, -1)], presence[dirToIndex(-1, +1, 0)],
                                               presence[dirToIndex(-1, +1, -1)]));  // top right
                    voxelAo.push_back(vertexAO(presence[dirToIndex(-1, 0, +1)], presence[dirToIndex(-1, +1, 0)],
                                               presence[dirToIndex(-1, +1, +1)]));  // top left

                    // Right
                    voxelAo.push_back(vertexAO(presence[dirToIndex(+1, 0, -1)], presence[dirToIndex(+1, -1, 0)],
                                               presence[dirToIndex(+1, -1, -1)]));  // bottom left
                    voxelAo.push_back(vertexAO(presence[dirToIndex(+1, 0, +1)], presence[dirToIndex(+1, -1, 0)],
                                               presence[dirToIndex(+1, -1, +1)]));  // bottom right
                    voxelAo.push_back(vertexAO(presence[dirToIndex(+1, 0, +1)], presence[dirToIndex(+1, +1, 0)],
                                               presence[dirToIndex(+1, +1, +1)]));  // top right
                    voxelAo.push_back(vertexAO(presence[dirToIndex(+1, 0, -1)], presence[dirToIndex(+1, +1, 0)],
                                               presence[dirToIndex(+1, +1, -1)]));  // top left

                    // Front
                    voxelAo.push_back(vertexAO(presence[dirToIndex(-1, 0, -1)], presence[dirToIndex(0, -1, -1)],
                                               presence[dirToIndex(-1, -1, -1)]));  // bottom left
                    voxelAo.push_back(vertexAO(presence[dirToIndex(+1, 0, -1)], presence[dirToIndex(0, -1, -1)],
                                               presence[dirToIndex(+1, -1, -1)]));  // bottom right
                    voxelAo.push_back(vertexAO(presence[dirToIndex(+1, 0, -1)], presence[dirToIndex(0, +1, -1)],
                                               presence[dirToIndex(+1, +1, -1)]));  // top right
                    voxelAo.push_back(vertexAO(presence[dirToIndex(-1, 0, -1)], presence[dirToIndex(0, +1, -1)],
                                               presence[dirToIndex(-1, +1, -1)]));  // top left

                    // Back
                    voxelAo.push_back(vertexAO(presence[dirToIndex(+1, 0, +1)], presence[dirToIndex(0, -1, +1)],
                                               presence[dirToIndex(+1, -1, +1)]));  // bottom left
                    voxelAo.push_back(vertexAO(presence[dirToIndex(-1, 0, +1)], presence[dirToIndex(0, -1, +1)],
                                               presence[dirToIndex(-1, -1, +1)]));  // bottom right
                    voxelAo.push_back(vertexAO(presence[dirToIndex(-1, 0, +1)], presence[dirToIndex(0, +1, +1)],
                                               presence[dirToIndex(-1, +1, +1)]));  // top right
                    voxelAo.push_back(vertexAO(presence[dirToIndex(+1, 0, +1)], presence[dirToIndex(0, +1, +1)],
                                               presence[dirToIndex(+1, +1, +1)]));  // top left

                    // Top
                    if (voxel != 2) {
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

                    // Left
                    if (boundsCheck(x, y, z, -1, 0, 0, worldSize, voxels)) {
                        if (voxelAo[4] + voxelAo[6] > voxelAo[7] + voxelAo[5]) {
                            ao.push_back(voxelAo[7]);
                            ao.push_back(voxelAo[6]);
                            ao.push_back(voxelAo[4]);
                            ao.push_back(voxelAo[4]);
                            ao.push_back(voxelAo[6]);
                            ao.push_back(voxelAo[5]);
                        } else {
                            ao.push_back(voxelAo[7]);
                            ao.push_back(voxelAo[6]);
                            ao.push_back(voxelAo[5]);
                            ao.push_back(voxelAo[5]);
                            ao.push_back(voxelAo[4]);
                            ao.push_back(voxelAo[7]);
                        }
                    }

                    // Right
                    if (boundsCheck(x, y, z, 1, 0, 0, worldSize, voxels)) {
                        if (voxelAo[8] + voxelAo[10] > voxelAo[11] + voxelAo[9]) {
                            ao.push_back(voxelAo[10]);
                            ao.push_back(voxelAo[11]);
                            ao.push_back(voxelAo[9]);
                            ao.push_back(voxelAo[9]);
                            ao.push_back(voxelAo[11]);
                            ao.push_back(voxelAo[8]);
                        } else {
                            ao.push_back(voxelAo[10]);
                            ao.push_back(voxelAo[11]);
                            ao.push_back(voxelAo[8]);
                            ao.push_back(voxelAo[8]);
                            ao.push_back(voxelAo[9]);
                            ao.push_back(voxelAo[10]);
                        }
                    }

                    // Front
                    if (boundsCheck(x, y, z, 0, 0, -1, worldSize, voxels)) {
                        if (voxelAo[12] + voxelAo[14] > voxelAo[15] + voxelAo[13]) {
                            ao.push_back(voxelAo[12]);
                            ao.push_back(voxelAo[13]);
                            ao.push_back(voxelAo[15]);
                            ao.push_back(voxelAo[15]);
                            ao.push_back(voxelAo[13]);
                            ao.push_back(voxelAo[14]);
                        } else {
                            ao.push_back(voxelAo[12]);
                            ao.push_back(voxelAo[13]);
                            ao.push_back(voxelAo[14]);
                            ao.push_back(voxelAo[14]);
                            ao.push_back(voxelAo[15]);
                            ao.push_back(voxelAo[12]);
                        }
                    }

                    // Back
                    if (boundsCheck(x, y, z, 0, 0, 1, worldSize, voxels)) {
                        if (voxelAo[16] + voxelAo[18] > voxelAo[19] + voxelAo[17]) {
                            ao.push_back(voxelAo[17]);
                            ao.push_back(voxelAo[16]);
                            ao.push_back(voxelAo[18]);
                            ao.push_back(voxelAo[18]);
                            ao.push_back(voxelAo[16]);
                            ao.push_back(voxelAo[19]);
                        } else {
                            ao.push_back(voxelAo[17]);
                            ao.push_back(voxelAo[16]);
                            ao.push_back(voxelAo[19]);
                            ao.push_back(voxelAo[19]);
                            ao.push_back(voxelAo[18]);
                            ao.push_back(voxelAo[17]);
                        }
                    }

                    // Add vertices
                    int *translated_vertices = new int[VERTICES_LENGTH];
                    std::memcpy(translated_vertices, vertices, VERTICES_LENGTH * sizeof(int));
                    for (int k = 0; k < 36; ++k) {
                        translated_vertices[3 * k] += x - 1;
                        translated_vertices[3 * k + 1] += y;
                        translated_vertices[3 * k + 2] += z - 1;
                    }

                    int *translated_flipped_vertices = new int[VERTICES_LENGTH];
                    std::memcpy(translated_flipped_vertices, flipped_vertices, VERTICES_LENGTH * sizeof(int));
                    for (int k = 0; k < 36; ++k) {
                        translated_flipped_vertices[3 * k] += x - 1;
                        translated_flipped_vertices[3 * k + 1] += y;
                        translated_flipped_vertices[3 * k + 2] += z - 1;
                    }

                    // Top face
                    if (voxel != 2) {
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
                        colours.insert(colours.end(), &green_face[0], &green_face[18]);
                    }

                    // Left
                    if (boundsCheck(x, y, z, -1, 0, 0, worldSize, voxels)) {
                        if (voxelAo[4] + voxelAo[6] > voxelAo[7] + voxelAo[5]) {
                            positions.insert(positions.end(), &translated_flipped_vertices[LEFT_FACE],
                                             &translated_flipped_vertices[LEFT_FACE + 18]);
                        } else {
                            positions.insert(positions.end(), &translated_vertices[LEFT_FACE],
                                             &translated_vertices[LEFT_FACE + 18]);
                        }
                        for (int i = 0; i < 6; i++) {
                            normals.push_back(LEFT_NORMAL);
                        }
                        if (voxel == 1) {
                            colours.insert(colours.end(), &green_face[0], &green_face[18]);
                        } else {
                            colours.insert(colours.end(), &red_face[0], &red_face[18]);
                        }
                    }

                    // Right
                    if (boundsCheck(x, y, z, 1, 0, 0, worldSize, voxels)) {
                        if (voxelAo[8] + voxelAo[10] > voxelAo[11] + voxelAo[9]) {
                            positions.insert(positions.end(), &translated_flipped_vertices[RIGHT_FACE],
                                             &translated_flipped_vertices[RIGHT_FACE + 18]);
                        } else {
                            positions.insert(positions.end(), &translated_vertices[RIGHT_FACE],
                                             &translated_vertices[RIGHT_FACE + 18]);
                        }
                        for (int i = 0; i < 6; i++) {
                            normals.push_back(RIGHT_NORMAL);
                        }
                        if (voxel == 1) {
                            colours.insert(colours.end(), &green_face[0], &green_face[18]);
                        } else {
                            colours.insert(colours.end(), &red_face[0], &red_face[18]);
                        }
                    }

                    // Front
                    if (boundsCheck(x, y, z, 0, 0, -1, worldSize, voxels)) {
                        if (voxelAo[12] + voxelAo[14] > voxelAo[15] + voxelAo[13]) {
                            positions.insert(positions.end(), &translated_flipped_vertices[FRONT_FACE],
                                             &translated_flipped_vertices[FRONT_FACE + 18]);
                        } else {
                            positions.insert(positions.end(), &translated_vertices[FRONT_FACE],
                                             &translated_vertices[FRONT_FACE + 18]);
                        }
                        for (int i = 0; i < 6; i++) {
                            normals.push_back(FRONT_NORMAL);
                        }
                        if (voxel == 1) {
                            colours.insert(colours.end(), &green_face[0], &green_face[18]);
                        } else {
                            colours.insert(colours.end(), &red_face[0], &red_face[18]);
                        }
                    }

                    // Back
                    if (boundsCheck(x, y, z, 0, 0, 1, worldSize, voxels)) {
                        if (voxelAo[16] + voxelAo[18] > voxelAo[19] + voxelAo[17]) {
                            positions.insert(positions.end(), &translated_flipped_vertices[BACK_FACE],
                                             &translated_flipped_vertices[BACK_FACE + 18]);
                        } else {
                            positions.insert(positions.end(), &translated_vertices[BACK_FACE],
                                             &translated_vertices[BACK_FACE + 18]);
                        }
                        for (int i = 0; i < 6; i++) {
                            normals.push_back(BACK_NORMAL);
                        }
                        if (voxel == 1) {
                            colours.insert(colours.end(), &green_face[0], &green_face[18]);
                        } else {
                            colours.insert(colours.end(), &red_face[0], &red_face[18]);
                        }
                    }

                    delete[] translated_vertices;
                    delete[] translated_flipped_vertices;
                }
            }
        }
    }

    for (int i = 0; i < positions.size() / 3; ++i) {
        uint32_t colour;
        if (colours[3 * i] == 0.278f) {
            colour = 0;
        }
        else {
            colour = 1;
        }

        uint32_t vertex =
                (uint32_t)positions[3 * i] |
                ((uint32_t)positions[3 * i + 1] << 5) |
                ((uint32_t)positions[3 * i + 2] << 10) |
                (colour << 15) |
                ((uint32_t)normals[i] << 16) |
                ((uint32_t)ao[i] << 19);

        uint32_t n = vertex;
        std::cout << "vertex: " << vertex << "\n";
        std::cout << "x: " << positions[3 * i] << "\ty: " << positions[3 * i + 1] << "\tz: " << positions[3 * i + 2] <<
            "\tcol: " << colour << "\tnor: " << normals[i] << "\tao: " << ao[i] << "\n";

        std::cout << "x: " << (n & 15)
            << "\ty: " << ((n >> 5) & 15)
            << "\tz: " << ((n >> 10) & 15)
            << "\tcol: " << ((n >> 15) & 1)
            << "\tnor: " << ((n >> 16) & 7)
            << "\tao: " << ((n >> 19) & 3) << "\n";

        data.push_back(vertex);
    }

    chunk->numVertices = positions.size();

    unsigned long long totalSize = 0;
//    totalSize += positions.capacity() * sizeof(positions[0]);
//    totalSize += normals.capacity() * sizeof(normals[0]);
//    totalSize += colours.capacity() * sizeof(colours[0]);
//    totalSize += ao.capacity() * sizeof(ao[0]);
//    totalSize += chunk->voxels.capacity() * sizeof(int);
    std::cout << "totalSize: " << totalSize << std::endl;

//    std::cout << "chunk data size: " << chunk->data.capacity() * sizeof(uint64_t) << std::endl;
}

bool boundsCheck(int x, int y, int z, int i, int j, int k, int worldSize, std::vector<int> &voxels) {
    bool adjInBounds = inBounds(x + i, y + j, z + k, worldSize, CHUNK_HEIGHT);

    bool isAdjVoxelEmpty = true;
    if (adjInBounds) {
        isAdjVoxelEmpty = voxels[getVoxelIndex(x + i, y + j, z + k, CHUNK_SIZE + 2)] == 0;
    }

    return adjInBounds && isAdjVoxelEmpty;
}
