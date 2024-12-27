#version 460 core

layout (local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

uniform int WORLD_SIZE;
uniform int CHUNK_HEIGHT;
uniform vec3 light_color;
uniform vec3 light_position;

layout (binding = 4) readonly buffer Voxels {
    int voxels[];  // voxels[y][z][x]
};

layout (binding = 5, rgba16f) uniform writeonly image3D voxelsTexture;

vec3 directLight(ivec3 pos, vec3 nrm) {
    // ambient
    vec3 ambient = 0.1f * light_color;

    // diffuse
    vec3 diffuse = vec3(0.0f);
    vec3 lightDir = normalize(light_position - pos);
    float diff = max(dot(nrm, lightDir), 0.0);
    diff = 1.0f;
    diffuse = 0.9f * diff * light_color;

    // Attenuation
    float distance = length(light_position - pos);
    float attenuation = 1.0f / (1.0f + 0.7f * distance + 1.8f * (distance * distance));

    return (ambient + diffuse) * vec3(0.278f, 0.600f, 0.141f) * attenuation;
}

void main() {
    ivec3 pos = ivec3(gl_GlobalInvocationID.xyz);
    ivec3 imageSize = imageSize(voxelsTexture);

//    imageStore(voxelsTexture, pos, vec4(pos / vec3(imageSize), 1.0));

    // Calculate the linear index
    int index = pos.x + pos.z * WORLD_SIZE + pos.y * WORLD_SIZE * WORLD_SIZE;

    if (pos.x >= imageSize.x || pos.y >= imageSize.y || pos.z >= imageSize.z) {
        return;
    }

    int voxel = voxels[index];
    if (voxel == 0) {
        return;
    }

    vec3 direct_light = directLight(pos, vec3(0.0, 1.0, 0.0));

    imageStore(voxelsTexture, pos, vec4(direct_light, 1.0));
//    imageStore(voxelsTexture, pos, vec4(0.05, 0.05, 0.05, 1.0));
//    imageStore(voxelsTexture, pos, vec4(get_color(uint(voxel - 1)) / 2.0f, 1.0));
}
