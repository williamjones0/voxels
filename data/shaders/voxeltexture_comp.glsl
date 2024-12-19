#version 460 core

layout (local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

uniform int WORLD_SIZE;
uniform int CHUNK_HEIGHT;

layout (binding = 4) readonly buffer Voxels {
    int voxels[];  // voxels[y][z][x]
};

layout (binding = 5, rgba16f) uniform writeonly image3D voxelsTexture;

vec3 get_color(uint type) {
    switch (type) {
        case 0: return vec3(0.278, 0.600, 0.141);
        case 1: return vec3(0.6, 0.1, 0.1);
    }
    return vec3(0.0);
}

void main() {
    ivec3 pos = ivec3(gl_GlobalInvocationID.xyz);
    ivec3 imageSize = imageSize(voxelsTexture);

    // Calculate the linear index
    int index = pos.x + pos.z * WORLD_SIZE + pos.y * WORLD_SIZE * WORLD_SIZE;

    if (pos.x >= imageSize.x || pos.y >= imageSize.y || pos.z >= imageSize.z) {
        return;
    }

    int voxel = voxels[index];
    if (voxel == 0) {
        return;
    }

    imageStore(voxelsTexture, pos, vec4(get_color(uint(voxel - 1)), 1.0));
}
