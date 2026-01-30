#ifndef ENGINE_RENDERING_TEXTUREREFERENCE_H
#define ENGINE_RENDERING_TEXTUREREFERENCE_H
class Texture;

#include <Engine/Includes/Standard.h>

class TextureReference {
public:
	Texture* TexturePtr;
	unsigned References;

	TextureReference(Texture* ptr);
	void TakeRef();
	bool ReleaseRef();
};

#endif /* ENGINE_RENDERING_TEXTUREREFERENCE_H */
