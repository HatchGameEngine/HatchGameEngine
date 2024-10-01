#ifndef ENGINE_RENDERING_MATERIAL_H
#define ENGINE_RENDERING_MATERIAL_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/ResourceTypes/Image.h>

class Material {
public:
    char* Name = nullptr;
    float ColorDiffuse[4];
    float ColorSpecular[4];
    float ColorAmbient[4];
    float ColorEmissive[4];
    float Shininess = 0.0f;
    float ShininessStrength = 1.0f;
    float Opacity = 1.0f;
    char* TextureDiffuseName = nullptr;
    char* TextureSpecularName = nullptr;
    char* TextureAmbientName = nullptr;
    char* TextureEmissiveName = nullptr;
    Image* TextureDiffuse = nullptr;
    Image* TextureSpecular = nullptr;
    Image* TextureAmbient = nullptr;
    Image* TextureEmissive = nullptr;

    Material();
    void Dispose();
    ~Material();
};

#endif /* ENGINE_RENDERING_MATERIAL_H */
