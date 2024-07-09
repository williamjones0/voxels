#version 460 core

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout (binding = 2) buffer CommandCount {
    uint commandCount;
};

void main() {
    commandCount = 0;
}
