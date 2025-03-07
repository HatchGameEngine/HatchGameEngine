#ifndef ENGINE_RENDERING_VIEWTEXTURE_H
#define ENGINE_RENDERING_VIEWTEXTURE_H

#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/GameTexture.h>

class ViewTexture : public GameTexture {
public:
	int ViewIndex;

	ViewTexture(int viewIndex);
	virtual Texture* GetTexture();
	virtual int GetID();
};

#endif /* ENGINE_RENDERING_VIEWTEXTURE_H */
