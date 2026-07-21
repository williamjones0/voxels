#include "Palette.hpp"

#include <filesystem>
#include <iostream>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

size_t TextureAtlas::addTexture(const std::string& path) {
    int w, h, c;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &c, 4);

    textures.push_back({
        path,
        w,
        h,
        std::vector<unsigned char>(data, data + w * h * 4)
    });

    stbi_image_free(data);

    return textures.size() - 1;
}

void TextureAtlas::upload() {
    if (textures.empty()) return;

    const int tileSize = textures[0].width;  // assume square + same size
    const int count = static_cast<int>(textures.size());
    const int atlasDim = static_cast<int>(std::ceil(std::sqrt(count)));

    const int atlasWidth = atlasDim * tileSize;
    const int atlasHeight = atlasDim * tileSize;

    std::vector<unsigned char> atlasData(atlasWidth * atlasHeight * 4, 0);

    regions.resize(count);

    for (int i = 0; i < count; i++) {
        const int tileX = i % atlasDim;
        const int tileY = i / atlasDim;

        const int x = tileX * tileSize;
        const int y = tileY * tileSize;

        const auto& tex = textures[i];

        // Copy pixels
        for (int row = 0; row < tileSize; row++) {
            memcpy(
                &atlasData[((y + row) * atlasWidth + x) * 4],
                &tex.data[row * tileSize * 4],
                tileSize * 4
            );
        }

        // Compute UVs
        regions[i].offset = glm::vec2(
            static_cast<float>(x) / atlasWidth,
            static_cast<float>(y) / atlasHeight
        );

        regions[i].scale = glm::vec2(
            static_cast<float>(tileSize) / atlasWidth,
            static_cast<float>(tileSize) / atlasHeight
        );
    }

    // Delete old texture if it exists
    if (textureID) {
        glDeleteTextures(1, &textureID);
        textureID = 0;
    }

    // Create new texture
    glCreateTextures(GL_TEXTURE_2D, 1, &textureID);

    glTextureStorage2D(textureID, 1, GL_RGBA8, atlasWidth, atlasHeight);
    glTextureSubImage2D(textureID, 0, 0, 0, atlasWidth, atlasHeight,
                        GL_RGBA, GL_UNSIGNED_BYTE, atlasData.data());

    glTextureParameteri(textureID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(textureID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void TextureAtlas::clear() {
    textures.clear();
}

void Palette::uploadToGPU() {
    // Create palette SSBO
    std::vector<GPUPaletteEntry> gpuPalette(entries.size());
    for (size_t i = 0; i < entries.size(); i++) {
        const auto& [colour, lightLevel, texturePath, useTexture, uvOffset, uvScale] = entries[i];

        gpuPalette[i] = {
            glm::vec4(colour, 1.0f),
            glm::vec4(uvOffset, uvScale),
            useTexture
        };
    }

    // Create buffer if needed
    if (paletteBuffer == 0) {
        glCreateBuffers(1, &paletteBuffer);
    }

    // Upload TODO: glNamedBufferSubData
    glNamedBufferData(
        paletteBuffer,
        gpuPalette.size() * sizeof(GPUPaletteEntry),
        gpuPalette.data(),
        GL_DYNAMIC_DRAW
    );

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, paletteBuffer);

    // Bind atlas texture
    glBindTextureUnit(0, atlas.textureID);
}

void Palette::rebuildAtlas() {
    atlas.clear();

    // First pass: add textures
    std::unordered_map<std::string, size_t> indices;

    for (auto& entry : entries) {
        if (!entry.useTexture) continue;

        if (!std::filesystem::exists(entry.texturePath)) {
            std::cerr << "Texture file not found: " << entry.texturePath << std::endl;
            entry.useTexture = false;
            continue;
        }

        // Avoid duplicates
        if (!indices.contains(entry.texturePath)) {
            indices[entry.texturePath] = atlas.addTexture(entry.texturePath);
        }
    }

    // Upload atlas to GPU
    atlas.upload();

    // Second pass: assign UVs
    for (auto& entry : entries) {
        if (!entry.useTexture) continue;

        const size_t index = indices.at(entry.texturePath);
        const auto& [offset, scale] = atlas.getRegion(index);

        entry.uvOffset = offset;
        entry.uvScale  = scale;
    }
}

PaletteEntry& Palette::getEntry(size_t i) {
    return entries[i];
}

size_t Palette::size() const {
    return entries.size();
}

GLuint Palette::getAtlasTextureID() const {
    return atlas.textureID;
}
