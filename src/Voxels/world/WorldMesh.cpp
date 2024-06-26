#include "WorldMesh.hpp"

void WorldMesh::createBuffers() {
    std::cout << "data size: " << data.size() << std::endl;

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &dataVBO);
    glBindBuffer(GL_ARRAY_BUFFER, dataVBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(uint64_t), &data[0], GL_STATIC_DRAW);
    glVertexAttribLPointer(0, 1, GL_DOUBLE, sizeof(uint64_t), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);
}

WorldMesh::WorldMesh() {}
