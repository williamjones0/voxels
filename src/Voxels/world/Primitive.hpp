#pragma once

struct Primitive {
    virtual ~Primitive() = default;
};

struct Cuboid : Primitive {
    Cuboid(const float sx, const float sy, const float sz,
           const float ex, const float ey, const float ez)
        : startX(sx), startY(sy), startZ(sz),
          endX(ex), endY(ey), endZ(ez) {}

    float startX;
    float startY;
    float startZ;
    float endX;
    float endY;
    float endZ;
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
