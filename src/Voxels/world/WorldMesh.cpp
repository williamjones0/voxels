#include "WorldMesh.hpp"

#include <glad/glad.h>

#include <iostream>

void WorldMesh::createBuffers() {
    std::cout << "data size: " << data.size() << std::endl;

#ifdef VERTEX_PACKING
    glCreateBuffers(1, &VBO);
    glNamedBufferStorage(VBO, sizeof(uint32_t) * data.size(), &data[0], GL_DYNAMIC_STORAGE_BIT);

    glCreateVertexArrays(1, &VAO);

    glVertexArrayVertexBuffer(VAO, 0, VBO, 0, sizeof(uint32_t));

    glEnableVertexArrayAttrib(VAO, 0);

    glVertexArrayAttribFormat(VAO, 0, 1, GL_FLOAT, GL_FALSE, 0);

    glVertexArrayAttribBinding(VAO, 0, 0);
#else
    glCreateBuffers(1, &VBO);
    glNamedBufferStorage(VBO, sizeof(float) * data.size(), &data[0], GL_DYNAMIC_STORAGE_BIT);

    glCreateVertexArrays(1, &VAO);

    glVertexArrayVertexBuffer(VAO, 0, VBO, 0, sizeof(float) * 6);

    glEnableVertexArrayAttrib(VAO, 0);
    glEnableVertexArrayAttrib(VAO, 1);
    glEnableVertexArrayAttrib(VAO, 2);
    glEnableVertexArrayAttrib(VAO, 3);

    glVertexArrayAttribFormat(VAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribFormat(VAO, 1, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
    glVertexArrayAttribFormat(VAO, 2, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float));
    glVertexArrayAttribFormat(VAO, 3, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float));

    glVertexArrayAttribBinding(VAO, 0, 0);
    glVertexArrayAttribBinding(VAO, 1, 0);
    glVertexArrayAttribBinding(VAO, 2, 0);
    glVertexArrayAttribBinding(VAO, 3, 0);
#endif
}

WorldMesh::WorldMesh(int numChunks) {
    VAO = 0;
    VBO = 0;

    chunkVertexStarts = std::vector<int>(numChunks);

    chunkVertexStarts[0] = 0;
}
