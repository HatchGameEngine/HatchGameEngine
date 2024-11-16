#ifndef ENGINE_FONTFACE_H
#define ENGINE_FONTFACE_H

#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Application.h>
#include <Engine/IO/Stream.h>
#include <Engine/ResourceTypes/ISprite.h>

class FontFace {
public:
    static ISprite* SpriteFromFont(Stream* stream, int pixelSize, char* filename);
};

#endif /* ENGINE_FONTFACE_H */
