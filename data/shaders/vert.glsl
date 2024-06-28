#version 460 core

layout (location = 0) in uint aData;

out vec3 ourColor;
flat out int normal;
out float fragAo;

uniform mat4 view;
uniform mat4 projection;

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

void main() {
    ChunkModel chunkModel = chunkModelBuffer.Models[gl_DrawID];
    float x = float(aData & 31u);
    float y = float((aData >> 5) & 31u);
    float z = float((aData >> 10) & 31u);
    float ao = float((aData >> 19) & 3u);

    gl_Position = projection * view * chunkModel.model * vec4(x, y, z, 1.0);
    // ourColor = get_color(uint(aColor));
    // ourColor = vec3(gl_DrawID / 4.0f, gl_DrawID / 4.0f, gl_DrawID / 4.0f);
    ourColor = get_color(uint((aData >> 15) & 1u));
    normal = int((aData >> 16) & 7u);
    fragAo = clamp(float(ao) / 3.0, 0.5, 1.0);
}
