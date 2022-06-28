#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/ResourceTypes/Image.h>

class Material {
public:
    float Diffuse[4];
    float Specular[4];
    float Ambient[4];
    float Emissive[4];

    float Shininess;
    float ShininessStrength;
    float Opacity;
    float IndexOfRefraction;

    Image* ImagePtr;
};
#endif

#include <Engine/Rendering/Material.h>

PUBLIC Material::Material() {
    for (int i = 0; i < 3; i++) {
        Diffuse[i] = 0.0f;
        Specular[i] = 0.0f;
        Ambient[i] = 0.0f;
        Emissive[i] = 0.0f;
    }

    Diffuse[3] = 1.0f;
    Specular[3] = 1.0f;
    Ambient[3] = 1.0f;
    Emissive[3] = 1.0f;

    Shininess = 0.0f;
    ShininessStrength = 1.0f;
    Opacity = 1.0f;
    IndexOfRefraction = 1.0f;

    ImagePtr = nullptr;
}

PUBLIC void Material::Dispose() {
    delete ImagePtr;
}

PUBLIC Material::~Material() {
    Dispose();
}
