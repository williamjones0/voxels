#version 460 core
#extension GL_ARB_gpu_shader_int64 : enable

layout (location = 0) in uint64_t aData;

out vec3 ourColor;
flat out int normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

vec3 get_color(uint type) {
    switch (type) {
        case 0: return vec3(0.278, 0.600, 0.141);
        case 1: return vec3(0.6, 0.1, 0.1);
    }
}

void main() {
    float x = float(aData & 2047u);
    float y = float((aData >> 11) & 2047u);
    float z = float((aData >> 22) & 2047u);
    gl_Position = projection * view * model * vec4(x, y, z, 1.0);
    ourColor = get_color(uint((aData >> 33) & 1u));
    normal = int((aData >> 34) & 7u);
}
