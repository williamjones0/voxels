#version 460 core

struct Chunk {
    int cx;
    int cz;
    int minY;
    int maxY;
    uint numVertices;
    uint firstIndex;
    uint lightmapOffset;
    uint _pad0;
};

struct ChunkDrawCommand {
    uint count;
    uint instanceCount;
    uint firstIndex;
    uint baseInstance;
    uint chunkIndex;
};

out vec3 vLocalPos;
flat out int normal;
flat out uint vLightmapOffset;
flat out int vColourIndex;
out vec2 vTexCoord;
out float fragAO;

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
uniform uint lightBits;

uniform uint xShift;
uniform uint yShift;
uniform uint zShift;
uniform uint colourShift;
uniform uint normalShift;
uniform uint aoShift;
uniform uint lightShift;

uniform uint xMask;
uniform uint yMask;
uniform uint zMask;
uniform uint colourMask;
uniform uint normalMask;
uniform uint aoMask;
uniform uint lightMask;

layout (binding = 0) readonly buffer DrawCommands {
    ChunkDrawCommand drawCommands[];
};

layout (binding = 1) readonly buffer Chunks {
    Chunk chunks[];
};

layout (binding = 3) readonly buffer Vertices {
    uint vertices[];
};

layout (binding = 4) readonly buffer Lightmap {
    uint lightmap[];
};

void main() {
    ChunkDrawCommand drawCommand = drawCommands[gl_DrawID];
    Chunk chunk = chunks[drawCommand.chunkIndex];

    uint vertex = vertices[gl_VertexID];
    uint x = vertex & xMask;
    uint y = (vertex >> yShift) & yMask;
    uint z = (vertex >> zShift) & zMask;
    vColourIndex = int((vertex >> colourShift) & colourMask);
    normal = int((vertex >> normalShift) & normalMask);
    uint ao = (vertex >> aoShift) & aoMask;

    vLightmapOffset = chunk.lightmapOffset;

    vLocalPos = vec3(float(x), float(y), float(z));

    mat4 model = mat4(1.0, 0.0, 0.0, 0.0,
                      0.0, 1.0, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      float(chunk.cx << chunkSizeShift), 0, float(chunk.cz << chunkSizeShift), 1.0);

    vec4 worldPos = model * vec4(x, y, z, 1.0);

    // Per-face UVs
    //  - X faces  -> (z, y)
    //  - Y faces  -> (x, z)
    //  - Z faces  -> (x, y)
    vec2 uv;
    int n = normal;
    if (n == 2 || n == 3) {         // X faces
        uv = vec2(z, y);
    } else if (n == 4 || n == 5) {  // Y faces
        uv = vec2(x, z);
    } else {                        // Z faces
        uv = vec2(x, y);
    }

    const float tileScale = 1;
    vTexCoord = uv * tileScale;

    gl_Position = projection * view * worldPos;
    fragAO = clamp(float(ao) / 3.0, 0.5, 1.0);
}
