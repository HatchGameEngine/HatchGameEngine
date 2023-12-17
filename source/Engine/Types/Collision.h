#ifndef COLLISION_H
#define COLLISION_H

struct EntityHitbox {
    float Width   = 0.0f;
    float Height  = 0.0f;
    float OffsetX = 0.0f;
    float OffsetY = 0.0f;

    float GetLeft() {
        return (-Width / 2) + OffsetX;
    }
    float GetRight() {
        return (Width / 2) + OffsetX;
    }
    float GetTop() {
        return (-Height / 2) + OffsetY;
    }
    float GetBottom() {
        return (Height / 2) + OffsetY;
    }

    void SetLeft(float left) {
        Width = GetRight() - left;
        OffsetX = left + Width * 0.5f;
    }
    void SetRight(float right) {
        Width = right - GetLeft();
        OffsetX = right - Width * 0.5f;
    }
    void SetTop(float top) {
        Height = GetBottom() - top;
        OffsetY = top + Height * 0.5f;
    }
    void SetBottom(float bottom) {
        Height = bottom - GetTop();
        OffsetY = bottom - Height * 0.5f;
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
