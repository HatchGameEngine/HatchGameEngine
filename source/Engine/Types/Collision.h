#ifndef COLLISION_H
#define COLLISION_H

struct CollisionBox {
    int Left;
    int Top;
    int Right;
    int Bottom;
};

struct EntityHitbox {
    float Left   = 0.0f;
    float Top    = 0.0f;
    float Right  = 0.0f;
    float Bottom = 0.0f;

    float GetWidth() {
        return Right - Left;
    }
    float GetHeight() {
        return Bottom - Top;
    }
    float GetOffsetX() {
        return Left + GetWidth() * 0.5f;
    }
    float GetOffsetY() {
        return Top + GetHeight() * 0.5f;
    }

    void Clear() {
        Left = Top = Right = Bottom = 0.0f;
    }
    void Set(CollisionBox box) {
        Left = box.Left;
        Top = box.Top;
        Right = box.Right;
        Bottom = box.Bottom;
    }
};

#endif
