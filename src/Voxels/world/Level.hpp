#pragma once

#include <array>
#include <filesystem>
#include <vector>

#include <glm/vec3.hpp>

class Level {
public:
    Level();

    std::vector<int> data;
    int maxX = 0;
    int maxZ = 0;
    int maxY = 0;

    std::array<glm::vec3, 16> colors{};

    void read(const std::filesystem::path& filepath);
    void save(const std::filesystem::path& filepath);
};
