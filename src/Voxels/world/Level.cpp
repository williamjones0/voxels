#include "Level.hpp"

#include <fstream>
#include <sstream>

void Level::read(std::string_view file) {
    std::ifstream infile(file.data());

    if (!infile.is_open()) {
        throw std::runtime_error("Failed to open level file: " + std::string(file));
    }

    // The first three numbers on the first line are the width, depth and height of the world
    std::string line;
    if (std::getline(infile, line)) {
        std::istringstream iss(line);
        iss >> maxX >> maxZ >> maxY;
    } else {
        throw std::runtime_error("Failed to read level file: " + std::string(file));
    }

    data.clear();
    data.reserve(maxX * maxZ * maxY);
    while (std::getline(infile, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream iss(line);
        int value;
        while (iss >> value) {
            data.push_back(value);
        }
    }

    if (data.size() != maxX * maxZ * maxY) {
        throw std::runtime_error("Level file data size does not match specified dimensions");
    }

    infile.close();
}
