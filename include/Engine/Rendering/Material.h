#ifndef ENGINE_RENDERING_MATERIAL_H
#define ENGINE_RENDERING_MATERIAL_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED

class Image;

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

    Material();
    void Dispose();
    ~Material();
};

#endif /* ENGINE_RENDERING_MATERIAL_H */
