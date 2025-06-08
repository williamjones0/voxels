#include "Level.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

Level::Level() {
    colors[1] = glm::vec3(0.278, 0.600, 0.141);
    colors[2] = glm::vec3(0.600, 0.100, 0.100);
}

void Level::read(const std::filesystem::path &filepath) {
    std::cout << "Reading level file: " << filepath << std::endl;
    std::ifstream infile(filepath);

    if (!infile.is_open()) {
        throw std::runtime_error("Failed to open level file: " + filepath.string());
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

void Level::save(const std::filesystem::path &filepath) {
    std::cout << "Saving level file: " << filepath << std::endl;

    json levelJson;

    // Palette
    json paletteJson;
    for (int i = 0; i < colors.size(); ++i) {
        paletteJson[std::to_string(i)] = {colors[i].r, colors[i].g, colors[i].b};
    }
    levelJson["palette"] = paletteJson;

    // Bounds
    levelJson["bounds"]["x"] = {0, maxX};
    levelJson["bounds"]["z"] = {0, maxZ};
    levelJson["bounds"]["y"] = {0, maxY};

    // Data
    json dataJson;
    int count = 1;
    bool need_to_add = false;
    for (size_t i = 1; i < data.size(); ++i) {
        if (data[i] == data[i - 1]) {
            count++;
            need_to_add = true;
        } else {
            dataJson.push_back({data[i - 1], count});
            count = 1;
            need_to_add = false;
        }
    }
    if (need_to_add) {
        dataJson.push_back({data.back(), count});
    }

    levelJson["data"] = dataJson;

    std::ofstream outfile(filepath);
    if (!outfile.is_open()) {
        throw std::runtime_error("Failed to open level file for writing: " + filepath.string());
    }

    outfile << levelJson.dump(4);
    outfile.close();
}
