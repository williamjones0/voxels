#include "WorldMesh.hpp"

void WorldMesh::createBuffers() {
    std::cout << "data size: " << data.size() << std::endl;

    glCreateBuffers(1, &VBO);
    glNamedBufferStorage(VBO, sizeof(int) * data.size(), &data[0], GL_DYNAMIC_STORAGE_BIT);

    glCreateVertexArrays(1, &VAO);

    glVertexArrayVertexBuffer(VAO, 0, VBO, 0, sizeof(int) * 6);

    glEnableVertexArrayAttrib(VAO, 0);
    glEnableVertexArrayAttrib(VAO, 1);
    glEnableVertexArrayAttrib(VAO, 2);
    glEnableVertexArrayAttrib(VAO, 3);

    glVertexArrayAttribFormat(VAO, 0, 3, GL_INT, GL_FALSE, 0);
    glVertexArrayAttribFormat(VAO, 1, 1, GL_INT, GL_FALSE, 3 * sizeof(int));
    glVertexArrayAttribFormat(VAO, 2, 1, GL_INT, GL_FALSE, 4 * sizeof(int));
    glVertexArrayAttribFormat(VAO, 3, 1, GL_INT, GL_FALSE, 5 * sizeof(int));

    glVertexArrayAttribBinding(VAO, 0, 0);
    glVertexArrayAttribBinding(VAO, 1, 0);
    glVertexArrayAttribBinding(VAO, 2, 0);
    glVertexArrayAttribBinding(VAO, 3, 0);
}

WorldMesh::WorldMesh() {}
