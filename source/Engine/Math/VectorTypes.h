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

    FVector2 operator+(const FVector2& b) {
        FVector2 result(X + b.X, Y + b.Y);
        return result;
    }

    FVector2 operator-(const FVector2& b) {
        FVector2 result(X - b.X, Y - b.Y);
        return result;
    }

    FVector2 operator-() {
        return FVector2(-X, -Y);
    }

    bool operator==(const FVector2& rhs) {
        return X == rhs.X && Y == rhs.Y;
    }

    bool operator!=(const FVector2& rhs) {
        return !(*this == rhs);
    }

    static float CrossProduct(FVector2 v1, FVector2 v2, FVector2 v3) {
        FVector2 va = v2 - v1;
        FVector2 vb = v3 - v2;
        return Determinant(va, vb);
    }

    static float DotProduct(FVector2 v1, FVector2 v2) {
        return (v1.X * v2.X) + (v1.Y * v2.Y);
    }

    static float Determinant(FVector2 v1, FVector2 v2) {
        return (v1.X * v2.Y) - (v1.Y * v2.X);
    }
};

#endif /* VECTORTYPES_H */
