#include "Mesher.hpp"

#include <chrono>

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

constexpr int COLOUR_GREEN = 0;
constexpr int COLOUR_RED = 1;

long long int Mesher::totalMesherTime = 0;
long long int Mesher::presenceTime = 0;
long long int Mesher::voxelAoTime = 0;
long long int Mesher::aoPushTime = 0;
long long int Mesher::addVertexTime = 0;

inline int Mesher::vertexAO(uint8_t side1, uint8_t side2, uint8_t corner) {
    return (side1 && side2) ? 0 : (3 - (side1 + side2 + corner));
}

bool Mesher::inBounds(int x, int y, int z, int size, int height) {
    return (0 <= x && x < size)
        && (0 <= y && y < height)
        && (0 <= z && z < size);
}

int Mesher::dirToIndex(int i, int j, int k) {
    return (i + 1) * 9 + (j + 1) * 3 + k + 1;
}

void Mesher::meshChunk(Chunk *chunk, int worldSize, WorldMesh &worldMesh, std::vector<uint32_t> &vertices, bool reMesh) {
    auto startTime = std::chrono::high_resolution_clock::now();
    std::vector<int> &voxels = chunk->voxels;
    std::vector<int> positions;
    std::vector<int> colours;
    std::vector<int> normals;
    std::vector<int> ao;

    auto endInitTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endInitTime - startTime);
    //std::cout << "Initialisation: " << duration.count() << "us" << std::endl;

    auto loopStart = std::chrono::high_resolution_clock::now();
    for (int y = chunk->minY; y < chunk->maxY; ++y) {
        for (int z = 1; z < CHUNK_SIZE + 1; ++z) {
            for (int x = 1; x < CHUNK_SIZE + 1; ++x) {
                int voxel = voxels[getVoxelIndex(x, y, z, CHUNK_SIZE + 2)];
                if (voxel != 0 && voxel != 3) {
                    auto start = std::chrono::high_resolution_clock::now();
                    // Ambient occlusion (computed first so that quads can be flipped if necessary)
                    // (-1, -1, -1) to (1, 1, 1)
                    std::array<bool, 27> presence{};
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

                    voxelAoTime += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();

                    start = std::chrono::high_resolution_clock::now();

                    // Top
                    if (shouldMeshFace(x, y, z, 0, 1, 0, worldSize, voxels)) {
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
                    if (shouldMeshFace(x, y, z, 0, -1, 0, worldSize, voxels)) {
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
                    if (shouldMeshFace(x, y, z, -1, 0, 0, worldSize, voxels)) {
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
                    if (shouldMeshFace(x, y, z, 1, 0, 0, worldSize, voxels)) {
                        if (voxelAo[12] + voxelAo[14] > voxelAo[15] + voxelAo[13]) {
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
                    if (shouldMeshFace(x, y, z, 0, 0, -1, worldSize, voxels)) {
                        if (voxelAo[16] + voxelAo[18] > voxelAo[19] + voxelAo[17]) {
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
                    if (shouldMeshFace(x, y, z, 0, 0, 1, worldSize, voxels)) {
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

                    aoPushTime += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();

                    start = std::chrono::high_resolution_clock::now();

                    // Add vertices
                    int *translated_vertices = new int[VERTICES_LENGTH];
                    for (int k = 0; k < 36; ++k) {
                        translated_vertices[3 * k] = cube_vertices[3 * k] + x - 1;
                        translated_vertices[3 * k + 1] = cube_vertices[3 * k + 1] + y;
                        translated_vertices[3 * k + 2] = cube_vertices[3 * k + 2] + z - 1;
                    }

                    int *translated_flipped_vertices = new int[VERTICES_LENGTH];
                    for (int k = 0; k < 36; ++k) {
                        translated_flipped_vertices[3 * k] = flipped_cube_vertices[3 * k] + x - 1;
                        translated_flipped_vertices[3 * k + 1] = flipped_cube_vertices[3 * k + 1] + y;
                        translated_flipped_vertices[3 * k + 2] = flipped_cube_vertices[3 * k + 2] + z - 1;
                    }

                    // Top face
                    if (shouldMeshFace(x, y, z, 0, 1, 0, worldSize, voxels)) {
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
                        if (voxel == 1) {
                            colours.push_back(COLOUR_GREEN);
                        } else {
                            colours.push_back(COLOUR_RED);
                        }
                    }

                    // Bottom
                    if (shouldMeshFace(x, y, z, 0, -1, 0, worldSize, voxels)) {
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
                        if (voxel == 1) {
                            colours.push_back(COLOUR_GREEN);
                        } else {
                            colours.push_back(COLOUR_RED);
                        }
                    }

                    // Left
                    if (shouldMeshFace(x, y, z, -1, 0, 0, worldSize, voxels)) {
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
                        if (voxel == 1) {
                            colours.push_back(COLOUR_GREEN);
                        } else {
                            colours.push_back(COLOUR_RED);
                        }
                    }

                    // Right
                    if (shouldMeshFace(x, y, z, 1, 0, 0, worldSize, voxels)) {
                        if (voxelAo[12] + voxelAo[14] > voxelAo[15] + voxelAo[13]) {
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
                            colours.push_back(COLOUR_GREEN);
                        } else {
                            colours.push_back(COLOUR_RED);
                        }
                    }

                    // Front
                    if (shouldMeshFace(x, y, z, 0, 0, -1, worldSize, voxels)) {
                        if (voxelAo[16] + voxelAo[18] > voxelAo[19] + voxelAo[17]) {
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
                            colours.push_back(COLOUR_GREEN);
                        } else {
                            colours.push_back(COLOUR_RED);
                        }
                    }

                    // Back
                    if (shouldMeshFace(x, y, z, 0, 0, 1, worldSize, voxels)) {
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
                        if (voxel == 1) {
                            colours.push_back(COLOUR_GREEN);
                        } else {
                            colours.push_back(COLOUR_RED);
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

    //std::cout << "presenceTime: " << presenceTime / 1000 << "us\n";
    //std::cout << "voxelAoTime: " << voxelAoTime / 1000 << "us\n";
    //std::cout << "aoPushTime: " << aoPushTime / 1000 << "us\n";
    //std::cout << "addVertexTime: " << addVertexTime / 1000 << "us\n";

    totalMesherTime += (presenceTime + voxelAoTime + aoPushTime + addVertexTime) / 1000;
    //std::cout << "Total mesher time: " << (presenceTime + voxelAoTime + aoPushTime + addVertexTime) / 1000 << "us\n";
    //std::cout << "Actual loop time: " << std::chrono::duration_cast<std::chrono::microseconds>(loopEnd - loopStart).count() << "us\n";

    auto start = std::chrono::high_resolution_clock::now();

    if (reMesh) {
        int chunkIndex = chunk->cx * (worldSize / CHUNK_SIZE) + chunk->cz;
        int chunkVertexStart = worldMesh.chunkVertexStarts[chunkIndex];
        int chunkVertexEnd;
        if (chunkIndex + 1 < worldMesh.chunkVertexStarts.size()) {
            chunkVertexEnd = worldMesh.chunkVertexStarts[chunkIndex + 1];
        } else {
            chunkVertexEnd = worldMesh.data.size();
        }

        // Clear the old vertices
        worldMesh.data.erase(worldMesh.data.begin() + chunkVertexStart, worldMesh.data.begin() + chunkVertexEnd);
    }

    for (int i = 0; i < positions.size() / 3; ++i) {
        uint32_t vertex =
            (uint32_t)positions[3 * i] |
            ((uint32_t)positions[3 * i + 1] << (CHUNK_SIZE_SHIFT + 1)) |
            ((uint32_t)positions[3 * i + 2] << (CHUNK_SIZE_SHIFT + CHUNK_HEIGHT_SHIFT + 2)) |
            ((uint32_t)colours[i / 6] << (2 * CHUNK_SIZE_SHIFT + CHUNK_HEIGHT_SHIFT + 3)) |
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

        if (reMesh) {
            int chunkIndex = chunk->cx * (worldSize / CHUNK_SIZE) + chunk->cz;
            int chunkVertexStart = worldMesh.chunkVertexStarts[chunkIndex];
            worldMesh.data.insert(worldMesh.data.begin() + chunkVertexStart + i, vertex);
        } else {
            vertices.push_back(vertex);
        }
    }

    if (reMesh) {
        // Update the chunkVertexStarts
        int chunkIndex = chunk->cx * (worldSize / CHUNK_SIZE) + chunk->cz;
        int chunkVertexStart = worldMesh.chunkVertexStarts[chunkIndex];
        int chunkVertexEnd;
        if (chunkIndex + 1 < worldMesh.chunkVertexStarts.size()) {
            chunkVertexEnd = worldMesh.chunkVertexStarts[chunkIndex + 1];
        } else {
            chunkVertexEnd = worldMesh.data.size();
        }

        int oldChunkVerticesSize = (chunkVertexEnd - chunkVertexStart);
        for (int i = chunkIndex + 1; i < worldMesh.chunkVertexStarts.size(); ++i) {
            worldMesh.chunkVertexStarts[i] += positions.size() / 3 - oldChunkVerticesSize;
        }
    } else {
        int chunkIndex = chunk->cx * (worldSize / CHUNK_SIZE) + chunk->cz;
        if (chunkIndex + 1 < worldMesh.chunkVertexStarts.size()) {
            worldMesh.chunkVertexStarts[chunkIndex + 1] = worldMesh.data.size();
        }
    }

    //std::cout << "vertex push time: " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count() << "us\n";

    chunk->numVertices = positions.size() / 3;

    auto endTime = std::chrono::high_resolution_clock::now();
    //std::cout << "meshChunk in-function time: " << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << "us\n";
}

bool Mesher::shouldMeshFace(int x, int y, int z, int i, int j, int k, int worldSize, std::vector<int> &voxels) {
    bool adjInBounds = inBounds(x + i, y + j, z + k, worldSize + 2, CHUNK_HEIGHT);

    bool isAdjVoxelEmpty = true;
    if (adjInBounds) {
        isAdjVoxelEmpty = voxels[getVoxelIndex(x + i, y + j, z + k, CHUNK_SIZE + 2)] == 0;
    }

    return adjInBounds && isAdjVoxelEmpty;
}
