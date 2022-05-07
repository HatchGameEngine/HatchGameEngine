#if INTERFACE
#include <Engine/Includes/Standard.h>

need_t Material;

class Material {
public:
    float Specular[4];
    float Ambient[4];
    float Emission[4];
    float Diffuse[4];

    float Shininess;
    float Transparency;
    float IndexOfRefraction;

    void* Texture;
};
#endif

#include <Engine/Rendering/Material.h>

#include <Engine/Diagnostics/Memory.h>

PUBLIC STATIC Material* Material::New(void) {
    Material* material = (Material*)Memory::TrackedCalloc("Material::Material", 1, sizeof(Material));

    for (int i = 0; i < 4; i++) {
        material->Specular[i] = 1.0f;
        material->Ambient[i] = 1.0f;
        material->Emission[i] = 1.0f;
        material->Diffuse[i] = 1.0f;
    }

    material->Shininess = 0.0f;
    material->Transparency = 1.0f;
    material->IndexOfRefraction = 1.0f;

    material->Texture = nullptr;

    return material;
}
