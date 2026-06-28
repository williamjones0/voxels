#version 460 core

out vec4 FragColor;

in float fragAO;
in vec2 vTexCoord;
flat in int vColourIndex;
flat in int normal;
flat in int fragLightLevel;

uniform sampler2D atlas;
uniform int windowWidth;
uniform int windowHeight;

struct PaletteEntry {
    vec4 colour;
    vec4 uv;
    int hasTexture;
};

layout(std430, binding = 4) buffer PaletteBuffer {
    PaletteEntry palette[];
};

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

void main() {
    float shade = get_shade(normal);
    float ao = clamp(fragAO, 0.0, 1.0);

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

    float light = brightness[fragLightLevel];

    PaletteEntry entry = palette[vColourIndex];

    vec3 color;
    if (entry.hasTexture == 1) {
        vec2 tiledUV = fract(vTexCoord);
        vec2 uv = entry.uv.xy + tiledUV * entry.uv.zw;
        color = texture(atlas, uv).rgb;
    } else {
        color = entry.colour.rgb;
    }

    color *= shade;
    color *= smoothstep(0.0, 1.0, ao);
    color *= light;

    // Crosshair
    if (distance(gl_FragCoord.xy, vec2(windowWidth / 2, windowHeight / 2)) < 5) {
        FragColor = vec4(1.0, 0.0, 0.0, 1.0);
    } else {
        FragColor = vec4(color, 1.0);
    }
}
