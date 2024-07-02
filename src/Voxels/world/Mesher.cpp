#include "Mesher.hpp"

#include <cstring>
#include <iostream>
#include <chrono>
#include <bitset>

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

bool inBounds(int x, int y, int z, int size, int height) {
    return (0 <= x && x < size)
        && (0 <= y && y < height)
        && (0 <= z && z < size);
}

int dirToIndex(int i, int j, int k) {
    return (i + 1) * 9 + (j + 1) * 3 + k + 1;
}

#ifdef VERTEX_PACKING
void meshChunk(Chunk *chunk, int worldSize, std::vector<uint32_t> &data) {
#else
void meshChunk(Chunk *chunk, int worldSize, std::vector<float> &data) {
#endif
    auto startTime = std::chrono::high_resolution_clock::now();
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

    volatile long long int presenceTime = 0;
    volatile long long int voxelAoTime = 0;
    volatile long long int aoPushTime = 0;
    volatile long long int addVertexTime = 0;

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    std::cout << "Initialisation: " << duration.count() << "us" << std::endl;

    auto loopStart = std::chrono::high_resolution_clock::now();
    for (int y = 0; y < CHUNK_HEIGHT - 1; ++y) {
        for (int z = 1; z < CHUNK_SIZE + 1; ++z) {
            for (int x = 1; x < CHUNK_SIZE + 1; ++x) {
                int voxel = voxels[getVoxelIndex(x, y, z, CHUNK_SIZE + 2)];
                if (voxel != 0) {
                    auto start = std::chrono::high_resolution_clock::now();
                    // Ambient occlusion (computed first so that quads can be flipped if necessary)
                    // (-1, -1, -1) to (1, 1, 1)
                    std::array<bool, 27> presence;
                    int index = 0;
                    for (int i = -1; i <= 1; ++i) {
                        for (int j = -1; j <= 1; ++j) {
                            for (int k = -1; k <= 1; ++k) {
                                presence[index] = inBounds(x + i, y + j, z + k, CHUNK_SIZE + 2, CHUNK_HEIGHT)
                                    && voxels[getVoxelIndex(x + i, y + j, z + k, CHUNK_SIZE + 2)] != 0;
                                ++index;
                            }
                        }
                    }

                    presenceTime += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();

                    start = std::chrono::high_resolution_clock::now();
                    std::array<int, 20> voxelAo;

                    // Top
                    voxelAo[0] = (vertexAO(presence[dirToIndex(0, +1, -1)], presence[dirToIndex(-1, +1, 0)],
                                               presence[dirToIndex(-1, +1, -1)]));  // bottom left   a00
                    voxelAo[1] = (vertexAO(presence[dirToIndex(0, +1, -1)], presence[dirToIndex(+1, +1, 0)],
                                               presence[dirToIndex(+1, +1, -1)]));  // bottom right  a10
                    voxelAo[2] = (vertexAO(presence[dirToIndex(0, +1, +1)], presence[dirToIndex(+1, +1, 0)],
                                               presence[dirToIndex(+1, +1, +1)]));  // top right     a11
                    voxelAo[3] = (vertexAO(presence[dirToIndex(0, +1, +1)], presence[dirToIndex(-1, +1, 0)],
                                               presence[dirToIndex(-1, +1, +1)]));  // top left      a01

                    // Left
                    voxelAo[4] = (vertexAO(presence[dirToIndex(-1, 0, +1)], presence[dirToIndex(-1, -1, 0)],
                                               presence[dirToIndex(-1, -1, +1)]));  // bottom left
                    voxelAo[5] = (vertexAO(presence[dirToIndex(-1, 0, -1)], presence[dirToIndex(-1, -1, 0)],
                                               presence[dirToIndex(-1, -1, -1)]));  // bottom right
                    voxelAo[6] = (vertexAO(presence[dirToIndex(-1, 0, -1)], presence[dirToIndex(-1, +1, 0)],
                                               presence[dirToIndex(-1, +1, -1)]));  // top right
                    voxelAo[7] = (vertexAO(presence[dirToIndex(-1, 0, +1)], presence[dirToIndex(-1, +1, 0)],
                                               presence[dirToIndex(-1, +1, +1)]));  // top left

                    // Right
                    voxelAo[8] = (vertexAO(presence[dirToIndex(+1, 0, -1)], presence[dirToIndex(+1, -1, 0)],
                                               presence[dirToIndex(+1, -1, -1)]));  // bottom left
                    voxelAo[9] = (vertexAO(presence[dirToIndex(+1, 0, +1)], presence[dirToIndex(+1, -1, 0)],
                                               presence[dirToIndex(+1, -1, +1)]));  // bottom right
                    voxelAo[10] = (vertexAO(presence[dirToIndex(+1, 0, +1)], presence[dirToIndex(+1, +1, 0)],
                                               presence[dirToIndex(+1, +1, +1)]));  // top right
                    voxelAo[11] = (vertexAO(presence[dirToIndex(+1, 0, -1)], presence[dirToIndex(+1, +1, 0)],
                                               presence[dirToIndex(+1, +1, -1)]));  // top left

                    // Front
                    voxelAo[12] = (vertexAO(presence[dirToIndex(-1, 0, -1)], presence[dirToIndex(0, -1, -1)],
                                               presence[dirToIndex(-1, -1, -1)]));  // bottom left
                    voxelAo[13] = (vertexAO(presence[dirToIndex(+1, 0, -1)], presence[dirToIndex(0, -1, -1)],
                                               presence[dirToIndex(+1, -1, -1)]));  // bottom right
                    voxelAo[14] = (vertexAO(presence[dirToIndex(+1, 0, -1)], presence[dirToIndex(0, +1, -1)],
                                               presence[dirToIndex(+1, +1, -1)]));  // top right
                    voxelAo[15] = (vertexAO(presence[dirToIndex(-1, 0, -1)], presence[dirToIndex(0, +1, -1)],
                                               presence[dirToIndex(-1, +1, -1)]));  // top left

                    // Back
                    voxelAo[16] = (vertexAO(presence[dirToIndex(+1, 0, +1)], presence[dirToIndex(0, -1, +1)],
                                               presence[dirToIndex(+1, -1, +1)]));  // bottom left
                    voxelAo[17] = (vertexAO(presence[dirToIndex(-1, 0, +1)], presence[dirToIndex(0, -1, +1)],
                                               presence[dirToIndex(-1, -1, +1)]));  // bottom right
                    voxelAo[18] = (vertexAO(presence[dirToIndex(-1, 0, +1)], presence[dirToIndex(0, +1, +1)],
                                               presence[dirToIndex(-1, +1, +1)]));  // top right
                    voxelAo[19] = (vertexAO(presence[dirToIndex(+1, 0, +1)], presence[dirToIndex(0, +1, +1)],
                                               presence[dirToIndex(+1, +1, +1)]));  // top left

                    voxelAoTime += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();

                    start = std::chrono::high_resolution_clock::now();

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

                    aoPushTime += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();

                    start = std::chrono::high_resolution_clock::now();

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

                    addVertexTime += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();
                }
            }
        }
    }

    auto loopEnd = std::chrono::high_resolution_clock::now();

    std::cout << "presenceTime: " << presenceTime / 1000 << "us\n";
    std::cout << "voxelAoTime: " << voxelAoTime / 1000 << "us\n";
    std::cout << "aoPushTime: " << aoPushTime / 1000 << "us\n";
    std::cout << "addVertexTime: " << addVertexTime / 1000 << "us\n";

    totalMesherTime += (presenceTime + voxelAoTime + aoPushTime + addVertexTime) / 1000;
    std::cout << "Total mesher time: " << (presenceTime + voxelAoTime + aoPushTime + addVertexTime) / 1000 << "us\n";
    std::cout << "Actual loop time: " << std::chrono::duration_cast<std::chrono::microseconds>(loopEnd - loopStart).count() << "us\n";

    auto start = std::chrono::high_resolution_clock::now();

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
                ((uint32_t)positions[3 * i + 1] << (CHUNK_SIZE_SHIFT + 1)) |
                ((uint32_t)positions[3 * i + 2] << (CHUNK_SIZE_SHIFT + CHUNK_HEIGHT_SHIFT + 2)) |
                (colour << (2 * CHUNK_SIZE_SHIFT + CHUNK_HEIGHT_SHIFT + 3)) |
                ((uint32_t)normals[i] << (2 * CHUNK_SIZE_SHIFT + CHUNK_HEIGHT_SHIFT + 4)) |
                ((uint32_t)ao[i] << (2 * CHUNK_SIZE_SHIFT + CHUNK_HEIGHT_SHIFT + 7));

        //uint32_t n = vertex;
        //std::cout << "vertex: " << std::bitset<32>(n) << "\n";

        //std::cout << "x: " << positions[3 * i]
        //        << "\ty: " << positions[3 * i + 1]
        //        << "\tz: " << positions[3 * i + 2]
        //        << "\tcol: " << colour
        //        << "\tnor: " << normals[i]
        //        << "\tao: " << ao[i] << "\n";

        //std::cout << "x: " << (n & CHUNK_SIZE_MASK)
        //    << "\ty: " << ((n >> (CHUNK_SIZE_SHIFT + 1)) & CHUNK_HEIGHT_MASK)
        //    << "\tz: " << ((n >> (CHUNK_SIZE_SHIFT + CHUNK_HEIGHT_SHIFT + 2)) & CHUNK_SIZE_MASK)
        //    << "\tcol: " << ((n >> (2 * CHUNK_SIZE_SHIFT + CHUNK_HEIGHT_SHIFT + 3)) & 1)
        //    << "\tnor: " << ((n >> (2 * CHUNK_SIZE_SHIFT + CHUNK_HEIGHT_SHIFT + 4)) & 7)
        //    << "\tao: " << ((n >> (2 * CHUNK_SIZE_SHIFT + CHUNK_HEIGHT_SHIFT + 7)) & 3) << "\n";

        //std::cout << "\n";

#ifdef VERTEX_PACKING
        data.push_back(vertex);
#else
        data.push_back((float)positions[3 * i]);
        data.push_back((float)positions[3 * i + 1]);
        data.push_back((float)positions[3 * i + 2]);
        data.push_back((float)colour);
        data.push_back((float)normals[i]);
        data.push_back((float)ao[i]);
#endif
    }

    std::cout << "vertex push time: " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count() << "us\n";

    chunk->numVertices = positions.size() / 3;
}

bool boundsCheck(int x, int y, int z, int i, int j, int k, int worldSize, std::vector<int> &voxels) {
    bool adjInBounds = inBounds(x + i, y + j, z + k, worldSize + 2, CHUNK_HEIGHT);

    bool isAdjVoxelEmpty = true;
    if (adjInBounds) {
        isAdjVoxelEmpty = voxels[getVoxelIndex(x + i, y + j, z + k, CHUNK_SIZE + 2)] == 0;
    }

    return adjInBounds && isAdjVoxelEmpty;
}
