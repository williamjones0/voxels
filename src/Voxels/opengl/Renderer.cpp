#include "Renderer.hpp"

#include "../entity/components/CameraProperties.hpp"
#include "../entity/components/Transform.hpp"
#include "../world/VertexFormat.hpp"

#include <cstdint>

namespace {
    constexpr float Pi = 3.14159265359f;
}

struct ChunkDrawCommand {
    unsigned int count;
    unsigned int instanceCount;
    unsigned int firstIndex;
    unsigned int baseInstance;
    unsigned int chunkIndex;
};

Renderer::Renderer(int windowWidth, int windowHeight)
    : windowWidth{windowWidth}
    , windowHeight{windowHeight} {}

void Renderer::load(const WorldManager& worldManager) {
    glCreateVertexArrays(1, &dummyVAO);

    glCreateBuffers(1, &chunkDrawCmdBuffer);
    glNamedBufferStorage(chunkDrawCmdBuffer,
                         sizeof(ChunkDrawCommand) * MaxChunks,
                         nullptr,
                         GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, chunkDrawCmdBuffer);
    glObjectLabel(GL_BUFFER, chunkDrawCmdBuffer, -1, "chunkDrawCmdBuffer");

    glCreateBuffers(1, &chunkDataBuffer);
    glNamedBufferData(chunkDataBuffer,
                         sizeof(ChunkData) * worldManager.chunkData.size(),
                         static_cast<const void*>(worldManager.chunkData.data()),
                         GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, chunkDataBuffer);
    glObjectLabel(GL_BUFFER, chunkDataBuffer, -1, "chunkDataBuffer");

    glCreateBuffers(1, &commandCountBuffer);
    glNamedBufferStorage(commandCountBuffer,
                         sizeof(unsigned int),
                         nullptr,
                         GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, commandCountBuffer);
    glBindBuffer(GL_PARAMETER_BUFFER, commandCountBuffer);
    glObjectLabel(GL_BUFFER, commandCountBuffer, -1, "commandCountBuffer");

    glCreateBuffers(1, &verticesBuffer);
    glNamedBufferStorage(verticesBuffer,
                      sizeof(uint32_t) * InitialVertexBufferSize,
                      nullptr,
                      GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, verticesBuffer);
    glObjectLabel(GL_BUFFER, verticesBuffer, -1, "verticesBuffer");

    glCreateBuffers(1, &lightmapBuffer);
    glNamedBufferStorage(lightmapBuffer,
                      sizeof(uint8_t) * InitialLightmapBufferSize,
                      nullptr,
                      GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, lightmapBuffer);
    glObjectLabel(GL_BUFFER, lightmapBuffer, -1, "lightmapBuffer");
    std::cout << "Created lightmap buffer = " << lightmapBuffer << '\n';

    glCreateBuffers(1, &voxelsBuffer);
    glNamedBufferStorage(voxelsBuffer,
                      sizeof(uint8_t) * InitialLightmapBufferSize,
                      nullptr,
                        GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, voxelsBuffer);
    glObjectLabel(GL_BUFFER, voxelsBuffer, -1, "voxelsBuffer");

    shader = Shader("vert.glsl", "frag.glsl");
    drawCommandProgram = Shader("drawcmd_comp.glsl");

    shader.use();
    shader.setInt("chunkSizeShift", ChunkSizeShift);
    shader.setInt("chunkHeight", ChunkHeight);

    shader.setInt("debugToggle", 0);

    shader.setInt("windowWidth", windowWidth);
    shader.setInt("windowHeight", windowHeight);

    shader.setUInt("xBits", VertexFormat::XBits);
    shader.setUInt("yBits", VertexFormat::YBits);
    shader.setUInt("zBits", VertexFormat::ZBits);
    shader.setUInt("colourBits", VertexFormat::ColourBits);
    shader.setUInt("normalBits", VertexFormat::NormalBits);
    shader.setUInt("aoBits", VertexFormat::AOBits);
    shader.setUInt("lightBits", VertexFormat::LightBits);

    shader.setUInt("xShift", VertexFormat::XShift);
    shader.setUInt("yShift", VertexFormat::YShift);
    shader.setUInt("zShift", VertexFormat::ZShift);
    shader.setUInt("colourShift", VertexFormat::ColourShift);
    shader.setUInt("normalShift", VertexFormat::NormalShift);
    shader.setUInt("aoShift", VertexFormat::AOShift);
    shader.setUInt("lightShift", VertexFormat::LightShift);

    shader.setUInt("xMask", VertexFormat::XMask);
    shader.setUInt("yMask", VertexFormat::YMask);
    shader.setUInt("zMask", VertexFormat::ZMask);
    shader.setUInt("colourMask", VertexFormat::ColourMask);
    shader.setUInt("normalMask", VertexFormat::NormalMask);
    shader.setUInt("aoMask", VertexFormat::AOMask);
    shader.setUInt("lightMask", VertexFormat::LightMask);

    shader.setInt("atlas", 0);
    // shader.setUInt("atlas", worldManager.palette.getAtlasTextureID());
}

void Renderer::render(const Entity& camera, const Entity& player) const {
    const glm::mat4 projection = glm::perspective(camera.get<CameraProperties>()->FOV * Pi / 180,
                                        static_cast<float>(windowWidth) / static_cast<float>(windowHeight), 0.1f, 5000.0f);
    const glm::mat4 view = calculateViewMatrix(player.get<Transform>()->position, player.get<Transform>()->angles);

    glm::mat4 projectionT = glm::transpose(projection * view);
    glm::vec4 frustum[6];
    frustum[0] = projectionT[3] + projectionT[0];  // x + w < 0
    frustum[1] = projectionT[3] - projectionT[0];  // x - w > 0
    frustum[2] = projectionT[3] + projectionT[1];  // y + w < 0
    frustum[3] = projectionT[3] - projectionT[1];  // y - w > 0
    frustum[4] = projectionT[3] + projectionT[2];  // z + w < 0
    frustum[5] = projectionT[3] - projectionT[2];  // z - w > 0

    // Clear command count buffer
    glClearNamedBufferData(commandCountBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

    // Generate draw commands
    drawCommandProgram.use();
    drawCommandProgram.setVec4Array("frustum", frustum, 6);
    drawCommandProgram.setInt("CHUNK_SIZE", ChunkSize);

    glDispatchCompute(MaxChunks / 1, 1, 1);
    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

    // Render
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //    // Input latency testing
    //    if (background) {
    //        glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
    //    } else {
    //        glClearColor(0.8f, 0.8f, 0.7f, 1.0f);
    //    }
    //    background = !background;

    shader.use();
    shader.setMat4("view", view);
    shader.setMat4("projection", projection);

    glBindVertexArray(dummyVAO);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, chunkDrawCmdBuffer);
    glMultiDrawArraysIndirectCount(GL_TRIANGLES, nullptr, 0, MaxChunks, sizeof(ChunkDrawCommand));
}

void Renderer::cleanup() const {
    glDeleteBuffers(1, &chunkDrawCmdBuffer);
    glDeleteBuffers(1, &chunkDataBuffer);
    glDeleteBuffers(1, &commandCountBuffer);
    glDeleteBuffers(1, &verticesBuffer);
    glDeleteBuffers(1, &lightmapBuffer);
}

size_t Renderer::enlargeVerticesBuffer(const size_t currentCapacity) {
    const size_t newCapacity = currentCapacity * 2;

    GLuint newBuffer;
    glCreateBuffers(1, &newBuffer);
    glNamedBufferStorage(newBuffer,
                         sizeof(uint32_t) * newCapacity,
                         nullptr,
                         GL_DYNAMIC_STORAGE_BIT);

    glCopyNamedBufferSubData(verticesBuffer, newBuffer, 0, 0, sizeof(uint32_t) * currentCapacity);

    glDeleteBuffers(1, &verticesBuffer);
    verticesBuffer = newBuffer;

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, verticesBuffer);

    std::cout << "Enlarged vertices buffer to " << newCapacity << " elements." << std::endl;
    return newCapacity;
}

size_t Renderer::enlargeLightmapBuffer(size_t currentCapacity) {
    const size_t newCapacity = currentCapacity * 2;

    GLuint newBuffer;
    glCreateBuffers(1, &newBuffer);
    glNamedBufferStorage(newBuffer,
                         sizeof(uint8_t) * newCapacity,
                         nullptr,
                         GL_DYNAMIC_STORAGE_BIT);

    glCopyNamedBufferSubData(lightmapBuffer, newBuffer, 0, 0, sizeof(uint8_t) * currentCapacity);

    glDeleteBuffers(1, &lightmapBuffer);
    lightmapBuffer = newBuffer;

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, lightmapBuffer);

    std::cout << "Enlarged lightmap buffer to " << newCapacity << " elements." << std::endl;
    return newCapacity;
}

void Renderer::setDebugMode(bool debugMode) {
    shader.use();
    shader.setInt("debugToggle", debugMode ? 1 : 0);
}
