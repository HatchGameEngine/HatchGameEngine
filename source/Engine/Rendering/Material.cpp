#include <Engine/Rendering/Material.h>

#include <Engine/Diagnostics/Memory.h>

Material::Material() {
    for (int i = 0; i < 4; i++) {
        ColorDiffuse[i] =
        ColorSpecular[i] =
        ColorAmbient[i] =
        ColorEmissive[i] = 1.0f;
    }
}

void Material::Dispose() {
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

Material::~Material() {
    Dispose();
}
