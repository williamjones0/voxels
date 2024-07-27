#version 460 core

// #define VERTEX_PACKING

layout (location = 0) in uint in_vertex;
layout (location = 1) in uint in_sideAndAo;

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
out vec4 ao;
out vec2 surfacePos;

uniform mat4 view;
uniform mat4 projection;

// Vertex packing format uniforms
uniform int chunkSizeShift;
uniform int chunkHeightShift;

int chunkSizeMask = (1 << (chunkSizeShift + 1)) - 1;
int chunkHeightMask = (1 << (chunkHeightShift + 1)) - 1;

float AO_FACTORS[4] = float[](
    0.60, 0.70, 0.85, 1.00
);

struct ChunkModel {
    mat4 model;
};

layout (binding = 0) readonly buffer DrawCommands {
    ChunkDrawCommand drawCommands[];
};

layout (binding = 1) readonly buffer Chunks {
    Chunk chunks[];
};

layout (binding = 3) readonly buffer PositionsAndTypes {
    uint positionsAndTypes[];
};

layout (binding = 4) readonly buffer SidesAndAoFactors {
    uint sidesAndAoFactors[];
};

vec3 get_color(uint type) {
    switch (type) {
        case 0: return vec3(0.278, 0.600, 0.141);
        case 1: return vec3(0.6, 0.1, 0.1);
    }
}

vec2 surfpos(uint s, vec4 positionAndType) {
  return mix(positionAndType.yz, mix(positionAndType.zx, positionAndType.xy, step(4.0, float(s))), step(2.0, float(s)));
}

void main() {
    ChunkDrawCommand drawCommand = drawCommands[gl_DrawID];
    Chunk chunk = chunks[drawCommand.chunkIndex];

    uint vertex = positionsAndTypes[gl_VertexID];
    float x = float(vertex & 0xFFu);
    float y = float((vertex >> 8) & 0xFFu);
    float z = float((vertex >> 16) & 0xFFu);
    uint type = (vertex >> 24) & 0xFFu;

    vec4 positionAndType = vec4(x, y, z, type);

    uint sideAndAo = sidesAndAoFactors[gl_VertexID];
    uint side = sideAndAo & 0xFFu;

    surfacePos = surfpos(side, positionAndType);

    uint allAo = sideAndAo >> 8;
    ao = vec4(
        AO_FACTORS[allAo & 3u],
        AO_FACTORS[(allAo >> 2) & 3u],
        AO_FACTORS[(allAo >> 4) & 3u],
        AO_FACTORS[(allAo >> 6) & 3u]
    );

    normal = int(side);

    ourColor = get_color(0);
    fragAo = (in_vertex.x + in_sideAndAo.x) / 3482304.0;

    gl_Position = projection * view * chunk.model * vec4(x, y, z, 1.0);
}
