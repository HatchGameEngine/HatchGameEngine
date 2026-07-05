#ifndef ENGINE_RENDERING_FACEINFO_H
#define ENGINE_RENDERING_FACEINFO_H

#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/Material.h>
#include <Engine/Rendering/Texture.h>

class FaceInfo {
public:
	Uint32 DrawMode;
	Uint32 NumVertices;
	Uint32 VerticesStartIndex;
	bool UseMaterial;
	FaceMaterial MaterialInfo;
	BlendState Blend;
	Uint8 CullMode;
	int Depth;

	void SetMaterial(Material* material);
	void SetMaterial(Texture* texture);
	void SetBlendState(BlendState blendState);
};

#endif /* ENGINE_RENDERING_FACEINFO_H */
