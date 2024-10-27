#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <unordered_set>

#include "core/Camera.hpp"
#include "opengl/Shader.h"
#include "world/Chunk.hpp"
#include "world/WorldMesh.hpp"

typedef struct {
    unsigned int count;
    unsigned int instanceCount;
    unsigned int firstIndex;
    unsigned int baseInstance;
    unsigned int chunkIndex;
} ChunkDrawCommand;

typedef struct {
    glm::mat4 model;
    int cx;
    int cz;
    int minY;
    int maxY;
    unsigned int numVertices;
    unsigned int firstIndex;
    unsigned int _pad0;
    unsigned int _pad1;
} ChunkData;

typedef struct {
    int chunkIndex;
    int x;
    int y;
    int z;
    int face;
} RaycastResult;

class Application {
public:
    void run();

    void key_callback(int key, int scancode, int action, int mods);
    void mouse_button_callback(int button, int action, int mods);
    void cursor_position_callback(double xposIn, double yposIn);
    void scroll_callback(double xoffset, double yoffset);

private:
    bool init();
    bool load();
    void loop();
    void update();
    void render();
    void processInput();
    void cleanup();

    RaycastResult raycast();
    void updateVoxel(RaycastResult result, bool place);

    GLFWwindow *windowHandle;

    const int windowWidth = 2560 * 0.8;
    const int windowHeight = 1600 * 0.8;

    Camera camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f), 0.0f, 0.0f);

    float lastX;
    float lastY;
    bool firstMouse = true;

    std::unordered_set<int> keys;
    std::unordered_set<int> lastFrameKeys;
    std::unordered_set<int> buttons;
    std::unordered_set<int> lastFrameButtons;

    float deltaTime;
    float lastFrame;

    bool wireframe = false;

    Shader shader;
    Shader drawCommandProgram;

    std::vector<Chunk> chunks;
    std::vector<ChunkData> chunkData;
    WorldMesh worldMesh;

    GLuint chunkDrawCmdBuffer;
    GLuint chunkDataBuffer;
    GLuint commandCountBuffer;
    GLuint verticesBuffer;
};
