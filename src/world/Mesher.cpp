#include <world/Mesher.hpp>

#include <cstring>
#include <iostream>

#include <util/PerlinNoise.hpp>
#include <util/Util.hpp>

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

uint64_t createVertex(int x, int y, int z, int colour, int normal, int ao) {
    return (uint64_t) x |
           ((uint64_t) y << 11) |
           ((uint64_t) z << 22) |
           (colour < 33) |
           ((uint64_t) normal << 34) |
           ((uint64_t) ao << 37);
}

void meshChunk(Chunk *chunk, int worldSize) {
    std::vector<int> &voxels = chunk->voxels;
    std::vector<int> &world = chunk->positions;
    std::vector<float> &world_colours = chunk->colours;
    std::vector<int> &world_normals = chunk->normals;
    std::vector<int> &world_ao = chunk->ao;

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
                            world_ao.push_back(voxelAo[0]);
                            world_ao.push_back(voxelAo[1]);
                            world_ao.push_back(voxelAo[3]);
                            world_ao.push_back(voxelAo[3]);
                            world_ao.push_back(voxelAo[1]);
                            world_ao.push_back(voxelAo[2]);
                        } else {
                            world_ao.push_back(voxelAo[0]);
                            world_ao.push_back(voxelAo[1]);
                            world_ao.push_back(voxelAo[2]);
                            world_ao.push_back(voxelAo[2]);
                            world_ao.push_back(voxelAo[3]);
                            world_ao.push_back(voxelAo[0]);
                        }
                    }

                    // Left
                    if (boundsCheck(x, y, z, -1, 0, 0, worldSize, voxels)) {
                        if (voxelAo[4] + voxelAo[6] > voxelAo[7] + voxelAo[5]) {
                            world_ao.push_back(voxelAo[7]);
                            world_ao.push_back(voxelAo[6]);
                            world_ao.push_back(voxelAo[4]);
                            world_ao.push_back(voxelAo[4]);
                            world_ao.push_back(voxelAo[6]);
                            world_ao.push_back(voxelAo[5]);
                        } else {
                            world_ao.push_back(voxelAo[7]);
                            world_ao.push_back(voxelAo[6]);
                            world_ao.push_back(voxelAo[5]);
                            world_ao.push_back(voxelAo[5]);
                            world_ao.push_back(voxelAo[4]);
                            world_ao.push_back(voxelAo[7]);
                        }
                    }

                    // Right
                    if (boundsCheck(x, y, z, 1, 0, 0, worldSize, voxels)) {
                        if (voxelAo[8] + voxelAo[10] > voxelAo[11] + voxelAo[9]) {
                            world_ao.push_back(voxelAo[10]);
                            world_ao.push_back(voxelAo[11]);
                            world_ao.push_back(voxelAo[9]);
                            world_ao.push_back(voxelAo[9]);
                            world_ao.push_back(voxelAo[11]);
                            world_ao.push_back(voxelAo[8]);
                        } else {
                            world_ao.push_back(voxelAo[10]);
                            world_ao.push_back(voxelAo[11]);
                            world_ao.push_back(voxelAo[8]);
                            world_ao.push_back(voxelAo[8]);
                            world_ao.push_back(voxelAo[9]);
                            world_ao.push_back(voxelAo[10]);
                        }
                    }

                    // Front
                    if (boundsCheck(x, y, z, 0, 0, -1, worldSize, voxels)) {
                        if (voxelAo[12] + voxelAo[14] > voxelAo[15] + voxelAo[13]) {
                            world_ao.push_back(voxelAo[12]);
                            world_ao.push_back(voxelAo[13]);
                            world_ao.push_back(voxelAo[15]);
                            world_ao.push_back(voxelAo[15]);
                            world_ao.push_back(voxelAo[13]);
                            world_ao.push_back(voxelAo[14]);
                        } else {
                            world_ao.push_back(voxelAo[12]);
                            world_ao.push_back(voxelAo[13]);
                            world_ao.push_back(voxelAo[14]);
                            world_ao.push_back(voxelAo[14]);
                            world_ao.push_back(voxelAo[15]);
                            world_ao.push_back(voxelAo[12]);
                        }
                    }

                    // Back
                    if (boundsCheck(x, y, z, 0, 0, 1, worldSize, voxels)) {
                        if (voxelAo[16] + voxelAo[18] > voxelAo[19] + voxelAo[17]) {
                            world_ao.push_back(voxelAo[17]);
                            world_ao.push_back(voxelAo[16]);
                            world_ao.push_back(voxelAo[18]);
                            world_ao.push_back(voxelAo[18]);
                            world_ao.push_back(voxelAo[16]);
                            world_ao.push_back(voxelAo[19]);
                        } else {
                            world_ao.push_back(voxelAo[17]);
                            world_ao.push_back(voxelAo[16]);
                            world_ao.push_back(voxelAo[19]);
                            world_ao.push_back(voxelAo[19]);
                            world_ao.push_back(voxelAo[18]);
                            world_ao.push_back(voxelAo[17]);
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
                            world.insert(world.end(), &translated_flipped_vertices[TOP_FACE],
                                         &translated_flipped_vertices[TOP_FACE + 18]);
                        } else {
                            world.insert(world.end(), &translated_vertices[TOP_FACE],
                                         &translated_vertices[TOP_FACE + 18]);
                        }
                        for (int i = 0; i < 6; i++) {
                            world_normals.push_back(TOP_NORMAL);
                        }
                        world_colours.insert(world_colours.end(), &green_face[0], &green_face[18]);
                    }

                    // Left
                    if (boundsCheck(x, y, z, -1, 0, 0, worldSize, voxels)) {
                        if (voxelAo[4] + voxelAo[6] > voxelAo[7] + voxelAo[5]) {
                            world.insert(world.end(), &translated_flipped_vertices[LEFT_FACE],
                                         &translated_flipped_vertices[LEFT_FACE + 18]);
                        } else {
                            world.insert(world.end(), &translated_vertices[LEFT_FACE],
                                         &translated_vertices[LEFT_FACE + 18]);
                        }
                        for (int i = 0; i < 6; i++) {
                            world_normals.push_back(LEFT_NORMAL);
                        }
                        if (voxel == 1) {
                            world_colours.insert(world_colours.end(), &green_face[0], &green_face[18]);
                        } else {
                            world_colours.insert(world_colours.end(), &red_face[0], &red_face[18]);
                        }
                    }

                    // Right
                    if (boundsCheck(x, y, z, 1, 0, 0, worldSize, voxels)) {
                        if (voxelAo[8] + voxelAo[10] > voxelAo[11] + voxelAo[9]) {
                            world.insert(world.end(), &translated_flipped_vertices[RIGHT_FACE],
                                         &translated_flipped_vertices[RIGHT_FACE + 18]);
                        } else {
                            world.insert(world.end(), &translated_vertices[RIGHT_FACE],
                                         &translated_vertices[RIGHT_FACE + 18]);
                        }
                        for (int i = 0; i < 6; i++) {
                            world_normals.push_back(RIGHT_NORMAL);
                        }
                        if (voxel == 1) {
                            world_colours.insert(world_colours.end(), &green_face[0], &green_face[18]);
                        } else {
                            world_colours.insert(world_colours.end(), &red_face[0], &red_face[18]);
                        }
                    }

                    // Front
                    if (boundsCheck(x, y, z, 0, 0, -1, worldSize, voxels)) {
                        if (voxelAo[12] + voxelAo[14] > voxelAo[15] + voxelAo[13]) {
                            world.insert(world.end(), &translated_flipped_vertices[FRONT_FACE],
                                         &translated_flipped_vertices[FRONT_FACE + 18]);
                        } else {
                            world.insert(world.end(), &translated_vertices[FRONT_FACE],
                                         &translated_vertices[FRONT_FACE + 18]);
                        }
                        for (int i = 0; i < 6; i++) {
                            world_normals.push_back(FRONT_NORMAL);
                        }
                        if (voxel == 1) {
                            world_colours.insert(world_colours.end(), &green_face[0], &green_face[18]);
                        } else {
                            world_colours.insert(world_colours.end(), &red_face[0], &red_face[18]);
                        }
                    }

                    // Back
                    if (boundsCheck(x, y, z, 0, 0, 1, worldSize, voxels)) {
                        if (voxelAo[16] + voxelAo[18] > voxelAo[19] + voxelAo[17]) {
                            world.insert(world.end(), &translated_flipped_vertices[BACK_FACE],
                                         &translated_flipped_vertices[BACK_FACE + 18]);
                        } else {
                            world.insert(world.end(), &translated_vertices[BACK_FACE],
                                         &translated_vertices[BACK_FACE + 18]);
                        }
                        for (int i = 0; i < 6; i++) {
                            world_normals.push_back(BACK_NORMAL);
                        }
                        if (voxel == 1) {
                            world_colours.insert(world_colours.end(), &green_face[0], &green_face[18]);
                        } else {
                            world_colours.insert(world_colours.end(), &red_face[0], &red_face[18]);
                        }
                    }
                }
            }
        }
    }

    std::cout << "positions: " << world.size() << std::endl;
}

bool boundsCheck(int x, int y, int z, int i, int j, int k, int worldSize, std::vector<int> &voxels) {
    bool adjInBounds = inBounds(x + i, y + j, z + k, worldSize, CHUNK_HEIGHT);

    bool isAdjVoxelEmpty = true;
    if (adjInBounds) {
        isAdjVoxelEmpty = voxels[getVoxelIndex(x + i, y + j, z + k, CHUNK_SIZE + 2)] == 0;
    }

    return adjInBounds && isAdjVoxelEmpty;
}
