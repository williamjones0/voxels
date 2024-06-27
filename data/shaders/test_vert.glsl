#version 460 core

layout (location = 0) in vec3 aPos;

out vec3 ourColor;

layout (binding = 0, std430) buffer OffsetBuffer {
	float Offsets[];
} offsetBuffer;

void main() {
	ourColor = vec3(gl_DrawID / 10.0f, 0.0f, 0.0f);
    gl_Position = vec4(aPos.x + offsetBuffer.Offsets[gl_DrawID] / 20.0f, aPos.y, aPos.z, 1.0);
}
