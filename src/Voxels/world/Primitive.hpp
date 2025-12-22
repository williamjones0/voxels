#pragma once

struct Primitive {
    virtual ~Primitive() = default;
};

struct Cuboid : Primitive {
    Cuboid(const float sx, const float sy, const float sz)
        : sizeX(sx), sizeY(sy), sizeZ(sz) {}

    float sizeX;
    float sizeY;
    float sizeZ;
};

struct Sphere : Primitive {
    explicit Sphere(const float r) : radius(r) {}

    float radius;
};

struct Cylinder : Primitive {
    Cylinder(const float r, const float h)
        : radius(r), height(h) {}

    float radius;
    float height;
};
