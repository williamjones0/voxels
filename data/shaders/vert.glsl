#version 460 core

#define PACKED_DATA

#ifdef PACKED_DATA
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

#ifdef PACKED_DATA
void main() {
    ChunkModel chunkModel = chunkModelBuffer.Models[gl_DrawID];

    float x = float(aData & chunkSizeMask);
    float y = float((aData >> (chunkSizeShift + 1)) & chunkHeightMask);
    float z = float((aData >> (chunkSizeShift + chunkHeightShift + 2)) & chunkSizeMask);
    ourColor = get_color(uint((aData >> (2 * chunkSizeShift + chunkHeightShift + 3)) & 1u));
    normal = int((aData >> (2 * chunkSizeShift + chunkHeightShift + 4)) & 7u);
    float ao = float((aData >> (2 * chunkSizeShift + chunkHeightShift + 7)) & 3u);

    gl_Position = projection * view * chunkModel.model * vec4(x, y, z, 1.0);
    // ourColor = get_color(uint(aColor));
    // ourColor = vec3(gl_DrawID / 4.0f, gl_DrawID / 4.0f, gl_DrawID / 4.0f);
    fragAo = clamp(float(ao) / 3.0, 0.5, 1.0);
}
#else
void main() {
    ChunkModel chunkModel = chunkModelBuffer.Models[gl_DrawID];
    float x = aPos.x; // float(aData & 31u);
    float y = aPos.y; // float((aData >> 5) & 31u);
    float z = aPos.z; // float((aData >> 10) & 31u);
    float ao = aAo; // float((aData >> 19) & 3u);

    gl_Position = projection * view * chunkModel.model * vec4(x, y, z, 1.0);
    ourColor = get_color(uint(aColor));
    //ourColor = vec3(gl_DrawID / 4.0f, gl_DrawID / 4.0f, gl_DrawID / 4.0f);
    // ourColor = get_color(uint((aData >> 15) & 1u));
    normal = int(aNormal); //int((aData >> 16) & 7u);
    fragAo = clamp(float(ao) / 3.0, 0.5, 1.0);
}
#endif
