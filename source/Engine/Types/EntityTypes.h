#ifndef ENTITYTYPES_H
#define ENTITYTYPES_H

enum {
    Persistence_NONE,
    Persistence_SCENE,
    Persistence_GAME
};

enum {
    LEFT    = 0,
    TOP     = 1,
    RIGHT   = 2,
    BOTTOM  = 3
};

enum {
    ACTIVE_NEVER   = 0, // Never updates
    ACTIVE_ALWAYS  = 1, // Updates no matter what
    ACTIVE_NORMAL  = 2, // Updates no matter where the object is in the scene, but not if scene is paused
    ACTIVE_PAUSED  = 3, // Only updates when the scene is paused
    ACTIVE_BOUNDS  = 4, // Updates only when the object is within bounds
    ACTIVE_XBOUNDS = 5, // Updates within an x bound (not accounting for y bound)
    ACTIVE_YBOUNDS = 6, // Updates within a y bound (not accounting for x bound)
    ACTIVE_RBOUNDS = 7  // Updates within a radius (UpdateRegionW)
};

enum {
    SUNDAY      = 0,
    MONDAY      = 1,
    TUESDAY     = 2,
    WEDNESDAY   = 3,
    THURSDAY    = 4,
    FRIDAY      = 5,
    SATURDAY    = 6
};

enum {
    MORNING = 0, // Hours 5AM to 11AM. 0500 to 1100.
    MIDDAY  = 1, // Hours 12PM to 4PM. 1200 to 1600.
    EVENING = 2, // Hours 5PM to 8PM.  1700 to 2000.
    NIGHT   = 3  // Hours 9PM to 4AM.  2100 to 400.
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
