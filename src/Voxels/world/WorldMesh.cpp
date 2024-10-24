#include "WorldMesh.hpp"

#include <glad/glad.h>

#include <iostream>

void WorldMesh::createBuffers() {
    std::cout << "data size: " << data.size() << std::endl;

    glCreateBuffers(1, &VBO);
    glNamedBufferStorage(VBO, sizeof(uint32_t) * data.size(), &data[0], GL_DYNAMIC_STORAGE_BIT);

    glCreateVertexArrays(1, &VAO);

    glVertexArrayVertexBuffer(VAO, 0, VBO, 0, sizeof(uint32_t));

    glEnableVertexArrayAttrib(VAO, 0);

    glVertexArrayAttribFormat(VAO, 0, 1, GL_FLOAT, GL_FALSE, 0);

    glVertexArrayAttribBinding(VAO, 0, 0);
}

WorldMesh::WorldMesh(int numChunks) {
    VAO = 0;
    VBO = 0;

    chunkVertexStarts = std::vector<unsigned int>(numChunks);

    chunkVertexStarts[0] = 0;
}
