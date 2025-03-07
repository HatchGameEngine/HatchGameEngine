#ifndef ENGINE_SCENE_VIEW_H
#define ENGINE_SCENE_VIEW_H

#include <Engine/Includes/Standard.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Rendering/Texture.h>

class View {
public:
	bool Active = false;
	bool Visible = true;
	bool Software = false;
	int Priority = 0;
	float X = 0.0f;
	float Y = 0.0f;
	float Z = 0.0f;
	float RotateX = 0.0f;
	float RotateY = 0.0f;
	float RotateZ = 0.0f;
	float Width = 1.0f;
	float Height = 1.0f;
	float OutputX = 0.0f;
	float OutputY = 0.0f;
	float OutputWidth = 1.0f;
	float OutputHeight = 1.0f;
	int Stride = 1;
	float FOV = 45.0f;
	float NearPlane = 0.1f;
	float FarPlane = 1000.f;
	bool UsePerspective = false;
	bool UseDrawTarget = false;
	bool UseStencil = false;
	Texture* DrawTarget = NULL;
	Uint8* StencilBuffer = NULL;
	size_t StencilBufferSize = 0;
	Matrix4x4* ProjectionMatrix = NULL;
	Matrix4x4* BaseProjectionMatrix = NULL;

	void SetSize(float w, float h);
	void SetStencilEnabled(bool enabled);
	void ReallocStencil();
	void ClearStencil();
	void DeleteStencil();
};

#endif /* ENGINE_SCENE_VIEW_H */
