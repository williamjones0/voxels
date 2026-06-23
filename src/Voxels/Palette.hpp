#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <array>
#include <string>

#include "world/VertexFormat.hpp"

struct PaletteEntry {
    glm::vec3 colour = glm::vec3(0.0f);

    std::string texturePath;
    bool useTexture = false;

    glm::vec2 uvOffset = glm::vec2(0.0f);
    glm::vec2 uvScale  = glm::vec2(1.0f);
};

struct GPUPaletteEntry {
    glm::vec4 colour;
    glm::vec4 uv;  // xy = offset, zw = scale
    int hasTexture;
    int _pad[3];
};

class TextureAtlas {
public:
    struct Region {
        glm::vec2 offset;
        glm::vec2 scale;
    };

    size_t addTexture(const std::string& path);
    [[nodiscard]] const Region& getRegion(const size_t index) const {
        return regions[index];
    }

    void upload();
    void clear();

    GLuint textureID = 0;

private:
    struct PendingTexture {
        std::string path;
        int width;
        int height;
        std::vector<unsigned char> data;
    };

    std::vector<Region> regions;
    std::vector<PendingTexture> textures;
};

class Palette {
public:
    void uploadToGPU();
    void rebuildAtlas();

    PaletteEntry& getEntry(size_t i);
    size_t size() const;
    GLuint getAtlasTextureID() const;

    std::array<PaletteEntry, 1 << VertexFormat::ColourBits> entries{};
    size_t index = 0;

private:
    TextureAtlas atlas;

    GLuint paletteBuffer = 0;
};
