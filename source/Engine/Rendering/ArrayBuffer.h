#ifndef ARRAYBUFFER_H
#define ARRAYBUFFER_H

#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/VertexBuffer.h>

struct ArrayBuffer {
    VertexBuffer     Buffer;
    Uint32           LightingAmbientR;
    Uint32           LightingAmbientG;
    Uint32           LightingAmbientB;
    Uint32           LightingDiffuseR;
    Uint32           LightingDiffuseG;
    Uint32           LightingDiffuseB;
    Uint32           LightingSpecularR;
    Uint32           LightingSpecularG;
    Uint32           LightingSpecularB;
    Uint32           FogColorR;
    Uint32           FogColorG;
    Uint32           FogColorB;
    float            FogDensity;
    float            NearClippingPlane;
    float            FarClippingPlane;
    Matrix4x4        ProjectionMatrix;
    Matrix4x4        ViewMatrix;
    bool             Initialized;
};

#endif /* ARRAYBUFFER_H */
