#include <Engine/Rendering/FaceInfo.h>
#include <Engine/ResourceTypes/Image.h>

void FaceInfo::SetMaterial(Material* material) {
	if (!material) {
		UseMaterial = false;
		return;
	}

	UseMaterial = true;
	MaterialInfo.Texture = NULL;

	Image* image = (Image*)material->TextureDiffuse;
	if (image && image->IsLoaded()) {
		MaterialInfo.Texture = (Texture*)image->TexturePtr;
	}

	for (unsigned i = 0; i < 4; i++) {
		MaterialInfo.Specular[i] = material->ColorSpecular[i] * 0x100;
		MaterialInfo.Ambient[i] = material->ColorAmbient[i] * 0x100;
		MaterialInfo.Diffuse[i] = material->ColorDiffuse[i] * 0x100;
	}
}

void FaceInfo::SetMaterial(Texture* texture) {
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

void FaceInfo::SetBlendState(BlendState blendState) {
	Blend = blendState;
}
