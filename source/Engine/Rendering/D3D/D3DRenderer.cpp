#ifdef USING_DIRECT3D

#include <Engine/Rendering/D3D/D3DRenderer.h>

#include <Engine/Application.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Rendering/Texture.h>

#include <SDL2/SDL_syswm.h>

#define NUM_SHADERS 3
#define D3D_DEBUG_INFO

#include <d3d9.h>
#pragma comment (lib, "d3d9.lib")

struct Vertex {
    float x, y, z;
    DWORD color;
    float u, v;
};
struct D3D_RenderData {
    void*                  D3D_DLL;
    IDirect3D9*            D3D;
    IDirect3DDevice9*      Device;
    UINT                   Adapter;
    D3DPRESENT_PARAMETERS  PresentParams;
    bool                   UpdateBackBufferSize;
    bool                   BeginScene;
    bool                   EnableSeparateAlphaBlend;
    IDirect3DSurface9*     DefaultRenderTarget;
    IDirect3DSurface9*     CurrentRenderTarget;
    void*                  D3Dx_DLL;
    LPDIRECT3DPIXELSHADER9 Shaders[NUM_SHADERS];
};
struct D3D_TextureData {
    bool   Dirty;
    DWORD  Usage;
    Uint32 Format;
    D3DFORMAT D3DFormat;
    IDirect3DTexture9* Texture;
    IDirect3DTexture9* Staging;
    D3DTEXTUREFILTERTYPE ScaleMode;
};

int                D3D_DefaultFramebuffer;
Vertex*            D3D_BufferCircleFill;
Vertex*            D3D_BufferCircleStroke;
Vertex*            D3D_BufferSquareFill;

Uint32             D3D_BlendColorsAsHex = 0;

bool               D3D_PixelPerfectScale = false;
float              D3D_RenderScaleX = 1.0f;
float              D3D_RenderScaleY = 1.0f;
Matrix4x4*         D3D_MatrixIdentity = NULL;

DWORD              D3D_Blend_SRC_COLOR = D3DBLEND_SRCALPHA;
DWORD              D3D_Blend_DST_COLOR = D3DBLEND_INVSRCALPHA;
DWORD              D3D_Blend_SRC_ALPHA = D3DBLEND_SRCALPHA;
DWORD              D3D_Blend_DST_ALPHA = D3DBLEND_INVSRCALPHA;

D3D_RenderData*    renderData = NULL;

bool        D3D_LoadDLL(void** pD3DDLL, IDirect3D9** pDirect3D9Interface) {
    *pD3DDLL = SDL_LoadObject("D3D9.DLL");
    if (*pD3DDLL) {
        typedef IDirect3D9 *(WINAPI *Direct3DCreate9_t) (UINT SDKVersion);
        Direct3DCreate9_t Direct3DCreate9Func;

        #ifdef USE_D3D9EX
            typedef HRESULT (WINAPI *Direct3DCreate9Ex_t)(UINT SDKVersion, IDirect3D9Ex **ppD3D);
            Direct3DCreate9Ex_t Direct3DCreate9ExFunc;

            Direct3DCreate9ExFunc = (Direct3DCreate9Ex_t)SDL_LoadFunction(*pD3DDLL, "Direct3DCreate9Ex");
            if (Direct3DCreate9ExFunc) {
                IDirect3D9Ex *pDirect3D9ExInterface;
                HRESULT hr = Direct3DCreate9ExFunc(D3D_SDK_VERSION, &pDirect3D9ExInterface);
                if (SUCCEEDED(hr)) {
                    const GUID IDirect3D9_GUID = { 0x81bdcbca, 0x64d4, 0x426d, { 0xae, 0x8d, 0xad, 0x1, 0x47, 0xf4, 0x27, 0x5c } };
                    hr = IDirect3D9Ex_QueryInterface(pDirect3D9ExInterface, &IDirect3D9_GUID, (void**)pDirect3D9Interface);
                    IDirect3D9Ex_Release(pDirect3D9ExInterface);
                    if (SUCCEEDED(hr)) {
                        return true;
                    }
                }
            }
        #endif /* USE_D3D9EX */

        Direct3DCreate9Func = (Direct3DCreate9_t)SDL_LoadFunction(*pD3DDLL, "Direct3DCreate9");
        if (Direct3DCreate9Func) {
            *pDirect3D9Interface = Direct3DCreate9Func(D3D_SDK_VERSION);
            if (*pDirect3D9Interface) {
                return true;
            }
        }

        SDL_UnloadObject(*pD3DDLL);
        *pD3DDLL = NULL;
    }
    *pDirect3D9Interface = NULL;
    return false;
}
void        D3D_InitRenderState() {
    D3DMATRIX matrix;

    IDirect3DDevice9* device = renderData->Device;

    IDirect3DDevice9_SetVertexShader(device, NULL);
    IDirect3DDevice9_SetFVF(device, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    // IDirect3DDevice9_SetRenderState(device, D3DRS_ZENABLE, D3DZB_FALSE);
    IDirect3DDevice9_SetRenderState(device, D3DRS_ZENABLE, D3DZB_TRUE);
    IDirect3DDevice9_SetRenderState(device, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(device, D3DRS_LIGHTING, FALSE);

    /* Enable color modulation by diffuse color */
    IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

    /* Enable alpha modulation by diffuse alpha */
    IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

    /* Enable separate alpha blend function, if possible */
    if (renderData->EnableSeparateAlphaBlend) {
        IDirect3DDevice9_SetRenderState(device, D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    }

    /* Disable second texture stage, since we're done */
    IDirect3DDevice9_SetTextureStageState(device, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetTextureStageState(device, 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    /* Set an identity world and view matrix */
    matrix.m[0][0] = 1.0f;
    matrix.m[0][1] = 0.0f;
    matrix.m[0][2] = 0.0f;
    matrix.m[0][3] = 0.0f;
    matrix.m[1][0] = 0.0f;
    matrix.m[1][1] = 1.0f;
    matrix.m[1][2] = 0.0f;
    matrix.m[1][3] = 0.0f;
    matrix.m[2][0] = 0.0f;
    matrix.m[2][1] = 0.0f;
    matrix.m[2][2] = 1.0f;
    matrix.m[2][3] = 0.0f;
    matrix.m[3][0] = 0.0f;
    matrix.m[3][1] = 0.0f;
    matrix.m[3][2] = 0.0f;
    matrix.m[3][3] = 1.0f;
    IDirect3DDevice9_SetTransform(device, D3DTS_WORLD, &matrix);
    IDirect3DDevice9_SetTransform(device, D3DTS_VIEW, &matrix);

    /* Reset our current scale mode */
    // SDL_memset(renderData->ScaleMode, 0xFF, sizeof(renderData->ScaleMode));

    /* Start the render with BeginScene */
    renderData->BeginScene = true;
}
void        D3D_MakeShaders() {
    // D3D_ShaderShape             = new D3DShader(vertexShaderSource, sizeof(vertexShaderSource), fragmentShaderSource_Shape, sizeof(fragmentShaderSource_Shape));
    // D3D_ShaderTexturedShape     = new D3DShader(vertexShaderSource, sizeof(vertexShaderSource), fragmentShaderSource_TexturedShape, sizeof(fragmentShaderSource_TexturedShape));
    // ShaderTexturedShapeBGRA = new D3DShader(vertexShaderSource, sizeof(vertexShaderSource), fragmentShaderSource_TexturedShapeBGRA, sizeof(fragmentShaderSource_TexturedShapeBGRA));
}
void        D3D_MakeShapeBuffers() {
    Vertex* buffer;

    D3D_BufferCircleFill = (Vertex*)Memory::TrackedMalloc("D3DRenderer::D3D_BufferCircleFill", 362 * sizeof(Vertex));
    buffer = D3D_BufferCircleFill;
    buffer[0] = Vertex { 0.0f, 0.0f, 0.0f, 0xFFFFFFFF, 0.0f, 0.0f };
    for (int i = 0; i < 361; i++) {
        buffer[i + 1] = Vertex { cos(i * M_PI / 180.0f), sin(i * M_PI / 180.0f), 0.0f, 0xFFFFFFFF, 0.0f, 0.0f };
    }

    D3D_BufferCircleStroke = (Vertex*)Memory::TrackedMalloc("D3DRenderer::D3D_BufferCircleStroke", 361 * sizeof(Vertex));
    buffer = (Vertex*)D3D_BufferCircleStroke;
    for (int i = 0; i < 361; i++) {
        buffer[i + 0] = Vertex { cos(i * M_PI / 180.0f), sin(i * M_PI / 180.0f), 0.0f, 0xFFFFFFFF, 0.0f, 0.0f };
    }

    D3D_BufferSquareFill = (Vertex*)Memory::TrackedMalloc("D3DRenderer::D3D_BufferSquareFill", 4 * sizeof(Vertex));
    buffer = (Vertex*)D3D_BufferSquareFill;
    buffer[0] = Vertex { 0.0f, 0.0f, 0.0f, 0xFFFFFFFF, 0.0f, 0.0f };
    buffer[1] = Vertex { 1.0f, 0.0f, 0.0f, 0xFFFFFFFF, 0.0f, 0.0f };
    buffer[2] = Vertex { 1.0f, 1.0f, 0.0f, 0xFFFFFFFF, 0.0f, 0.0f };
    buffer[3] = Vertex { 0.0f, 1.0f, 0.0f, 0xFFFFFFFF, 0.0f, 0.0f };
}
const char* D3D_GetResultString(HRESULT result) {
    switch (result) {
    case D3DERR_WRONGTEXTUREFORMAT:
        return "WRONGTEXTUREFORMAT";
    case D3DERR_UNSUPPORTEDCOLOROPERATION:
        return "UNSUPPORTEDCOLOROPERATION";
    case D3DERR_UNSUPPORTEDCOLORARG:
        return "UNSUPPORTEDCOLORARG";
    case D3DERR_UNSUPPORTEDALPHAOPERATION:
        return "UNSUPPORTEDALPHAOPERATION";
    case D3DERR_UNSUPPORTEDALPHAARG:
        return "UNSUPPORTEDALPHAARG";
    case D3DERR_TOOMANYOPERATIONS:
        return "TOOMANYOPERATIONS";
    case D3DERR_CONFLICTINGTEXTUREFILTER:
        return "CONFLICTINGTEXTUREFILTER";
    case D3DERR_UNSUPPORTEDFACTORVALUE:
        return "UNSUPPORTEDFACTORVALUE";
    case D3DERR_CONFLICTINGRENDERSTATE:
        return "CONFLICTINGRENDERSTATE";
    case D3DERR_UNSUPPORTEDTEXTUREFILTER:
        return "UNSUPPORTEDTEXTUREFILTER";
    case D3DERR_CONFLICTINGTEXTUREPALETTE:
        return "CONFLICTINGTEXTUREPALETTE";
    case D3DERR_DRIVERINTERNALERROR:
        return "DRIVERINTERNALERROR";
    case D3DERR_NOTFOUND:
        return "NOTFOUND";
    case D3DERR_MOREDATA:
        return "MOREDATA";
    case D3DERR_DEVICELOST:
        return "DEVICELOST";
    case D3DERR_DEVICENOTRESET:
        return "DEVICENOTRESET";
    case D3DERR_NOTAVAILABLE:
        return "NOTAVAILABLE";
    case D3DERR_OUTOFVIDEOMEMORY:
        return "OUTOFVIDEOMEMORY";
    case D3DERR_INVALIDDEVICE:
        return "INVALIDDEVICE";
    case D3DERR_INVALIDCALL:
        return "INVALIDCALL";
    case D3DERR_DRIVERINVALIDCALL:
        return "DRIVERINVALIDCALL";
    case D3DERR_WASSTILLDRAWING:
        return "WASSTILLDRAWING";
    default:
        return "UNKNOWN";
    }
    return NULL;
}
D3DFORMAT   D3D_PixelFormatToD3DFORMAT(Uint32 format) {
    switch (format) {
    case SDL_PIXELFORMAT_RGB565:
        return D3DFMT_R5G6B5;
    case SDL_PIXELFORMAT_RGB888:
        return D3DFMT_X8R8G8B8;
    case SDL_PIXELFORMAT_ARGB8888:
        return D3DFMT_A8R8G8B8;
    case SDL_PIXELFORMAT_YV12:
    case SDL_PIXELFORMAT_IYUV:
    case SDL_PIXELFORMAT_NV12:
    case SDL_PIXELFORMAT_NV21:
        return D3DFMT_L8;
    default:
        return D3DFMT_UNKNOWN;
    }
}
Uint32      D3D_D3DFORMATToPixelFormat(D3DFORMAT format) {
    switch (format) {
        case D3DFMT_R5G6B5:
            return SDL_PIXELFORMAT_RGB565;
        case D3DFMT_X8R8G8B8:
            return SDL_PIXELFORMAT_RGB888;
        case D3DFMT_A8R8G8B8:
            return SDL_PIXELFORMAT_ARGB8888;
        default:
            return SDL_PIXELFORMAT_UNKNOWN;
    }
    return SDL_PIXELFORMAT_UNKNOWN;
}
int         D3D_SetError(const char* error, HRESULT result) {
    Log::Print(Log::LOG_ERROR, "D3D: %s (%s)", error, D3D_GetResultString(result));
    exit(-1);
    return 0;
}

void        D3D_CreateTexture(Texture* texture) {
    texture->DriverData = Memory::TrackedCalloc("Texture::DriverData", 1, sizeof(D3D_TextureData));

    D3D_TextureData* textureData = (D3D_TextureData*)texture->DriverData;

    textureData->ScaleMode = Graphics::TextureInterpolate ? D3DTEXF_LINEAR : D3DTEXF_POINT;

    DWORD usage = 0;
    if (texture->Access == SDL_TEXTUREACCESS_TARGET)
        usage = D3DUSAGE_RENDERTARGET;

    // D3D_CreateTextureRep
    HRESULT result;
    textureData->Dirty = false;
    textureData->Usage = usage;
    textureData->Format = texture->Format;
    textureData->D3DFormat = D3D_PixelFormatToD3DFORMAT(texture->Format);

    result = IDirect3DDevice9_CreateTexture(renderData->Device, texture->Width, texture->Height, 1, usage, textureData->D3DFormat, D3DPOOL_DEFAULT, &textureData->Texture, NULL);
    if (FAILED(result)) {
        Log::Print(Log::LOG_ERROR, "CreateTextureD3DPOOL_DEFAULT() %s", D3D_GetResultString(result));
        Log::Print(Log::LOG_INFO, "IDirect3DDevice9_CreateTexture(%p, %u, %u, %u, %u", renderData->Device, texture->Width, texture->Height, 1, usage);
        exit(-1);
    }
}
int         D3D_RecreateTexture(Texture* texture) {
    D3D_TextureData* textureData = (D3D_TextureData*)texture->DriverData;
    if (!textureData)
        return 0;

    if (textureData->Texture) {
        IDirect3DTexture9_Release(textureData->Texture);
        textureData->Texture = NULL;
    }
    if (textureData->Staging) {
        IDirect3DTexture9_AddDirtyRect(textureData->Staging, NULL);
        textureData->Dirty = true;
    }
    return 0;
}

int         D3D_SetRenderTarget(Texture* texture) {
    HRESULT result;
    /* Release the previous render target if it wasn't the default one */
    if (renderData->CurrentRenderTarget != NULL) {
        IDirect3DSurface9_Release(renderData->CurrentRenderTarget);
        renderData->CurrentRenderTarget = NULL;
    }

    if (texture == NULL) {
        IDirect3DDevice9_SetRenderTarget(renderData->Device, 0, renderData->DefaultRenderTarget);
        return 0;
    }

    D3D_TextureData* textureData = (D3D_TextureData*)texture->DriverData;
    if (!textureData) {
        SDL_SetError("Texture is not currently available");
        return -1;
    }

    /* Make sure the render target is updated if it was locked and written to */
    if (textureData->Dirty && textureData->Staging) {
        if (!textureData->Texture) {
            result = IDirect3DDevice9_CreateTexture(renderData->Device, texture->Width, texture->Height, 1, textureData->Usage, D3D_PixelFormatToD3DFORMAT(textureData->Format), D3DPOOL_DEFAULT, &textureData->Texture, NULL);
            if (FAILED(result)) {
                return D3D_SetError("CreateTexture(D3DPOOL_DEFAULT)", result);
            }
        }

        result = IDirect3DDevice9_UpdateTexture(renderData->Device, (IDirect3DBaseTexture9*)textureData->Staging, (IDirect3DBaseTexture9*)textureData->Texture);
        if (FAILED(result)) {
            return D3D_SetError("UpdateTexture()", result);
        }
        textureData->Dirty = false;
    }

    result = IDirect3DTexture9_GetSurfaceLevel(textureData->Texture, 0, &renderData->CurrentRenderTarget);
    if (FAILED(result)) {
        return D3D_SetError("GetSurfaceLevel()", result);
    }

    result = IDirect3DDevice9_SetRenderTarget(renderData->Device, 0, renderData->CurrentRenderTarget);
    if (FAILED(result)) {
        return D3D_SetError("SetRenderTarget()", result);
    }

    return 0;
}
void        D3D_Reset() {
    HRESULT result;

    /* Release the default render target before reset */
    if (renderData->DefaultRenderTarget) {
        IDirect3DSurface9_Release(renderData->DefaultRenderTarget);
        renderData->DefaultRenderTarget = NULL;
    }
    if (renderData->CurrentRenderTarget) {
        IDirect3DSurface9_Release(renderData->CurrentRenderTarget);
        renderData->CurrentRenderTarget = NULL;
    }

    /* Release application render targets */
    for (Texture* texture = Graphics::TextureHead; texture != NULL; texture = texture->Next) {
        if (texture->Access == SDL_TEXTUREACCESS_TARGET)
            D3DRenderer::DisposeTexture(texture);
        else
            D3D_RecreateTexture(texture);
    }

    result = IDirect3DDevice9_Reset(renderData->Device, &renderData->PresentParams);
    if (FAILED(result)) {
        if (result == D3DERR_DEVICELOST) {
            /* Don't worry about it, we'll reset later... */
            return;
        }
        else {
            D3D_SetError("Reset() %s", result);
            return;
        }
    }

    /* Allocate application render targets */
    for (Texture* texture = Graphics::TextureHead; texture != NULL; texture = texture->Next) {
        if (texture->Access == SDL_TEXTUREACCESS_TARGET)
            D3D_CreateTexture(texture);
    }

    IDirect3DDevice9_GetRenderTarget(renderData->Device, 0, &renderData->DefaultRenderTarget);
    D3D_InitRenderState();
    D3D_SetRenderTarget(Graphics::CurrentRenderTarget);
    D3DRenderer::UpdateViewport();
}
void        D3D_Predraw() {
    HRESULT result;

    if (renderData->UpdateBackBufferSize) {
        SDL_Window* window = Application::Window;
        int w, h;
        Uint32 window_flags = SDL_GetWindowFlags(window);
        SDL_GetWindowSize(window, &w, &h);

        renderData->PresentParams.BackBufferWidth = w;
        renderData->PresentParams.BackBufferHeight = h;
        if (window_flags & SDL_WINDOW_FULLSCREEN && (window_flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != SDL_WINDOW_FULLSCREEN_DESKTOP) {
            SDL_DisplayMode fullscreen_mode;
            SDL_GetWindowDisplayMode(window, &fullscreen_mode);
            renderData->PresentParams.Windowed = FALSE;
            renderData->PresentParams.BackBufferFormat = D3D_PixelFormatToD3DFORMAT(fullscreen_mode.format);
            renderData->PresentParams.FullScreen_RefreshRateInHz = fullscreen_mode.refresh_rate;
        }
        else {
            renderData->PresentParams.Windowed = TRUE;
            renderData->PresentParams.BackBufferFormat = D3DFMT_UNKNOWN;
            renderData->PresentParams.FullScreen_RefreshRateInHz = 0;
        }

        D3D_Reset();
        // if (D3D_Reset(renderer) < 0) {
        //     return -1;
        // }

        renderData->UpdateBackBufferSize = false;
    }
    if (renderData->BeginScene) {
        result = IDirect3DDevice9_BeginScene(renderData->Device);
        if (result == D3DERR_DEVICELOST) {
            D3D_Reset();
            // if (D3D_Reset(renderer) < 0) {
            //     return -1;
            // }
            result = IDirect3DDevice9_BeginScene(renderData->Device);
        }
        if (FAILED(result)) {
            D3D_SetError("BeginScene() %s", result);
        }
        renderData->BeginScene = false;
    }
}

void        D3D_SetBlendMode() {
    IDirect3DDevice9_SetRenderState(renderData->Device, D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(renderData->Device, D3DRS_SRCBLEND, D3D_Blend_SRC_COLOR);
    IDirect3DDevice9_SetRenderState(renderData->Device, D3DRS_DESTBLEND, D3D_Blend_DST_COLOR);
    if (renderData->EnableSeparateAlphaBlend) {
        IDirect3DDevice9_SetRenderState(renderData->Device, D3DRS_SRCBLENDALPHA, D3D_Blend_SRC_ALPHA);
        IDirect3DDevice9_SetRenderState(renderData->Device, D3DRS_DESTBLENDALPHA, D3D_Blend_DST_ALPHA);
    }

    // Enable multisample
    if (Graphics::MultisamplingEnabled) {
        IDirect3DDevice9_SetRenderState(renderData->Device, D3DRS_MULTISAMPLEANTIALIAS, TRUE);
    }
}
DWORD       D3D_GetBlendFactorFromHatch(int factor) {
    switch (factor) {
        case BlendFactor_ZERO:
            return D3DBLEND_ZERO;
        case BlendFactor_ONE:
            return D3DBLEND_ONE;
        case BlendFactor_SRC_COLOR:
            return D3DBLEND_SRCCOLOR;
        case BlendFactor_INV_SRC_COLOR:
            return D3DBLEND_INVSRCCOLOR;
        case BlendFactor_SRC_ALPHA:
            return D3DBLEND_SRCALPHA;
        case BlendFactor_INV_SRC_ALPHA:
            return D3DBLEND_INVSRCALPHA;
        case BlendFactor_DST_COLOR:
            return D3DBLEND_DESTCOLOR;
        case BlendFactor_INV_DST_COLOR:
            return D3DBLEND_INVDESTCOLOR;
        case BlendFactor_DST_ALPHA:
            return D3DBLEND_DESTALPHA;
        case BlendFactor_INV_DST_ALPHA:
            return D3DBLEND_INVDESTALPHA;
    }
    return 0;
}
void        D3D_BindTexture(Texture* texture, int index) {
    HRESULT result;

    if (!texture) {
        result = IDirect3DDevice9_SetTexture(renderData->Device, index, NULL);
        if (FAILED(result)) {
            D3D_SetError("SetTexture() %s", result);
        }
        return;
    }

    D3D_TextureData* textureData = (D3D_TextureData*)texture->DriverData;

    IDirect3DDevice9_SetSamplerState(renderData->Device, index, D3DSAMP_MINFILTER, textureData->ScaleMode);
    IDirect3DDevice9_SetSamplerState(renderData->Device, index, D3DSAMP_MAGFILTER, textureData->ScaleMode);
    IDirect3DDevice9_SetSamplerState(renderData->Device, index, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(renderData->Device, index, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    if (textureData->Dirty && textureData->Staging) {
        if (!textureData->Texture) {
            result = IDirect3DDevice9_CreateTexture(renderData->Device, texture->Width, texture->Height, 1, textureData->Usage, D3D_PixelFormatToD3DFORMAT(texture->Format), D3DPOOL_DEFAULT, &textureData->Texture, NULL);
            if (FAILED(result)) {
                D3D_SetError("CreateTexture(D3DPOOL_DEFAULT)", result);
            }
        }

        result = IDirect3DDevice9_UpdateTexture(renderData->Device, (IDirect3DBaseTexture9*)textureData->Staging, (IDirect3DBaseTexture9*)textureData->Texture);
        if (FAILED(result)) {
            D3D_SetError("UpdateTexture()", result);
        }
        textureData->Dirty = false;
    }

    result = IDirect3DDevice9_SetTexture(renderData->Device, index, (IDirect3DBaseTexture9*)textureData->Texture);
    if (FAILED(result)) {
        D3D_SetError("SetTexture() %s", result);
    }
}

void        D3D_DrawTextureRaw(Texture* texture, float sx, float sy, float sw, float sh, float x, float y, float w, float h, bool flipX, bool flipY) {
    HRESULT result;
    D3D_TextureData* textureData = (D3D_TextureData*)texture->DriverData;

    LPDIRECT3DPIXELSHADER9 shader = (LPDIRECT3DPIXELSHADER9)NULL;

    D3D_Predraw();

    float minu = (sx) / texture->Width;
    float maxu = (sx + sw) / texture->Width;
    float minv = (sy) / texture->Height;
    float maxv = (sy + sh) / texture->Height;
    DWORD color = 0xFFFFFFFF;
    if (Graphics::TextureBlend)
        color = D3D_BlendColorsAsHex;

    float minx = x;
    float maxx = x + w;
    float miny = y;
    float maxy = y + h;

    // NOTE: We do this because Direct3D9's origin for render targets is top-left,
    //    instead of the usual bottom-left.
    if (texture->Access == SDL_TEXTUREACCESS_TARGET) {
        // miny = y + h;
        // maxy = y;
        // if (Graphics::CurrentRenderTarget != NULL)
        //     flipY = !flipY;
        maxv = (sy) / texture->Height;
        minv = (sy + sh) / texture->Height;
    }

    Vertex vertices[4];
    // vertices[0] = Vertex { minx, miny, 0.0f, color, minu, minv };
    // vertices[1] = Vertex { maxx, miny, 0.0f, color, maxu, minv };
    // vertices[2] = Vertex { maxx, maxy, 0.0f, color, maxu, maxv };
    // vertices[3] = Vertex { minx, maxy, 0.0f, color, minu, maxv };
    vertices[0] = Vertex { 0.0f, 0.0f, 0.0f, color, minu, minv };
    vertices[1] = Vertex { 1.0f, 0.0f, 0.0f, color, maxu, minv };
    vertices[2] = Vertex { 1.0f, 1.0f, 0.0f, color, maxu, maxv };
    vertices[3] = Vertex { 0.0f, 1.0f, 0.0f, color, minu, maxv };

    if (D3D_PixelPerfectScale) {
        Point points[4];
        for (int i = 0; i < 4; i++) {
            points[i] = Graphics::ProjectToScreen(vertices[i].x, vertices[i].y, vertices[i].z);
            vertices[i].x = points[i].X ;
            vertices[i].y = points[i].Y;
            vertices[i].z = points[i].Z;
        }
    }

    // NOTE: This is not necessary in D3D10 and onwards
    float fx = flipX ? -1.0f : 1.0f;
    float fy = flipY ? -1.0f : 1.0f;
    // if (Graphics::CurrentRenderTarget) {
    //     for (int i = 0; i < 4; i++) {
    //         vertices[i].x -= 0.5f * fx;
    //         vertices[i].y += 0.5f * fy; // NOTE: Note the plus here, instead of minus
    //     }
    // }
    // else {
    //     for (int i = 0; i < 4; i++) {
    //         vertices[i].x -= 0.5f * fx;
    //         vertices[i].y -= 0.5f * fy;
    //     }
    // }

    D3D_SetBlendMode();
    D3D_BindTexture(texture, 0);

    if (shader) {
        // result = IDirect3DDevice9_SetPixelShader(renderData->Device, shader);
        // if (FAILED(result)) {
        //     return D3D_SetError("SetShader()", result);
        // }
    }

    D3DMATRIX matrix;
    if (D3D_PixelPerfectScale) {
        memcpy(&matrix.m, D3D_MatrixIdentity->Values, sizeof(float) * 16);
    }
    else {
        // memcpy(&matrix.m, Graphics::ModelViewMatrix->Values, sizeof(float) * 16);
        Graphics::Save();
        Graphics::Translate(x, y, 0.0f);
        if (texture->Access != SDL_TEXTUREACCESS_TARGET)
            Graphics::Translate(-0.5f * fx, 0.5f * fy, 0.0f);
        else
            Graphics::Translate(-0.5f * fx, -0.5f * fy, 0.0f);
        Graphics::Scale(w, h, 1.0f);
            memcpy(&matrix.m, Graphics::ModelViewMatrix->Values, sizeof(float) * 16);
        Graphics::Restore();
    }
    IDirect3DDevice9_SetTransform(renderData->Device, D3DTS_VIEW, &matrix);

    result = IDirect3DDevice9_DrawPrimitiveUP(renderData->Device, D3DPT_TRIANGLEFAN, 2, vertices, sizeof(*vertices));
    if (FAILED(result)) {
        D3D_SetError("DrawPrimitiveUP() %s", result);
    }

    if (shader) {
        // IDirect3DDevice9_SetPixelShader(renderData->Device, NULL);
    }
    // return FAILED(result) ? -1 : 0;
}

void        D3D_BeginDrawShape(Vertex* shapeBuffer, int vertexCount) {
    D3D_Predraw();

    Vertex* buffer = (Vertex*)shapeBuffer;
    for (int i = 0; i < vertexCount; i++) {
        buffer[i].color = D3D_BlendColorsAsHex;
    }

    D3D_SetBlendMode();
    D3D_BindTexture(NULL, 0);
}
void        D3D_EndDrawShape(Vertex* shapeBuffer, D3DPRIMITIVETYPE prim, int triangleCount) {
    HRESULT result = IDirect3DDevice9_DrawPrimitiveUP(renderData->Device, prim, triangleCount, shapeBuffer, sizeof(Vertex));
    if (FAILED(result))
        D3D_SetError("DrawPrimitiveUP() %s", result);
}

// Initialization and disposal functions
void     D3DRenderer::Init() {
    Graphics::PreferredPixelFormat = SDL_PIXELFORMAT_ARGB8888;
    D3D_MatrixIdentity = Matrix4x4::Create();

    renderData = (D3D_RenderData*)calloc(1, sizeof(D3D_RenderData));

    // renderData->D3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!D3D_LoadDLL(&renderData->D3D_DLL, &renderData->D3D)) {
        Log::Print(Log::LOG_ERROR, "Unable to create Direct3D interface.");
        return;
    }

    Log::Print(Log::LOG_INFO, "Renderer: Direct3D 9");

    D3DCAPS9 caps;
    HRESULT result;
    DWORD device_flags;
    Uint32 window_flags;
    SDL_SysWMinfo windowinfo;
    IDirect3DSwapChain9* chain;
    D3DPRESENT_PARAMETERS pparams;
    SDL_DisplayMode fullscreen_mode;
    int displayIndex, w, h, texture_max_w, texture_max_h;

	SDL_VERSION(&windowinfo.version);
    SDL_GetWindowWMInfo(Application::Window, &windowinfo);

    window_flags = SDL_GetWindowFlags(Application::Window);
    SDL_GetWindowSize(Application::Window, &w, &h);
    SDL_GetWindowDisplayMode(Application::Window, &fullscreen_mode);

    memset(&pparams, 0, sizeof(pparams));
    pparams.hDeviceWindow = windowinfo.info.win.window;
    pparams.BackBufferWidth = w;
    pparams.BackBufferHeight = h;
    pparams.BackBufferCount = 1;
    pparams.SwapEffect = D3DSWAPEFFECT_DISCARD;

    pparams.EnableAutoDepthStencil = TRUE;
    pparams.AutoDepthStencilFormat = D3DFMT_D16;

    if (window_flags & SDL_WINDOW_FULLSCREEN && (window_flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != SDL_WINDOW_FULLSCREEN_DESKTOP) {
        pparams.Windowed = FALSE;
        pparams.BackBufferFormat = D3D_PixelFormatToD3DFORMAT(fullscreen_mode.format);
        pparams.FullScreen_RefreshRateInHz = fullscreen_mode.refresh_rate;
    }
    else {
        pparams.Windowed = TRUE;
        pparams.BackBufferFormat = D3DFMT_UNKNOWN;
        pparams.FullScreen_RefreshRateInHz = 0;
    }

    if (Graphics::VsyncEnabled) {
        pparams.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    }
    else {
        pparams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    }

    displayIndex = SDL_GetWindowDisplayIndex(Application::Window);
    renderData->Adapter = SDL_Direct3D9GetAdapterIndex(displayIndex);

    // Check for full-scene antialiasing
    if (Graphics::MultisamplingEnabled) {
        pparams.MultiSampleType = D3DMULTISAMPLE_2_SAMPLES;
        if (SUCCEEDED(result = IDirect3D9_CheckDeviceMultiSampleType(renderData->D3D, renderData->Adapter,
            D3DDEVTYPE_HAL,
            D3D_PixelFormatToD3DFORMAT(SDL_GetWindowPixelFormat(Application::Window)),
            pparams.Windowed,
            pparams.MultiSampleType, NULL))) {
            pparams.SwapEffect = D3DSWAPEFFECT_DISCARD;
        }
        else if (FAILED(result)) {
            pparams.MultiSampleType = D3DMULTISAMPLE_NONE;
            Graphics::MultisamplingEnabled = 0;
        }
    }

    IDirect3D9_GetDeviceCaps(renderData->D3D, renderData->Adapter, D3DDEVTYPE_HAL, &caps);

    device_flags = D3DCREATE_FPU_PRESERVE;
    if (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
        device_flags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
    else
        device_flags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;

    if (SDL_GetHintBoolean(SDL_HINT_RENDER_DIRECT3D_THREADSAFE, SDL_FALSE)) {
        device_flags |= D3DCREATE_MULTITHREADED;
    }

    result = IDirect3D9_CreateDevice(
        renderData->D3D, renderData->Adapter,
        D3DDEVTYPE_HAL,
        pparams.hDeviceWindow,
        device_flags,
        &pparams, &renderData->Device);
    if (FAILED(result)) {
        D3DRenderer::Dispose();
        Log::Print(Log::LOG_ERROR, "CreateDevice() %s", D3D_GetResultString(result));
        exit(-1);
    }

    /* Get presentation parameters to fill info */
    result = IDirect3DDevice9_GetSwapChain(renderData->Device, 0, &chain);
    if (FAILED(result)) {
        D3DRenderer::Dispose();
        Log::Print(Log::LOG_ERROR, "GetSwapChain() %d", result);
        return;
    }
    result = IDirect3DSwapChain9_GetPresentParameters(chain, &pparams);
    if (FAILED(result)) {
        IDirect3DSwapChain9_Release(chain);
        D3DRenderer::Dispose();
        Log::Print(Log::LOG_ERROR, "GetPresentParameters() %d", result);
        return;
    }
    IDirect3DSwapChain9_Release(chain);
    if (pparams.PresentationInterval == D3DPRESENT_INTERVAL_ONE) {
        Graphics::VsyncEnabled = true;
    }
    renderData->PresentParams = pparams;

    IDirect3DDevice9_GetDeviceCaps(renderData->Device, &caps);
    texture_max_w = caps.MaxTextureWidth;
    texture_max_h = caps.MaxTextureHeight;
    if (caps.NumSimultaneousRTs >= 2) {
        // renderer->info.flags |= SDL_RENDERER_TARGETTEXTURE;
    }

    if (caps.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND) {
        renderData->EnableSeparateAlphaBlend = SDL_TRUE;
    }

    /* Store the default render target */
    IDirect3DDevice9_GetRenderTarget(renderData->Device, 0, &renderData->DefaultRenderTarget);
    renderData->CurrentRenderTarget = NULL;

    /* Set up parameters for rendering */
    D3D_InitRenderState();

    if (caps.MaxSimultaneousTextures >= 3) {
        // int i;
        // for (i = 0; i < SDL_arraysize(D3DRenderer::shaders); ++i) {
        //     result = D3D9_CreatePixelShader(renderData->Device, (D3D9_Shader)i, &D3DRenderer::shaders[i]);
        //     if (FAILED(result)) {
        //         D3D_SetError("CreatePixelShader()", result);
        //     }
        // }
        // if (D3DRenderer::shaders[SHADER_YUV_JPEG] && D3DRenderer::shaders[SHADER_YUV_BT601] && D3DRenderer::shaders[SHADER_YUV_BT709]) {
        //     renderer->info.texture_formats[renderer->info.num_texture_formats++] = SDL_PIXELFORMAT_YV12;
        //     renderer->info.texture_formats[renderer->info.num_texture_formats++] = SDL_PIXELFORMAT_IYUV;
        // }
    }

    D3D_MakeShaders();
    D3D_MakeShapeBuffers();

    // UseShader(D3D_ShaderShape);

    Graphics::MaxTextureWidth = texture_max_w;
    Graphics::MaxTextureHeight = texture_max_h;

    D3DADAPTER_IDENTIFIER9 indent;
    IDirect3D9_GetAdapterIdentifier(renderData->D3D, renderData->Adapter, 0, &indent);

    Log::Print(Log::LOG_INFO, "Graphics Card: %s", indent.Description);
}
Uint32   D3DRenderer::GetWindowFlags() {
    return 0;
}
void     D3DRenderer::SetVSync(bool enabled) {
    
}
void     D3DRenderer::SetGraphicsFunctions() {
    Graphics::PixelOffset = 0.5f;

    Graphics::Internal.Init = D3DRenderer::Init;
    Graphics::Internal.GetWindowFlags = D3DRenderer::GetWindowFlags;
    Graphics::Internal.SetVSync = D3DRenderer::SetVSync;
    Graphics::Internal.Dispose = D3DRenderer::Dispose;

    // Texture management functions
    Graphics::Internal.CreateTexture = D3DRenderer::CreateTexture;
    Graphics::Internal.LockTexture = D3DRenderer::LockTexture;
    Graphics::Internal.UpdateTexture = D3DRenderer::UpdateTexture;
    Graphics::Internal.UnlockTexture = D3DRenderer::UnlockTexture;
    Graphics::Internal.DisposeTexture = D3DRenderer::DisposeTexture;

    // Viewport and view-related functions
    Graphics::Internal.SetRenderTarget = D3DRenderer::SetRenderTarget;
    Graphics::Internal.UpdateWindowSize = D3DRenderer::UpdateWindowSize;
    Graphics::Internal.UpdateViewport = D3DRenderer::UpdateViewport;
    Graphics::Internal.UpdateClipRect = D3DRenderer::UpdateClipRect;
    Graphics::Internal.UpdateOrtho = D3DRenderer::UpdateOrtho;
    Graphics::Internal.UpdatePerspective = D3DRenderer::UpdatePerspective;
    Graphics::Internal.UpdateProjectionMatrix = D3DRenderer::UpdateProjectionMatrix;
    Graphics::Internal.MakePerspectiveMatrix = D3DRenderer::MakePerspectiveMatrix;

    // Shader-related functions
    Graphics::Internal.UseShader = D3DRenderer::UseShader;
    Graphics::Internal.SetUniformF = D3DRenderer::SetUniformF;
    Graphics::Internal.SetUniformI = D3DRenderer::SetUniformI;
    Graphics::Internal.SetUniformTexture = D3DRenderer::SetUniformTexture;

    // These guys
    Graphics::Internal.Clear = D3DRenderer::Clear;
    Graphics::Internal.Present = D3DRenderer::Present;

    // Draw mode setting functions
    Graphics::Internal.SetBlendColor = D3DRenderer::SetBlendColor;
    Graphics::Internal.SetBlendMode = D3DRenderer::SetBlendMode;
    Graphics::Internal.SetTintColor = D3DRenderer::SetTintColor;
    Graphics::Internal.SetTintMode = D3DRenderer::SetTintMode;
    Graphics::Internal.SetTintEnabled = D3DRenderer::SetTintEnabled;
    Graphics::Internal.SetLineWidth = D3DRenderer::SetLineWidth;

    // Primitive drawing functions
    Graphics::Internal.StrokeLine = D3DRenderer::StrokeLine;
    Graphics::Internal.StrokeCircle = D3DRenderer::StrokeCircle;
    Graphics::Internal.StrokeEllipse = D3DRenderer::StrokeEllipse;
    Graphics::Internal.StrokeRectangle = D3DRenderer::StrokeRectangle;
    Graphics::Internal.FillCircle = D3DRenderer::FillCircle;
    Graphics::Internal.FillEllipse = D3DRenderer::FillEllipse;
    Graphics::Internal.FillTriangle = D3DRenderer::FillTriangle;
    Graphics::Internal.FillRectangle = D3DRenderer::FillRectangle;

    // Texture drawing functions
    Graphics::Internal.DrawTexture = D3DRenderer::DrawTexture;
    Graphics::Internal.DrawSprite = D3DRenderer::DrawSprite;
    Graphics::Internal.DrawSpritePart = D3DRenderer::DrawSpritePart;

    // 3D drawing functions
    Graphics::Internal.DrawPolygon3D = D3DRenderer::DrawPolygon3D;
    Graphics::Internal.DrawSceneLayer3D = D3DRenderer::DrawSceneLayer3D;
    Graphics::Internal.DrawModel = D3DRenderer::DrawModel;
    Graphics::Internal.DrawModelSkinned = D3DRenderer::DrawModelSkinned;
    Graphics::Internal.DrawVertexBuffer = D3DRenderer::DrawVertexBuffer;
    Graphics::Internal.BindVertexBuffer = D3DRenderer::BindVertexBuffer;
    Graphics::Internal.UnbindVertexBuffer = D3DRenderer::UnbindVertexBuffer;
    Graphics::Internal.BindScene3D = D3DRenderer::BindScene3D;
    Graphics::Internal.ClearScene3D = D3DRenderer::ClearScene3D;
    Graphics::Internal.DrawScene3D = D3DRenderer::DrawScene3D;
}
void     D3DRenderer::Dispose() {
    Memory::Free(D3D_BufferCircleFill);
    Memory::Free(D3D_BufferCircleStroke);
    Memory::Free(D3D_BufferSquareFill);

    // D3D_ShaderShape->Dispose(); delete D3D_ShaderShape;
    // D3D_ShaderTexturedShape->Dispose(); delete D3D_ShaderTexturedShape;
    // ShaderTexturedShapeYUV->Dispose(); delete ShaderTexturedShapeYUV;
    // ShaderTexturedShapeBlur->Dispose(); delete ShaderTexturedShapeBlur;
}

// Texture management functions
Texture* D3DRenderer::CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height) {
    Texture* texture = Texture::New(format, access, width, height);

    D3D_CreateTexture(texture);

    texture->ID = Graphics::TextureMap->Count() + 1;
    Graphics::TextureMap->Put(texture->ID, texture);

    return texture;
}
int      D3DRenderer::LockTexture(Texture* texture, void** pixels, int* pitch) {
    return 0;
}
int      D3DRenderer::UpdateTexture(Texture* texture, SDL_Rect* r, void* pixels, int pitch) {
    int inputPixelsX = 0;
    int inputPixelsY = 0;
    int inputPixelsW = texture->Width;
    int inputPixelsH = texture->Height;
    if (r) {
        inputPixelsX = r->x;
        inputPixelsY = r->y;
        inputPixelsW = r->w;
        inputPixelsH = r->h;
    }

    if (Graphics::NoInternalTextures) {
        if (inputPixelsW > Graphics::MaxTextureWidth)
            inputPixelsW = Graphics::MaxTextureWidth;
        if (inputPixelsH > Graphics::MaxTextureHeight)
            inputPixelsH = Graphics::MaxTextureHeight;
    }

    D3D_TextureData* textureData = (D3D_TextureData*)texture->DriverData;

    RECT d3drect;
    D3DLOCKED_RECT locked;
    const Uint8 *src;
    Uint8 *dst;
    int row, length;
    HRESULT result;

    d3drect.left   = inputPixelsX;
    d3drect.right  = inputPixelsX + inputPixelsW;
    d3drect.top    = inputPixelsY;
    d3drect.bottom = inputPixelsY + inputPixelsH;

    if (textureData->Staging == NULL) {
        result = IDirect3DDevice9_CreateTexture(renderData->Device, texture->Width, texture->Height, 1, 0, textureData->D3DFormat, D3DPOOL_SYSTEMMEM, &textureData->Staging, NULL);
        if (FAILED(result)) {
            return D3D_SetError("CreateTexture(D3DPOOL_SYSTEMMEM)", result);
        }
    }

    result = IDirect3DTexture9_LockRect(textureData->Staging, 0, &locked, &d3drect, 0);
    if (FAILED(result)) {
        D3D_SetError("LockRect() %s", result);
        return -1;
    }

    src = (const Uint8*)pixels;
    dst = (Uint8*)locked.pBits;
    length = inputPixelsW * SDL_BYTESPERPIXEL(texture->Format);
    if (length == pitch && length == locked.Pitch) {
        memcpy(dst, src, length * inputPixelsH);
    }
    else {
        if (length > pitch) {
            length = pitch;
        }
        if (length > locked.Pitch) {
            length = locked.Pitch;
        }
        for (row = 0; row < inputPixelsH; ++row) {
            memcpy(dst, src, length);
            src += pitch;
            dst += locked.Pitch;
        }
    }
    result = IDirect3DTexture9_UnlockRect(textureData->Staging, 0);
    if (FAILED(result)) {
        D3D_SetError("UnlockRect() %s", result);
        return -1;
    }
    textureData->Dirty = true;

    return 0;
}
void     D3DRenderer::UnlockTexture(Texture* texture) {

}
void     D3DRenderer::DisposeTexture(Texture* texture) {
    D3D_TextureData* textureData = (D3D_TextureData*)texture->DriverData;
    if (!textureData)
        return;

    if (textureData->Texture) {
        IDirect3DTexture9_Release(textureData->Texture);
        textureData->Texture = NULL;
    }
    if (textureData->Staging) {
        IDirect3DTexture9_Release(textureData->Staging);
        textureData->Staging = NULL;
    }

    // free(texture->Pixels);
    Memory::Free(texture->DriverData);

    texture->DriverData = NULL;
}

// Viewport and view-related functions
void     D3DRenderer::SetRenderTarget(Texture* texture) {
    D3D_Predraw();
    D3D_SetRenderTarget(texture);
}
void     D3DRenderer::UpdateWindowSize(int width, int height) {
    // D3D_UpdateViewport(0, 0, width, height);
    renderData->UpdateBackBufferSize = true;
}
void     D3DRenderer::UpdateViewport() {
    Viewport* vp = &Graphics::CurrentViewport;

    D3DVIEWPORT9 viewport;
    viewport.X = vp->X;
    viewport.Y = vp->Y;
    viewport.Width = vp->Width;
    viewport.Height = vp->Height; // * RetinaScale
    viewport.MinZ = 0.0f;
    viewport.MaxZ = 1.0f;
    IDirect3DDevice9_SetViewport(renderData->Device, &viewport);

    // NOTE: According to SDL2 we should be setting projection matrix here.
    // D3DRenderer::UpdateOrtho(vp->Width, vp->Height);
    D3DRenderer::UpdateProjectionMatrix();
}
void     D3DRenderer::UpdateClipRect() {
    ClipArea clip = Graphics::CurrentClip;
    if (Graphics::CurrentClip.Enabled) {
        Viewport view = Graphics::CurrentViewport;

        IDirect3DDevice9_SetRenderState(renderData->Device, D3DRS_SCISSORTESTENABLE, TRUE);


        int w, h;
        float scaleW = 1.0f, scaleH = 1.0f;
        SDL_GetWindowSize(Application::Window, &w, &h);
        View* currentView = &Scene::Views[Scene::ViewCurrent];
        scaleW *= w / currentView->Width;
        scaleH *= h / currentView->Height;

        RECT r;
        r.left   = view.X + (int)((clip.X) * scaleW);
        r.top    = view.Y + (int)((clip.Y) * scaleH);
        r.right  = view.X + (int)((clip.X + clip.Width) * scaleW);
        r.bottom = view.Y + (int)((clip.Y + clip.Height) * scaleH);

        HRESULT result = IDirect3DDevice9_SetScissorRect(renderData->Device, &r);
        if (result != D3D_OK) {
            D3D_SetError("SetScissor()", result);
        }
    }
    else {
        IDirect3DDevice9_SetRenderState(renderData->Device, D3DRS_SCISSORTESTENABLE, FALSE);
    }
}
void     D3DRenderer::UpdateOrtho(float left, float top, float right, float bottom) {
    if (Scene::Views[Scene::ViewCurrent].Software)
        Matrix4x4::Ortho(Scene::Views[Scene::ViewCurrent].BaseProjectionMatrix, left, right, bottom, top, 500.0f, -500.0f);
    else
        Matrix4x4::Ortho(Scene::Views[Scene::ViewCurrent].BaseProjectionMatrix, left, right, top, bottom, 500.0f, -500.0f);
    Matrix4x4::Copy(Scene::Views[Scene::ViewCurrent].ProjectionMatrix, Scene::Views[Scene::ViewCurrent].BaseProjectionMatrix);

    if (D3D_PixelPerfectScale) {
        D3D_RenderScaleX = Graphics::CurrentViewport.Width / right;
        D3D_RenderScaleY = Graphics::CurrentViewport.Height / top;
    }
}
void     D3DRenderer::UpdatePerspective(float fovy, float aspect, float nearv, float farv) {
    MakePerspectiveMatrix(Scene::Views[Scene::ViewCurrent].BaseProjectionMatrix, fovy, nearv, farv, aspect);
    Matrix4x4::Copy(Scene::Views[Scene::ViewCurrent].ProjectionMatrix, Scene::Views[Scene::ViewCurrent].BaseProjectionMatrix);
}
void     D3DRenderer::UpdateProjectionMatrix() {
    D3DMATRIX matrix;
    memcpy(&matrix.m, Scene::Views[Scene::ViewCurrent].ProjectionMatrix->Values, sizeof(float) * 16);
    IDirect3DDevice9_SetTransform(renderData->Device, D3DTS_PROJECTION, &matrix);
}
void     D3DRenderer::MakePerspectiveMatrix(Matrix4x4* out, float fov, float near, float far, float aspect) {
    Matrix4x4::Perspective(out, fov, aspect, near, far);
}

// Shader-related functions
void     D3DRenderer::UseShader(void* shader) {
    // if (D3D_CurrentShader != (D3DShader*)shader) {
    //     D3D_CurrentShader = (D3DShader*)shader;
    //     D3D_CurrentShader->Use();
    // }
}
void     D3DRenderer::SetUniformF(int location, int count, float* values) {
    // switch (count) {
    //     case 1: glUniform1f(location, values[0]); CHECK_GL(); break;
    //     case 2: glUniform2f(location, values[0], values[1]); CHECK_GL(); break;
    //     case 3: glUniform3f(location, values[0], values[1], values[2]); CHECK_GL(); break;
    //     case 4: glUniform4f(location, values[0], values[1], values[2], values[3]); CHECK_GL(); break;
    // }
}
void     D3DRenderer::SetUniformI(int location, int count, int* values) {
    // glUniform1iv(location, count, values);
}
void     D3DRenderer::SetUniformTexture(Texture* texture, int uniform_index, int slot) {

}

// These guys
void     D3DRenderer::Clear() {
    DWORD color;
    HRESULT result;
    int BackBufferWidth, BackBufferHeight;

    D3D_Predraw();

    if (Graphics::CurrentRenderTarget) {
        BackBufferWidth = Graphics::CurrentRenderTarget->Width;
        BackBufferHeight = Graphics::CurrentRenderTarget->Height;
    }
    else {
        BackBufferWidth = renderData->PresentParams.BackBufferWidth;
        BackBufferHeight = renderData->PresentParams.BackBufferHeight;
    }

    if (Graphics::CurrentClip.Enabled)
        IDirect3DDevice9_SetRenderState(renderData->Device, D3DRS_SCISSORTESTENABLE, FALSE);

    DWORD clear_flags = D3DCLEAR_TARGET;

    clear_flags |= D3DCLEAR_ZBUFFER;

    color = 0x00000000;

    /* Don't reset the viewport if we don't have to! */
    Viewport* vp = &Graphics::CurrentViewport;
    if (vp->X == 0.0 &&
        vp->Y == 0.0 &&
        vp->Width == BackBufferWidth &&
        vp->Height == BackBufferHeight) {
        result = IDirect3DDevice9_Clear(renderData->Device, 0, NULL, clear_flags, color, 1.0f, 0);
    }
    else {
        D3DVIEWPORT9 viewport;

        /* Clear is defined to clear the entire render target */
        viewport.X = 0.0;
        viewport.Y = 0.0;
        viewport.Width = BackBufferWidth;
        viewport.Height = BackBufferHeight;
        viewport.MinZ = 0.0f;
        viewport.MaxZ = 1.0f;
        IDirect3DDevice9_SetViewport(renderData->Device, &viewport);

        result = IDirect3DDevice9_Clear(renderData->Device, 0, NULL, clear_flags, color, 1.0f, 0);

        /* Reset the viewport */
        viewport.X = vp->X;
        viewport.Y = vp->Y;
        viewport.Width = vp->Width;
        viewport.Height = vp->Height;
        viewport.MinZ = 0.0f;
        viewport.MaxZ = 1.0f;
        IDirect3DDevice9_SetViewport(renderData->Device, &viewport);
    }

    if (Graphics::CurrentClip.Enabled)
        IDirect3DDevice9_SetRenderState(renderData->Device, D3DRS_SCISSORTESTENABLE, TRUE);

    if (FAILED(result)) {
        D3D_SetError("Clear()", result);
    }
}
void     D3DRenderer::Present() {
    HRESULT result;

    if (!renderData->BeginScene) {
        IDirect3DDevice9_EndScene(renderData->Device);
        renderData->BeginScene = true;
    }

    result = IDirect3DDevice9_TestCooperativeLevel(renderData->Device);
    if (result == D3DERR_DEVICELOST) {
        /* We'll reset later */
        return;
    }
    if (result == D3DERR_DEVICENOTRESET) {
        D3D_Reset();
        // D3D_Reset(renderer);
    }

    result = IDirect3DDevice9_Present(renderData->Device, NULL, NULL, NULL, NULL);
    if (FAILED(result)) {
        D3D_SetError("Present()", result);
    }
}

// Draw mode setting functions
void     D3DRenderer::SetBlendColor(float r, float g, float b, float a) {
    int ha = (int)(a * 0xFF) << 24;
    int hr = (int)(r * 0xFF) << 16;
    int hg = (int)(g * 0xFF) << 8;
    int hb = (int)(b * 0xFF);
    D3D_BlendColorsAsHex = ha | hr | hg | hb;
}
void     D3DRenderer::SetBlendMode(int srcC, int dstC, int srcA, int dstA) {
    D3D_Blend_SRC_COLOR = D3D_GetBlendFactorFromHatch(srcC);
    D3D_Blend_DST_COLOR = D3D_GetBlendFactorFromHatch(dstC);
    D3D_Blend_SRC_ALPHA = D3D_GetBlendFactorFromHatch(srcA);
    D3D_Blend_DST_ALPHA = D3D_GetBlendFactorFromHatch(dstA);
}
void     D3DRenderer::SetTintColor(float r, float g, float b, float a) {

}
void     D3DRenderer::SetTintMode(int mode) {

}
void     D3DRenderer::SetTintEnabled(bool enabled) {

}
void     D3DRenderer::SetLineWidth(float n) {

}

// Primitive drawing functions
void     D3DRenderer::StrokeLine(float x1, float y1, float x2, float y2) {
    // UseShader(D3D_SelectedShader ? D3D_SelectedShader : D3D_ShaderShape);

    Graphics::Save();
        // glUniformMatrix4fv(D3D_CurrentShader->LocModelViewMatrix, 1, false, Graphics::ModelViewMatrix->Values);

        float v[6];
        v[0] = x1; v[1] = y1; v[2] = 0.0f;
        v[3] = x2; v[4] = y2; v[5] = 0.0f;

        // glBindBuffer(GL_ARRAY_BUFFER, 0);
        // glVertexAttribPointer(D3D_CurrentShader->LocPosition, 3, GL_FLOAT, GL_FALSE, 0, v);
        // glDrawArrays(GL_LINES, 0, 2);
    Graphics::Restore();
}
void     D3DRenderer::StrokeCircle(float x, float y, float rad, float thickness) {
    D3D_BeginDrawShape(D3D_BufferCircleStroke, 361);

    Graphics::Save();
    Graphics::Translate(x, y, 0.0f);
    Graphics::Scale(rad, rad, 1.0f);
        D3DMATRIX matrix;
        memcpy(&matrix.m, Graphics::ModelViewMatrix->Values, sizeof(float) * 16);
        IDirect3DDevice9_SetTransform(renderData->Device, D3DTS_VIEW, &matrix);
    Graphics::Restore();

    D3D_EndDrawShape(D3D_BufferCircleStroke, D3DPT_LINESTRIP, 360);
}
void     D3DRenderer::StrokeEllipse(float x, float y, float w, float h) {
    // UseShader(D3D_SelectedShader ? D3D_SelectedShader : D3D_ShaderShape);

    Graphics::Save();
    Graphics::Translate(x + w / 2, y + h / 2, 0.0f);
    Graphics::Scale(w / 2, h / 2, 1.0f);
        // glUniformMatrix4fv(D3D_CurrentShader->LocModelViewMatrix, 1, false, Graphics::ModelViewMatrix->Values);

        // glBindBuffer(GL_ARRAY_BUFFER, D3D_BufferCircleStroke);
        // glVertexAttribPointer(D3D_CurrentShader->LocPosition, 3, GL_FLOAT, GL_FALSE, 0, 0);
        // glDrawArrays(GL_LINE_STRIP, 0, 361);
    Graphics::Restore();
}
void     D3DRenderer::StrokeRectangle(float x, float y, float w, float h) {
    StrokeLine(x, y, x + w, y);
    StrokeLine(x, y + h, x + w, y + h);

    StrokeLine(x, y, x, y + h);
    StrokeLine(x + w, y, x + w, y + h);
}
void     D3DRenderer::FillCircle(float x, float y, float rad) {
    D3D_BeginDrawShape(D3D_BufferCircleFill, 362);

    Graphics::Save();
    Graphics::Translate(x, y, 0.0f);
    Graphics::Scale(rad, rad, 1.0f);
        D3DMATRIX matrix;
        memcpy(&matrix.m, Graphics::ModelViewMatrix->Values, sizeof(float) * 16);
        IDirect3DDevice9_SetTransform(renderData->Device, D3DTS_VIEW, &matrix);
    Graphics::Restore();

    D3D_EndDrawShape(D3D_BufferCircleFill, D3DPT_TRIANGLEFAN, 360);
}
void     D3DRenderer::FillEllipse(float x, float y, float w, float h) {
    D3D_BeginDrawShape(D3D_BufferCircleFill, 362);

    w *= 0.5f;
    h *= 0.5f;

    Graphics::Save();
    Graphics::Translate(x + w, y + h, 0.0f);
    Graphics::Scale(w, h, 1.0f);
        D3DMATRIX matrix;
        memcpy(&matrix.m, Graphics::ModelViewMatrix->Values, sizeof(float) * 16);
        IDirect3DDevice9_SetTransform(renderData->Device, D3DTS_VIEW, &matrix);
    Graphics::Restore();

    D3D_EndDrawShape(D3D_BufferCircleFill, D3DPT_TRIANGLEFAN, 360);
}
void     D3DRenderer::FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3) {
    Vertex vertices[3];
    vertices[0] = Vertex { x1, y1, 0.0f, D3D_BlendColorsAsHex, 0.0f, 0.0f };
    vertices[1] = Vertex { x2, y2, 0.0f, D3D_BlendColorsAsHex, 0.0f, 0.0f };
    vertices[2] = Vertex { x3, y3, 0.0f, D3D_BlendColorsAsHex, 0.0f, 0.0f };

    D3D_BeginDrawShape(vertices, 3);

    D3DMATRIX matrix;
    memcpy(&matrix.m, Graphics::ModelViewMatrix->Values, sizeof(float) * 16);
    IDirect3DDevice9_SetTransform(renderData->Device, D3DTS_VIEW, &matrix);

    D3D_EndDrawShape(vertices, D3DPT_TRIANGLEFAN, 1);
}
void     D3DRenderer::FillRectangle(float x, float y, float w, float h) {
    D3D_BeginDrawShape(D3D_BufferSquareFill, 4);

    Graphics::Save();
    Graphics::Translate(x, y, 0.0f);
    Graphics::Scale(w, h, 1.0f);
        D3DMATRIX matrix;
        memcpy(&matrix.m, Graphics::ModelViewMatrix->Values, sizeof(float) * 16);
        IDirect3DDevice9_SetTransform(renderData->Device, D3DTS_VIEW, &matrix);
    Graphics::Restore();

    D3D_EndDrawShape(D3D_BufferSquareFill, D3DPT_TRIANGLEFAN, 2);
}

// Texture drawing functions
void     D3DRenderer::DrawTexture(Texture* texture, float sx, float sy, float sw, float sh, float x, float y, float w, float h) {
    // UseShader(D3D_SelectedShader ? D3D_SelectedShader : D3D_ShaderTexturedShape);
    if (sx < 0)
        sx = 0.0, sy = 0.0, sw = texture->Width, sh = texture->Height;
    D3D_DrawTextureRaw(texture, sx, sy, sw, sh, x, y, w, h, false, texture->Access != SDL_TEXTUREACCESS_TARGET);
}
void     D3DRenderer::DrawSprite(ISprite* sprite, int animation, int frame, int x, int y, bool flipX, bool flipY, float scaleW, float scaleH, float rotation, unsigned paletteID) {
	if (Graphics::SpriteRangeCheck(sprite, animation, frame)) return;

	AnimFrame animframe = sprite->Animations[animation].Frames[frame];

	float fX = flipX ? -1.0 : 1.0;
	float fY = flipY ? -1.0 : 1.0;
	// DrawTexture(sprite->Spritesheets[animframe.SheetNumber], animframe.X, animframe.Y, animframe.W, animframe.H, x + fX * animframe.OffX, y + fY * animframe.OffY, fX * animframe.W, fY * animframe.H);

    // UseShader(D3D_SelectedShader ? D3D_SelectedShader : D3D_ShaderTexturedShape);
    Graphics::Save();
    Graphics::Translate(x, y, 0.0f);
    Graphics::Scale(fX, fY, 1.0f);
        D3D_DrawTextureRaw(sprite->Spritesheets[animframe.SheetNumber], animframe.X, animframe.Y, animframe.Width, animframe.Height, animframe.OffsetX, animframe.OffsetY, animframe.Width, animframe.Height, flipX, flipY);
    Graphics::Restore();
}
void     D3DRenderer::DrawSpritePart(ISprite* sprite, int animation, int frame, int sx, int sy, int sw, int sh, int x, int y, bool flipX, bool flipY, float scaleW, float scaleH, float rotation, unsigned paletteID) {
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

    // UseShader(D3D_SelectedShader ? D3D_SelectedShader : D3D_ShaderTexturedShape);
    Graphics::Save();
    Graphics::Translate(x, y, 0.0f);
    Graphics::Scale(fX, fY, 1.0f);
        D3D_DrawTextureRaw(sprite->Spritesheets[animframe.SheetNumber],
            animframe.X + sx, animframe.Y + sy,
            sw, sh,
            sx + animframe.OffsetX, sy + animframe.OffsetY,
            sw, sh,
            flipX, flipY);
    Graphics::Restore();
}
// 3D drawing functions
void     D3DRenderer::DrawPolygon3D(void* data, int vertexCount, int vertexFlag, Texture* texture, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {

}
void     D3DRenderer::DrawSceneLayer3D(void* layer, int sx, int sy, int sw, int sh, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {

}
void     D3DRenderer::DrawModel(void* model, Uint16 animation, Uint32 frame, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {

}
void     D3DRenderer::DrawModelSkinned(void* model, Uint16 armature, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {

}
void     D3DRenderer::DrawVertexBuffer(Uint32 vertexBufferIndex, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {

}
void     D3DRenderer::BindVertexBuffer(Uint32 vertexBufferIndex) {

}
void     D3DRenderer::UnbindVertexBuffer() {

}
void     D3DRenderer::BindScene3D(Uint32 sceneIndex) {

}
void     D3DRenderer::ClearScene3D(Uint32 sceneIndex) {

}
void     D3DRenderer::DrawScene3D(Uint32 sceneIndex, Uint32 drawMode) {

}

/*
// Draw buffering
Uint32   D3DRenderer::CreateTexturedShapeBuffer(float** data, int vertexCount) {
    // x, y, z, u, v
    Uint32 bufferID;
    glGenBuffers(1, &bufferID);
    glBindBuffer(GL_ARRAY_BUFFER, bufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertexCount * 5, data, GL_STATIC_DRAW);

    *data = NULL;

    return bufferID;
}

void     D3DRenderer::DrawTexturedShapeBuffer(Texture* texture, Uint32 bufferID, int vertexCount) {
    UseShader(D3D_SelectedShader ? D3D_SelectedShader : D3D_ShaderTexturedShape);

    GLTextureData* textureData = (GLTextureData*)texture->DriverData;
    glUniformMatrix4fv(D3DRenderer::D3D_CurrentShader->LocModelViewMatrix, 1, false, D3DRenderer::Graphics::ModelViewMatrix->Values);

    glActiveTexture(GL_TEXTURE0);
    glUniform1i(D3DRenderer::D3D_CurrentShader->LocTexture, 0);
    glBindTexture(GL_TEXTURE_2D, textureData->TextureID);

    glEnableVertexAttribArray(D3DRenderer::D3D_CurrentShader->LocTexCoord);

        glBindBuffer(GL_ARRAY_BUFFER, bufferID);
        glVertexAttribPointer(D3DRenderer::D3D_CurrentShader->LocPosition, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (GLvoid*)0);
        glVertexAttribPointer(D3DRenderer::D3D_CurrentShader->LocTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (GLvoid*)12);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);

    glDisableVertexAttribArray(D3DRenderer::D3D_CurrentShader->LocTexCoord);
}
//*/

#endif
