#ifndef ENGINE_RENDERING_GAMETEXTURE_H
#define ENGINE_RENDERING_GAMETEXTURE_H

#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/Texture.h>

class GameTexture {
public:
	Texture* TexturePtr = nullptr;
	int UnloadPolicy;
	bool OwnsTexture = true;

	GameTexture();
	GameTexture(Uint32 width, Uint32 height, int unloadPolicy);
	virtual Texture* GetTexture();
	virtual int GetID();
	virtual ~GameTexture();
};

#endif /* ENGINE_RENDERING_GAMETEXTURE_H */
