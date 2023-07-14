#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/ResourceTypes/ISprite.h>

class Tileset {
public:
    ISprite* Sprite = nullptr;
    char*    Filename = nullptr;
    size_t   StartTile = 0;
    size_t   TileCount = 0;
};
#endif

#include <Engine/Types/Tileset.h>
#include <Engine/Utilities/StringUtils.h>

PUBLIC Tileset::Tileset(ISprite* sprite, size_t startTile, size_t tileCount, char* filename) {
    Sprite = sprite;
    TileCount = tileCount;
    StartTile = startTile;
    Filename = StringUtils::Duplicate(filename);
}
