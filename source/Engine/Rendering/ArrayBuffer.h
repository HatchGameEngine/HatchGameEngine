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
    Uint8            FogColorR;
    Uint8            FogColorG;
    Uint8            FogColorB;
    float            FogDensity;
    float            NearClippingPlane;
    float            FarClippingPlane;
    Matrix4x4        ProjectionMatrix;
    Matrix4x4        ViewMatrix;
    bool             ClipPolygons;
    bool             Initialized;
};

#endif /* ARRAYBUFFER_H */
