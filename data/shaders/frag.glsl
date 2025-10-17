#version 460 core
out vec4 FragColor;

in vec3 ourColor;
flat in int normal;
in float fragAO;

uniform int windowWidth;
uniform int windowHeight;

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
    vec3 color = ourColor * get_shade(normal);
    float ao = clamp(fragAO, 0.0, 1.0);
    color *= smoothstep(0.0, 1.0, ao);

    if (distance(gl_FragCoord.xy, vec2(windowWidth / 2, windowHeight / 2)) < 5) {
        FragColor = vec4(1.0, 0.0, 0.0, 1.0);
    } else {
        FragColor = vec4(color, 1.0);
    }
}
