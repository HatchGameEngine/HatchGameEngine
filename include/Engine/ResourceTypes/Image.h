#ifndef ENGINE_RESOURCETYPES_IMAGE_H
#define ENGINE_RESOURCETYPES_IMAGE_H

#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/GameTexture.h>
#include <Engine/Rendering/Texture.h>

class Image {
private:
	static Image* New(const char* filename);

public:
	int ID = -1;
	int References = 0;
	char* Filename;
	Texture* TexturePtr = NULL;

	Image(const char* filename);
	void AddRef();
	bool TakeRef();
	void Dispose();
	~Image();

	static Texture* LoadTextureFromResource(const char* filename);
};

#endif /* ENGINE_RESOURCETYPES_IMAGE_H */
