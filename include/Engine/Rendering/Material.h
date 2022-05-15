#ifndef ENGINE_RENDERING_MATERIAL_H
#define ENGINE_RENDERING_MATERIAL_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED

class Material;

#include <Engine/Includes/Standard.h>

class Material {
public:
    float Specular[4];
    float Ambient[4];
    float Emission[4];
    float Diffuse[4];
    float Shininess;
    float Transparency;
    float IndexOfRefraction;
    void* Image;

    static Material* New(void);
};

#endif /* ENGINE_RENDERING_MATERIAL_H */
