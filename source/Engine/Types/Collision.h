#ifndef COLLISION_H
#define COLLISION_H

struct CollisionBox {
    int Left;
    int Top;
    int Right;
    int Bottom;
};

struct EntityHitbox {
    float Width   = 0.0f;
    float Height  = 0.0f;
    float OffsetX = 0.0f;
    float OffsetY = 0.0f;

    float GetLeft() {
        return -Width / 2;
    }
    float GetRight() {
        return Width / 2;
    }
    float GetTop() {
        return -Height / 2;
    }
    float GetBottom() {
        return Height / 2;
    }

    void SetLeft(float left) {
        Width = GetRight() - left;
        OffsetX = GetLeft() + Width * 0.5f;
    }
    void SetRight(float right) {
        Width = right - GetLeft();
        OffsetX = GetLeft() + Width * 0.5f;
    }
    void SetTop(float top) {
        Height = GetBottom() - top;
        OffsetY = GetTop() + Height * 0.5f;
    }
    void SetBottom(float bottom) {
        Height = bottom - GetTop();
        OffsetY = GetTop() + Height * 0.5f;
    }

    void Clear() {
        Width = 0.0f;
        Height = 0.0f;
        OffsetX = 0.0f;
        OffsetY = 0.0f;
    }

    void Set(CollisionBox box) {
        Width = box.Right - box.Left;
        Height = box.Bottom - box.Top;
        OffsetX = box.Left + Width * 0.5f;
        OffsetY = box.Top + Height * 0.5f;
    }
};

#endif
