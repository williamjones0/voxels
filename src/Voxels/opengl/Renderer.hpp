#pragma once

#include "Shader.h"
#include "../entity/Entity.hpp"
#include "../world/WorldManager.hpp"

#include <glad/glad.h>

class Renderer {
public:
    Renderer(int windowWidth, int windowHeight);
    void load(const WorldManager& worldManager);
    void render(const Entity& camera, const Entity& player) const;
    void cleanup() const;

    size_t enlargeVerticesBuffer(size_t currentCapacity);
    size_t enlargeLightmapBuffer(size_t currentCapacity);

    GLuint chunkDrawCmdBuffer = 0;
    GLuint chunkDataBuffer = 0;
    GLuint commandCountBuffer = 0;
    GLuint verticesBuffer = 0;
    GLuint lightmapBuffer = 0;

private:
    int windowWidth;
    int windowHeight;

    Shader shader{};
    Shader drawCommandProgram{};

    GLuint dummyVAO = 0;
};
