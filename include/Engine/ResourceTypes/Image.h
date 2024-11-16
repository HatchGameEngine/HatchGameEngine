#ifndef ENGINE_RESOURCETYPES_IMAGE_H
#define ENGINE_RESOURCETYPES_IMAGE_H

#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/Texture.h>

class Image {
public:
    char Filename[256];
    Texture* TexturePtr = NULL;

    Image(const char* filename);
    void Dispose();
    ~Image();
    static Texture* LoadTextureFromResource(const char* filename);
};

#endif /* ENGINE_RESOURCETYPES_IMAGE_H */
