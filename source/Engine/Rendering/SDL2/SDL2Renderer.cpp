#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/Includes/HashMap.h>

class SDL2Renderer {
public:
    static float RenderScale;
};
#endif

#include <Engine/Rendering/SDL2/SDL2Renderer.h>

#include <Engine/Application.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Rendering/Texture.h>

#ifdef IOS
extern "C" {
    #include <Engine/Rendering/SDL2/SDL2MetalFunc.h>
}
#endif

SDL_Renderer* Renderer = NULL;
float         SDL2Renderer::RenderScale = 1.0f;

// Initialization and disposal functions
PUBLIC STATIC void     SDL2Renderer::Init() {
    Log::Print(Log::LOG_INFO, "Renderer: SDL2");

    Uint32 flags = SDL_RENDERER_ACCELERATED;
    if (Graphics::VsyncEnabled)
        flags |= SDL_RENDERER_PRESENTVSYNC;
    Renderer = SDL_CreateRenderer(Application::Window, -1, flags);
    
    SDL_RendererInfo rendererInfo;
    SDL_GetRendererInfo(Renderer, &rendererInfo);

    Graphics::SupportsBatching = false;
    Graphics::PreferredPixelFormat = SDL_PIXELFORMAT_ARGB8888;
    
    Graphics::MaxTextureWidth = rendererInfo.max_texture_width;
    Graphics::MaxTextureHeight = rendererInfo.max_texture_height;

    int w, h, ww, wh;
    // SDL_GL_GetDrawableSize(Application::Window, &w, &h);
    SDL_GetRendererOutputSize(Renderer, &w, &h);
    SDL_GetWindowSize(Application::Window, &ww, &wh);

    RenderScale = 1.0f;
    if (h > wh)
        RenderScale = h / wh;
}
PUBLIC STATIC Uint32   SDL2Renderer::GetWindowFlags() {
    return 0;
}
PUBLIC STATIC void     SDL2Renderer::SetVSync(bool enabled) {
    Graphics::VsyncEnabled = enabled;
    SDL_RenderSetVSync(Renderer, enabled);
}
PUBLIC STATIC void     SDL2Renderer::SetGraphicsFunctions() {
    Graphics::PixelOffset = 0.0f;

    Graphics::Internal.Init = SDL2Renderer::Init;
    Graphics::Internal.GetWindowFlags = SDL2Renderer::GetWindowFlags;
    Graphics::Internal.SetVSync = SDL2Renderer::SetVSync;
    Graphics::Internal.Dispose = SDL2Renderer::Dispose;

    // Texture management functions
    Graphics::Internal.CreateTexture = SDL2Renderer::CreateTexture;
    Graphics::Internal.LockTexture = SDL2Renderer::LockTexture;
    Graphics::Internal.UpdateTexture = SDL2Renderer::UpdateTexture;
    Graphics::Internal.UpdateYUVTexture = SDL2Renderer::UpdateTextureYUV;
    Graphics::Internal.UnlockTexture = SDL2Renderer::UnlockTexture;
    Graphics::Internal.DisposeTexture = SDL2Renderer::DisposeTexture;

    // Viewport and view-related functions
    Graphics::Internal.SetRenderTarget = SDL2Renderer::SetRenderTarget;
    Graphics::Internal.UpdateWindowSize = SDL2Renderer::UpdateWindowSize;
    Graphics::Internal.UpdateViewport = SDL2Renderer::UpdateViewport;
    Graphics::Internal.UpdateClipRect = SDL2Renderer::UpdateClipRect;
    Graphics::Internal.UpdateOrtho = SDL2Renderer::UpdateOrtho;
    Graphics::Internal.UpdatePerspective = SDL2Renderer::UpdatePerspective;
    Graphics::Internal.UpdateProjectionMatrix = SDL2Renderer::UpdateProjectionMatrix;
    Graphics::Internal.MakePerspectiveMatrix = SDL2Renderer::MakePerspectiveMatrix;

    // Shader-related functions
    Graphics::Internal.UseShader = SDL2Renderer::UseShader;
    Graphics::Internal.SetUniformF = SDL2Renderer::SetUniformF;
    Graphics::Internal.SetUniformI = SDL2Renderer::SetUniformI;
    Graphics::Internal.SetUniformTexture = SDL2Renderer::SetUniformTexture;

    // These guys
    Graphics::Internal.Clear = SDL2Renderer::Clear;
    Graphics::Internal.Present = SDL2Renderer::Present;

    // Draw mode setting functions
    Graphics::Internal.SetBlendColor = SDL2Renderer::SetBlendColor;
    Graphics::Internal.SetBlendMode = SDL2Renderer::SetBlendMode;
    Graphics::Internal.SetTintColor = SDL2Renderer::SetTintColor;
    Graphics::Internal.SetTintMode = SDL2Renderer::SetTintMode;
    Graphics::Internal.SetTintEnabled = SDL2Renderer::SetTintEnabled;
    Graphics::Internal.SetLineWidth = SDL2Renderer::SetLineWidth;

    // Primitive drawing functions
    Graphics::Internal.StrokeLine = SDL2Renderer::StrokeLine;
    Graphics::Internal.StrokeCircle = SDL2Renderer::StrokeCircle;
    Graphics::Internal.StrokeEllipse = SDL2Renderer::StrokeEllipse;
    Graphics::Internal.StrokeRectangle = SDL2Renderer::StrokeRectangle;
    Graphics::Internal.FillCircle = SDL2Renderer::FillCircle;
    Graphics::Internal.FillEllipse = SDL2Renderer::FillEllipse;
    Graphics::Internal.FillTriangle = SDL2Renderer::FillTriangle;
    Graphics::Internal.FillRectangle = SDL2Renderer::FillRectangle;

    // Texture drawing functions
    Graphics::Internal.DrawTexture = SDL2Renderer::DrawTexture;
    Graphics::Internal.DrawSprite = SDL2Renderer::DrawSprite;
    Graphics::Internal.DrawSpritePart = SDL2Renderer::DrawSpritePart;

    // 3D drawing functions
    Graphics::Internal.DrawPolygon3D = SDL2Renderer::DrawPolygon3D;
    Graphics::Internal.DrawSceneLayer3D = SDL2Renderer::DrawSceneLayer3D;
    Graphics::Internal.DrawModel = SDL2Renderer::DrawModel;
    Graphics::Internal.DrawModelSkinned = SDL2Renderer::DrawModelSkinned;
    Graphics::Internal.DrawVertexBuffer = SDL2Renderer::DrawVertexBuffer;
    Graphics::Internal.BindVertexBuffer = SDL2Renderer::BindVertexBuffer;
    Graphics::Internal.UnbindVertexBuffer = SDL2Renderer::UnbindVertexBuffer;
    Graphics::Internal.BindArrayBuffer = SDL2Renderer::BindArrayBuffer;
    Graphics::Internal.DrawArrayBuffer = SDL2Renderer::DrawArrayBuffer;

    Graphics::Internal.MakeFrameBufferID = SDL2Renderer::MakeFrameBufferID;
}
PUBLIC STATIC void     SDL2Renderer::Dispose() {
    SDL_DestroyRenderer(Renderer);
}

// Texture management functions
PUBLIC STATIC Texture* SDL2Renderer::CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height) {
    Texture* texture = Texture::New(format, access, width, height);
    texture->DriverData = Memory::TrackedCalloc("Texture::DriverData", 1, sizeof(SDL_Texture*));

    SDL_Texture** textureData = (SDL_Texture**)texture->DriverData;

    *textureData = SDL_CreateTexture(Renderer, format, access, width, height);

    texture->ID = Graphics::TextureMap->Count;
    Graphics::TextureMap->Put(texture->ID, texture);

    return texture;
}
PUBLIC STATIC int      SDL2Renderer::LockTexture(Texture* texture, void** pixels, int* pitch) {
    return 0;
}
PUBLIC STATIC int      SDL2Renderer::UpdateTexture(Texture* texture, SDL_Rect* src, void* pixels, int pitch) {
    SDL_Texture* textureData = *(SDL_Texture**)texture->DriverData;
    return SDL_UpdateTexture(textureData, src, pixels, pitch);
}
PUBLIC STATIC int      SDL2Renderer::UpdateTextureYUV(Texture* texture, SDL_Rect* src, void* pixelsY, int pitchY, void* pixelsU, int pitchU, void* pixelsV, int pitchV) {
    // SDL_Texture* textureData = *(SDL_Texture**)texture->DriverData;
    return 0;
}
PUBLIC STATIC void     SDL2Renderer::UnlockTexture(Texture* texture) {

}
PUBLIC STATIC void     SDL2Renderer::DisposeTexture(Texture* texture) {
    SDL_Texture** textureData = (SDL_Texture**)texture->DriverData;
    if (!textureData)
        return;

    Graphics::TextureMap->Remove(texture->ID);

    SDL_DestroyTexture(*textureData);
    Memory::Free(texture->DriverData);
}

// Viewport and view-related functions
PUBLIC STATIC void     SDL2Renderer::SetRenderTarget(Texture* texture) {
    if (texture == NULL) {
        SDL_SetRenderTarget(Renderer, NULL);
    }
    else {
        SDL_SetRenderTarget(Renderer, *(SDL_Texture**)texture->DriverData);
    }
}
PUBLIC STATIC void     SDL2Renderer::UpdateWindowSize(int width, int height) {
    SDL2Renderer::UpdateViewport();
}
PUBLIC STATIC void     SDL2Renderer::UpdateViewport() {
    Viewport* vp = &Graphics::CurrentViewport;
    SDL_Rect r = {
        (int)(vp->X * RenderScale),
        (int)(vp->Y * RenderScale),
        (int)(vp->Width * RenderScale),
        (int)(vp->Height * RenderScale),
    };
    SDL_RenderSetViewport(Renderer, &r);

    SDL2Renderer::UpdateProjectionMatrix();
}
PUBLIC STATIC void     SDL2Renderer::UpdateClipRect() {
    ClipArea clip = Graphics::CurrentClip;
    if (Graphics::CurrentClip.Enabled) {
        SDL_Rect r = {
            (int)clip.X,
            (int)clip.Y,
            (int)clip.Width,
            (int)clip.Height,
        };
        SDL_RenderSetClipRect(Renderer, &r);
    }
    else {
        SDL_RenderSetClipRect(Renderer, NULL);
    }
}
PUBLIC STATIC void     SDL2Renderer::UpdateOrtho(float left, float top, float right, float bottom) {

}
PUBLIC STATIC void     SDL2Renderer::UpdatePerspective(float fovy, float aspect, float nearv, float farv) {

}
PUBLIC STATIC void     SDL2Renderer::UpdateProjectionMatrix() {

}
PUBLIC STATIC void     SDL2Renderer::MakePerspectiveMatrix(Matrix4x4* out, float fov, float near, float far, float aspect) {
    Matrix4x4::Perspective(out, fov, aspect, near, far);
}

PUBLIC STATIC void     SDL2Renderer::GetMetalSize(int* width, int* height) {
    // #ifdef IOS
    //     SDL2MetalFunc_GetMetalSize(width, height, Renderer);
    // #endif
    SDL_GetRendererOutputSize(Renderer, width, height);
}

// Shader-related functions
PUBLIC STATIC void     SDL2Renderer::UseShader(void* shader) {

}
PUBLIC STATIC void     SDL2Renderer::SetUniformF(int location, int count, float* values) {

}
PUBLIC STATIC void     SDL2Renderer::SetUniformI(int location, int count, int* values) {

}
PUBLIC STATIC void     SDL2Renderer::SetUniformTexture(Texture* texture, int uniform_index, int slot) {

}

// These guys
PUBLIC STATIC void     SDL2Renderer::Clear() {
    SDL_RenderClear(Renderer);

    int w, h, ww, wh;
    SDL_GetRendererOutputSize(Renderer, &w, &h);
    SDL_GetWindowSize(Application::Window, &ww, &wh);

    RenderScale = 1.0f;
    if (h > wh)
        RenderScale = h / wh;
}
PUBLIC STATIC void     SDL2Renderer::Present() {
	SDL_RenderPresent(Renderer);
}

// Draw mode setting functions
PUBLIC STATIC void     SDL2Renderer::SetBlendColor(float r, float g, float b, float a) {

}
PUBLIC STATIC void     SDL2Renderer::SetBlendMode(int srcC, int dstC, int srcA, int dstA) {

}
PUBLIC STATIC void     SDL2Renderer::SetTintColor(float r, float g, float b, float a) {

}
PUBLIC STATIC void     SDL2Renderer::SetTintMode(int mode) {

}
PUBLIC STATIC void     SDL2Renderer::SetTintEnabled(bool enabled) {

}
PUBLIC STATIC void     SDL2Renderer::SetLineWidth(float n) {

}

// Primitive drawing functions
PUBLIC STATIC void     SDL2Renderer::StrokeLine(float x1, float y1, float x2, float y2) {

}
PUBLIC STATIC void     SDL2Renderer::StrokeCircle(float x, float y, float rad) {

}
PUBLIC STATIC void     SDL2Renderer::StrokeEllipse(float x, float y, float w, float h) {

}
PUBLIC STATIC void     SDL2Renderer::StrokeRectangle(float x, float y, float w, float h) {

}
PUBLIC STATIC void     SDL2Renderer::FillCircle(float x, float y, float rad) {

}
PUBLIC STATIC void     SDL2Renderer::FillEllipse(float x, float y, float w, float h) {

}
PUBLIC STATIC void     SDL2Renderer::FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3) {

}
PUBLIC STATIC void     SDL2Renderer::FillRectangle(float x, float y, float w, float h) {

}
// Texture drawing functions
PUBLIC STATIC void     SDL2Renderer::DrawTexture(Texture* texture, float sx, float sy, float sw, float sh, float x, float y, float w, float h) {
    x *= RenderScale;
    y *= RenderScale;
    w *= RenderScale;
    h *= RenderScale;
    SDL_Rect src = {
        (int)sx,
        (int)sy,
        (int)sw,
        (int)sh,
    };
    SDL_Rect dst = {
        (int)x,
        (int)y,
        (int)w,
        (int)h,
    };
    SDL_Texture** textureData = (SDL_Texture**)texture->DriverData;
    int flip = 0;
    if (dst.w < 0) {
        dst.w = -dst.w;
        flip |= SDL_FLIP_HORIZONTAL;
    }
    if (dst.h < 0) {
        dst.h = -dst.h;
        flip |= SDL_FLIP_VERTICAL;
    }
    SDL_RenderCopyEx(Renderer, *textureData, &src, &dst, 0.0, NULL, (SDL_RendererFlip)flip);
}
PUBLIC STATIC void     SDL2Renderer::DrawSprite(ISprite* sprite, int animation, int frame, int x, int y, bool flipX, bool flipY, float scaleW, float scaleH, float rotation) {
    if (Graphics::SpriteRangeCheck(sprite, animation, frame)) return;

    AnimFrame animframe = sprite->Animations[animation].Frames[frame];
    float fX = flipX ? -1.0 : 1.0;
    float fY = flipY ? -1.0 : 1.0;
    float sw  = animframe.Width;
    float sh  = animframe.Height;

    SDL2Renderer::DrawTexture(sprite->Spritesheets[animframe.SheetNumber],
        animframe.X, animframe.Y, sw, sh,
        x + fX * animframe.OffsetX,
        y + fY * animframe.OffsetY, fX * sw, fY * sh);
}
PUBLIC STATIC void     SDL2Renderer::DrawSpritePart(ISprite* sprite, int animation, int frame, int sx, int sy, int sw, int sh, int x, int y, bool flipX, bool flipY, float scaleW, float scaleH, float rotation) {
    if (Graphics::SpriteRangeCheck(sprite, animation, frame)) return;

    AnimFrame animframe = sprite->Animations[animation].Frames[frame];
    if (sx == animframe.Width)
        return;
    if (sy == animframe.Height)
        return;

    float fX = flipX ? -1.0 : 1.0;
    float fY = flipY ? -1.0 : 1.0;
    if (sw >= animframe.Width - sx)
        sw  = animframe.Width - sx;
    if (sh >= animframe.Height - sy)
        sh  = animframe.Height - sy;

    SDL2Renderer::DrawTexture(sprite->Spritesheets[animframe.SheetNumber],
        animframe.X + sx, animframe.Y + sy,
        sw, sh,
        x + fX * (sx + animframe.OffsetX),
        y + fY * (sy + animframe.OffsetY), fX * sw, fY * sh);
}
// 3D drawing functions
PUBLIC STATIC void     SDL2Renderer::DrawPolygon3D(void* data, int vertexCount, int vertexFlag, Texture* texture, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {

}
PUBLIC STATIC void     SDL2Renderer::DrawSceneLayer3D(void* layer, int sx, int sy, int sw, int sh, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {

}
PUBLIC STATIC void     SDL2Renderer::DrawModel(void* model, Uint16 animation, Uint32 frame, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {

}
PUBLIC STATIC void     SDL2Renderer::DrawModelSkinned(void* model, Uint16 armature, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {

}
PUBLIC STATIC void     SDL2Renderer::DrawVertexBuffer(Uint32 vertexBufferIndex, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {

}
PUBLIC STATIC void     SDL2Renderer::BindVertexBuffer(Uint32 vertexBufferIndex) {

}
PUBLIC STATIC void     SDL2Renderer::UnbindVertexBuffer() {

}
PUBLIC STATIC void     SDL2Renderer::BindArrayBuffer(Uint32 arrayBufferIndex) {

}
PUBLIC STATIC void     SDL2Renderer::DrawArrayBuffer(Uint32 arrayBufferIndex, Uint32 drawMode) {

}

PUBLIC STATIC void     SDL2Renderer::MakeFrameBufferID(ISprite* sprite, AnimFrame* frame) {
    frame->ID = 0;
}
