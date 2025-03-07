#ifndef ENGINE_FONTFACE_H
#define ENGINE_FONTFACE_H

#include <Engine/Application.h>
#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/ResourceTypes/ISprite.h>

class FontFace {
public:
	static ISprite* SpriteFromFont(Stream* stream, int pixelSize, const char* filename);
};

#endif /* ENGINE_FONTFACE_H */
