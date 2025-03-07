#ifndef SCENE3D_H
#define SCENE3D_H

#include <Engine/Math/Matrix4x4.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/VertexBuffer.h>

struct Scene3D {
	VertexBuffer* Buffer = nullptr;
	Uint32 DrawMode;
	Uint8 FaceCullMode;

	float FOV = 90.0f;
	float NearClippingPlane = 1.0f;
	float FarClippingPlane = 32768.0f;
	bool UseCustomProjectionMatrix;

	Matrix4x4 ProjectionMatrix;
	Matrix4x4 ViewMatrix;

	struct {
		struct {
			float R, G, B;
		} Ambient;
		struct {
			float R, G, B;
		} Diffuse;
		struct {
			float R, G, B;
		} Specular;
	} Lighting;

	struct {
		FogEquation Equation = FogEquation_Linear;
		float Start = 0.0f;
		float End = 1.0f;
		float Density = 1.0f;
		float Smoothness = 1.0f;
		struct {
			float R, G, B;
		} Color;
	} Fog;

	float PointSize = 1.0f;
	bool ClipPolygons = false;
	bool Initialized = false;
	int UnloadPolicy;

	void Clear() {
		if (Buffer) {
			Buffer->Clear();
		}
	}
	void SetAmbientLighting(Uint32 r, Uint32 g, Uint32 b) {
		Lighting.Ambient.R = r;
		Lighting.Ambient.G = g;
		Lighting.Ambient.B = b;
	}
	void SetDiffuseLighting(Uint32 r, Uint32 g, Uint32 b) {
		Lighting.Diffuse.R = r;
		Lighting.Diffuse.G = g;
		Lighting.Diffuse.B = b;
	}
	void SetSpecularLighting(Uint32 r, Uint32 g, Uint32 b) {
		Lighting.Specular.R = r;
		Lighting.Specular.G = g;
		Lighting.Specular.B = b;
	}
	void SetFogEquation(FogEquation mode) { Fog.Equation = mode; }
	void SetFogStart(float start) { Fog.Start = start; }
	void SetFogEnd(float end) { Fog.End = end; }
	void SetFogDensity(float density) { Fog.Density = density; }
	void SetFogColor(float r, float g, float b) {
		Fog.Color.R = r;
		Fog.Color.G = g;
		Fog.Color.B = b;
	}
	void SetFogSmoothness(float smoothness) { Fog.Smoothness = smoothness; }
	void SetClipPolygons(bool clipPolygons) { ClipPolygons = clipPolygons; }
	void SetProjectionMatrix(Matrix4x4* projMat) { ProjectionMatrix = *projMat; }
	void SetClippingPlanes() {
		FarClippingPlane =
			ProjectionMatrix.Values[14] / (ProjectionMatrix.Values[10] - 1.0f);
		NearClippingPlane =
			ProjectionMatrix.Values[14] / (ProjectionMatrix.Values[10] + 1.0f);
	}
	void SetCustomProjectionMatrix(Matrix4x4* projMat) {
		if (projMat) {
			SetProjectionMatrix(projMat);
			SetClippingPlanes();
			UseCustomProjectionMatrix = true;
		}
		else {
			UseCustomProjectionMatrix = false;
		}
	}
	void SetViewMatrix(Matrix4x4* viewMat) { ViewMatrix = *viewMat; }
};

#endif /* SCENE3D_H */
