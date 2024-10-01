#ifndef ENGINE_RENDERING_TEXTUREREFERENCE_H
#define ENGINE_RENDERING_TEXTUREREFERENCE_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED

class Texture;

#include <Engine/Includes/Standard.h>

class TextureReference {
public:
    Texture* TexturePtr;
    unsigned References;

    TextureReference(Texture *ptr);
    void AddRef();
    bool TakeRef();
};

#endif /* ENGINE_RENDERING_TEXTUREREFERENCE_H */
