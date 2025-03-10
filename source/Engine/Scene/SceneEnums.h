#ifndef ENGINE_SCENE_ENUMS
#define ENGINE_SCENE_ENUMS

enum { TILECOLLISION_NONE = 0, TILECOLLISION_DOWN = 1, TILECOLLISION_UP = 2 };

enum { C_NONE = 0, C_TOP = 1, C_LEFT = 2, C_RIGHT = 3, C_BOTTOM = 4 };

enum { FLIP_NONE = 0, FLIP_X = 1, FLIP_Y = 2, FLIP_XY = 3 };

enum { CMODE_FLOOR = 0, CMODE_LWALL = 1, CMODE_ROOF = 2, CMODE_RWALL = 3 };

enum { H_TYPE_TOUCH = 0, H_TYPE_CIRCLE = 1, H_TYPE_BOX = 2, H_TYPE_PLAT = 3 };

#define TILE_FLIPX_MASK 0x80000000U
#define TILE_FLIPY_MASK 0x40000000U
// #define TILE_DIAGO_MASK 0x20000000U
#define TILE_COLLA_MASK 0x30000000U
#define TILE_COLLB_MASK 0x0C000000U
#define TILE_COLLC_MASK 0x03000000U
#define TILE_IDENT_MASK 0x00FFFFFFU // Max. 16777216 tiles

#endif
