#version 330 core
out vec4 FragColor;  
in vec3 ourColor;
flat in int normal;

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
    FragColor = vec4(ourColor * get_shade(normal), 1.0);
}
