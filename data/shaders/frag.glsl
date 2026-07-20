#version 460 core

out vec4 FragColor;

in vec3 vLocalPos;
flat in int normal;
flat in uint vLightmapOffset;
flat in int vColourIndex;
in vec2 vTexCoord;
in float fragAO;

uniform sampler2D atlas;
uniform int windowWidth;
uniform int windowHeight;
uniform int chunkSizeShift;
uniform int chunkHeight;

struct PaletteEntry {
    vec4 colour;
    vec4 uv;
    int hasTexture;
};

layout (binding = 4) readonly buffer Lightmap {
    uint lightmap[];
};

layout (std430, binding = 5) readonly buffer PaletteBuffer {
    PaletteEntry palette[];
};

const float brightness[16] = float[](
    0.08,
    0.10,
    0.12,
    0.15,
    0.19,
    0.24,
    0.30,
    0.37,
    0.46,
    0.56,
    0.67,
    0.77,
    0.85,
    0.91,
    0.96,
    1.00
);

float get_shade(int type) {
    switch (type) {
        case 0: return 0.8;
        case 1: return 0.5;
        case 2: return 0.4;
        case 3: return 0.6;
        case 4: return 0.3;
        case 5: return 0.9;
    }

    return 0.0;
}

vec3 normalDir(int n) {
    if (n == 0) return vec3(0.0, 0.0, -1.0);
    if (n == 1) return vec3(0.0, 0.0, 1.0);
    if (n == 2) return vec3(-1.0, 0.0, 0.0);
    if (n == 3) return vec3(1.0, 0.0, 0.0);
    if (n == 4) return vec3(0.0, -1.0, 0.0);
    return vec3(0.0, 1.0, 0.0);
}

uint readLightByte(uint byteIndex) {
    uint word = lightmap[byteIndex >> 2u];
    uint shift = (byteIndex & 3u) * 8u;
    return (word >> shift) & 0xFFu;
}

float sampleLightCell(int cx, int cy, int cz, uint ChunkSize) {
    if (cy < 0 || cy >= chunkHeight) return 0.0;

    uint idx = vLightmapOffset
             + uint(cy) * (ChunkSize + 2u) * (ChunkSize + 2u)
             + uint(cz + 1) * (ChunkSize + 2u)
             + uint(cx + 1);

    uint packed = readLightByte(idx);
    uint sun = (packed >> 4u) & 0xFu;
    uint torch = packed & 0xFu;
    return float(max(sun, torch));
}

void main() {
    uint ChunkSize = 1u << uint(chunkSizeShift);
    vec3 nrm = normalDir(normal);

    vec3 p = vLocalPos - nrm * 0.5;
    ivec3 voxel = ivec3(floor(p));
    ivec3 airCell = voxel + ivec3(nrm);

    float light = sampleLightCell(airCell.x, airCell.y, airCell.z, ChunkSize);
    float lightF = brightness[clamp(int(light), 0, 15)];

    PaletteEntry entry = palette[vColourIndex];
    vec3 color;
    if (entry.hasTexture == 1) {
        vec2 tiledUV = fract(vTexCoord);
        vec2 uv = entry.uv.xy + tiledUV * entry.uv.zw;
        color = texture(atlas, uv).rgb;
    } else {
        color = entry.colour.rgb;
    }

    float shade = get_shade(normal);
    float ao = clamp(fragAO, 0.0, 1.0);

    color *= shade;
    color *= smoothstep(0.0, 1.0, ao);
    color *= lightF;

    // Crosshair
    if (distance(gl_FragCoord.xy, vec2(windowWidth / 2, windowHeight / 2)) < 5) {
        FragColor = vec4(1.0, 0.0, 0.0, 1.0);
    } else {
        FragColor = vec4(color, 1.0);
    }
}
