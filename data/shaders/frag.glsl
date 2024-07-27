#version 330 core
out vec4 FragColor;

in vec3 ourColor;
flat in int normal;
in float fragAo;
in vec4 ao;
in vec2 surfacePos;

float get_shade(int type) {
  switch (type) {
    case 0: return 0.8;
    case 1: return 0.5;
    case 2: return 0.4;
    case 3: return 0.6;
    case 4: return 0.1;
    case 5: return 0.9;
  }
}

void main() {
    vec3 color = ourColor * get_shade(normal);
    float ao = clamp(fragAo, 0.0, 1.0);
    color *= smoothstep(0.0, 1.0, ao);

    color = vec3(1.0) - vec3(1.0 / 6) * (normal);

    FragColor = vec4(color, 1.0);
}
