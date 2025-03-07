#ifndef I3D_H
#define I3D_H

#include <Engine/Math/VectorTypes.h>
#include <Engine/Rendering/Enums.h>

enum VertexType {
	VertexType_Position = 0,
	VertexType_Normal = 1,
	VertexType_UV = 2,
	VertexType_Color = 4,
};

enum FogEquation { FogEquation_Linear, FogEquation_Exp };

enum { FaceCull_None, FaceCull_Back, FaceCull_Front };

#define MAX_3D_SCENES 0x20
#define MAX_VERTEX_BUFFERS 2048
#define MAX_POLYGON_VERTICES 16
#define NUM_FRUSTUM_PLANES 6

struct VertexAttribute {
	Vector4 Position;
	Vector4 Normal;
	Vector2 UV;
	Uint32 Color;
};
struct FaceMaterial {
	Uint32 Specular[4];
	Uint32 Ambient[4];
	Uint32 Diffuse[4];
	void* Texture;
};
struct Frustum {
	Vector4 Plane;
	Vector4 Normal;
};
struct PolygonClipBuffer {
	VertexAttribute Buffer[MAX_POLYGON_VERTICES];
	int NumPoints;
	int MaxPoints;
};

#define APPLY_MAT4X4(vec4out, vec3in, M) \
	{ \
		float vecX = vec3in.X; \
		float vecY = vec3in.Y; \
		float vecZ = vec3in.Z; \
		vec4out.X = FP16_TO(M[3]) + ((int)(vecX * M[0])) + ((int)(vecY * M[1])) + \
			((int)(vecZ * M[2])); \
		vec4out.Y = FP16_TO(M[7]) + ((int)(vecX * M[4])) + ((int)(vecY * M[5])) + \
			((int)(vecZ * M[6])); \
		vec4out.Z = FP16_TO(M[11]) + ((int)(vecX * M[8])) + ((int)(vecY * M[9])) + \
			((int)(vecZ * M[10])); \
		vec4out.W = FP16_TO(M[15]) + ((int)(vecX * M[12])) + ((int)(vecY * M[13])) + \
			((int)(vecZ * M[14])); \
	}
#define COPY_VECTOR(vecout, vecin) vecout = vecin
#define COPY_NORMAL(vec4out, vec3in) \
	vec4out.X = vec3in.X; \
	vec4out.Y = vec3in.Y; \
	vec4out.Z = vec3in.Z; \
	vec4out.W = 0

#endif /* I3D_H */
