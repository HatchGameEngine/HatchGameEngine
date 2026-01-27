#ifndef ENGINE_RESOURCETYPES_IMAGE_H
#define ENGINE_RESOURCETYPES_IMAGE_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/GameTexture.h>
#include <Engine/Rendering/Texture.h>

enum { IMAGE_FORMAT_UNKNOWN, IMAGE_FORMAT_PNG, IMAGE_FORMAT_GIF, IMAGE_FORMAT_JPEG };

class Image {
public:
	int ID = -1;
	int References = 0;
	char* Filename;
	Texture* TexturePtr = NULL;

	Image(const char* filename);
	Image(Texture* texturePtr);
	void AddRef();
	bool TakeRef();
	void Dispose();
	~Image();

	static Uint8 DetectFormat(Stream* stream);
	static bool IsFile(Stream* stream);
	static Texture* LoadTextureFromResource(const char* filename);
};

#endif /* ENGINE_RESOURCETYPES_IMAGE_H */
