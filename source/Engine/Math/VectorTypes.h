#ifndef VECTORTYPES_H
#define VECTORTYPES_H

struct Vector2 {
    Sint64 X;
    Sint64 Y;
};
struct Vector3 {
    Sint64 X;
    Sint64 Y;
    Sint64 Z;
};
struct Vector4 {
    Sint64 X;
    Sint64 Y;
    Sint64 Z;
    Sint64 W;
};

struct FVector2 {
    float X = 0.0;
    float Y = 0.0;

    FVector2() { }

    FVector2(float x, float y) {
        X = x;
        Y = y;
    }
};

#endif /* VECTORTYPES_H */
