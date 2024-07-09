#version 460 core

// #define VERTEX_PACKING

struct Chunk {
    mat4 model;
    int cx;
    int cz;
    int minY;
    int maxY;
    uint numVertices;
    uint firstIndex;
    uint _pad0;
    uint _pad1;
};

struct ChunkDrawCommand {
    uint count;
    uint instanceCount;
    uint firstIndex;
    uint baseInstance;
    uint chunkIndex;
};

out vec3 ourColor;
flat out int normal;
out float fragAo;

uniform mat4 view;
uniform mat4 projection;

// Vertex packing format uniforms
uniform int chunkSizeShift;
uniform int chunkHeightShift;

int chunkSizeMask = (1 << (chunkSizeShift + 1)) - 1;
int chunkHeightMask = (1 << (chunkHeightShift + 1)) - 1;

struct ChunkModel {
    mat4 model;
};

layout (binding = 0) readonly buffer DrawCommands {
    ChunkDrawCommand drawCommands[];
};

layout (binding = 1) readonly buffer Chunks {
    Chunk chunks[];
};

layout (binding = 3) readonly buffer Vertices {
    uint vertices[];
};

vec3 get_color(uint type) {
    switch (type) {
        case 0: return vec3(0.278, 0.600, 0.141);
        case 1: return vec3(0.6, 0.1, 0.1);
    }
}

void main() {
    ChunkDrawCommand drawCommand = drawCommands[gl_DrawID];
    Chunk chunk = chunks[drawCommand.chunkIndex];

    uint vertex = vertices[gl_VertexID];
    float x = float(vertex & chunkSizeMask);
    float y = float((vertex >> (chunkSizeShift + 1)) & chunkHeightMask);
    float z = float((vertex >> (chunkSizeShift + chunkHeightShift + 2)) & chunkSizeMask);
    ourColor = get_color(uint((vertex >> (2 * chunkSizeShift + chunkHeightShift + 3)) & 1u));
    normal = int((vertex >> (2 * chunkSizeShift + chunkHeightShift + 4)) & 7u);
    float ao = float((vertex >> (2 * chunkSizeShift + chunkHeightShift + 7)) & 3u);

    gl_Position = projection * view * chunk.model * vec4(x, y, z, 1.0);
    // ourColor = get_color(uint(aColor));
    // ourColor = vec3(gl_DrawID / 4.0f, gl_DrawID / 4.0f, gl_DrawID / 4.0f);
    fragAo = clamp(float(ao) / 3.0, 0.5, 1.0);
}
