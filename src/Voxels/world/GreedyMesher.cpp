#include "GreedyMesher.hpp"

#include "Chunk.hpp"
#include "../util/Util.hpp"

#include <iostream>

GreedyMesher::GreedyMesher(int ny, int py, int dx, int dz, Chunk *chunkIn, WorldMesh &worldMesh) :
    ny(ny),
    py(py + 1),
    dx(dx),
    dy(py + 1 - ny),
    dz(dz),
    chunk(*chunkIn),
    worldMesh(worldMesh)
{
    int size = std::max(dx, dy) * std::max(dy, dz);
    m = std::vector<int>(size);

    voxels = chunk.voxels;

    computeNeighbourConfigs();
}

void GreedyMesher::computeNeighbourConfigs() {
    for (int i = 0; i < 256; i++) {
        bool cxny = (i & 1) == 1, nxny = (i & 1 << 1) == 1 << 1, nxcy = (i & 1 << 2) == 1 << 2, nxpy = (i & 1 << 3) == 1 << 3;
        bool cxpy = (i & 1 << 4) == 1 << 4, pxpy = (i & 1 << 5) == 1 << 5, pxcy = (i & 1 << 6) == 1 << 6, pxny = (i & 1 << 7) == 1 << 7;
        NEIGHBOUR_CONFIGS[i] = (cxny ? 1 : 0) + (nxny ? 2 : 0) + (nxcy ? 4 : 0) | (cxny ? 1 : 0) + (pxny ? 2 : 0) + (pxcy ? 4 : 0) << 3
            | (cxpy ? 1 : 0) + (nxpy ? 2 : 0) + (nxcy ? 4 : 0) << 6 | (cxpy ? 1 : 0) + (pxpy ? 2 : 0) + (pxcy ? 4 : 0) << 9;
        NEIGHBOUR_CONFIGS[i] = NEIGHBOUR_CONFIGS[i] << 8;
    }
}

int GreedyMesher::at(int x, int y, int z) {
    return voxels[getVoxelIndex(x, y, z, CHUNK_SIZE + 2)];
}

// u0: the U coordinate of the minimum corner
// v0: the V coordinate of the minimum corner
// u1: the U coordinate of the maximum corner
// v1: the V coordinate of the maximum corner
// p:  the main coordinate of the face (depending on the side)
// s:  the side of the face (including positive or negative)
// v:  the face value (includes neighbor configuration)
void GreedyMesher::consume(int u0, int v0, int u1, int v1, int p, int s, int v) {
    int n00 = v >> 8 & 7, n10 = v >> 11 & 7;
    int n01 = v >> 14 & 7, n11 = v >> 17 & 7;
    switch (s >> 1) {
    case 0:
        fillPositionsTypesSideAndAoFactorsX(p, u0, v0, u1, v1, s, n00, n10, n01, n11, v);
        break;
    case 1:
        fillPositionsTypesSideAndAoFactorsY(p, u0, v0, u1, v1, s, n00, n10, n01, n11, v);
        break;
    case 2:
        fillPositionsTypesSideAndAoFactorsZ(p, u0, v0, u1, v1, s, n00, n10, n01, n11, v);
        break;
    }

    fillIndices(s, faceConsumerIndex);

    faceConsumerIndex++;
}

int GreedyMesher::aoFactors(int n00, int n10, int n01, int n11) {
    return (int)(aoFactor(n00) | aoFactor(n10) << 2 | aoFactor(n01) << 4 | aoFactor(n11) << 6);
}

int GreedyMesher::aoFactor(int n) {
    return (n & 1) == 1 && (n & 4) == 4 ? 0 : 3 - bitCount(n);
}

void GreedyMesher::fillPositionsTypesSideAndAoFactorsX(int p, int u0, int v0, int u1, int v1, int s, int n00, int n10, int n01, int n11, int v) {
    uint32_t sideAndAoFactors = s | aoFactors(n00, n10, n01, n11) << 8;

    worldMesh.positionsAndTypes.push_back((uint32_t)(p | u0 << 8 | v0 << 16 | v << 24));
    worldMesh.positionsAndTypes.push_back((uint32_t)(p | u1 << 8 | v0 << 16 | v << 24));
    worldMesh.positionsAndTypes.push_back((uint32_t)(p | u0 << 8 | v1 << 16 | v << 24));
    worldMesh.positionsAndTypes.push_back((uint32_t)(p | u1 << 8 | v1 << 16 | v << 24));

    chunk.numVertices += 4;

    worldMesh.sidesAndAoFactors.push_back(sideAndAoFactors);
}

void GreedyMesher::fillPositionsTypesSideAndAoFactorsY(int p, int u0, int v0, int u1, int v1, int s, int n00, int n10, int n01, int n11, int v) {
    uint32_t sideAndAoFactors = s | aoFactors(n00, n10, n01, n11) << 8;

    worldMesh.positionsAndTypes.push_back((uint32_t)(v0 | p << 8 | u0 << 16 | v << 24));
    worldMesh.positionsAndTypes.push_back((uint32_t)(v0 | p << 8 | u1 << 16 | v << 24));
    worldMesh.positionsAndTypes.push_back((uint32_t)(v1 | p << 8 | u0 << 16 | v << 24));
    worldMesh.positionsAndTypes.push_back((uint32_t)(v1 | p << 8 | u1 << 16 | v << 24));

    chunk.numVertices += 4;

    worldMesh.sidesAndAoFactors.push_back(sideAndAoFactors);
}

void GreedyMesher::fillPositionsTypesSideAndAoFactorsZ(int p, int u0, int v0, int u1, int v1, int s, int n00, int n10, int n01, int n11, int v) {
    uint32_t sideAndAoFactors = s | aoFactors(n00, n10, n01, n11) << 8;

    worldMesh.positionsAndTypes.push_back((uint32_t)(u0 | v0 << 8 | p << 16 | v << 24));
    worldMesh.positionsAndTypes.push_back((uint32_t)(u1 | v0 << 8 | p << 16 | v << 24));
    worldMesh.positionsAndTypes.push_back((uint32_t)(u0 | v1 << 8 | p << 16 | v << 24));
    worldMesh.positionsAndTypes.push_back((uint32_t)(u1 | v1 << 8 | p << 16 | v << 24));

    chunk.numVertices += 4;

    worldMesh.sidesAndAoFactors.push_back(sideAndAoFactors);
}

// Writes indices of the face at index i for TRIANGLES rendering
// s: the side of the face
// i: the index of the face
void GreedyMesher::fillIndices(int s, int i) {
    if ((s & 1) != 0) {
        // First triangle (indices 1, 3, 0)
        worldMesh.indices.push_back((i << 2) + 1);
        worldMesh.indices.push_back((i << 2) + 3);
        worldMesh.indices.push_back((i << 2) + 0);

        // Second triangle (indices 0, 3, 2)
        worldMesh.indices.push_back((i << 2) + 0);
        worldMesh.indices.push_back((i << 2) + 3);
        worldMesh.indices.push_back((i << 2) + 2);
    } else {
        // First triangle (indices 2, 3, 0)
        worldMesh.indices.push_back((i << 2) + 2);
        worldMesh.indices.push_back((i << 2) + 3);
        worldMesh.indices.push_back((i << 2) + 0);

        // Second triangle (indices 0, 3, 1)
        worldMesh.indices.push_back((i << 2) + 0);
        worldMesh.indices.push_back((i << 2) + 3);
        worldMesh.indices.push_back((i << 2) + 1);
    }
}


int GreedyMesher::mesh() {
    meshX();
    meshY();
    meshZ();
    return count;
}

void GreedyMesher::meshX() {
    for (int x = 0; x < dx;) {
        generateMaskX(x);
        mergeAndGenerateFacesX(++x);
    }
}

void GreedyMesher::meshY() {
    for (int y = ny - 1; y < py;) {
        generateMaskY(y);
        mergeAndGenerateFacesY(++y);
    }
}

void GreedyMesher::meshZ() {
    for (int z = 0; z < dz;) {
        generateMaskZ(z);
        mergeAndGenerateFacesZ(++z);
    }
}

void GreedyMesher::generateMaskX(int x) {
    int n = 0;
    for (int z = 0; z < dz; z++) {
        for (int y = ny; y < py; y++, n++) {
            int a = at(x, y, z), b = at(x + 1, y, z);
            if (((a == 0) == (b == 0))) {
                m[n] = 0;
            } else if (a != 0) {
                m[n] = (a & 0xFF) | neighboursX(x + 1, y, z);
            } else {
                m[n] = (b & 0xFF) | neighboursX(x, y, z) | 1 << 31;
            }
        }
    }
}

void GreedyMesher::generateMaskY(int y) {
    int n = 0;
    for (int x = 0; x < dx; x++) {
        for (int z = 0; z < dz; z++, n++) {
            int a = at(x, y, z), b = at(x, y + 1, z);
            if (((a == 0) == (b == 0))) {
                m[n] = 0;
            } else if (a != 0) {
                m[n] = (a & 0xFF) | neighboursY(x, y + 1, z);
            } else {
                m[n] = (b & 0xFF) | (y >= 0 ? neighboursY(x, y, z) : 0) | 1 << 31;
            }
        }
    }
}

void GreedyMesher::generateMaskZ(int z) {
    int n = 0;
    for (int y = ny; y < py; y++) {
        for (int x = 0; x < dx; x++, n++) {
            int a = at(x, y, z), b = at(x, y, z + 1);
            if (((a == 0) == (b == 0))) {
                m[n] = 0;
            } else if (a != 0)
                m[n] = (a & 0xFF) | neighboursZ(x, y, z + 1);
            else {
                m[n] = (b & 0xFF) | neighboursZ(x, y, z) | 1 << 31;
            }
        }
    }
}

int GreedyMesher::neighboursX(int x, int y, int z) {
    /* UV = YZ */
    int n1 = at(x, y - 1, z - 1) != 0 ? 2 : 0;
    int n2 = at(x, y - 1, z) != 0 ? 4 : 0;
    int n3 = at(x, y - 1, z + 1) != 0 ? 8 : 0;
    int n0 = at(x, y, z - 1) != 0 ? 1 : 0;
    int n4 = at(x, y, z + 1) != 0 ? 16 : 0;
    int n7 = at(x, y + 1, z - 1) != 0 ? 128 : 0;
    int n6 = at(x, y + 1, z) != 0 ? 64 : 0;
    int n5 = at(x, y + 1, z + 1) != 0 ? 32 : 0;
    return NEIGHBOUR_CONFIGS[n0 | n1 | n2 | n3 | n4 | n5 | n6 | n7];
}

int GreedyMesher::neighboursY(int x, int y, int z) {
    /* UV = ZX */
    int n1 = at(x - 1, y, z - 1) != 0 ? 2 : 0;
    int n2 = at(x, y, z - 1) != 0 ? 4 : 0;
    int n3 = at(x + 1, y, z - 1) != 0 ? 8 : 0;
    int n0 = at(x - 1, y, z) != 0 ? 1 : 0;
    int n6 = at(x, y, z + 1) != 0 ? 64 : 0;
    int n4 = at(x + 1, y, z) != 0 ? 16 : 0;
    int n7 = at(x - 1, y, z + 1) != 0 ? 128 : 0;
    int n5 = at(x + 1, y, z + 1) != 0 ? 32 : 0;
    return NEIGHBOUR_CONFIGS[n0 | n1 | n2 | n3 | n4 | n5 | n6 | n7];
}

int GreedyMesher::neighboursZ(int x, int y, int z) {
    /* UV = XY */
    int n1 = at(x - 1, y - 1, z) != 0 ? 2 : 0;
    int n0 = at(x, y - 1, z) != 0 ? 1 : 0;
    int n7 = at(x + 1, y - 1, z) != 0 ? 128 : 0;
    int n2 = at(x - 1, y, z) != 0 ? 4 : 0;
    int n6 = at(x + 1, y, z) != 0 ? 64 : 0;
    int n3 = at(x - 1, y + 1, z) != 0 ? 8 : 0;
    int n4 = at(x, y + 1, z) != 0 ? 16 : 0;
    int n5 = at(x + 1, y + 1, z) != 0 ? 32 : 0;
    return NEIGHBOUR_CONFIGS[n0 | n1 | n2 | n3 | n4 | n5 | n6 | n7];
}

void GreedyMesher::mergeAndGenerateFacesX(int x) {
    int i, j, n, incr;
    for (j = 0, n = 0; j < dz; j++) {
        for (i = ny; i < py; i += incr, n += incr) {
            incr = mergeAndGenerateFaceX(x, n, i, j);
        }
    }
}

void GreedyMesher::mergeAndGenerateFacesY(int y) {
    int i, j, n, incr;
    for (j = 0, n = 0; j < dx; j++) {
        for (i = 0; i < dz; i += incr, n += incr) {
            incr = mergeAndGenerateFaceY(y, n, i, j);
        }
    }
}

void GreedyMesher::mergeAndGenerateFacesZ(int z) {
    int i, j, n, incr;
    for (j = ny, n = 0; j < py; j++) {
        for (i = 0; i < dx; i += incr, n += incr) {
            incr = mergeAndGenerateFaceZ(z, n, i, j);
        }
    }
}

int GreedyMesher::mergeAndGenerateFaceX(int x, int n, int i, int j) {
    int mn = m[n];
    if (mn == 0)
        return 1;
    int w = determineWidthX(mn, n, i);
    int h = determineHeightX(mn, n, j, w);
    consume(i, j, i + w, j + h, x, mn > 0 ? 1 : 0, mn);
    count++;
    eraseMask(n, w, h, dy);
    return w;
}

int GreedyMesher::mergeAndGenerateFaceY(int y, int n, int i, int j) {
    int mn = m[n];
    if (mn == 0)
        return 1;
    int w = determineWidthY(mn, n, i);
    int h = determineHeightY(mn, n, j, w);
    consume(i, j, i + w, j + h, y, 2 + (mn > 0 ? 1 : 0), mn);
    count++;
    eraseMask(n, w, h, dz);
    return w;
}

int GreedyMesher::mergeAndGenerateFaceZ(int z, int n, int i, int j) {
    int mn = m[n];
    if (mn == 0)
        return 1;
    int w = determineWidthZ(mn, n, i);
    int h = determineHeightZ(mn, n, j, w);
    consume(i, j, i + w, j + h, z, 4 + (mn > 0 ? 1 : 0), mn);
    count++;
    eraseMask(n, w, h, dx);
    return w;
}

void GreedyMesher::eraseMask(int n, int w, int h, int d) {
    for (int l = 0, ls = 0; l < h; l++, ls += d) {
        for (int k = 0; k < w; k++) {
            m[n + k + ls] = 0;
        }
    }
}

int GreedyMesher::determineWidthX(int c, int n, int i) {
    int w = 1;
    for (; w < MAX_MERGE_LENGTH && i + w < py && c == m[n + w]; w++)
        ;
    return w;
}

int GreedyMesher::determineWidthY(int c, int n, int i) {
    int w = 1;
    for (; i + w < dz && c == m[n + w]; w++)
        ;
    return w;
}

int GreedyMesher::determineWidthZ(int c, int n, int i) {
    int w = 1;
    for (; i + w < dx && c == m[n + w]; w++)
        ;
    return w;
}

int GreedyMesher::determineHeightX(int c, int n, int j, int w) {
    int h = 1;
    for (int hs = dy; j + h < dz; h++, hs += dy)
        for (int k = 0; k < w; k++)
            if (c != m[n + k + hs])
                return h;
    return h;
}

int GreedyMesher::determineHeightY(int c, int n, int j, int w) {
    int h = 1;
    for (int hs = dz; j + h < dx; h++, hs += dz)
        for (int k = 0; k < w; k++)
            if (c != m[n + k + hs])
                return h;
    return h;
}

int GreedyMesher::determineHeightZ(int c, int n, int j, int w) {
    int h = 1;
    for (int hs = dx; h < MAX_MERGE_LENGTH && j + h < py; h++, hs += dx)
        for (int k = 0; k < w; k++)
            if (c != m[n + k + hs])
                return h;
    return h;
}
