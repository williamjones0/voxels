#version 330 core
out vec4 FragColor;  
in vec3 ourColor;
in vec2 texCoord;
flat in int normal;
// in float fragAo;
in vec4 quadAo;

float get_shade(int type) {
    return 1.0;
    switch (type) {
        case 0: return 0.8;
        case 1: return 0.5;
        case 2: return 0.4;
        case 3: return 0.6;
        case 4: return 0.1;
        case 5: return 0.9;
    }
}

float jeff(float ao) {
    float lol = clamp(ao, 0.0, 1.0);
    return smoothstep(0.0, 1.0, lol);
}

void main() {
    vec3 color = vec3(1.0);
    // float ao = clamp(fragAo, 0.0, 1.0);
    // bl, br, tl, tr
    float ao = mix(mix(quadAo[0], quadAo[1], texCoord.x), mix(quadAo[2], quadAo[3], texCoord.x), texCoord.y);
    color *= smoothstep(0.0, 1.0, ao);

    // color = vec3(texCoord.x, texCoord.y, 0.0);

    FragColor = vec4(color, 1.0);
}
