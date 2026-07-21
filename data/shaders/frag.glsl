#version 460 core

out vec4 FragColor;

in vec3 vLocalPos;
flat in int normal;
flat in uint vCellOffset;
flat in int vColourIndex;
in vec2 vTexCoord;
in float fragAO;

uniform sampler2D atlas;
uniform int windowWidth;
uniform int windowHeight;
uniform int chunkSizeShift;
uniform int chunkHeight;

uniform int debugToggle;

struct PaletteEntry {
    vec4 colour;
    vec4 uv;
    int hasTexture;
};

layout (binding = 4) readonly buffer Lightmap {
    uint lightmap[];
};

layout (std430, binding = 5) readonly buffer Voxels {
    int voxels[];
};

layout (std430, binding = 6) readonly buffer PaletteBuffer {
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

//uint readLightByte(uint byteIndex) {
//    uint word = lightmap[byteIndex >> 2u];
//    uint shift = (byteIndex & 3u) * 8u;
//    return (word >> shift) & 0xFFu;
//}

uint readLightByte(uint cellIdx) {
    uint bytePos = vCellOffset + cellIdx;
    uint word = lightmap[bytePos >> 2u];
    uint shift = (bytePos & 3u) * 8u;
    return (word >> shift) & 0xFFu;
}

int readVoxel(uint cellIdx) {
    return voxels[vCellOffset + cellIdx];
}

uint cellIndex(int cx, int cy, int cz, uint ChunkSize) {
    return uint(cy) * (ChunkSize + 2u) * (ChunkSize + 2u)
         + uint(cz + 1) * (ChunkSize + 2u)
         + uint(cx + 1);
}

float lightAtCell(int cx, int cy, int cz, uint ChunkSize) {
    if (cy < 0 || cy >= chunkHeight) return 0.0;

    uint packed = readLightByte(cellIndex(cx, cy, cz, ChunkSize));
    uint sun = (packed >> 4u) & 0xFu;
    uint torch = packed & 0xFu;
    return float(max(sun, torch));
}

bool cellSolid(int cx, int cy, int cz, uint ChunkSize) {
    if (cy < 0 || cy >= chunkHeight) return true;
    return readVoxel(cellIndex(cx, cy, cz, ChunkSize)) != 0;
}

float sampleLight(vec3 localPos, int n, uint ChunkSize) {
    vec3 nrm = normalDir(n);

    vec3 p = localPos - nrm * 0.5;
    ivec3 voxel = ivec3(floor(p));
    ivec3 airCell = voxel + ivec3(nrm);

    vec2 f;
    ivec3 du, dv;
    if (n == 0 || n == 1) {
        du = ivec3(1, 0, 0);
        dv = ivec3(0, 1, 0);
        f = localPos.xy;
    } else if (n == 2 || n == 3) {
        du = ivec3(0, 1, 0);
        dv = ivec3(0, 0, 1);
        f = localPos.yz;
    } else {
        du = ivec3(1, 0, 0);
        dv = ivec3(0, 0, 1);
        f = localPos.xz;
    }

    vec2 g = f - 0.5;
    ivec2 base = ivec2(floor(g));
    vec2 w = g - vec2(base);

    ivec3 c00 = airCell + base.x * du + base.y * dv;
    ivec3 c10 = c00 + du;
    ivec3 c01 = c00 + dv;
    ivec3 c11 = c00 + du + dv;

    bool s00 = cellSolid(c00.x, c00.y, c00.z, ChunkSize);
    bool s10 = cellSolid(c10.x, c10.y, c10.z, ChunkSize);
    bool s01 = cellSolid(c01.x, c01.y, c01.z, ChunkSize);
    bool s11 = cellSolid(c11.x, c11.y, c11.z, ChunkSize);

    float v00 = s00 ? 0.0 : 1.0;
    float v10 = s10 ? 0.0 : 1.0;
    float v01 = s01 ? 0.0 : 1.0;
    float v11 = s11 ? 0.0 : 1.0;

    // Anti-leak
    if (s10 || s01) v11 = 0.0;

    float l00 = lightAtCell(c00.x, c00.y, c00.z, ChunkSize);
    float l10 = lightAtCell(c10.x, c10.y, c10.z, ChunkSize);
    float l01 = lightAtCell(c01.x, c01.y, c01.z, ChunkSize);
    float l11 = lightAtCell(c11.x, c11.y, c11.z, ChunkSize);

    float wx = w.x, wy = w.y;
    float b00 = (1.0 - wx) * (1.0 - wy) * v00;
    float b10 = wx * (1.0 - wy) * v10;
    float b01 = (1.0 - wx) * wy * v01;
    float b11 = wx * wy * v11;

    if (debugToggle == 1) {
//        FragColor =
//        vec4(
//        (c00.x+1)/18.0,
//        (c00.y+1)/18.0,
//        (c00.z+1)/18.0,
//        1
//        );

//        FragColor = vec4(b00, b10, b01, 1);

//        FragColor = vec4(fract(f.x), fract(f.y), 0, 1);

        FragColor = vec4(w, 0, 1);

        FragColor = vec4(vec3(l11 / 15.0), 1);

        uint bytePos = vCellOffset + cellIndex(c11.x, c11.y, c11.z, ChunkSize);

        FragColor = vec4(
        float(bytePos & 255u) / 255.0,
        float((bytePos >> 8) & 255u) / 255.0,
        0,
        1
        );

//        FragColor = vec4(
//        w.x,
//        w.y,
//        0,
//        1
//        );

//            FragColor = vec4(
//            vec3(base.x & 1),
//            1);
        //
        //    FragColor = vec4(
        //    float(base.x + 10) / 20.0,
        //    float(base.y + 10) / 20.0,
        //    0,
        //    1);
    }

    float wsum = b00 + b10 + b01 + b11;
    if (wsum < 1e-4) {
        return lightAtCell(airCell.x, airCell.y, airCell.z, ChunkSize);
    }
    return (b00 * l00 + b10 * l10 + b01 * l01 + b11 * l11) / wsum;
}

float brightnessLookup(float level) {
    level = clamp(level, 0.0, 15.0);
    int lo = int(floor(level));
    int hi = int(ceil(level));
    return mix(brightness[lo], brightness[hi], level - float(lo));
}

void main() {
    uint ChunkSize = 1u << uint(chunkSizeShift);

    float light = sampleLight(vLocalPos, normal, ChunkSize);
    float lightF = brightnessLookup(light);

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
        if (debugToggle == 0)
            FragColor = vec4(color, 1.0);
    }
}
