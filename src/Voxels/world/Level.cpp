#include "Level.hpp"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

Level::Level() {
    colors[1] = glm::vec3(0.278, 0.600, 0.141);
    colors[2] = glm::vec3(0.600, 0.100, 0.100);
}

void Level::read(std::string_view filepath) {
    std::ifstream infile(filepath.data());

    if (!infile.is_open()) {
        throw std::runtime_error("Failed to open level file: " + std::string(filepath));
    }

    json levelJson = json::parse(infile);

    // Palette
    for (const auto& [key, value] : levelJson["palette"].items()) {
        int id = std::stoi(key);
        glm::vec3 color = glm::vec3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
        colors[id] = color;
    }

    // Bounds
    maxX = levelJson["bounds"]["x"][1].get<int>();
    maxZ = levelJson["bounds"]["z"][1].get<int>();
    maxY = levelJson["bounds"]["y"][1].get<int>();

    // Data
    data.clear();
    data.reserve(maxX * maxZ * maxY);
    for (const auto &item : levelJson["data"]) {
        int value = item[0].get<int>();
        int count = item[1].get<int>();
        data.insert(data.end(), count, value);
    }

    if (data.size() != maxX * maxZ * maxY) {
        throw std::runtime_error("Level file data size does not match specified dimensions");
    }

    infile.close();
}

void Level::read_txt(std::string_view filepath) {
    std::ifstream infile(filepath.data());

    if (!infile.is_open()) {
        throw std::runtime_error("Failed to open level file: " + std::string(filepath));
    }

    // The first three numbers on the first line are the width, depth and height of the world
    std::string line;
    if (std::getline(infile, line)) {
        std::istringstream iss(line);
        iss >> maxX >> maxZ >> maxY;
    } else {
        throw std::runtime_error("Failed to read level file: " + std::string(filepath));
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
