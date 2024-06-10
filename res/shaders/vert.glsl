#version 460 core
#extension GL_ARB_gpu_shader_int64 : enable

layout (location = 0) in uint64_t aData;
layout (location = 1) in vec3 aPos;
layout (location = 2) in vec4 aAo;

out vec3 ourColor;
out vec2 texCoord;
flat out int normal;
// out float fragAo;
out vec4 quadAo;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

vec3 get_color(uint type) {
    switch (type) {
        case 0: return vec3(0.278, 0.600, 0.141);
        case 1: return vec3(0.6, 0.1, 0.1);
    }
}

float calc(float ao) {
    return clamp(float(ao) / 3.0, 0.5, 1.0);
}

void main() {
    float x = aPos.x; // float(aData & 2047u);
    float y = aPos.y; // float((aData >> 11) & 2047u);
    float z = aPos.z; // float((aData >> 22) & 2047u);
    // float ao = aAo; // float((aData >> 37) & 3u);

    float bl = float(aAo.x);
    float br = float(aAo.y); // float((aData >> 39) & 3u);
    float tr = float(aAo.z); // float((aData >> 41) & 3u);
    float tl = float(aAo.w); // float((aData >> 43) & 3u);

    gl_Position = projection * view * model * vec4(x, y, z, 1.0);
    ourColor = get_color(uint((aData >> 33) & 1u));
    texCoord = vec2((aData >> 47) & 1u, (aData >> 48) & 1u);
    normal = int((aData >> 34) & 7u);
    // fragAo = clamp(float(ao) / 3.0, 0.5, 1.0);
    quadAo = vec4(calc(bl), calc(br), calc(tr), calc(tl));
}
