#ifndef ENGINE_RENDERING_MODELRENDERER_H
#define ENGINE_RENDERING_MODELRENDERER_H
class Matrix4x4;

#include <Engine/Includes/Standard.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/FaceInfo.h>
#include <Engine/Rendering/PolygonRenderer.h>
#include <Engine/Rendering/VertexBuffer.h>
#include <Engine/ResourceTypes/IModel.h>

class ModelRenderer {
private:
	void Init();
	void AddFace(int faceVertexCount, Material* material);
	int ClipFace(int faceVertexCount);
	void DrawMesh(IModel* model, Mesh* mesh, Skeleton* skeleton, Matrix4x4& mvpMatrix);
	void
	DrawMesh(IModel* model, Mesh* mesh, Uint16 animation, Uint32 frame, Matrix4x4& mvpMatrix);
	void DrawMesh(IModel* model,
		Mesh* mesh,
		Vector3* positionBuffer,
		Vector3* normalBuffer,
		Vector2* uvBuffer,
		Matrix4x4& mvpMatrix);
	void DrawNode(IModel* model, ModelNode* node, Matrix4x4* world);
	void DrawModelInternal(IModel* model, Uint16 animation, Uint32 frame);

public:
	PolygonRenderer* PolyRenderer;
	VertexBuffer* Buffer;
	VertexAttribute* AttribBuffer;
	VertexAttribute* Vertex;
	FaceInfo* FaceItem;
	Matrix4x4* ModelMatrix;
	Matrix4x4* ViewMatrix;
	Matrix4x4* ProjectionMatrix;
	Matrix4x4* NormalMatrix;
	Matrix4x4 MVPMatrix;
	bool DoProjection;
	bool ClipFaces;
	Armature* ArmaturePtr;
	Uint32 DrawMode;
	Uint8 FaceCullMode;
	Uint32 CurrentColor;

	ModelRenderer(PolygonRenderer* polyRenderer);
	ModelRenderer(VertexBuffer* buffer);
	void
	SetMatrices(Matrix4x4* model, Matrix4x4* view, Matrix4x4* projection, Matrix4x4* normal);
	void DrawModel(IModel* model, Uint16 animation, Uint32 frame);
};

#endif /* ENGINE_RENDERING_MODELRENDERER_H */
