#ifndef ENGINE_RENDERING_POLYGONRENDERER_H
#define ENGINE_RENDERING_POLYGONRENDERER_H

#include <Engine/Includes/Standard.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/Enums.h>
#include <Engine/Rendering/Scene3D.h>
#include <Engine/Rendering/VertexBuffer.h>
#include <Engine/ResourceTypes/IModel.h>
#include <Engine/Scene/SceneLayer.h>

class PolygonRenderer {
public:
	Scene3D* ScenePtr = nullptr;
	VertexBuffer* VertexBuf = nullptr;
	Matrix4x4* ModelMatrix = nullptr;
	Matrix4x4* NormalMatrix = nullptr;
	Matrix4x4* ViewMatrix = nullptr;
	Matrix4x4* ProjectionMatrix = nullptr;
	Uint32 DrawMode = 0;
	Uint8 FaceCullMode = 0;
	Uint32 CurrentColor = 0;
	bool DoProjection = false;
	bool DoClipping = false;
	bool ClipPolygonsByFrustum = false;
	int NumFrustumPlanes = 0;
	Frustum ViewFrustum[NUM_FRUSTUM_PLANES];

	static int FaceSortFunction(const void* a, const void* b);
	void BuildFrustumPlanes(float nearClippingPlane, float farClippingPlane);
	bool SetBuffers();
	void
	DrawPolygon3D(VertexAttribute* data, int vertexCount, int vertexFlag, Texture* texture);
	void DrawSceneLayer3D(SceneLayer* layer, int sx, int sy, int sw, int sh);
	void DrawModel(IModel* model, Uint16 animation, Uint32 frame);
	void DrawModelSkinned(IModel* model, Uint16 armature);
	void DrawVertexBuffer();
	int ClipPolygon(PolygonClipBuffer& clipper, VertexAttribute* input, int numVertices);
	static bool CheckPolygonVisible(VertexAttribute* vertex, int vertexCount);
	static void CopyVertices(VertexAttribute* buffer, VertexAttribute* output, int numVertices);
};

#endif /* ENGINE_RENDERING_POLYGONRENDERER_H */
