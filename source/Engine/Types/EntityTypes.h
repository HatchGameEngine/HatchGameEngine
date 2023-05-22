#ifndef ENTITYTYPES_H
#define ENTITYTYPES_H

struct String {
    Uint16 Length;
    char*  Data;
};

struct Color {
    Uint8 A;
    Uint8 G;
    Uint8 B;
    Uint8 R;
};
struct Position {
    Sint32 X;
    Sint32 Y;
};
union Interface {
    Uint8    asUint8;
    Uint16   asUint16;
    Uint32   asUint32;
    Sint8    asInt8;
    Sint16   asInt16;
    Sint32   asInt32;
    bool     asBool;
    String   asString;
    Position asPosition;
    Color    asColor;
};
struct InterfaceValue {
    Uint8     Type;
    Uint8     Size;
    Interface As;
};
#define STACK_MAX 64

struct GlobalTableEntry {
    Uint32    Key;
    bool      Used;
    Uint8     Type;
    Uint8     Size;
    void*     Pointer;
};
#define GLOBAL_TABLE_MAX 32

struct AttributeValue {
    Uint32    Key = 0x00000000U;
    bool      Used = false;
    Uint8     Type;
    Interface Value;
};

struct Rect {
    int  Left = 0x0;
    int  Right = 0x0;
    int  Top = 0x0;
    int  Bottom = 0x0;

    Rect() : Left(0), Right(0), Top(0), Bottom(0) {};
    Rect(int l, int r, int t, int b) : Left(l), Right(r), Top(t), Bottom(b) {};

    bool Empty() { return !Left && !Right && !Top && !Bottom; };
    bool Collides(int x1, int y1, int x2, int y2) {
        return  x1 + Right  >= x2 + Left &&
                x1 + Left   <  x2 + Right &&
                y1 + Bottom >= y2 + Top &&
                y1 + Top    <  y2 + Bottom;
    };
    Rect FlipX(bool flipX) { if (!flipX) return Rect(Left, Right, Top, Bottom); Left = -Left; Right = -Right; return Rect(Left, Right, Top, Bottom); };
    Rect FlipY(bool flipY) { if (!flipY) return Rect(Left, Right, Top, Bottom); Top = -Top; Bottom = -Bottom; return Rect(Left, Right, Top, Bottom); };
};


namespace CollideSide {
    enum {
        NONE = 0,
        LEFT = 8,
        BOTTOM = 4,
        RIGHT = 2,
        TOP = 1,

        SIDES = 10,
        TOP_SIDES = 11,
        BOTTOM_SIDES = 14,
    };
};

struct Sensor {
    int X;
    int Y;
    int Collided;
    int Angle;
};

struct CollisionSensor {
    float   X;
    float   Y;
    int     Collided;
    int     Angle;
};

#define DEBUG_HITBOX_COUNT (0x400)

struct DebugHitboxInfo {
    int             type;
    int             collision;
    Entity*         entity;
    CollisionBox    hitbox;
    int             x;
    int             y;
};

#endif /* ENTITYTYPES_H */
