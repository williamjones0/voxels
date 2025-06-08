#pragma once

#include <array>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/vec3.hpp>

class Level {
public:
    Level();

    std::vector<int> data;
    int maxX = 0;
    int maxZ = 0;
    int maxY = 0;

    std::array<glm::vec3, 16> colors {};

    void read(std::string_view file);
    void read_txt(std::string_view filepath);
};
