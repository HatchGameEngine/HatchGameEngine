#if INTERFACE
#include <Engine/Includes/Standard.h>

class Tileset {
public:
    char*  Filename = nullptr;
    size_t StartTile = 0;
    size_t TileCount = 0;
};
#endif

#include <Engine/Types/Tileset.h>
#include <Engine/Utilities/StringUtils.h>

PUBLIC Tileset::Tileset(size_t tileCount) {
    TileCount = tileCount;
}

PUBLIC Tileset::Tileset(size_t startTile, size_t tileCount, char* filename) {
    TileCount = tileCount;
    StartTile = startTile;
    Filename = StringUtils::Duplicate(filename);
}
