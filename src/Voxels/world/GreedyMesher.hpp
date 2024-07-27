#pragma once

#include <array>
#include <vector>

#include "WorldMesh.hpp"
#include "Chunk.hpp"

const int MAX_MERGE_LENGTH = 32;

class GreedyMesher {
public:
    GreedyMesher(int ny, int py, int dx, int dz, Chunk *chunk, WorldMesh &worldMesh);

    int NEIGHBOUR_CONFIGS[256];
    std::vector<int> m; // int *m;
    int dx, dy, dz, ny, py;
    std::vector<int> voxels; // int *voxels;
    int count;

    Chunk &chunk;
    WorldMesh &worldMesh;

    void computeNeighbourConfigs();
    int at(int x, int y, int z);

    int faceConsumerIndex = 0;
    void consume(int u0, int v0, int u1, int v1, int p, int s, int v);

    int aoFactors(int n00, int n10, int n01, int n11);
    int aoFactor(int n);

    void fillPositionsTypesSideAndAoFactorsX(int p, int u0, int v0, int u1, int v1, int s, int n00, int n10, int n01, int n11, int v);
    void fillPositionsTypesSideAndAoFactorsY(int p, int u0, int v0, int u1, int v1, int s, int n00, int n10, int n01, int n11, int v);
    void fillPositionsTypesSideAndAoFactorsZ(int p, int u0, int v0, int u1, int v1, int s, int n00, int n10, int n01, int n11, int v);

    void fillIndices(int s, int i);

    int mesh();

    void meshX();
    void meshY();
    void meshZ();

    void generateMaskX(int x);
    void generateMaskY(int y);
    void generateMaskZ(int z);

    int neighboursX(int x, int y, int z);
    int neighboursY(int x, int y, int z);
    int neighboursZ(int x, int y, int z);

    void mergeAndGenerateFacesX(int x);
    void mergeAndGenerateFacesY(int y);
    void mergeAndGenerateFacesZ(int z);

    int mergeAndGenerateFaceX(int x, int n, int i, int j);
    int mergeAndGenerateFaceY(int y, int n, int i, int j);
    int mergeAndGenerateFaceZ(int z, int n, int i, int j);

    void eraseMask(int n, int w, int h, int d);

    int determineWidthX(int c, int n, int i);
    int determineWidthY(int c, int n, int i);
    int determineWidthZ(int c, int n, int i);

    int determineHeightX(int c, int n, int j, int w);
    int determineHeightY(int c, int n, int j, int w);
    int determineHeightZ(int c, int n, int j, int w);
};
