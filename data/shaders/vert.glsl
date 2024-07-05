#version 460 core

// #define VERTEX_PACKING

#ifdef VERTEX_PACKING
layout (location = 0) in uint aData;
#else
layout (location = 0) in vec3 aPos;
layout (location = 1) in float aColor;
layout (location = 2) in float aNormal;
layout (location = 3) in float aAo;
#endif

out vec3 ourColor;
flat out int normal;
out float fragAo;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 mvp;

// Vertex packing format uniforms
uniform int chunkSizeShift;
uniform int chunkHeightShift;

int chunkSizeMask = (1 << (chunkSizeShift + 1)) - 1;
int chunkHeightMask = (1 << (chunkHeightShift + 1)) - 1;

struct ChunkModel {
    mat4 model;
};

layout (binding = 0, std430) buffer ChunkModelBuffer {
    ChunkModel Models[];
} chunkModelBuffer;

vec3 get_color(uint type) {
    switch (type) {
        case 0: return vec3(0.278, 0.600, 0.141);
        case 1: return vec3(0.6, 0.1, 0.1);
    }
}

#ifdef VERTEX_PACKING
void main() {
    ChunkModel chunkModel = chunkModelBuffer.Models[gl_DrawID];

    float x = float(aData & chunkSizeMask);
    float y = float((aData >> (chunkSizeShift + 1)) & chunkHeightMask);
    float z = float((aData >> (chunkSizeShift + chunkHeightShift + 2)) & chunkSizeMask);
    ourColor = get_color(uint((aData >> (2 * chunkSizeShift + chunkHeightShift + 3)) & 1u));
    normal = int((aData >> (2 * chunkSizeShift + chunkHeightShift + 4)) & 7u);
    float ao = float((aData >> (2 * chunkSizeShift + chunkHeightShift + 7)) & 3u);

    gl_Position = mvp * chunkModel.model * vec4(x, y, z, 1.0);
    // ourColor = get_color(uint(aColor));
    // ourColor = vec3(gl_DrawID / 4.0f, gl_DrawID / 4.0f, gl_DrawID / 4.0f);
    fragAo = clamp(float(ao) / 3.0, 0.5, 1.0);
}
#else
void main() {
    ChunkModel chunkModel = chunkModelBuffer.Models[gl_DrawID];
    float x = aPos.x;
    float y = aPos.y;
    float z = aPos.z;
    float ao = aAo;

    gl_Position = projection * view * chunkModel.model * vec4(x, y, z, 1.0);
    ourColor = get_color(uint(aColor));
    //ourColor = vec3(gl_DrawID / 4.0f, gl_DrawID / 4.0f, gl_DrawID / 4.0f);
    // ourColor = get_color(uint((aData >> 15) & 1u));
    normal = int(aNormal);
    fragAo = clamp(float(ao) / 3.0, 0.5, 1.0);
}
#endif
