#pragma once

#include <vector>
#include <string>

class Level {
public:
    std::vector<int> data;
    int maxX = 0;
    int maxZ = 0;
    int maxY = 0;

    void read(std::string_view file);
};
