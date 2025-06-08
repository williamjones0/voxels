#version 460 core

struct Chunk {
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

uniform uint xBits;
uniform uint yBits;
uniform uint zBits;
uniform uint colourBits;
uniform uint normalBits;
uniform uint aoBits;

uniform uint xShift;
uniform uint yShift;
uniform uint zShift;
uniform uint colourShift;
uniform uint normalShift;
uniform uint aoShift;

uniform uint xMask;
uniform uint yMask;
uniform uint zMask;
uniform uint colourMask;
uniform uint normalMask;
uniform uint aoMask;

// Colour palette
uniform vec3 palette[16];

layout (binding = 0) readonly buffer DrawCommands {
    ChunkDrawCommand drawCommands[];
};

layout (binding = 1) readonly buffer Chunks {
    Chunk chunks[];
};

layout (binding = 3) readonly buffer Vertices {
    uint vertices[];
};

void main() {
    ChunkDrawCommand drawCommand = drawCommands[gl_DrawID];
    Chunk chunk = chunks[drawCommand.chunkIndex];

    uint vertex = vertices[gl_VertexID];
    float x = float(vertex & xMask);
    float y = float((vertex >> yShift) & yMask);
    float z = float((vertex >> zShift) & zMask);
    uint colourIndex = (vertex >> colourShift) & colourMask;
    ourColor = palette[colourIndex];
    normal = int((vertex >> normalShift) & normalMask);
    float ao = float((vertex >> aoShift) & aoMask);

    mat4 model = mat4(1.0, 0.0, 0.0, 0.0,
                      0.0, 1.0, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      float(chunk.cx << chunkSizeShift), 0, float(chunk.cz << chunkSizeShift), 1.0);

    gl_Position = projection * view * model * vec4(x, y, z, 1.0);
    // ourColor = get_color(uint(aColor));
    // ourColor = vec3(gl_DrawID / 4.0f, gl_DrawID / 4.0f, gl_DrawID / 4.0f);
    fragAo = clamp(float(ao) / 3.0, 0.5, 1.0);
}
