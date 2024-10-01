#ifndef ENGINE_RENDERING_VIEWTEXTURE_H
#define ENGINE_RENDERING_VIEWTEXTURE_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


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
