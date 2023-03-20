#ifndef ARRAYBUFFER_H
#define ARRAYBUFFER_H

#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/VertexBuffer.h>
#include <Engine/Math/Matrix4x4.h>

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

    void Init(Uint32 numVertices) {
        Buffer.Init(numVertices);
        Initialized = true;
        ClipPolygons = true;
    }
    void Clear() {
        Buffer.Clear();
    }
    void SetAmbientLighting(Uint32 r, Uint32 g, Uint32 b) {
        LightingAmbientR = r;
        LightingAmbientG = g;
        LightingAmbientB = b;
    }
    void SetDiffuseLighting(Uint32 r, Uint32 g, Uint32 b) {
        LightingDiffuseR = r;
        LightingDiffuseG = g;
        LightingDiffuseB = b;
    }
    void SetSpecularLighting(Uint32 r, Uint32 g, Uint32 b) {
        LightingSpecularR = r;
        LightingSpecularG = g;
        LightingSpecularB = b;
    }
    void SetFogDensity(float density) {
        FogDensity = density;
    }
    void SetFogColor(Uint32 r, Uint32 g, Uint32 b) {
        FogColorR = r;
        FogColorG = g;
        FogColorB = b;
    }
    void SetClipPolygons(bool clipPolygons) {
        ClipPolygons = clipPolygons;
    }
    void SetProjectionMatrix(Matrix4x4* projMat) {
        ProjectionMatrix  = *projMat;
        FarClippingPlane  = projMat->Values[14] / (projMat->Values[10] - 1.0f);
        NearClippingPlane = projMat->Values[14] / (projMat->Values[10] + 1.0f);
    }
    void SetViewMatrix(Matrix4x4* viewMat) {
        ViewMatrix = *viewMat;
    }
};

#endif /* ARRAYBUFFER_H */
