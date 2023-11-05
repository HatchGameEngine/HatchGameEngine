#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>

#include <Engine/Application.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/ResourceTypes/IModel.h>
#include <Engine/Scene/SceneLayer.h>
#include <Engine/Scene/View.h>
#include <Engine/Includes/HashMap.h>
#include <Engine/Rendering/Enums.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/GraphicsFunctions.h>
#include <Engine/Rendering/Scene3D.h>
#include <Engine/Rendering/VertexBuffer.h>
#include <Engine/Utilities/ColorUtils.h>

need_t ISprite;
need_t IModel;

class Graphics {
public:
    static HashMap<Texture*>*   TextureMap;
    static HashMap<Texture*>*   SpriteSheetTextureMap;
    static bool                 VsyncEnabled;
    static int                  MultisamplingEnabled;
    static int                  FontDPI;
    static bool                 SupportsBatching;
    static bool                 TextureBlend;
    static bool                 TextureInterpolate;
    static Uint32               PreferredPixelFormat;

    static Uint32               MaxTextureWidth;
    static Uint32               MaxTextureHeight;
    static Texture*             TextureHead;

    static VertexBuffer*        VertexBuffers[MAX_VERTEX_BUFFERS];
    static Scene3D              Scene3Ds[MAX_3D_SCENES];

    static stack<GraphicsState> StateStack;
    static stack<Matrix4x4*>    MatrixStack;

    static Matrix4x4*           ModelViewMatrix;

    static Viewport             CurrentViewport;
    static Viewport             BackupViewport;
    static ClipArea             CurrentClip;
    static ClipArea             BackupClip;

    static View*                CurrentView;

    static float                BlendColors[4];
    static float                TintColors[4];

    static int                  BlendMode;
    static int                  TintMode;

    static int                  StencilTest;
    static int                  StencilOpPass;
    static int                  StencilOpFail;

    static void*                FramebufferPixels;
    static size_t               FramebufferSize;

    static Uint32               PaletteColors[MAX_PALETTE_COUNT][0x100];
    static Uint8                PaletteIndexLines[MAX_FRAMEBUFFER_HEIGHT];
    static bool                 PaletteUpdated;

    static Texture*             PaletteTexture;

    static Texture*             CurrentRenderTarget;
    static Sint32               CurrentScene3D;
    static Sint32               CurrentVertexBuffer;

    static void*                CurrentShader;
    static bool                 SmoothFill;
    static bool                 SmoothStroke;

    static float                PixelOffset;
    static bool                 NoInternalTextures;
    static bool                 UsePalettes;
    static bool                 UseTinting;
    static bool                 UseDepthTesting;
    static bool                 UseSoftwareRenderer;

    static unsigned             CurrentFrame;

    // Rendering functions
    static GraphicsFunctions    Internal;
    static GraphicsFunctions*   GfxFunctions;
    static const char*          Renderer;
};
#endif

#include <Engine/Graphics.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Math/Math.h>

#include <Engine/Rendering/Software/SoftwareRenderer.h>
#ifdef USING_OPENGL
    #include <Engine/Rendering/GL/GLRenderer.h>
#endif
#ifdef USING_DIRECT3D
    #include <Engine/Rendering/D3D/D3DRenderer.h>
#endif
#include <Engine/Rendering/SDL2/SDL2Renderer.h>

#include <Engine/Bytecode/BytecodeObjectManager.h>

HashMap<Texture*>*   Graphics::TextureMap = NULL;
HashMap<Texture*>*   Graphics::SpriteSheetTextureMap = NULL;
bool                 Graphics::VsyncEnabled = true;
int                  Graphics::MultisamplingEnabled = 0;
int                  Graphics::FontDPI = 1;
bool                 Graphics::SupportsBatching = false;
bool                 Graphics::TextureBlend = false;
bool                 Graphics::TextureInterpolate = false;
Uint32               Graphics::PreferredPixelFormat = SDL_PIXELFORMAT_ARGB8888;
Uint32               Graphics::MaxTextureWidth = 1;
Uint32               Graphics::MaxTextureHeight = 1;
Texture*             Graphics::TextureHead = NULL;

VertexBuffer*        Graphics::VertexBuffers[MAX_VERTEX_BUFFERS];
Scene3D              Graphics::Scene3Ds[MAX_3D_SCENES];

stack<GraphicsState> Graphics::StateStack;
stack<Matrix4x4*>    Graphics::MatrixStack;

Matrix4x4*           Graphics::ModelViewMatrix;

Viewport             Graphics::CurrentViewport;
Viewport             Graphics::BackupViewport;
ClipArea             Graphics::CurrentClip;
ClipArea             Graphics::BackupClip;

View*                Graphics::CurrentView = NULL;

float                Graphics::BlendColors[4];
float                Graphics::TintColors[4];

int                  Graphics::BlendMode = BlendMode_NORMAL;
int                  Graphics::TintMode = TintMode_SRC_NORMAL;

int                  Graphics::StencilTest = StencilTest_Always;
int                  Graphics::StencilOpPass = StencilOp_Keep;
int                  Graphics::StencilOpFail = StencilOp_Keep;

void*                Graphics::FramebufferPixels = NULL;
size_t               Graphics::FramebufferSize = 0;

Uint32               Graphics::PaletteColors[MAX_PALETTE_COUNT][0x100];
Uint8                Graphics::PaletteIndexLines[MAX_FRAMEBUFFER_HEIGHT];
bool                 Graphics::PaletteUpdated = false;

Texture*             Graphics::PaletteTexture = NULL;

Texture*             Graphics::CurrentRenderTarget = NULL;
Sint32               Graphics::CurrentScene3D = -1;
Sint32               Graphics::CurrentVertexBuffer = -1;

void*                Graphics::CurrentShader = NULL;
bool                 Graphics::SmoothFill = false;
bool                 Graphics::SmoothStroke = false;

float                Graphics::PixelOffset = 0.0f;
bool                 Graphics::NoInternalTextures = false;
bool                 Graphics::UsePalettes = false;
bool                 Graphics::UseTinting = false;
bool                 Graphics::UseDepthTesting = false;
bool                 Graphics::UseSoftwareRenderer = false;

unsigned             Graphics::CurrentFrame = 0;

GraphicsFunctions    Graphics::Internal;
GraphicsFunctions*   Graphics::GfxFunctions = &Graphics::Internal;
const char*          Graphics::Renderer = "default";

PUBLIC STATIC void     Graphics::Init() {
    Graphics::TextureMap = new HashMap<Texture*>(NULL, 32);
    Graphics::SpriteSheetTextureMap = new HashMap<Texture*>(NULL, 32);

    Graphics::ModelViewMatrix = Matrix4x4::Create();
    Graphics::MatrixStack.push(Graphics::ModelViewMatrix);

    Graphics::CurrentClip.Enabled = false;
    Graphics::BackupClip.Enabled = false;

    Graphics::Reset();

    int w, h;
    SDL_GetWindowSize(Application::Window, &w, &h);

    Viewport* vp = &Graphics::CurrentViewport;
    vp->X =
    vp->Y = 0.0f;
    vp->Width = w;
    vp->Height = h;
    Graphics::BackupViewport = Graphics::CurrentViewport;

    Graphics::GfxFunctions->Init();

    Log::Print(Log::LOG_VERBOSE, "Window Size: %d x %d", w, h);
    Log::Print(Log::LOG_INFO, "Window Pixel Format: %s", SDL_GetPixelFormatName(SDL_GetWindowPixelFormat(Application::Window)));
    Log::Print(Log::LOG_INFO, "VSync: %s", Graphics::VsyncEnabled ? "true" : "false");
    if (Graphics::MultisamplingEnabled)
        Log::Print(Log::LOG_VERBOSE, "Multisample: %d", Graphics::MultisamplingEnabled);
    else
        Log::Print(Log::LOG_VERBOSE, "Multisample: false");
    Log::Print(Log::LOG_VERBOSE, "Max Texture Size: %d x %d", Graphics::MaxTextureWidth, Graphics::MaxTextureHeight);
}
PUBLIC STATIC void     Graphics::ChooseBackend() {
    char renderer[64];

    SoftwareRenderer::SetGraphicsFunctions();

    // Set renderers
    Graphics::Renderer = NULL;
    if (Application::Settings->GetString("dev", "renderer", renderer, sizeof renderer)) {
        #ifdef USING_OPENGL
            if (!strcmp(renderer, "opengl")) {
                Graphics::Renderer = "opengl";
                GLRenderer::SetGraphicsFunctions();
                return;
            }
        #endif
        #ifdef USING_DIRECT3D
            if (!strcmp(renderer, "direct3d")) {
                Graphics::Renderer = "direct3d";
                D3DRenderer::SetGraphicsFunctions();
                return;
            }
        #endif
            if (!strcmp(renderer, "sdl2")) {
                Graphics::Renderer = "sdl2";
                SDL2Renderer::SetGraphicsFunctions();
                return;
            }
        if (!Graphics::Renderer) {
            Log::Print(Log::LOG_WARN, "Could not find renderer \"%s\" on this platform!", renderer);
        }
    }

    // Compiler-directed renderer
    #ifdef USING_DIRECT3D
        if (!Graphics::Renderer) {
            Graphics::Renderer = "direct3d";
            D3DRenderer::SetGraphicsFunctions();
        }
    #endif
    #ifdef USING_OPENGL
        if (!Graphics::Renderer) {
            Graphics::Renderer = "opengl";
            GLRenderer::SetGraphicsFunctions();
        }
    #endif
    #ifdef USING_METAL
        if (!Graphics::Renderer) {
            Graphics::Renderer = "metal";
            MetalRenderer::SetGraphicsFunctions();
        }
    #endif

    if (!Graphics::Renderer) {
        Graphics::Renderer = "sdl2";
        SDL2Renderer::SetGraphicsFunctions();
    }
}
PUBLIC STATIC Uint32   Graphics::GetWindowFlags() {
    return Graphics::GfxFunctions->GetWindowFlags();
}
PUBLIC STATIC void     Graphics::SetVSync(bool enabled) {
    Graphics::GfxFunctions->SetVSync(enabled);
}
PUBLIC STATIC void     Graphics::Reset() {
    Graphics::UseSoftwareRenderer = false;
    Graphics::UsePalettes = false;

    Graphics::BlendColors[0] =
    Graphics::BlendColors[1] =
    Graphics::BlendColors[2] =
    Graphics::BlendColors[3] = 1.0;

    Graphics::TintColors[0] =
    Graphics::TintColors[1] =
    Graphics::TintColors[2] =
    Graphics::TintColors[3] = 1.0;

    for (int p = 0; p < MAX_PALETTE_COUNT; p++) {
        for (int c = 0; c < 0x100; c++) {
            Graphics::PaletteColors[p][c]  = 0xFF000000U;
            Graphics::PaletteColors[p][c] |= (c & 0x07) << 5; // Red?
            Graphics::PaletteColors[p][c] |= (c & 0x18) << 11; // Green?
            Graphics::PaletteColors[p][c] |= (c & 0xE0) << 16; // Blue?
        }
    }
    memset(Graphics::PaletteIndexLines, 0, sizeof(Graphics::PaletteIndexLines));
    Graphics::PaletteUpdated = true;

    Graphics::StencilTest = StencilTest_Always;
    Graphics::StencilOpPass = StencilOp_Keep;
    Graphics::StencilOpFail = StencilOp_Keep;
}
PUBLIC STATIC void     Graphics::Dispose() {
    for (Uint32 i = 0; i < MAX_VERTEX_BUFFERS; i++)
        Graphics::DeleteVertexBuffer(i);

    for (Uint32 i = 0; i < MAX_3D_SCENES; i++)
        Graphics::DeleteScene3D(i);

    for (Texture* texture = Graphics::TextureHead, *next; texture != NULL; texture = next) {
        next = texture->Next;
        Graphics::DisposeTexture(texture);
    }
    Graphics::TextureHead = NULL;
    Graphics::PaletteTexture = NULL;

    Graphics::SpriteSheetTextureMap->Clear();

    Graphics::GfxFunctions->Dispose();

    delete Graphics::TextureMap;
    delete Graphics::SpriteSheetTextureMap;
    while (Graphics::MatrixStack.size()) {
        delete Graphics::MatrixStack.top();
        Graphics::MatrixStack.pop();
    }

    if (Graphics::FramebufferPixels)
        Memory::Free(Graphics::FramebufferPixels);
}

PUBLIC STATIC Point    Graphics::ProjectToScreen(float x, float y, float z) {
    Point p;
    float vec4[4];
    Matrix4x4* matrix;

    matrix = Graphics::MatrixStack.top();

    vec4[0] = x; vec4[1] = y; vec4[2] = z; vec4[3] = 1.0f;
    Matrix4x4::Multiply(matrix, &vec4[0]);

    p.X = (vec4[0] + 1.0f) / 2.0f * Graphics::CurrentViewport.Width;
    p.Y = (vec4[1] - 1.0f) / -2.0f * Graphics::CurrentViewport.Height;
    p.Z = vec4[2];
    return p;
}

PUBLIC STATIC Texture* Graphics::CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height) {
    Texture* texture;
    if (Graphics::GfxFunctions == &SoftwareRenderer::BackendFunctions ||
        Graphics::NoInternalTextures) {
        texture = Texture::New(format, access, width, height);
        if (!texture)
            return NULL;
    }
    else {
        texture = Graphics::GfxFunctions->CreateTexture(format, access, width, height);
        if (!texture)
            return NULL;
    }

    texture->Next = Graphics::TextureHead;
    if (Graphics::TextureHead)
        Graphics::TextureHead->Prev = texture;

    Graphics::TextureHead = texture;

    return texture;
}
PUBLIC STATIC Texture* Graphics::CreateTextureFromPixels(Uint32 width, Uint32 height, void* pixels, int pitch) {
    Texture* texture = Graphics::CreateTexture(SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);
    if (texture)
        Graphics::UpdateTexture(texture, NULL, pixels, pitch);

    return texture;
}
PUBLIC STATIC Texture* Graphics::CreateTextureFromSurface(SDL_Surface* surface) {
    Texture* texture = Graphics::CreateTexture(surface->format->format, SDL_TEXTUREACCESS_STATIC, surface->w, surface->h);
    if (texture)
        Graphics::GfxFunctions->UpdateTexture(texture, NULL, surface->pixels, surface->pitch);
    return texture;
}
PUBLIC STATIC int      Graphics::LockTexture(Texture* texture, void** pixels, int* pitch) {
    if (Graphics::GfxFunctions == &SoftwareRenderer::BackendFunctions ||
        Graphics::NoInternalTextures)
        return 1;
    return Graphics::GfxFunctions->LockTexture(texture, pixels, pitch);
}
PUBLIC STATIC int      Graphics::UpdateTexture(Texture* texture, SDL_Rect* src, void* pixels, int pitch) {
    memcpy(texture->Pixels, pixels, sizeof(Uint32) * texture->Width * texture->Height);
    if (Graphics::GfxFunctions == &SoftwareRenderer::BackendFunctions ||
        Graphics::NoInternalTextures)
        return 1;
    return Graphics::GfxFunctions->UpdateTexture(texture, src, pixels, pitch);
}
PUBLIC STATIC int      Graphics::UpdateYUVTexture(Texture* texture, SDL_Rect* src, Uint8* pixelsY, int pitchY, Uint8* pixelsU, int pitchU, Uint8* pixelsV, int pitchV) {
    if (!Graphics::GfxFunctions->UpdateYUVTexture)
        return 0;
    if (Graphics::GfxFunctions == &SoftwareRenderer::BackendFunctions ||
        Graphics::NoInternalTextures)
        return 1;
    return Graphics::GfxFunctions->UpdateYUVTexture(texture, src, pixelsY, pitchY, pixelsU, pitchU, pixelsV, pitchV);
}
PUBLIC STATIC int      Graphics::SetTexturePalette(Texture* texture, void* palette, unsigned numPaletteColors) {
    texture->SetPalette((Uint32*)palette, numPaletteColors);
    if (Graphics::GfxFunctions == &SoftwareRenderer::BackendFunctions ||
        !Graphics::GfxFunctions->SetTexturePalette || Graphics::NoInternalTextures)
        return 1;
    return Graphics::GfxFunctions->SetTexturePalette(texture, palette, numPaletteColors);
}
PUBLIC STATIC void     Graphics::UnlockTexture(Texture* texture) {
    Graphics::GfxFunctions->UnlockTexture(texture);
}
PUBLIC STATIC void     Graphics::DisposeTexture(Texture* texture) {
    Graphics::GfxFunctions->DisposeTexture(texture);

    if (texture->Next)
        texture->Next->Prev = texture->Prev;

    if (texture->Prev)
        texture->Prev->Next = texture->Next;
    else
        Graphics::TextureHead = texture->Next;

    texture->Dispose();

    Memory::Free(texture);
}

PUBLIC STATIC Uint32   Graphics::CreateVertexBuffer(Uint32 maxVertices, int unloadPolicy) {
    for (Uint32 i = 0; i < MAX_VERTEX_BUFFERS; i++) {
        if (Graphics::VertexBuffers[i] == NULL) {
            VertexBuffer* vtxbuf;
            if (Graphics::Internal.CreateVertexBuffer)
                vtxbuf = (VertexBuffer*)Graphics::Internal.CreateVertexBuffer(maxVertices);
            else
                vtxbuf = new VertexBuffer(maxVertices);

            vtxbuf->UnloadPolicy = unloadPolicy;
            Graphics::VertexBuffers[i] = vtxbuf;
            return i;
        }
    }

    return 0xFFFFFFFF;
}
PUBLIC STATIC void     Graphics::DeleteVertexBuffer(Uint32 vertexBufferIndex) {
    if (vertexBufferIndex < 0 || vertexBufferIndex >= MAX_3D_SCENES)
        return;
    if (!Graphics::VertexBuffers[vertexBufferIndex])
        return;

    if (Graphics::Internal.DeleteVertexBuffer)
        Graphics::Internal.DeleteVertexBuffer(Graphics::VertexBuffers[vertexBufferIndex]);
    else
        delete Graphics::VertexBuffers[vertexBufferIndex];
    Graphics::VertexBuffers[vertexBufferIndex] = NULL;
}

PUBLIC STATIC void     Graphics::UseShader(void* shader) {
    Graphics::CurrentShader = shader;
    Graphics::GfxFunctions->UseShader(shader);
}
PUBLIC STATIC void     Graphics::SetTextureInterpolation(bool interpolate) {
    Graphics::TextureInterpolate = interpolate;
}

PUBLIC STATIC void     Graphics::Clear() {
    Graphics::GfxFunctions->Clear();
}
PUBLIC STATIC void     Graphics::Present() {
    Graphics::GfxFunctions->Present();
    Graphics::CurrentFrame++;
}

PUBLIC STATIC void     Graphics::SoftwareStart() {
    Graphics::GfxFunctions = &SoftwareRenderer::BackendFunctions;
    SoftwareRenderer::RenderStart();
}
PUBLIC STATIC void     Graphics::SoftwareEnd() {
    SoftwareRenderer::RenderEnd();
    Graphics::GfxFunctions = &Graphics::Internal;
    Graphics::UpdateTexture(Graphics::CurrentRenderTarget, NULL, Graphics::CurrentRenderTarget->Pixels, Graphics::CurrentRenderTarget->Width * 4);
}

PUBLIC STATIC void     Graphics::UpdateGlobalPalette() {
    if (!Graphics::GfxFunctions->UpdateGlobalPalette)
        return;

    if (Graphics::PaletteTexture == NULL)
        Graphics::PaletteTexture = Graphics::CreateTexture(SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 0x100, MAX_PALETTE_COUNT);

    if (Graphics::PaletteTexture == NULL) {
        Log::Print(Log::LOG_ERROR, "Couldn't create palette texture!");
        abort();
    }

    Graphics::UpdateTexture(Graphics::PaletteTexture, NULL, (Uint32*)Graphics::PaletteColors, 0x100 * 4);

    Graphics::GfxFunctions->UpdateGlobalPalette();
}

PUBLIC STATIC void     Graphics::UnloadSceneData() {
    for (Uint32 i = 0; i < MAX_VERTEX_BUFFERS; i++) {
        VertexBuffer* buffer = Graphics::VertexBuffers[i];
        if (!buffer || buffer->UnloadPolicy > SCOPE_SCENE)
            continue;

        Graphics::DeleteVertexBuffer(i);
    }

    for (Uint32 i = 0; i < MAX_3D_SCENES; i++) {
        Scene3D* scene = &Graphics::Scene3Ds[i];
        if (!scene->Initialized || scene->UnloadPolicy > SCOPE_SCENE)
            continue;

        Graphics::DeleteScene3D(i);
    }
}

PUBLIC STATIC void     Graphics::SetRenderTarget(Texture* texture) {
    if (texture && !Graphics::CurrentRenderTarget) {
        Graphics::BackupViewport = Graphics::CurrentViewport;
        Graphics::BackupClip = Graphics::CurrentClip;
    }

    Graphics::CurrentRenderTarget = texture;

    Graphics::GfxFunctions->SetRenderTarget(texture);

    Viewport* vp = &Graphics::CurrentViewport;
    if (texture) {
        vp->X = 0.0;
        vp->Y = 0.0;
        vp->Width = texture->Width;
        vp->Height = texture->Height;
    }
    else {
        Graphics::CurrentViewport = Graphics::BackupViewport;
        Graphics::CurrentClip = Graphics::BackupClip;
    }

    Graphics::GfxFunctions->UpdateViewport();
    Graphics::GfxFunctions->UpdateClipRect();
}
PUBLIC STATIC void     Graphics::CopyScreen(int source_x, int source_y, int source_w, int source_h, int dest_x, int dest_y, int dest_w, int dest_h, Texture* texture) {
    if (!Graphics::GfxFunctions->ReadFramebuffer)
        return;

    if (source_x == 0 && source_y == 0 && dest_x == 0 && dest_y == 0 && dest_w == source_w && dest_h == source_h) {
        Graphics::GfxFunctions->ReadFramebuffer(texture->Pixels, dest_w, dest_h);
    }
    else {
        size_t sz = source_w * source_h * 4;

        if (Graphics::FramebufferPixels == NULL)
            Graphics::FramebufferPixels = (Uint32*)Memory::TrackedCalloc("Graphics::FramebufferPixels", 1, sz);
        else if (sz != Graphics::FramebufferSize)
            Graphics::FramebufferPixels = (Uint32*)Memory::Realloc(Graphics::FramebufferPixels, sz);

        Graphics::FramebufferSize = sz;

        Graphics::GfxFunctions->ReadFramebuffer(Graphics::FramebufferPixels, source_w, source_h);

        Uint32 *d = (Uint32*)texture->Pixels;
        Uint32 *s = (Uint32*)Graphics::FramebufferPixels;

        int xs = FP16_DIVIDE(0x10000, FP16_DIVIDE(dest_w << 16, source_w << 16));
        int ys = FP16_DIVIDE(0x10000, FP16_DIVIDE(dest_h << 16, source_h << 16));

        for (int read_y = source_y << 16, write_y = dest_y; write_y < dest_h; write_y++, read_y += ys) {
            int isy = read_y >> 16;
            if (isy >= source_h)
                return;
            for (int read_x = source_x << 16, write_x = dest_x; write_x < dest_w; write_x++, read_x += xs) {
                int isx = read_x >> 16;
                if (isx >= source_w)
                    break;
                d[(write_y * texture->Width) + write_x] = s[(isy * source_w) + isx];
            }
        }
    }
}
PUBLIC STATIC void     Graphics::UpdateOrtho(float width, float height) {
    Graphics::GfxFunctions->UpdateOrtho(0.0f, 0.0f, width, height);
}
PUBLIC STATIC void     Graphics::UpdateOrthoFlipped(float width, float height) {
    Graphics::GfxFunctions->UpdateOrtho(0.0f, height, width, 0.0f);
}
PUBLIC STATIC void     Graphics::UpdatePerspective(float fovy, float aspect, float nearv, float farv) {
    Graphics::GfxFunctions->UpdatePerspective(fovy, aspect, nearv, farv);
}
PUBLIC STATIC void     Graphics::UpdateProjectionMatrix() {
    Graphics::GfxFunctions->UpdateProjectionMatrix();
}
PUBLIC STATIC void     Graphics::MakePerspectiveMatrix(Matrix4x4* out, float fov, float near, float far, float aspect) {
    Graphics::GfxFunctions->MakePerspectiveMatrix(out, fov, near, far, aspect);
}
PUBLIC STATIC void     Graphics::CalculateMVPMatrix(Matrix4x4* output, Matrix4x4* modelMatrix, Matrix4x4* viewMatrix, Matrix4x4* projMatrix) {
    if (viewMatrix && projMatrix) {
        Matrix4x4 modelView;

        if (modelMatrix) {
            Matrix4x4::Multiply(&modelView, modelMatrix, viewMatrix);
            Matrix4x4::Multiply(output, &modelView, projMatrix);
        }
        else
            Matrix4x4::Multiply(output, viewMatrix, projMatrix);
    }
    else if (modelMatrix)
        *output = *modelMatrix;
    else
        Matrix4x4::Identity(output);
}
PUBLIC STATIC void     Graphics::SetViewport(float x, float y, float w, float h) {
    Viewport* vp = &Graphics::CurrentViewport;
    if (x < 0) {
        vp->X = 0.0;
        vp->Y = 0.0;
        if (Graphics::CurrentRenderTarget) {
            vp->Width = Graphics::CurrentRenderTarget->Width;
            vp->Height = Graphics::CurrentRenderTarget->Height;
        }
        else {
            int w, h;
            SDL_GetWindowSize(Application::Window, &w, &h);
            vp->Width = w;
            vp->Height = h;
        }
    }
    else {
        vp->X = x;
        vp->Y = y;
        vp->Width = w;
        vp->Height = h;
    }
    Graphics::GfxFunctions->UpdateViewport();
}
PUBLIC STATIC void     Graphics::ResetViewport() {
    Graphics::SetViewport(-1.0, 0.0, 0.0, 0.0);
}
PUBLIC STATIC void     Graphics::Resize(int width, int height) {
    Graphics::GfxFunctions->UpdateWindowSize(width, height);
}

PUBLIC STATIC void     Graphics::SetClip(int x, int y, int width, int height) {
    Graphics::CurrentClip.Enabled = true;
    Graphics::CurrentClip.X = x;
    Graphics::CurrentClip.Y = y;
    Graphics::CurrentClip.Width = width;
    Graphics::CurrentClip.Height = height;
    Graphics::GfxFunctions->UpdateClipRect();
}
PUBLIC STATIC void     Graphics::ClearClip() {
    Graphics::CurrentClip.Enabled = false;
    Graphics::GfxFunctions->UpdateClipRect();
}

PUBLIC STATIC void     Graphics::Save() {
    Matrix4x4* top = MatrixStack.top();
    Matrix4x4* push = Matrix4x4::Create();
    Matrix4x4::Copy(push, top);

    if (MatrixStack.size() == 256) {
        Log::Print(Log::LOG_ERROR, "Draw.Save stack too big!");
        exit(-1);
    }

    MatrixStack.push(push);
    ModelViewMatrix = push;
}
PUBLIC STATIC void     Graphics::Translate(float x, float y, float z) {
    Matrix4x4::Translate(ModelViewMatrix, ModelViewMatrix, x, y, z);
}
PUBLIC STATIC void     Graphics::Rotate(float x, float y, float z) {
    Matrix4x4::Rotate(ModelViewMatrix, ModelViewMatrix, x, 1.0, 0.0, 0.0);
    Matrix4x4::Rotate(ModelViewMatrix, ModelViewMatrix, y, 0.0, 1.0, 0.0);
    Matrix4x4::Rotate(ModelViewMatrix, ModelViewMatrix, z, 0.0, 0.0, 1.0);
}
PUBLIC STATIC void     Graphics::Scale(float x, float y, float z) {
    Matrix4x4::Scale(ModelViewMatrix, ModelViewMatrix, x, y, z);
}
PUBLIC STATIC void     Graphics::Restore() {
    if (MatrixStack.size() == 1) return;

    delete MatrixStack.top();
    MatrixStack.pop();

    ModelViewMatrix = MatrixStack.top();
}

PUBLIC STATIC BlendState Graphics::GetBlendState() {
    BlendState state;
    state.Opacity = (int)(Graphics::BlendColors[3] * 0xFF);
    state.Mode = Graphics::BlendMode;
    state.Tint.Enabled = Graphics::UseTinting;
    state.Tint.Color = ColorUtils::ToRGB(Graphics::TintColors);
    state.Tint.Amount = (int)(Graphics::TintColors[3] * 0xFF);
    state.Tint.Mode = Graphics::TintMode;
    state.FilterTable = NULL;
    return state;
}

PUBLIC STATIC void     Graphics::PushState() {
    if (StateStack.size() == 256) {
        Log::Print(Log::LOG_ERROR, "Graphics::PushState stack too big!");
        exit(-1);
    }

    GraphicsState state;

    state.CurrentViewport = Graphics::CurrentViewport;
    state.CurrentClip     = Graphics::CurrentClip;
    state.BlendMode       = Graphics::BlendMode;
    state.TintMode        = Graphics::TintMode;
    state.TextureBlend    = Graphics::TextureBlend;
    state.UsePalettes     = Graphics::UsePalettes;
    state.UseTinting      = Graphics::UseTinting;
    state.UseDepthTesting = Graphics::UseDepthTesting;

    memcpy(state.BlendColors, Graphics::BlendColors, sizeof(Graphics::BlendColors));
    memcpy(state.TintColors, Graphics::TintColors, sizeof(Graphics::TintColors));

    StateStack.push(state);

    Graphics::Save();
}
PUBLIC STATIC void     Graphics::PopState() {
    if (StateStack.size() == 0) return;

    GraphicsState state = StateStack.top();

    Graphics::SetBlendMode(state.BlendMode);
    Graphics::SetBlendColor(state.BlendColors[0], state.BlendColors[1], state.BlendColors[2], state.BlendColors[3]);

    Graphics::SetTintEnabled(state.UseTinting);
    Graphics::SetTintMode(state.TintMode);
    Graphics::SetTintColor(state.TintColors[0], state.TintColors[1], state.TintColors[2], state.TintColors[3]);

    Graphics::CurrentViewport = state.CurrentViewport;
    Graphics::CurrentClip     = state.CurrentClip;
    Graphics::TextureBlend    = state.TextureBlend;
    Graphics::UsePalettes     = state.UsePalettes;
    Graphics::UseDepthTesting = state.UseDepthTesting;

    Graphics::SetDepthTesting(Graphics::UseDepthTesting);

    Graphics::GfxFunctions->UpdateViewport();
    Graphics::GfxFunctions->UpdateClipRect();

    StateStack.pop();

    Graphics::Restore();
}

PUBLIC STATIC void     Graphics::SetBlendColor(float r, float g, float b, float a) {
    Graphics::BlendColors[0] = Math::Clamp(r, 0.0f, 1.0f);
    Graphics::BlendColors[1] = Math::Clamp(g, 0.0f, 1.0f);
    Graphics::BlendColors[2] = Math::Clamp(b, 0.0f, 1.0f);
    Graphics::BlendColors[3] = Math::Clamp(a, 0.0f, 1.0f);
    Graphics::GfxFunctions->SetBlendColor(r, g, b, a);
}
PUBLIC STATIC void     Graphics::SetBlendMode(int blendMode) {
    Graphics::BlendMode = blendMode;
    switch (blendMode) {
        case BlendMode_NORMAL:
            Graphics::SetBlendMode(
                BlendFactor_SRC_ALPHA, BlendFactor_INV_SRC_ALPHA,
                BlendFactor_SRC_ALPHA, BlendFactor_INV_SRC_ALPHA);
            break;
        case BlendMode_ADD:
            Graphics::SetBlendMode(
                BlendFactor_SRC_ALPHA, BlendFactor_ONE,
                BlendFactor_SRC_ALPHA, BlendFactor_ONE);
            break;
        case BlendMode_MAX:
            Graphics::SetBlendMode(
                BlendFactor_SRC_ALPHA, BlendFactor_INV_SRC_COLOR,
                BlendFactor_SRC_ALPHA, BlendFactor_INV_SRC_COLOR);
            break;
        case BlendMode_SUBTRACT:
            Graphics::SetBlendMode(
                BlendFactor_ZERO, BlendFactor_INV_SRC_COLOR,
                BlendFactor_SRC_ALPHA, BlendFactor_INV_SRC_ALPHA);
            break;
        default:
            Graphics::SetBlendMode(
                BlendFactor_SRC_ALPHA, BlendFactor_INV_SRC_ALPHA,
                BlendFactor_SRC_ALPHA, BlendFactor_INV_SRC_ALPHA);
    }
}
PUBLIC STATIC void     Graphics::SetBlendMode(int srcC, int dstC, int srcA, int dstA) {
    Graphics::GfxFunctions->SetBlendMode(srcC, dstC, srcA, dstA);
}
PUBLIC STATIC void     Graphics::SetTintColor(float r, float g, float b, float a) {
    Graphics::TintColors[0] = Math::Clamp(r, 0.0f, 1.0f);
    Graphics::TintColors[1] = Math::Clamp(g, 0.0f, 1.0f);
    Graphics::TintColors[2] = Math::Clamp(b, 0.0f, 1.0f);
    Graphics::TintColors[3] = Math::Clamp(a, 0.0f, 1.0f);
    Graphics::GfxFunctions->SetTintColor(r, g, b, a);
}
PUBLIC STATIC void     Graphics::SetTintMode(int mode) {
    Graphics::TintMode = mode;
    Graphics::GfxFunctions->SetTintMode(mode);
}
PUBLIC STATIC void     Graphics::SetTintEnabled(bool enabled) {
    Graphics::UseTinting = enabled;
    Graphics::GfxFunctions->SetTintEnabled(enabled);
}
PUBLIC STATIC void     Graphics::SetLineWidth(float n) {
    Graphics::GfxFunctions->SetLineWidth(n);
}

PUBLIC STATIC void     Graphics::StrokeLine(float x1, float y1, float x2, float y2) {
    Graphics::GfxFunctions->StrokeLine(x1, y1, x2, y2);
}
PUBLIC STATIC void     Graphics::StrokeCircle(float x, float y, float rad) {
    Graphics::GfxFunctions->StrokeCircle(x, y, rad);
}
PUBLIC STATIC void     Graphics::StrokeEllipse(float x, float y, float w, float h) {
    Graphics::GfxFunctions->StrokeEllipse(x, y, w, h);
}
PUBLIC STATIC void     Graphics::StrokeTriangle(float x1, float y1, float x2, float y2, float x3, float y3) {
    Graphics::GfxFunctions->StrokeLine(x1, y1, x2, y2);
    Graphics::GfxFunctions->StrokeLine(x2, y2, x3, y3);
    Graphics::GfxFunctions->StrokeLine(x3, y3, x1, y1);
}
PUBLIC STATIC void     Graphics::StrokeRectangle(float x, float y, float w, float h) {
    Graphics::GfxFunctions->StrokeRectangle(x, y, w, h);
}
PUBLIC STATIC void     Graphics::FillCircle(float x, float y, float rad) {
    Graphics::GfxFunctions->FillCircle(x, y, rad);
}
PUBLIC STATIC void     Graphics::FillEllipse(float x, float y, float w, float h) {
    Graphics::GfxFunctions->FillEllipse(x, y, w, h);
}
PUBLIC STATIC void     Graphics::FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3) {
    Graphics::GfxFunctions->FillTriangle(x1, y1, x2, y2, x3, y3);
}
PUBLIC STATIC void     Graphics::FillRectangle(float x, float y, float w, float h) {
    Graphics::GfxFunctions->FillRectangle(x, y, w, h);
}

PUBLIC STATIC void     Graphics::DrawTexture(Texture* texture, float sx, float sy, float sw, float sh, float x, float y, float w, float h) {
    Graphics::GfxFunctions->DrawTexture(texture, sx, sy, sw, sh, x, y, w, h);
}
PUBLIC STATIC void     Graphics::DrawSprite(ISprite* sprite, int animation, int frame, int x, int y, bool flipX, bool flipY, float scaleW, float scaleH, float rotation) {
    Graphics::GfxFunctions->DrawSprite(sprite, animation, frame, x, y, flipX, flipY, scaleW, scaleH, rotation);
}
PUBLIC STATIC void     Graphics::DrawSpritePart(ISprite* sprite, int animation, int frame, int sx, int sy, int sw, int sh, int x, int y, bool flipX, bool flipY, float scaleW, float scaleH, float rotation) {
    Graphics::GfxFunctions->DrawSpritePart(sprite, animation, frame, sx, sy, sw, sh, x, y, flipX, flipY, scaleW, scaleH, rotation);
}

PUBLIC STATIC void     Graphics::DrawTile(int tile, int x, int y, bool flipX, bool flipY) {
    // If possible, uses optimized software-renderer call instead.
    if (Graphics::GfxFunctions == &SoftwareRenderer::BackendFunctions) {
        SoftwareRenderer::DrawTile(tile, x, y, flipX, flipY);
        return;
    }

    TileSpriteInfo info = Scene::TileSpriteInfos[tile];
    Graphics::GfxFunctions->DrawSprite(info.Sprite, info.AnimationIndex, info.FrameIndex, x, y, flipX, flipY, 1.0f, 1.0f, 0.0f);
}
PUBLIC STATIC void     Graphics::DrawSceneLayer_HorizontalParallax(SceneLayer* layer, View* currentView) {
    int tileWidth = Scene::TileWidth;
    int tileWidthHalf = tileWidth >> 1;
    int tileHeight = Scene::TileHeight;
    int tileHeightHalf = tileHeight >> 1;
    int tileCellMaxWidth = 3 + (currentView->Width / tileWidth);
    int tileCellMaxHeight = 2 + (currentView->Height / tileHeight);

    int flipX, flipY, col;
    int TileBaseX, TileBaseY, baseX, baseY, tile, tileOrig, baseXOff, baseYOff;

    TileConfig* baseTileCfg = NULL;
    if (Scene::TileCfg.size()) {
        size_t collisionPlane = Scene::ShowTileCollisionFlag - 1;
        if (collisionPlane < Scene::TileCfg.size())
            baseTileCfg = Scene::TileCfg[collisionPlane];
    }

    if (layer->ScrollInfosSplitIndexes && layer->ScrollInfosSplitIndexesCount > 0) {
        int height, index;
        int ix, iy, sourceTileCellX, sourceTileCellY;

        // Vertical
        if (layer->DrawBehavior == DrawBehavior_VerticalParallax) {
            baseXOff = ((((int)currentView->X + layer->OffsetX) * layer->RelativeY) + Scene::Frame * layer->ConstantY) >> 8;
            TileBaseX = 0;

            for (int split = 0, spl; split < 4096; split++) {
                spl = split;
                while (spl >= layer->ScrollInfosSplitIndexesCount) spl -= layer->ScrollInfosSplitIndexesCount;

                height = (layer->ScrollInfosSplitIndexes[spl] >> 8) & 0xFF;

                sourceTileCellX = (TileBaseX >> 4);
                baseX = (sourceTileCellX << 4) + 8;
                baseX -= baseXOff;

                if (baseX - 8 + height < -tileWidth)
                    goto SKIP_TILE_ROW_DRAW_ROT90;
                if (baseX - 8 >= currentView->Width + tileWidth)
                    break;

                index = layer->ScrollInfosSplitIndexes[spl] & 0xFF;

                baseYOff = ((((int)currentView->Y + layer->OffsetY) * layer->ScrollInfos[index].RelativeParallax) + Scene::Frame * layer->ScrollInfos[index].ConstantParallax) >> 8;
                TileBaseY = baseYOff;

                // Loop or cut off sourceTileCellX
                if (layer->Flags & SceneLayer::FLAGS_NO_REPEAT_X) {
                    if (sourceTileCellX < 0) goto SKIP_TILE_ROW_DRAW_ROT90;
                    if (sourceTileCellX >= layer->Width) goto SKIP_TILE_ROW_DRAW_ROT90;
                }
                while (sourceTileCellX < 0) sourceTileCellX += layer->Width;
                while (sourceTileCellX >= layer->Width) sourceTileCellX -= layer->Width;

                // Draw row of tiles
                iy = TileBaseY >> 4;
                baseY = tileHeightHalf;
                baseY -= baseYOff & 15;

                // To get the leftmost tile, ix--, and start t = -1
                baseY -= 16;
                iy--;

                for (int t = 0; t < tileCellMaxHeight; t++) {
                    // Loop or cut off sourceTileCellX
                    sourceTileCellY = iy;
                    if (layer->Flags & SceneLayer::FLAGS_NO_REPEAT_Y) {
                        if (sourceTileCellY < 0) goto SKIP_TILE_DRAW_ROT90;
                        if (sourceTileCellY >= layer->Height) goto SKIP_TILE_DRAW_ROT90;
                    }
                    else {
                        // sourceTileCellY = ((sourceTileCellY % layer->Height) + layer->Height) % layer->Height;
                        while (sourceTileCellY < 0) sourceTileCellY += layer->Height;
                        while (sourceTileCellY >= layer->Height) sourceTileCellY -= layer->Height;
                    }

                    tileOrig = tile = layer->Tiles[sourceTileCellX + (sourceTileCellY << layer->WidthInBits)];

                    tile &= TILE_IDENT_MASK;
                    // "li == 0" should ideally be "layer->DrawGroup == 0", but in some games multiple layers will use DrawGroup == 0, which would look bad & lag
                    if (tile != Scene::EmptyTile) { // || li == 0
                        flipX = (tileOrig & TILE_FLIPX_MASK);
                        flipY = (tileOrig & TILE_FLIPY_MASK);

                        int partY = TileBaseX & 0xF;
                        if (flipX) partY = tileWidth - height - partY;

                        TileSpriteInfo info = Scene::TileSpriteInfos[tile];
                        Graphics::DrawSpritePart(info.Sprite, info.AnimationIndex, info.FrameIndex, partY, 0, height, tileWidth, baseX, baseY, flipX, flipY, 1.0f, 1.0f, 0.0f);

                        if (Scene::ShowTileCollisionFlag && baseTileCfg && layer->ScrollInfoCount <= 1) {
                            col = 0;
                            if (Scene::ShowTileCollisionFlag == 1)
                                col = (tileOrig & TILE_COLLA_MASK) >> 28;
                            else if (Scene::ShowTileCollisionFlag == 2)
                                col = (tileOrig & TILE_COLLB_MASK) >> 26;

                            switch (col) {
                                case 1:
                                    Graphics::SetBlendColor(1.0, 1.0, 0.0, 1.0);
                                    break;
                                case 2:
                                    Graphics::SetBlendColor(1.0, 0.0, 0.0, 1.0);
                                    break;
                                case 3:
                                    Graphics::SetBlendColor(1.0, 1.0, 1.0, 1.0);
                                    break;
                            }

                            int xorFlipX = 0;
                            if (flipX)
                                xorFlipX = tileWidth - 1;

                            if (baseTileCfg[tile].IsCeiling ^ flipY) {
                                for (int checkX = 0, realCheckX = 0; checkX < tileWidth; checkX++) {
                                    realCheckX = checkX ^ xorFlipX;
                                    if (baseTileCfg[tile].CollisionTop[realCheckX] < 0xF0) continue;

                                    Uint8 colH = baseTileCfg[tile].CollisionTop[realCheckX];
                                    Graphics::FillRectangle(baseX - 8 + checkX, baseY - 8, 1, tileHeight - colH);
                                }
                            }
                            else {
                                for (int checkX = 0, realCheckX = 0; checkX < tileWidth; checkX++) {
                                    realCheckX = checkX ^ xorFlipX;
                                    if (baseTileCfg[tile].CollisionTop[realCheckX] < 0xF0) continue;

                                    Uint8 colH = baseTileCfg[tile].CollisionBottom[realCheckX];
                                    Graphics::FillRectangle(baseX - 8 + checkX, baseY - 8 + colH, 1, tileHeight - colH);
                                }
                            }
                        }
                    }

                    SKIP_TILE_DRAW_ROT90:
                    iy++;
                    baseY += tileHeight;
                }

                SKIP_TILE_ROW_DRAW_ROT90:
                TileBaseX += height;
            }
        }
        // Horizontal
        else {
            baseYOff = ((((int)currentView->Y + layer->OffsetY) * layer->RelativeY) + Scene::Frame * layer->ConstantY) >> 8;
            TileBaseY = 0;

            for (int split = 0, spl; split < 4096; split++) {
                spl = split;
                while (spl >= layer->ScrollInfosSplitIndexesCount) spl -= layer->ScrollInfosSplitIndexesCount;

                height = (layer->ScrollInfosSplitIndexes[spl] >> 8) & 0xFF;

                sourceTileCellY = (TileBaseY >> 4);
                baseY = (sourceTileCellY << 4) + 8;
                baseY -= baseYOff;

                if (baseY - 8 + height < -tileHeight)
                    goto SKIP_TILE_ROW_DRAW;
                if (baseY - 8 >= currentView->Height + tileHeight)
                    break;

                index = layer->ScrollInfosSplitIndexes[spl] & 0xFF;
                baseXOff = ((((int)currentView->X + layer->OffsetX) * layer->ScrollInfos[index].RelativeParallax) + Scene::Frame * layer->ScrollInfos[index].ConstantParallax) >> 8;
                TileBaseX = baseXOff;

                // Loop or cut off sourceTileCellY
                if (layer->Flags & SceneLayer::FLAGS_NO_REPEAT_Y) {
                    if (sourceTileCellY < 0) goto SKIP_TILE_ROW_DRAW;
                    if (sourceTileCellY >= layer->Height) goto SKIP_TILE_ROW_DRAW;
                }
                while (sourceTileCellY < 0) sourceTileCellY += layer->Height;
                while (sourceTileCellY >= layer->Height) sourceTileCellY -= layer->Height;

                // Draw row of tiles
                ix = TileBaseX >> 4;
                baseX = tileWidthHalf;
                baseX -= baseXOff & 15;

                // To get the leftmost tile, ix--, and start t = -1
                baseX -= 16;
                ix--;

                // sourceTileCellX = ((sourceTileCellX % layer->Width) + layer->Width) % layer->Width;
                for (int t = 0; t < tileCellMaxWidth; t++) {
                    // Loop or cut off sourceTileCellX
                    sourceTileCellX = ix;
                    if (layer->Flags & SceneLayer::FLAGS_NO_REPEAT_X) {
                        if (sourceTileCellX < 0) goto SKIP_TILE_DRAW;
                        if (sourceTileCellX >= layer->Width) goto SKIP_TILE_DRAW;
                    }
                    else {
                        // sourceTileCellX = ((sourceTileCellX % layer->Width) + layer->Width) % layer->Width;
                        while (sourceTileCellX < 0) sourceTileCellX += layer->Width;
                        while (sourceTileCellX >= layer->Width) sourceTileCellX -= layer->Width;
                    }

                    tileOrig = tile = layer->Tiles[sourceTileCellX + (sourceTileCellY << layer->WidthInBits)];

                    tile &= TILE_IDENT_MASK;
                    // "li == 0" should ideally be "layer->DrawGroup == 0", but in some games multiple layers will use DrawGroup == 0, which would look bad & lag
                    if (tile != Scene::EmptyTile) { // || li == 0
                        flipX = !!(tileOrig & TILE_FLIPX_MASK);
                        flipY = !!(tileOrig & TILE_FLIPY_MASK);

                        int partY = TileBaseY & 0xF;
                        if (flipY) partY = tileHeight - height - partY;

                        TileSpriteInfo info = Scene::TileSpriteInfos[tile];
                        Graphics::DrawSpritePart(info.Sprite, info.AnimationIndex, info.FrameIndex, 0, partY, tileWidth, height, baseX, baseY, flipX, flipY, 1.0f, 1.0f, 0.0f);

                        if (Scene::ShowTileCollisionFlag && baseTileCfg && layer->ScrollInfoCount <= 1) {
                            col = 0;
                            if (Scene::ShowTileCollisionFlag == 1)
                                col = (tileOrig & TILE_COLLA_MASK) >> 28;
                            else if (Scene::ShowTileCollisionFlag == 2)
                                col = (tileOrig & TILE_COLLB_MASK) >> 26;

                            switch (col) {
                                case 1:
                                    Graphics::SetBlendColor(1.0, 1.0, 0.0, 1.0);
                                    break;
                                case 2:
                                    Graphics::SetBlendColor(1.0, 0.0, 0.0, 1.0);
                                    break;
                                case 3:
                                    Graphics::SetBlendColor(1.0, 1.0, 1.0, 1.0);
                                    break;
                            }

                            int xorFlipX = 0;
                            if (flipX)
                                xorFlipX = tileWidth - 1;

                            if (baseTileCfg[tile].IsCeiling ^ flipY) {
                                for (int checkX = 0, realCheckX = 0; checkX < tileWidth; checkX++) {
                                    realCheckX = checkX ^ xorFlipX;
                                    if (baseTileCfg[tile].CollisionTop[realCheckX] < 0xF0) continue;

                                    Uint8 colH = baseTileCfg[tile].CollisionTop[realCheckX];
                                    Graphics::FillRectangle(baseX - 8 + checkX, baseY - 8, 1, tileHeight - colH);
                                }
                            }
                            else {
                                for (int checkX = 0, realCheckX = 0; checkX < tileWidth; checkX++) {
                                    realCheckX = checkX ^ xorFlipX;
                                    if (baseTileCfg[tile].CollisionBottom[realCheckX] < 0xF0) continue;

                                    Uint8 colH = baseTileCfg[tile].CollisionBottom[realCheckX];
                                    Graphics::FillRectangle(baseX - 8 + checkX, baseY - 8 + colH, 1, tileHeight - colH);
                                }
                            }
                        }
                    }

                    SKIP_TILE_DRAW:
                    ix++;
                    baseX += tileWidth;
                }

                SKIP_TILE_ROW_DRAW:
                TileBaseY += height;
            }
        }
    }
}
PUBLIC STATIC void     Graphics::DrawSceneLayer_VerticalParallax(SceneLayer* layer, View* currentView) {

}
PUBLIC STATIC void     Graphics::DrawSceneLayer(SceneLayer* layer, View* currentView, int layerIndex, bool useCustomFunction) {
    // If possible, uses optimized software-renderer call instead.
    if (Graphics::GfxFunctions == &SoftwareRenderer::BackendFunctions) {
        SoftwareRenderer::DrawSceneLayer(layer, currentView, layerIndex, useCustomFunction);
        return;
    }

    if (layer->UsingCustomRenderFunction && useCustomFunction) {
        Graphics::RunCustomSceneLayerFunction(&layer->CustomRenderFunction, layerIndex);
        return;
    }

    switch (layer->DrawBehavior) {
        default:
        case DrawBehavior_HorizontalParallax:
        case DrawBehavior_CustomTileScanLines:
        case DrawBehavior_PGZ1_BG:
            Graphics::DrawSceneLayer_HorizontalParallax(layer, currentView);
            break;
        // case DrawBehavior_VerticalParallax:
        //  Graphics::DrawSceneLayer_VerticalParallax(layer, currentView);
            break;
    }
}
PUBLIC STATIC void     Graphics::RunCustomSceneLayerFunction(ObjFunction* func, int layerIndex) {
    VMThread* thread = &BytecodeObjectManager::Threads[0];
    if (func->Arity == 1) {
        thread->Push(INTEGER_VAL(layerIndex));
        thread->RunEntityFunction(func, 1);
    }
    else {
        thread->RunEntityFunction(func, 0);
    }
}

// 3D drawing
PUBLIC STATIC void     Graphics::DrawPolygon3D(void* data, int vertexCount, int vertexFlag, Texture* texture, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {
    if (Graphics::GfxFunctions->DrawPolygon3D)
        Graphics::GfxFunctions->DrawPolygon3D(data, vertexCount, vertexFlag, texture, modelMatrix, normalMatrix);
}
PUBLIC STATIC void     Graphics::DrawSceneLayer3D(void* layer, int sx, int sy, int sw, int sh, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {
    if (Graphics::GfxFunctions->DrawSceneLayer3D)
        Graphics::GfxFunctions->DrawSceneLayer3D(layer, sx, sy, sw, sh, modelMatrix, normalMatrix);
}
PUBLIC STATIC void     Graphics::DrawModel(void* model, Uint16 animation, Uint32 frame, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {
    if (Graphics::GfxFunctions->DrawModel)
        Graphics::GfxFunctions->DrawModel(model, animation, frame, modelMatrix, normalMatrix);
}
PUBLIC STATIC void     Graphics::DrawModelSkinned(void* model, Uint16 armature, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {
    if (Graphics::GfxFunctions->DrawModelSkinned)
        Graphics::GfxFunctions->DrawModelSkinned(model, armature, modelMatrix, normalMatrix);
}
PUBLIC STATIC void     Graphics::DrawVertexBuffer(Uint32 vertexBufferIndex, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {
    if (Graphics::GfxFunctions->DrawVertexBuffer)
        Graphics::GfxFunctions->DrawVertexBuffer(vertexBufferIndex, modelMatrix, normalMatrix);
}
PUBLIC STATIC void     Graphics::BindVertexBuffer(Uint32 vertexBufferIndex) {
    if (vertexBufferIndex < 0 || vertexBufferIndex >= MAX_VERTEX_BUFFERS) {
        UnbindVertexBuffer();
        return;
    }

    if (Graphics::GfxFunctions->BindVertexBuffer)
        Graphics::GfxFunctions->BindVertexBuffer(vertexBufferIndex);
    CurrentVertexBuffer = vertexBufferIndex;
}
PUBLIC STATIC void     Graphics::UnbindVertexBuffer() {
    if (Graphics::GfxFunctions->UnbindVertexBuffer)
        Graphics::GfxFunctions->UnbindVertexBuffer();
    CurrentVertexBuffer = -1;
}
PUBLIC STATIC void     Graphics::BindScene3D(Uint32 sceneIndex) {
    if (sceneIndex < 0 || sceneIndex >= MAX_3D_SCENES)
        return;

    Scene3D* scene = &Graphics::Scene3Ds[sceneIndex];

    if (!scene->UseCustomProjectionMatrix) {
        View* currentView = Graphics::CurrentView;
        if (currentView) {
            float fov = scene->FOV * M_PI / 180.0f;
            float aspect = currentView->Width / currentView->Height;
            Graphics::MakePerspectiveMatrix(&scene->ProjectionMatrix, fov, scene->NearClippingPlane, scene->FarClippingPlane, aspect);
        }
    }

    if (Graphics::GfxFunctions->BindScene3D)
        Graphics::GfxFunctions->BindScene3D(sceneIndex);
    CurrentScene3D = sceneIndex;
}
PUBLIC STATIC void     Graphics::ClearScene3D(Uint32 sceneIndex) {
    if (sceneIndex < 0 || sceneIndex >= MAX_3D_SCENES)
        return;

    Scene3D* scene = &Graphics::Scene3Ds[sceneIndex];
    scene->Clear();

    if (Graphics::GfxFunctions->ClearScene3D)
        Graphics::GfxFunctions->ClearScene3D(sceneIndex);
}
PUBLIC STATIC void     Graphics::DrawScene3D(Uint32 sceneIndex, Uint32 drawMode) {
    if (Graphics::GfxFunctions->DrawScene3D)
        Graphics::GfxFunctions->DrawScene3D(sceneIndex, drawMode);
}

PUBLIC STATIC Uint32   Graphics::CreateScene3D(int unloadPolicy) {
    for (Uint32 i = 0; i < MAX_3D_SCENES; i++) {
        if (!Graphics::Scene3Ds[i].Initialized) {
            Graphics::InitScene3D(i, 0);
            Graphics::Scene3Ds[i].UnloadPolicy = unloadPolicy;
            return i;
        }
    }

    return 0xFFFFFFFF;
}
PUBLIC STATIC void     Graphics::DeleteScene3D(Uint32 sceneIndex) {
    if (sceneIndex < 0 || sceneIndex >= MAX_3D_SCENES)
        return;

    Scene3D* scene = &Graphics::Scene3Ds[sceneIndex];
    if (scene->Initialized) {
        if (Graphics::GfxFunctions->DeleteVertexBuffer)
            Graphics::GfxFunctions->DeleteVertexBuffer(scene->Buffer);
        else
            delete scene->Buffer;

        scene->Initialized = false;
    }
}
PUBLIC STATIC void     Graphics::InitScene3D(Uint32 sceneIndex, Uint32 numVertices) {
    if (sceneIndex < 0 || sceneIndex >= MAX_3D_SCENES)
        return;

    Scene3D* scene = &Graphics::Scene3Ds[sceneIndex];

    Matrix4x4 viewMat;
    Matrix4x4::Identity(&viewMat);
    scene->SetViewMatrix(&viewMat);

    if (!numVertices)
        numVertices = 192;

    if (Graphics::Internal.CreateVertexBuffer)
        scene->Buffer = (VertexBuffer*)Graphics::Internal.CreateVertexBuffer(numVertices);
    else
        scene->Buffer = new VertexBuffer(numVertices);

    scene->ClipPolygons = true;
    scene->Initialized = true;
    scene->UnloadPolicy = SCOPE_GAME;

    scene->Clear();
}

PUBLIC STATIC void     Graphics::MakeSpritePolygon(VertexAttribute data[4], float x, float y, float z, int flipX, int flipY, float scaleX, float scaleY, Texture* texture, int frameX, int frameY, int frameW, int frameH) {
    data[3].Position.X = FP16_TO(x);
    data[3].Position.Y = FP16_TO(y - (frameH * scaleY));
    data[3].Position.Z = FP16_TO(z);

    data[2].Position.X = FP16_TO(x + (frameW * scaleX));
    data[2].Position.Y = FP16_TO(y - (frameH * scaleY));
    data[2].Position.Z = FP16_TO(z);

    data[1].Position.X = FP16_TO(x + (frameW * scaleX));
    data[1].Position.Y = FP16_TO(y);
    data[1].Position.Z = FP16_TO(z);

    data[0].Position.X = FP16_TO(x);
    data[0].Position.Y = FP16_TO(y);
    data[0].Position.Z = FP16_TO(z);

    for (int i = 0; i < 4; i++) {
        data[i].Normal.X   = data[i].Normal.Y = data[i].Normal.Z = data[i].Normal.W = 0;
        data[i].Position.W = 0;
    }

    Graphics::MakeSpritePolygonUVs(data, flipX, flipY, texture, frameX, frameY, frameW, frameH);
}
PUBLIC STATIC void     Graphics::MakeSpritePolygonUVs(VertexAttribute data[4], int flipX, int flipY, Texture* texture, int frameX, int frameY, int frameW, int frameH) {
    Sint64 textureWidth = FP16_TO(texture->Width);
    Sint64 textureHeight = FP16_TO(texture->Height);

    int uv_left   = frameX;
    int uv_right  = frameX + frameW;
    int uv_top    = frameY;
    int uv_bottom = frameY + frameH;

    int left_u, right_u, top_v, bottom_v;

    if (flipX) {
        left_u  = uv_right;
        right_u = uv_left;
    } else {
        left_u  = uv_left;
        right_u = uv_right;
    }

    if (flipY) {
        top_v    = uv_bottom;
        bottom_v = uv_top;
    } else {
        top_v    = uv_top;
        bottom_v = uv_bottom;
    }

    data[3].UV.X       = FP16_DIVIDE(FP16_TO(left_u), textureWidth);
    data[3].UV.Y       = FP16_DIVIDE(FP16_TO(bottom_v), textureHeight);

    data[2].UV.X       = FP16_DIVIDE(FP16_TO(right_u), textureWidth);
    data[2].UV.Y       = FP16_DIVIDE(FP16_TO(bottom_v), textureHeight);

    data[1].UV.X       = FP16_DIVIDE(FP16_TO(right_u), textureWidth);
    data[1].UV.Y       = FP16_DIVIDE(FP16_TO(top_v), textureHeight);

    data[0].UV.X       = FP16_DIVIDE(FP16_TO(left_u), textureWidth);
    data[0].UV.Y       = FP16_DIVIDE(FP16_TO(top_v), textureHeight);
}

PUBLIC STATIC void     Graphics::MakeFrameBufferID(ISprite* sprite, AnimFrame* frame) {
    if (Graphics::GfxFunctions->MakeFrameBufferID)
        Graphics::GfxFunctions->MakeFrameBufferID(sprite, frame);
}
PUBLIC STATIC void     Graphics::DeleteFrameBufferID(AnimFrame* frame) {
    if (Graphics::GfxFunctions->DeleteFrameBufferID)
        Graphics::GfxFunctions->DeleteFrameBufferID(frame);
}

PUBLIC STATIC void     Graphics::SetDepthTesting(bool enabled) {
    if (Graphics::GfxFunctions->SetDepthTesting)
        Graphics::GfxFunctions->SetDepthTesting(enabled);
}

PUBLIC STATIC bool     Graphics::SpriteRangeCheck(ISprite* sprite, int animation, int frame) {
    //#ifdef DEBUG
    if (!sprite) return true;
    if (animation < 0 || animation >= (int)sprite->Animations.size()) {
        BytecodeObjectManager::Threads[0].ThrowRuntimeError(false, "Animation %d does not exist in sprite %s!", animation, sprite->Filename);
        return true;
    }
    if (frame < 0 || frame >= (int)sprite->Animations[animation].Frames.size()) {
        BytecodeObjectManager::Threads[0].ThrowRuntimeError(false, "Frame %d in animation \"%s\" does not exist in sprite %s!", frame, sprite->Animations[animation].Name, sprite->Filename);
        return true;
    }
    //#endif
    return false;
}

PUBLIC STATIC void     Graphics::ConvertFromARGBtoNative(Uint32* argb, int count) {
    if (Graphics::PreferredPixelFormat == SDL_PIXELFORMAT_ABGR8888)
        ColorUtils::ConvertFromARGBtoABGR(argb, count);
}

PUBLIC STATIC void     Graphics::SetStencilEnabled(bool enabled) {
    if (Graphics::GfxFunctions->SetStencilEnabled)
        Graphics::GfxFunctions->SetStencilEnabled(enabled);
}
PUBLIC STATIC bool     Graphics::GetStencilEnabled() {
    if (Graphics::GfxFunctions->IsStencilEnabled)
        return Graphics::GfxFunctions->IsStencilEnabled();

    return false;
}
PUBLIC STATIC void     Graphics::SetStencilTestFunc(int stencilTest) {
    if (stencilTest >= StencilTest_Never && stencilTest <= StencilTest_GEqual) {
        StencilTest = stencilTest;
        if (Graphics::GfxFunctions->SetStencilTestFunc)
            Graphics::GfxFunctions->SetStencilTestFunc(stencilTest);
    }
}
PUBLIC STATIC void     Graphics::SetStencilPassFunc(int stencilOp) {
    if (stencilOp >= StencilOp_Keep && stencilOp <= StencilOp_DecrWrap) {
        StencilOpPass = stencilOp;
        if (Graphics::GfxFunctions->SetStencilPassFunc)
            Graphics::GfxFunctions->SetStencilPassFunc(stencilOp);
    }
}
PUBLIC STATIC void     Graphics::SetStencilFailFunc(int stencilOp) {
    if (stencilOp >= StencilOp_Keep && stencilOp <= StencilOp_DecrWrap) {
        StencilOpFail = stencilOp;
        if (Graphics::GfxFunctions->SetStencilFailFunc)
            Graphics::GfxFunctions->SetStencilFailFunc(stencilOp);
    }
}
PUBLIC STATIC void     Graphics::SetStencilValue(int value) {
    if (value < 0)
        value = 0;
    else if (value > 255)
        value = 255;
    if (Graphics::GfxFunctions->SetStencilValue)
        Graphics::GfxFunctions->SetStencilValue(value);
}
PUBLIC STATIC void     Graphics::SetStencilMask(int mask) {
    if (mask < 0)
        mask = 0;
    else if (mask > 255)
        mask = 255;
    if (Graphics::GfxFunctions->SetStencilMask)
        Graphics::GfxFunctions->SetStencilMask(mask);
}
PUBLIC STATIC void     Graphics::ClearStencil() {
    if (Graphics::GfxFunctions->ClearStencil)
        Graphics::GfxFunctions->ClearStencil();
}
