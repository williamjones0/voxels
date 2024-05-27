#pragma once

#include <vector>
#include <util/PerlinNoise.hpp>

#define FRONT_FACE 0
#define BACK_FACE 18
#define LEFT_FACE 36
#define RIGHT_FACE 54
#define BOTTOM_FACE 72
#define TOP_FACE 90

#define VERTICES_LENGTH 108

#define FRONT_NORMAL 0
#define BACK_NORMAL 1
#define LEFT_NORMAL 2
#define RIGHT_NORMAL 3
#define BOTTOM_NORMAL 4
#define TOP_NORMAL 5

static inline const int vertexAO(uint8_t side1, uint8_t side2, uint8_t corner) {
    return (side1 && side2) ? 0 : (3 - (side1 + side2 + corner));
}

bool inBounds(int i, int j, int size) {
    return (0 <= i && i < size) && (0 <= j && j < size);
}

void mesh(
    std::vector<int> &world,
    std::vector<float> &world_colours,
    std::vector<int> &world_normals,
    std::vector<int> &world_ao
);

void mesh(
    std::vector<int> &world,
    std::vector<float> &world_colours,
    std::vector<int> &world_normals,
    std::vector<int> &world_ao
) {
    float vertices[] = {
        // Front
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,

        // Back
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,

        // Left
        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,

        // Right
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,

         // Bottom
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,

        // Top
        -0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f
    };

    const int WORLD_SIZE = 256;
    const double HEIGHT_SCALE = 32;

    int* chunk = new int[WORLD_SIZE * WORLD_SIZE];

    const siv::PerlinNoise::seed_type seed = 123456u;
    const siv::PerlinNoise perlin{ seed };

    for (int y = 0; y < WORLD_SIZE; ++y) {
        for (int x = 0; x < WORLD_SIZE; ++x) {
            const double noise = perlin.octave2D_01((x * 0.01), (y * 0.01), 4);
            chunk[y * WORLD_SIZE + x] = noise * HEIGHT_SCALE;
        }
    }

    //int* chunk = new int[] {
    //    1, 1, 1, 1,
    //        1, 3, 3, 1,
    //        1, 3, 2, 1,
    //        1, 1, 1, 1
    //    };

    //int* chunk = new int[] {
    //        1, 1, 1,
    //        1, 2, 3,
    //        1, 3, 3
    //};

    float red_face[] = {
        0.6, 0.1, 0.1,
        0.6, 0.1, 0.1,
        0.6, 0.1, 0.1,
        0.6, 0.1, 0.1,
        0.6, 0.1, 0.1,
        0.6, 0.1, 0.1
    };

    float green_face[] = {
        0.278, 0.600, 0.141,
        0.278, 0.600, 0.141,
        0.278, 0.600, 0.141,
        0.278, 0.600, 0.141,
        0.278, 0.600, 0.141,
        0.278, 0.600, 0.141
    };

    for (int i = 0; i < WORLD_SIZE; ++i) {
        for (int j = 0; j < WORLD_SIZE; ++j) {
            float* translated_vertices_f = new float[VERTICES_LENGTH];
            std::memcpy(translated_vertices_f, vertices, VERTICES_LENGTH * sizeof(float));
            for (int k = 0; k < 36; ++k) {
                translated_vertices_f[3 * k] += j;
                translated_vertices_f[3 * k + 1] += chunk[WORLD_SIZE * i + j];
                translated_vertices_f[3 * k + 2] += i;
            }

            int* translated_vertices = new int[VERTICES_LENGTH];
            for (int k = 0; k < 108; ++k) {
                translated_vertices[k] = (int)(translated_vertices_f[k] + 0.5f);
            }

            int height = chunk[WORLD_SIZE * i + j];

            int x_plus1 = WORLD_SIZE * i + j + 1;
            int x_minus1 = WORLD_SIZE * i + j - 1;
            int z_plus1 = WORLD_SIZE * (i + 1) + j;
            int z_minus1 = WORLD_SIZE * (i - 1) + j;

            int x_plus1h = -1;
            int x_minus1h = -1;
            int z_plus1h = -1;
            int z_minus1h = -1;

            if (0 <= x_plus1 && x_plus1 < WORLD_SIZE * WORLD_SIZE) {
                x_plus1h = chunk[x_plus1];
            }
            if (0 <= x_minus1 && x_minus1 < WORLD_SIZE * WORLD_SIZE) {
                x_minus1h = chunk[x_minus1];
            }
            if (0 <= z_plus1 && z_plus1 < WORLD_SIZE * WORLD_SIZE) {
                z_plus1h = chunk[z_plus1];
            }
            if (0 <= z_minus1 && z_minus1 < WORLD_SIZE * WORLD_SIZE) {
                z_minus1h = chunk[z_minus1];
            }

            // Draw top face always
            world.insert(world.end(), &translated_vertices[TOP_FACE], &translated_vertices[TOP_FACE + 18]);
            world_colours.insert(world_colours.end(), &green_face[0], &green_face[18]);
            for (int i = 0; i < 6; i++) {
                world_normals.push_back(TOP_NORMAL);
            }

            if (height > x_plus1h && x_plus1h != -1) {
                for (int k = 0; k < height - x_plus1h; ++k) {
                    world.insert(world.end(), &translated_vertices[RIGHT_FACE], &translated_vertices[RIGHT_FACE + 18]);
                    for (int i = 0; i < 6; i++) {
                        world_normals.push_back(RIGHT_NORMAL);
                    }

                    if (k == 0) {
                        world_colours.insert(world_colours.end(), &green_face[0], &green_face[18]);
                    }
                    else {
                        world_colours.insert(world_colours.end(), &red_face[0], &red_face[18]);
                    }

                    // Correct the y position of each face's vertices (first face, subtract nothing; second, subtract one, etc.)
                    for (int v = 0; v < 6; ++v) {
                        world[world.size() - 18 + 3 * v + 1] -= k;
                    }
                }
            }

            if (height > x_minus1h && x_minus1h != -1) {
                for (int k = 0; k < height - x_minus1h; ++k) {
                    world.insert(world.end(), &translated_vertices[LEFT_FACE], &translated_vertices[LEFT_FACE + 18]);
                    for (int i = 0; i < 6; i++) {
                        world_normals.push_back(LEFT_NORMAL);
                    }

                    if (k == 0) {
                        world_colours.insert(world_colours.end(), &green_face[0], &green_face[18]);
                    }
                    else {
                        world_colours.insert(world_colours.end(), &red_face[0], &red_face[18]);
                    }

                    for (int v = 0; v < 6; ++v) {
                        world[world.size() - 18 + 3 * v + 1] -= k;
                    }
                }
            }

            if (height > z_plus1h && z_plus1h != -1) {
                for (int k = 0; k < height - z_plus1h; ++k) {
                    world.insert(world.end(), &translated_vertices[BACK_FACE], &translated_vertices[BACK_FACE + 18]);
                    for (int i = 0; i < 6; i++) {
                        world_normals.push_back(BACK_NORMAL);
                    }

                    if (k == 0) {
                        world_colours.insert(world_colours.end(), &green_face[0], &green_face[18]);
                    }
                    else {
                        world_colours.insert(world_colours.end(), &red_face[0], &red_face[18]);
                    }

                    for (int v = 0; v < 6; ++v) {
                        world[world.size() - 18 + 3 * v + 1] -= k;
                    }
                }
            }

            if (height > z_minus1h && z_minus1h != -1) {
                for (int k = 0; k < height - z_minus1h; ++k) {
                    world.insert(world.end(), &translated_vertices[FRONT_FACE], &translated_vertices[FRONT_FACE + 18]);
                    for (int i = 0; i < 6; i++) {
                        world_normals.push_back(FRONT_NORMAL);
                    }

                    if (k == 0) {
                        world_colours.insert(world_colours.end(), &green_face[0], &green_face[18]);
                    }
                    else {
                        world_colours.insert(world_colours.end(), &red_face[0], &red_face[18]);
                    }

                    for (int v = 0; v < 6; ++v) {
                        world[world.size() - 18 + 3 * v + 1] -= k;
                    }
                }
            }
        }
    }

    // Ambient occlusion
    for (int i = 0; i < WORLD_SIZE; ++i) {
        for (int j = 0; j < WORLD_SIZE; ++j) {
            //std::cout << i << " " << j << std::endl;
            int height = chunk[WORLD_SIZE * i + j];

            int x_plus1 = WORLD_SIZE * i + j + 1;
            int x_minus1 = WORLD_SIZE * i + j - 1;
            int z_plus1 = WORLD_SIZE * (i + 1) + j;
            int z_minus1 = WORLD_SIZE * (i - 1) + j;

            int x_plus1h = -1;
            int x_minus1h = -1;
            int z_plus1h = -1;
            int z_minus1h = -1;

            if (0 <= x_plus1 && x_plus1 < WORLD_SIZE * WORLD_SIZE) {
                x_plus1h = chunk[x_plus1];
            }
            if (0 <= x_minus1 && x_minus1 < WORLD_SIZE * WORLD_SIZE) {
                x_minus1h = chunk[x_minus1];
            }
            if (0 <= z_plus1 && z_plus1 < WORLD_SIZE * WORLD_SIZE) {
                z_plus1h = chunk[z_plus1];
            }
            if (0 <= z_minus1 && z_minus1 < WORLD_SIZE * WORLD_SIZE) {
                z_minus1h = chunk[z_minus1];
            }

            bool ao_F = inBounds(i - 1, j, WORLD_SIZE) && chunk[WORLD_SIZE * (i - 1) + j] > chunk[WORLD_SIZE * i + j];
            bool ao_B = inBounds(i + 1, j, WORLD_SIZE) && chunk[WORLD_SIZE * (i + 1) + j] > chunk[WORLD_SIZE * i + j];
            bool ao_L = inBounds(i, j - 1, WORLD_SIZE) && chunk[WORLD_SIZE * i + j - 1] > chunk[WORLD_SIZE * i + j];
            bool ao_R = inBounds(i, j + 1, WORLD_SIZE) && chunk[WORLD_SIZE * i + j + 1] > chunk[WORLD_SIZE * i + j];

            bool ao_LFC = inBounds(i - 1, j - 1, WORLD_SIZE) && chunk[WORLD_SIZE * (i - 1) + j - 1] > chunk[WORLD_SIZE * i + j];
            bool ao_LBC = inBounds(i + 1, j - 1, WORLD_SIZE) && chunk[WORLD_SIZE * (i + 1) + j - 1] > chunk[WORLD_SIZE * i + j];
            bool ao_RFC = inBounds(i - 1, j + 1, WORLD_SIZE) && chunk[WORLD_SIZE * (i - 1) + j + 1] > chunk[WORLD_SIZE * i + j];
            bool ao_RBC = inBounds(i + 1, j + 1, WORLD_SIZE) && chunk[WORLD_SIZE * (i + 1) + j + 1] > chunk[WORLD_SIZE * i + j];

            int ao_LB = vertexAO(ao_L, ao_B, ao_LBC);  // -x, +z
            int ao_RB = vertexAO(ao_R, ao_B, ao_RBC);  // +x, +z
            int ao_RF = vertexAO(ao_R, ao_F, ao_RFC);  // +x, -z
            int ao_LF = vertexAO(ao_L, ao_F, ao_LFC);  // -x, -z

            //std::cout << "ao_F: " << ao_F << std::endl;
            //std::cout << "ao_B: " << ao_B << std::endl;
            //std::cout << "ao_L: " << ao_L << std::endl;
            //std::cout << "ao_R: " << ao_R << std::endl;

            //std::cout << "ao_LFC: " << ao_LFC << std::endl;
            //std::cout << "ao_LBC: " << ao_LBC << std::endl;
            //std::cout << "ao_RFC: " << ao_RFC << std::endl;
            //std::cout << "ao_RBC: " << ao_RBC << std::endl;

            //std::cout << "ao_LB: " << ao_LB << std::endl;
            //std::cout << "ao_RB: " << ao_RB << std::endl;
            //std::cout << "ao_RF: " << ao_RF << std::endl;
            //std::cout << "ao_LF: " << ao_LF << std::endl;

            //// Top (0)
            //std::cout << "top" << std::endl;
            world_ao.push_back(ao_LF);
            world_ao.push_back(ao_RF);
            world_ao.push_back(ao_RB);
            world_ao.push_back(ao_RB);
            world_ao.push_back(ao_LB);
            world_ao.push_back(ao_LF);

            //// Bottom (1)
            //world_ao.push_back(ao_LB);
            //world_ao.push_back(ao_LF);
            //world_ao.push_back(ao_RF);
            //world_ao.push_back(ao_RF);
            //world_ao.push_back(ao_RB);
            //world_ao.push_back(ao_LF);

            // Right
            if (height > x_plus1h && x_plus1h != -1) {
                for (int k = 0; k < height - x_plus1h; ++k) {
                    std::cout << "right" << std::endl;
                    world_ao.push_back(ao_LB);
                    world_ao.push_back(ao_LF);
                    world_ao.push_back(ao_RF);
                    world_ao.push_back(ao_RF);
                    world_ao.push_back(ao_RB);
                    world_ao.push_back(ao_LF);
                }
            }

            // Left
            if (height > x_minus1h && x_minus1h != -1) {
                for (int k = 0; k < height - x_minus1h; ++k) {
                    std::cout << "left" << std::endl;
                    world_ao.push_back(ao_LB);
                    world_ao.push_back(ao_RB);
                    world_ao.push_back(ao_RF);
                    world_ao.push_back(ao_RF);
                    world_ao.push_back(ao_LF);
                    world_ao.push_back(ao_RB);
                }
            }

            // Back
            if (height > z_plus1h && z_plus1h != -1) {
                for (int k = 0; k < height - z_plus1h; ++k) {
                    std::cout << "back" << std::endl;
                    world_ao.push_back(ao_LB);
                    world_ao.push_back(ao_LF);
                    world_ao.push_back(ao_RF);
                    world_ao.push_back(ao_RF);
                    world_ao.push_back(ao_RB);
                    world_ao.push_back(ao_LF);
                }
            }

            // Front
            if (height > z_minus1h && z_minus1h != -1) {
                for (int k = 0; k < height - z_minus1h; ++k) {
                    std::cout << "front" << std::endl;
                    world_ao.push_back(ao_LB);
                    world_ao.push_back(ao_RB);
                    world_ao.push_back(ao_RF);
                    world_ao.push_back(ao_RF);
                    world_ao.push_back(ao_LF);
                    world_ao.push_back(ao_RB);
                }
            }
        }
    }

    std::cout << "world vertices size:\t" << world.size() << std::endl;
    std::cout << "original num vertices:\t" << WORLD_SIZE * WORLD_SIZE * VERTICES_LENGTH << std::endl;
}
