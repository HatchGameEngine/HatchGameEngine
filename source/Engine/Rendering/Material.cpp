#include <Engine/Rendering/Material.h>

#include <Engine/Diagnostics/Memory.h>

#include <Engine/Utilities/StringUtils.h>

#include <Engine/Bytecode/Types.h>

std::vector<Material*> Material::List;

Material* Material::Create(char* name) {
    Material* material = new Material(name);

    material->Object = (void *)NewMaterial(material);

    List.push_back(material);

    return material;
}

void Material::Remove(Material* material) {
    auto it = std::find(List.begin(), List.end(), material);
    if (it != List.end())
        List.erase(it);
}

Material::Material(char* name) {
    Name = name;

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
