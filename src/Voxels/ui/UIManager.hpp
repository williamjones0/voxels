#pragma once

#include <functional>
#include <string>
#include <vector>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "../entity/components/Q1PlayerController.hpp"
#include "../world/WorldManager.hpp"

class UIManager {
public:
    using WindowFn = std::function<void()>;

    void load(GLFWwindow* window);
    void beginFrame();
    void drawWindows();
    void render();
    void cleanup();

    void registerStats(const WorldManager& worldManager, const Entity& player, float deltaTime);
    void registerController(const Entity& player);
    void registerCamera(const Entity& camera);
    void registerDemo();
    void registerPalette(Palette& palette);
    void registerPrimitives(WorldManager& worldManager, const Entity& player);
    void registerLookAt(WorldManager& worldManager, const Entity& player);

    void clearWindowFocus();

private:
    void registerWindow(const std::string& title, const WindowFn& fn, bool visible = true);

    void drawPaletteEntryEditor(Palette& palette, PaletteEntry& entry);

    struct Window { std::string title; WindowFn fn; bool visible; };
    std::vector<Window> windows;
};
