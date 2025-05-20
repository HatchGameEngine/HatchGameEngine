#ifndef ENGINE_RESOURCETYPES_IMAGE_H
#define ENGINE_RESOURCETYPES_IMAGE_H

#include <Engine/Includes/Standard.h>
#include <Engine/IO/Stream.h>
#include <Engine/Rendering/GameTexture.h>
#include <Engine/Rendering/Texture.h>

enum {
	IMAGE_FORMAT_UNKNOWN,
	IMAGE_FORMAT_PNG,
	IMAGE_FORMAT_GIF,
	IMAGE_FORMAT_JPEG
};

class Image {
private:
	static Image* New(const char* filename);

public:
	int ID = -1;
	char* Filename;
	Texture* TexturePtr = NULL;

	Image(const char* filename);
	void Dispose();
	~Image();

	static Uint8 DetectFormat(Stream* stream);
	static bool IsFile(Stream* stream);
	static Texture* LoadTextureFromResource(const char* filename);
};

#endif /* ENGINE_RESOURCETYPES_IMAGE_H */
