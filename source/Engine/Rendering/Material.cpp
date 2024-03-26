#if INTERFACE
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
};
#endif

#include <Engine/Rendering/Material.h>

#include <Engine/Diagnostics/Memory.h>

PUBLIC Material::Material() {
    for (int i = 0; i < 4; i++) {
        ColorDiffuse[i] =
        ColorSpecular[i] =
        ColorAmbient[i] =
        ColorEmissive[i] = 1.0f;
    }
}

PUBLIC void Material::Dispose() {
    delete TextureDiffuse;
    delete TextureSpecular;
    delete TextureAmbient;
    delete TextureEmissive;

    Memory::Free(Name);

    Memory::Free(TextureDiffuseName);
    Memory::Free(TextureSpecularName);
    Memory::Free(TextureAmbientName);
    Memory::Free(TextureEmissiveName);
}

PUBLIC Material::~Material() {
    Dispose();
}
