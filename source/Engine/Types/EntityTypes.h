#ifndef ENTITYTYPES_H
#define ENTITYTYPES_H

enum {
    Persistence_NONE,
    Persistence_SCENE,
    Persistence_GAME
};

enum {
    Active_NEVER   = 0, // Never updates
    Active_ALWAYS  = 1, // Updates no matter what
    Active_NORMAL  = 2, // Updates no matter where the object is in the scene, but not if scene is paused
    Active_PAUSED  = 3, // Only updates when the scene is paused
    Active_BOUNDS  = 4, // Updates only when the object is within bounds
    Active_XBOUNDS = 5, // Updates within an x bound (not accounting for y bound)
    Active_YBOUNDS = 6, // Updates within a y bound (not accounting for x bound)
    Active_RBOUNDS = 7  // Updates within a radius (UpdateRegionW)
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
