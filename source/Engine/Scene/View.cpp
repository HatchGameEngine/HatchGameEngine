#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Rendering/Texture.h>

class View {
public:
    bool       Active = false;
    bool       Visible = true;
    bool       Software = false;
    int        Priority = 0;
    float      X = 0.0f;
    float      Y = 0.0f;
    float      Z = 0.0f;
    float      RotateX = 0.0f;
    float      RotateY = 0.0f;
    float      RotateZ = 0.0f;
    float      Width = 1.0f;
    float      Height = 1.0f;
    float      OutputX = 0.0f;
    float      OutputY = 0.0f;
    float      OutputWidth = 1.0f;
    float      OutputHeight = 1.0f;
    int        Stride = 1;
    float      FOV = 45.0f;
    float      NearPlane = 0.1f;
    float      FarPlane = 1000.f;
    bool       UsePerspective = false;
    bool       UseDrawTarget = false;
    bool       UseStencil = false;
    Texture*   DrawTarget = NULL;
    Uint8*     StencilBuffer = NULL;
    size_t     StencilBufferSize = 0;
    Matrix4x4* ProjectionMatrix = NULL;
    Matrix4x4* BaseProjectionMatrix = NULL;
};
#endif

#include <Engine/Scene/View.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Math/Math.h>

PUBLIC void     View::SetSize(float w, float h) {
    Width = w;
    Height = h;
    Stride = Math::CeilPOT(w);

    if (UseStencil)
        ReallocStencil();
}

PUBLIC void     View::SetStencilEnabled(bool enabled) {
    UseStencil = enabled;
    if (!UseStencil)
        return;

    ReallocStencil();
}
PUBLIC void     View::ReallocStencil() {
    if (DrawTarget == nullptr)
        return;

    size_t bufSize = DrawTarget->Width * DrawTarget->Height;
    if (StencilBuffer == NULL || bufSize > StencilBufferSize) {
        StencilBufferSize = bufSize;
        StencilBuffer = (Uint8*)Memory::Realloc(StencilBuffer, StencilBufferSize * sizeof(*StencilBuffer));
        ClearStencil();
    }
}
PUBLIC void     View::ClearStencil() {
    if (StencilBuffer)
        memset(StencilBuffer, 0x00, StencilBufferSize * sizeof(*StencilBuffer));
}
PUBLIC void     View::DeleteStencil() {
    Memory::Free(StencilBuffer);
    StencilBuffer = NULL;
    StencilBufferSize = 0;
}
