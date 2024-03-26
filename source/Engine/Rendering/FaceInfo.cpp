#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/Material.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/Rendering/3D.h>

class FaceInfo {
public:
    Uint32       DrawMode;
    Uint32       NumVertices;
    Uint32       VerticesStartIndex;
    bool         UseMaterial;
    FaceMaterial MaterialInfo;
    BlendState   Blend;
    Uint8        CullMode;
    int          Depth;
};
#endif

#include <Engine/Rendering/FaceInfo.h>

PUBLIC void FaceInfo::SetMaterial(Material* material) {
    if (!material) {
        UseMaterial = false;
        return;
    }

    UseMaterial = true;
    MaterialInfo.Texture = NULL;

    Image* image = material->TextureDiffuse;
    if (image && image->TexturePtr)
        MaterialInfo.Texture = (Texture*)image->TexturePtr;

    for (unsigned i = 0; i < 4; i++) {
        MaterialInfo.Specular[i] = material->ColorSpecular[i] * 0x100;
        MaterialInfo.Ambient[i] = material->ColorAmbient[i] * 0x100;
        MaterialInfo.Diffuse[i] = material->ColorDiffuse[i] * 0x100;
    }
}

PUBLIC void FaceInfo::SetMaterial(Texture* texture) {
    if (!texture) {
        UseMaterial = false;
        return;
    }

    UseMaterial = true;
    MaterialInfo.Texture = texture;

    for (unsigned i = 0; i < 4; i++) {
        MaterialInfo.Specular[i] = 0x100;
        MaterialInfo.Ambient[i] = 0x100;
        MaterialInfo.Diffuse[i] = 0x100;
    }
}

PUBLIC void FaceInfo::SetBlendState(BlendState blendState) {
    Blend = blendState;
}
