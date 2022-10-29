#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Rendering/GL/Includes.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Rendering/GL/GLShader.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/Includes/HashMap.h>

class GLRenderer {
public:
    static SDL_GLContext      Context;
    static GLShader*          CurrentShader;
    static GLShader*          SelectedShader;

    static GLShader*          ShaderShape;
    static GLShader*          ShaderTexturedShape;
    static GLShader*          ShaderTexturedShapeYUV;

    static GLint              DefaultFramebuffer;
    static GLint              DefaultRenderbuffer;

    static GLuint             BufferCircleFill;
    static GLuint             BufferCircleStroke;
    static GLuint             BufferSquareFill;
};
#endif

#ifdef USING_OPENGL

#include <Engine/Rendering/GL/GLRenderer.h>

#include <Engine/Application.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Rendering/Texture.h>

SDL_GLContext      GLRenderer::Context = NULL;

GLShader*          GLRenderer::CurrentShader = NULL;
GLShader*          GLRenderer::SelectedShader = NULL;

GLShader*          GLRenderer::ShaderShape = NULL;
GLShader*          GLRenderer::ShaderTexturedShape = NULL;
GLShader*          GLRenderer::ShaderTexturedShapeYUV = NULL;

GLint              GLRenderer::DefaultFramebuffer;
GLint              GLRenderer::DefaultRenderbuffer;

GLuint             GLRenderer::BufferCircleFill;
GLuint             GLRenderer::BufferCircleStroke;
GLuint             GLRenderer::BufferSquareFill;

bool               UseDepthTesting = true;
float              RetinaScale = 1.0;
Texture*           GL_LastTexture = NULL;

// TODO:
// RetinaScale should belong to the texture (specifically TARGET_TEXTURES),
// and drawing functions should scale basded on the current render target.

struct   GL_Vec3 {
    float x;
    float y;
    float z;
};
struct   GL_Vec2 {
    float x;
    float y;
};
struct   GL_AnimFrameVert {
    float x;
    float y;
    float u;
    float v;
};
struct   GL_TextureData {
    GLuint TextureID;
    GLuint TextureU;
    GLuint TextureV;
    bool   YUV;
    bool   Framebuffer;
    GLuint FBO;
    GLuint RBO;
    GLenum TextureTarget;
    GLenum TextureStorageFormat;
    GLenum PixelDataFormat;
    GLenum PixelDataType;
    int    Slot;
};

#define GL_SUPPORTS_MULTISAMPLING
#define GL_SUPPORTS_SMOOTHING
#define GL_SUPPORTS_RENDERBUFFER
#define GL_MONOCHROME_PIXELFORMAT GL_RED
#define CHECK_GL() GLShader::CheckGLError(__LINE__)

#if GL_ES_VERSION_2_0 || GL_ES_VERSION_3_0
#define GL_ES
#undef GL_SUPPORTS_MULTISAMPLING
#undef GL_SUPPORTS_SMOOTHING
#undef GL_MONOCHROME_PIXELFORMAT
#define GL_MONOCHROME_PIXELFORMAT GL_LUMINANCE
#endif

void   GL_MakeShaders() {
    const GLchar* vertexShaderSource[] = {
        "attribute vec3    i_position;\n",
        "attribute vec2    i_uv;\n",
        "varying vec2      o_uv;\n",

        "uniform mat4      u_projectionMatrix;\n",
        "uniform mat4      u_modelViewMatrix;\n",

        "void main() {\n",
        "    gl_Position = u_projectionMatrix * u_modelViewMatrix * vec4(i_position, 1.0);\n",

        "    o_uv = i_uv;\n",
        "}",
    };
    const GLchar* fragmentShaderSource_Shape[] = {
        #ifdef GL_ES
        "precision mediump float;\n",
        #endif
        "varying vec2      o_uv;\n",

        "uniform vec4      u_color;\n",

        "void main() {",
        "    gl_FragColor = u_color;",
        "}",
    };
    const GLchar* fragmentShaderSource_TexturedShape[] = {
        #ifdef GL_ES
        "precision mediump float;\n",
        #endif
        "varying vec2      o_uv;\n",

        "uniform vec4      u_color;\n",
        "uniform sampler2D u_texture;\n",

        "void main() {\n",
        // #ifdef GL_ES
        "    vec4 base = texture2D(u_texture, o_uv);\n",
        "    if (base.a == 0.0) discard;\n",
        "    gl_FragColor = base * u_color;\n",
        "}",
    };
    const GLchar* fragmentShaderSource_TexturedShapeYUV[] = {
        #ifdef GL_ES
        "precision mediump float;\n",
        #endif
        "varying vec2      o_uv;\n",

        "uniform vec4      u_color;\n",
        "uniform sampler2D u_texture;\n",
        "uniform sampler2D u_textureU;\n",
        "uniform sampler2D u_textureV;\n",

        "const vec3 offset = vec3(-0.0625, -0.5, -0.5);\n",
        "const vec3 Rcoeff = vec3(1.164,  0.000,  1.596);\n",
        "const vec3 Gcoeff = vec3(1.164, -0.391, -0.813);\n",
        "const vec3 Bcoeff = vec3(1.164,  2.018,  0.000);\n",

        "void main() {\n",
        "    vec3 yuv, rgb;\n",
        "    vec2 uv = o_uv;\n"

        "    yuv.x = texture2D(u_texture,  uv).r;\n",
        "    yuv.y = texture2D(u_textureU, uv).r;\n",
        "    yuv.z = texture2D(u_textureV, uv).r;\n",
        "    yuv += offset;\n",

        "    rgb.r = dot(yuv, Rcoeff);\n",
        "    rgb.g = dot(yuv, Gcoeff);\n",
        "    rgb.b = dot(yuv, Bcoeff);\n",
        "    gl_FragColor = vec4(rgb, 1.0) * u_color;\n",
        "}",
    };

    GLRenderer::ShaderShape             = new GLShader(vertexShaderSource, sizeof(vertexShaderSource), fragmentShaderSource_Shape, sizeof(fragmentShaderSource_Shape));
    GLRenderer::ShaderTexturedShape     = new GLShader(vertexShaderSource, sizeof(vertexShaderSource), fragmentShaderSource_TexturedShape, sizeof(fragmentShaderSource_TexturedShape));
    GLRenderer::ShaderTexturedShapeYUV  = new GLShader(vertexShaderSource, sizeof(vertexShaderSource), fragmentShaderSource_TexturedShapeYUV, sizeof(fragmentShaderSource_TexturedShapeYUV));
}
void   GL_MakeShapeBuffers() {
    GL_Vec2 verticesSquareFill[4];
    GL_Vec3 verticesCircleFill[362];
    GL_Vec3 verticesCircleStroke[361];

    // Fill Square
    verticesSquareFill[0] = GL_Vec2 { 0.0f, 0.0f };
    verticesSquareFill[1] = GL_Vec2 { 1.0f, 0.0f };
    verticesSquareFill[2] = GL_Vec2 { 0.0f, 1.0f };
    verticesSquareFill[3] = GL_Vec2 { 1.0f, 1.0f };
    glGenBuffers(1, &GLRenderer::BufferSquareFill); CHECK_GL();
    glBindBuffer(GL_ARRAY_BUFFER, GLRenderer::BufferSquareFill); CHECK_GL();
    glBufferData(GL_ARRAY_BUFFER, sizeof(verticesSquareFill), verticesSquareFill, GL_STATIC_DRAW); CHECK_GL();

    // Filled Circle
    verticesCircleFill[0] = GL_Vec3 { 0.0f, 0.0f, 0.0f };
    for (int i = 0; i < 361; i++) {
        verticesCircleFill[i + 1] = GL_Vec3 { (float)cos(i * M_PI / 180.0f), (float)sin(i * M_PI / 180.0f), 0.0f };
    }
    glGenBuffers(1, &GLRenderer::BufferCircleFill); CHECK_GL();
    glBindBuffer(GL_ARRAY_BUFFER, GLRenderer::BufferCircleFill); CHECK_GL();
    glBufferData(GL_ARRAY_BUFFER, sizeof(verticesCircleFill), verticesCircleFill, GL_STATIC_DRAW); CHECK_GL();

    // Stroke Circle
    for (int i = 0; i < 361; i++) {
        verticesCircleStroke[i] = GL_Vec3 { (float)cos(i * M_PI / 180.0f), (float)sin(i * M_PI / 180.0f), 0.0f };
    }
    glGenBuffers(1, &GLRenderer::BufferCircleStroke); CHECK_GL();
    glBindBuffer(GL_ARRAY_BUFFER, GLRenderer::BufferCircleStroke); CHECK_GL();
    glBufferData(GL_ARRAY_BUFFER, sizeof(verticesCircleStroke), verticesCircleStroke, GL_STATIC_DRAW); CHECK_GL();

    // Reset buffer
    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL();
}
void   GL_Predraw(Texture* texture) {
    // Use appropriate shader if changed
    if (texture) {
        GL_TextureData* textureData = (GL_TextureData*)texture->DriverData;
        if (textureData && textureData->YUV) {
            GLRenderer::UseShader(GLRenderer::ShaderTexturedShapeYUV);

            glActiveTexture(GL_TEXTURE0); CHECK_GL();
            glUniform1i(GLRenderer::CurrentShader->LocTexture, 0); CHECK_GL();
            glBindTexture(GL_TEXTURE_2D, textureData->TextureID); CHECK_GL();

            glActiveTexture(GL_TEXTURE1); CHECK_GL();
            glUniform1i(GLRenderer::CurrentShader->LocTextureU, 1); CHECK_GL();
            glBindTexture(GL_TEXTURE_2D, textureData->TextureU); CHECK_GL();

            glActiveTexture(GL_TEXTURE2); CHECK_GL();
            glUniform1i(GLRenderer::CurrentShader->LocTextureV, 2); CHECK_GL();
            glBindTexture(GL_TEXTURE_2D, textureData->TextureV); CHECK_GL();
        }
        else {
            GLRenderer::UseShader(GLRenderer::ShaderTexturedShape);
        }

        glEnableVertexAttribArray(GLRenderer::CurrentShader->LocTexCoord); CHECK_GL();
    }
    else {
        if (GLRenderer::CurrentShader == GLRenderer::ShaderTexturedShape || GLRenderer::CurrentShader == GLRenderer::ShaderTexturedShapeYUV) {
            glDisableVertexAttribArray(GLRenderer::CurrentShader->LocTexCoord); CHECK_GL();
        }

        GLRenderer::UseShader(GLRenderer::ShaderShape);
    }

    // Do texture (re-)binding if necessary
    if (GL_LastTexture != texture) {
        if (texture) {
            GL_TextureData* textureData = (GL_TextureData*)texture->DriverData;

            glActiveTexture(GL_TEXTURE0); CHECK_GL();
            glBindTexture(GL_TEXTURE_2D, textureData->TextureID); CHECK_GL();
        }
        else {
            glBindTexture(GL_TEXTURE_2D, 0); CHECK_GL();
        }
    }
    GL_LastTexture = texture;

    // Update color if needed
    if (memcmp(&GLRenderer::CurrentShader->CachedBlendColors[0], &Graphics::BlendColors[0], sizeof(float) * 4) != 0) {
        memcpy(&GLRenderer::CurrentShader->CachedBlendColors[0], &Graphics::BlendColors[0], sizeof(float) * 4);

        glUniform4f(GLRenderer::CurrentShader->LocColor, Graphics::BlendColors[0], Graphics::BlendColors[1], Graphics::BlendColors[2], Graphics::BlendColors[3]); CHECK_GL();
    }

    // Update matrices
    if (!Matrix4x4::Equals(GLRenderer::CurrentShader->CachedProjectionMatrix, Scene::Views[Scene::ViewCurrent].ProjectionMatrix)) {
        if (!GLRenderer::CurrentShader->CachedProjectionMatrix)
            GLRenderer::CurrentShader->CachedProjectionMatrix = Matrix4x4::Create();

        Matrix4x4::Copy(GLRenderer::CurrentShader->CachedProjectionMatrix, Scene::Views[Scene::ViewCurrent].ProjectionMatrix);

        glUniformMatrix4fv(GLRenderer::CurrentShader->LocProjectionMatrix, 1, false, GLRenderer::CurrentShader->CachedProjectionMatrix->Values); CHECK_GL();
    }
    if (!Matrix4x4::Equals(GLRenderer::CurrentShader->CachedModelViewMatrix, Graphics::ModelViewMatrix.top())) {
        if (!GLRenderer::CurrentShader->CachedModelViewMatrix)
            GLRenderer::CurrentShader->CachedModelViewMatrix = Matrix4x4::Create();

        Matrix4x4::Copy(GLRenderer::CurrentShader->CachedModelViewMatrix, Graphics::ModelViewMatrix.top());

        glUniformMatrix4fv(GLRenderer::CurrentShader->LocModelViewMatrix, 1, false, GLRenderer::CurrentShader->CachedModelViewMatrix->Values); CHECK_GL();
    }
}
void   GL_DrawTextureBuffered(Texture* texture, GLuint buffer, int flip) {
    GL_Predraw(texture);

    if (!Graphics::TextureBlend) {
        GLRenderer::CurrentShader->CachedBlendColors[0] =
        GLRenderer::CurrentShader->CachedBlendColors[1] =
        GLRenderer::CurrentShader->CachedBlendColors[2] =
        GLRenderer::CurrentShader->CachedBlendColors[3] = 1.0;
        glUniform4f(GLRenderer::CurrentShader->LocColor, 1.0, 1.0, 1.0, 1.0); CHECK_GL();
    }

    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glVertexAttribPointer(GLRenderer::CurrentShader->LocPosition, 2, GL_FLOAT, GL_FALSE, sizeof(GL_AnimFrameVert), 0);
    glVertexAttribPointer(GLRenderer::CurrentShader->LocTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(GL_AnimFrameVert), (char*)NULL + 8);

    glDrawArrays(GL_TRIANGLE_STRIP, flip << 2, 4);
}
void   GL_DrawTexture(Texture* texture, float sx, float sy, float sw, float sh, float x, float y, float w, float h) {
    GL_Predraw(texture);

    if (!Graphics::TextureBlend) {
        GLRenderer::CurrentShader->CachedBlendColors[0] =
        GLRenderer::CurrentShader->CachedBlendColors[1] =
        GLRenderer::CurrentShader->CachedBlendColors[2] =
        GLRenderer::CurrentShader->CachedBlendColors[3] = 1.0;
        glUniform4f(GLRenderer::CurrentShader->LocColor, 1.0, 1.0, 1.0, 1.0);
    }

    // glEnableVertexAttribArray(GLRenderer::CurrentShader->LocTexCoord);
        GL_Vec2 v[4];
        v[0] = GL_Vec2 { x, y };
        v[1] = GL_Vec2 { x + w, y };
        v[2] = GL_Vec2 { x, y + h };
        v[3] = GL_Vec2 { x + w, y + h };
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glVertexAttribPointer(GLRenderer::CurrentShader->LocPosition, 2, GL_FLOAT, GL_FALSE, 0, v);
        // glBindBuffer(GL_ARRAY_BUFFER, GLRenderer::BufferSquareFill);
        // glVertexAttribPointer(GLRenderer::CurrentShader->LocPosition, 2, GL_FLOAT, GL_FALSE, 0, 0);

        GL_Vec2 v2[4];
        if (sx >= 0.0) {
            v2[0] = GL_Vec2 { (sx) / texture->Width     , (sy) / texture->Height };
            v2[1] = GL_Vec2 { (sx + sw) / texture->Width, (sy) / texture->Height };
            v2[2] = GL_Vec2 { (sx) / texture->Width     , (sy + sh) / texture->Height };
            v2[3] = GL_Vec2 { (sx + sw) / texture->Width, (sy + sh) / texture->Height };
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glVertexAttribPointer(GLRenderer::CurrentShader->LocTexCoord, 2, GL_FLOAT, GL_FALSE, 0, v2);
        }
        else {
            glBindBuffer(GL_ARRAY_BUFFER, GLRenderer::BufferSquareFill);
            glVertexAttribPointer(GLRenderer::CurrentShader->LocTexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0); // LocPosition
        }

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    // glDisableVertexAttribArray(GLRenderer::CurrentShader->LocTexCoord);
}
GLenum GL_GetBlendFactorFromHatchEnum(int factor) {
    switch (factor) {
        case BlendFactor_ZERO:
            return GL_ZERO;
        case BlendFactor_ONE:
            return GL_ONE;
        case BlendFactor_SRC_COLOR:
            return GL_SRC_COLOR;
        case BlendFactor_INV_SRC_COLOR:
            return GL_ONE_MINUS_SRC_COLOR;
        case BlendFactor_SRC_ALPHA:
            return GL_SRC_ALPHA;
        case BlendFactor_INV_SRC_ALPHA:
            return GL_ONE_MINUS_SRC_ALPHA;
        case BlendFactor_DST_COLOR:
            return GL_DST_COLOR;
        case BlendFactor_INV_DST_COLOR:
            return GL_ONE_MINUS_DST_COLOR;
        case BlendFactor_DST_ALPHA:
            return GL_DST_ALPHA;
        case BlendFactor_INV_DST_ALPHA:
            return GL_ONE_MINUS_DST_ALPHA;
    }
    return 0;
}

// Initialization and disposal functions
PUBLIC STATIC void     GLRenderer::Init() {
    Graphics::SupportsBatching = true;
    Graphics::PreferredPixelFormat = SDL_PIXELFORMAT_ABGR8888;

    Log::Print(Log::LOG_INFO, "Renderer: OpenGL");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); CHECK_GL();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1); CHECK_GL();

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); CHECK_GL();

    #ifdef GL_SUPPORTS_MULTISAMPLING
    if (Graphics::MultisamplingEnabled) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1); CHECK_GL();
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, Graphics::MultisamplingEnabled); CHECK_GL();
    }
    #endif

	if (Application::Platform == Platforms::iOS) {
		SDL_GL_SetAttribute(SDL_GL_RETAINED_BACKING, 0); CHECK_GL();
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0); CHECK_GL();
	}

    Context = SDL_GL_CreateContext(Application::Window); CHECK_GL();
    if (!Context) {
        Log::Print(Log::LOG_ERROR, "Could not create OpenGL context: %s", SDL_GetError()); CHECK_GL();
        exit(-1);
    }

    #ifdef WIN32
    glewExperimental = GL_TRUE;
    GLenum res = glewInit(); CHECK_GL();
    if (res != GLEW_OK) {
        Log::Print(Log::LOG_ERROR, "Could not create GLEW context: %s", glewGetErrorString(res)); CHECK_GL();
        exit(-1);
    }
    #endif

    if (Graphics::VsyncEnabled) {
        GLRenderer::SetVSync(true);
    }
    CHECK_GL();

    int max, w, h, ww, wh;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max); CHECK_GL();

    Graphics::MaxTextureWidth = max;
    Graphics::MaxTextureHeight = max;

    SDL_GL_GetDrawableSize(Application::Window, &w, &h); CHECK_GL();
    SDL_GetWindowSize(Application::Window, &ww, &wh);

    RetinaScale = 1.0;
    if (h > wh)
        RetinaScale = h / wh;

    Log::Print(Log::LOG_INFO, "OpenGL Version: %s", glGetString(GL_VERSION)); CHECK_GL();
    Log::Print(Log::LOG_INFO, "GLSL Version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION)); CHECK_GL();
    Log::Print(Log::LOG_INFO, "Graphics Card: %s %s", glGetString(GL_VENDOR), glGetString(GL_RENDERER)); CHECK_GL();
    Log::Print(Log::LOG_INFO, "Drawable Size: %d x %d", w, h);

    if (Application::Platform == Platforms::iOS ||
		Application::Platform == Platforms::Android) {
        UseDepthTesting = false;
    }

    // Enable/Disable GL features
    glEnable(GL_BLEND); CHECK_GL();
    if (UseDepthTesting) {
        glEnable(GL_DEPTH_TEST); CHECK_GL();
    }
    glDepthMask(GL_TRUE); CHECK_GL();

    #ifdef GL_SUPPORTS_MULTISAMPLING
    if (Graphics::MultisamplingEnabled) {
        glEnable(GL_MULTISAMPLE); CHECK_GL();
    }
    #endif

    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); CHECK_GL();
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD); CHECK_GL();
    glDepthFunc(GL_LEQUAL); CHECK_GL();

    #ifdef GL_SUPPORTS_SMOOTHING
        glEnable(GL_LINE_SMOOTH); CHECK_GL();
        // glEnable(GL_POLYGON_SMOOTH); CHECK_GL();
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST); CHECK_GL();
        glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST); CHECK_GL();
    #endif

    GL_MakeShaders();
    GL_MakeShapeBuffers();

    UseShader(ShaderShape);
    glEnableVertexAttribArray(GLRenderer::CurrentShader->LocPosition); CHECK_GL();

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &DefaultFramebuffer); CHECK_GL();
    #ifdef GL_SUPPORTS_RENDERBUFFER
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &DefaultRenderbuffer); CHECK_GL();
    #endif

    Log::Print(Log::LOG_INFO, "Default Framebuffer: %d", DefaultFramebuffer);
    Log::Print(Log::LOG_INFO, "Default Renderbuffer: %d", DefaultRenderbuffer);

    glClearColor(0.0, 0.0, 0.0, 0.0); CHECK_GL();
}
PUBLIC STATIC Uint32   GLRenderer::GetWindowFlags() {
    #ifdef GL_SUPPORTS_MULTISAMPLING
    if (Graphics::MultisamplingEnabled) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1); CHECK_GL();
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, Graphics::MultisamplingEnabled); CHECK_GL();
    }
    #endif

    return SDL_WINDOW_OPENGL;
}
PUBLIC STATIC void     GLRenderer::SetVSync(bool enabled) {
    if (enabled) {
        if (SDL_GL_SetSwapInterval(-1) < 0) {
            CHECK_GL();
            if (SDL_GL_SetSwapInterval(1) < 0) {
                CHECK_GL();
                Log::Print(Log::LOG_WARN, "Could not enable V-Sync: %s", SDL_GetError());
                CHECK_GL();
                enabled = false;
            }
        }
    }
    else if (SDL_GL_SetSwapInterval(0) < 0) {
        CHECK_GL();
    }
    Graphics::VsyncEnabled = enabled;
}
PUBLIC STATIC void     GLRenderer::SetGraphicsFunctions() {
    Graphics::PixelOffset = 0.0f;

    Graphics::Internal.Init = GLRenderer::Init;
    Graphics::Internal.GetWindowFlags = GLRenderer::GetWindowFlags;
    Graphics::Internal.SetVSync = GLRenderer::SetVSync;
    Graphics::Internal.Dispose = GLRenderer::Dispose;

    // Texture management functions
    Graphics::Internal.CreateTexture = GLRenderer::CreateTexture;
    Graphics::Internal.LockTexture = GLRenderer::LockTexture;
    Graphics::Internal.UpdateTexture = GLRenderer::UpdateTexture;
    Graphics::Internal.UpdateYUVTexture = GLRenderer::UpdateTextureYUV;
    Graphics::Internal.UnlockTexture = GLRenderer::UnlockTexture;
    Graphics::Internal.DisposeTexture = GLRenderer::DisposeTexture;

    // Viewport and view-related functions
    Graphics::Internal.SetRenderTarget = GLRenderer::SetRenderTarget;
    Graphics::Internal.UpdateWindowSize = GLRenderer::UpdateWindowSize;
    Graphics::Internal.UpdateViewport = GLRenderer::UpdateViewport;
    Graphics::Internal.UpdateClipRect = GLRenderer::UpdateClipRect;
    Graphics::Internal.UpdateOrtho = GLRenderer::UpdateOrtho;
    Graphics::Internal.UpdatePerspective = GLRenderer::UpdatePerspective;
    Graphics::Internal.UpdateProjectionMatrix = GLRenderer::UpdateProjectionMatrix;

    // Shader-related functions
    Graphics::Internal.UseShader = GLRenderer::UseShader;
    Graphics::Internal.SetUniformF = GLRenderer::SetUniformF;
    Graphics::Internal.SetUniformI = GLRenderer::SetUniformI;
    Graphics::Internal.SetUniformTexture = GLRenderer::SetUniformTexture;

    // These guys
    Graphics::Internal.Clear = GLRenderer::Clear;
    Graphics::Internal.Present = GLRenderer::Present;

    // Draw mode setting functions
    Graphics::Internal.SetBlendColor = GLRenderer::SetBlendColor;
    Graphics::Internal.SetBlendMode = GLRenderer::SetBlendMode;
    Graphics::Internal.SetLineWidth = GLRenderer::SetLineWidth;

    // Primitive drawing functions
    Graphics::Internal.StrokeLine = GLRenderer::StrokeLine;
    Graphics::Internal.StrokeCircle = GLRenderer::StrokeCircle;
    Graphics::Internal.StrokeEllipse = GLRenderer::StrokeEllipse;
    Graphics::Internal.StrokeRectangle = GLRenderer::StrokeRectangle;
    Graphics::Internal.FillCircle = GLRenderer::FillCircle;
    Graphics::Internal.FillEllipse = GLRenderer::FillEllipse;
    Graphics::Internal.FillTriangle = GLRenderer::FillTriangle;
    Graphics::Internal.FillRectangle = GLRenderer::FillRectangle;

    // Texture drawing functions
    Graphics::Internal.DrawTexture = GLRenderer::DrawTexture;
    Graphics::Internal.DrawSprite = GLRenderer::DrawSprite;
    Graphics::Internal.DrawSpritePart = GLRenderer::DrawSpritePart;
    Graphics::Internal.MakeFrameBufferID = GLRenderer::MakeFrameBufferID;
}
PUBLIC STATIC void     GLRenderer::Dispose() {
    glDeleteBuffers(1, &BufferCircleFill); CHECK_GL();
    glDeleteBuffers(1, &BufferCircleStroke); CHECK_GL();
    glDeleteBuffers(1, &BufferSquareFill); CHECK_GL();

    ShaderShape->Dispose(); delete ShaderShape;
    ShaderTexturedShape->Dispose(); delete ShaderTexturedShape;

    SDL_GL_DeleteContext(Context); CHECK_GL();
}

// Texture management functions
PUBLIC STATIC Texture* GLRenderer::CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height) {
    Texture* texture = Texture::New(format, access, width, height);
    texture->DriverData = Memory::TrackedCalloc("Texture::DriverData", 1, sizeof(GL_TextureData));

    GL_TextureData* textureData = (GL_TextureData*)texture->DriverData;

    textureData->TextureTarget = GL_TEXTURE_2D;

    textureData->TextureStorageFormat = GL_RGBA;
    textureData->PixelDataFormat = GL_RGBA;
    textureData->PixelDataType = GL_UNSIGNED_BYTE;

    // Set format
    switch (texture->Format) {
        case SDL_PIXELFORMAT_YV12:
        case SDL_PIXELFORMAT_IYUV:
        case SDL_PIXELFORMAT_NV12:
        case SDL_PIXELFORMAT_NV21:
            textureData->TextureStorageFormat = GL_MONOCHROME_PIXELFORMAT;
            textureData->PixelDataFormat = GL_MONOCHROME_PIXELFORMAT;
            textureData->PixelDataType = GL_UNSIGNED_BYTE;
            break;
        default:
            break;
    }

    // Set texture access
    switch (texture->Access) {
        case SDL_TEXTUREACCESS_TARGET: {
            textureData->Framebuffer = true;
            glGenFramebuffers(1, &textureData->FBO); CHECK_GL();

            #ifdef GL_SUPPORTS_RENDERBUFFER
            glGenRenderbuffers(1, &textureData->RBO); CHECK_GL();
            glBindRenderbuffer(GL_RENDERBUFFER, textureData->RBO); CHECK_GL();
            // glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height); CHECK_GL();
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height); CHECK_GL();
            #endif

            #ifdef GL_SUPPORTS_MULTISAMPLING
            // textureData->TextureTarget = GL_TEXTURE_2D_MULTISAMPLE;
            #endif

            width *= RetinaScale;
            height *= RetinaScale;
            break;
        }
        case SDL_TEXTUREACCESS_STREAMING: {
            texture->Pitch = texture->Width * SDL_BYTESPERPIXEL(texture->Format);

            size_t size = texture->Pitch * texture->Height;
            if (texture->Format == SDL_PIXELFORMAT_YV12 ||
                texture->Format == SDL_PIXELFORMAT_IYUV) {
                // Need to add size for the U and V planes.
                size += 2 * ((texture->Height + 1) / 2) * ((texture->Pitch + 1) / 2);
            }
            if (texture->Format == SDL_PIXELFORMAT_NV12 ||
                texture->Format == SDL_PIXELFORMAT_NV21) {
                // Need to add size for the U/V plane.
                size += 2 * ((texture->Height + 1) / 2) * ((texture->Pitch + 1) / 2);
            }
            texture->Pixels = calloc(1, size);
            break;
        }
    }

    // Generate texture buffer
    glGenTextures(1, &textureData->TextureID); CHECK_GL();
    glBindTexture(textureData->TextureTarget, textureData->TextureID); CHECK_GL();

    // Set target
    switch (textureData->TextureTarget) {
        case GL_TEXTURE_2D:
            // glTexImage2D(textureData->TextureTarget, 0, textureData->TextureStorageFormat, width, height, 0, textureData->PixelDataFormat, textureData->PixelDataType, texture->Pixels); CHECK_GL();
			glTexImage2D(textureData->TextureTarget, 0, textureData->TextureStorageFormat, width, height, 0, textureData->PixelDataFormat, textureData->PixelDataType, 0); CHECK_GL();
            break;
        {
        #ifdef GL_SUPPORTS_MULTISAMPLING
        case GL_TEXTURE_2D_MULTISAMPLE:
            glTexImage2DMultisample(textureData->TextureTarget, Graphics::MultisamplingEnabled, textureData->PixelDataFormat, width, height, GL_TRUE); CHECK_GL();
            break;
        #endif
        }
        default:
            Log::Print(Log::LOG_ERROR, "Unsupported GL texture target!");
            break;
    }

    // Set texture filter
    GLenum textureFilter = GL_NEAREST;
    if (Graphics::TextureInterpolate)
        textureFilter = GL_LINEAR;
    glTexParameteri(textureData->TextureTarget, GL_TEXTURE_MAG_FILTER, textureFilter); CHECK_GL();
    glTexParameteri(textureData->TextureTarget, GL_TEXTURE_MIN_FILTER, textureFilter); CHECK_GL();
    glTexParameteri(textureData->TextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); CHECK_GL();
    glTexParameteri(textureData->TextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); CHECK_GL();

    if (texture->Format == SDL_PIXELFORMAT_YV12 ||
        texture->Format == SDL_PIXELFORMAT_IYUV) {
        textureData->YUV = true;

        // offset:
        // 0x10   , 0x80, 0x80
        // -0.0625, -0.5, -0.5

        glGenTextures(1, &textureData->TextureU); CHECK_GL();
        glGenTextures(1, &textureData->TextureV); CHECK_GL();

        glBindTexture(textureData->TextureTarget, textureData->TextureU); CHECK_GL();
        glTexParameteri(textureData->TextureTarget, GL_TEXTURE_MAG_FILTER, textureFilter); CHECK_GL();
        glTexParameteri(textureData->TextureTarget, GL_TEXTURE_MIN_FILTER, textureFilter); CHECK_GL();
        glTexParameteri(textureData->TextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); CHECK_GL();
        glTexParameteri(textureData->TextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); CHECK_GL();
        glTexImage2D(textureData->TextureTarget, 0, textureData->TextureStorageFormat, (width + 1) / 2, (height + 1) / 2, 0, textureData->PixelDataFormat, textureData->PixelDataType, NULL); CHECK_GL();

        glBindTexture(textureData->TextureTarget, textureData->TextureV); CHECK_GL();
        glTexParameteri(textureData->TextureTarget, GL_TEXTURE_MAG_FILTER, textureFilter); CHECK_GL();
        glTexParameteri(textureData->TextureTarget, GL_TEXTURE_MIN_FILTER, textureFilter); CHECK_GL();
        glTexParameteri(textureData->TextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); CHECK_GL();
        glTexParameteri(textureData->TextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); CHECK_GL();
        glTexImage2D(textureData->TextureTarget, 0, textureData->TextureStorageFormat, (width + 1) / 2, (height + 1) / 2, 0, textureData->PixelDataFormat, textureData->PixelDataType, NULL); CHECK_GL();
    }

    glBindTexture(textureData->TextureTarget, 0); CHECK_GL();

    texture->ID = textureData->TextureID;
    Graphics::TextureMap->Put(texture->ID, texture);

    return texture;
}
PUBLIC STATIC int      GLRenderer::LockTexture(Texture* texture, void** pixels, int* pitch) {
    return 0;
}
PUBLIC STATIC int      GLRenderer::UpdateTexture(Texture* texture, SDL_Rect* src, void* pixels, int pitch) {
    Uint32 inputPixelsX = 0;
    Uint32 inputPixelsY = 0;
    Uint32 inputPixelsW = texture->Width;
    Uint32 inputPixelsH = texture->Height;
    if (src) {
        inputPixelsX = src->x;
        inputPixelsY = src->y;
        inputPixelsW = src->w;
        inputPixelsH = src->h;
    }

    if (Graphics::NoInternalTextures) {
        if (inputPixelsW > Graphics::MaxTextureWidth)
            inputPixelsW = Graphics::MaxTextureWidth;
        if (inputPixelsH > Graphics::MaxTextureHeight)
            inputPixelsH = Graphics::MaxTextureHeight;
    }

    GL_TextureData* textureData = (GL_TextureData*)texture->DriverData;

    textureData->TextureStorageFormat = GL_RGBA;
    textureData->PixelDataFormat = GL_RGBA;
    textureData->PixelDataType = GL_UNSIGNED_BYTE;

    glBindTexture(textureData->TextureTarget, textureData->TextureID); CHECK_GL();
    glTexSubImage2D(textureData->TextureTarget, 0,
        inputPixelsX, inputPixelsY, inputPixelsW, inputPixelsH,
        textureData->PixelDataFormat, textureData->PixelDataType, pixels); CHECK_GL();
    return 0;
}
PUBLIC STATIC int      GLRenderer::UpdateTextureYUV(Texture* texture, SDL_Rect* src, void* pixelsY, int pitchY, void* pixelsU, int pitchU, void* pixelsV, int pitchV) {
    int inputPixelsX = 0;
    int inputPixelsY = 0;
    int inputPixelsW = texture->Width;
    int inputPixelsH = texture->Height;
    if (src) {
        inputPixelsX = src->x;
        inputPixelsY = src->y;
        inputPixelsW = src->w;
        inputPixelsH = src->h;
    }

    GL_TextureData* textureData = (GL_TextureData*)texture->DriverData;

    glBindTexture(textureData->TextureTarget, textureData->TextureID); CHECK_GL();
    glTexSubImage2D(textureData->TextureTarget, 0,
        inputPixelsX, inputPixelsY, inputPixelsW, inputPixelsH,
        textureData->PixelDataFormat, textureData->PixelDataType, pixelsY); CHECK_GL();

    inputPixelsX = inputPixelsX / 2;
    inputPixelsY = inputPixelsY / 2;
    inputPixelsW = (inputPixelsW + 1) / 2;
    inputPixelsH = (inputPixelsH + 1) / 2;

    glBindTexture(textureData->TextureTarget, texture->Format != SDL_PIXELFORMAT_YV12 ? textureData->TextureV : textureData->TextureU); CHECK_GL();

    glTexSubImage2D(textureData->TextureTarget, 0,
        inputPixelsX, inputPixelsY, inputPixelsW, inputPixelsH,
        textureData->PixelDataFormat, textureData->PixelDataType, pixelsU); CHECK_GL();

    glBindTexture(textureData->TextureTarget, texture->Format != SDL_PIXELFORMAT_YV12 ? textureData->TextureU : textureData->TextureV); CHECK_GL();

    glTexSubImage2D(textureData->TextureTarget, 0,
        inputPixelsX, inputPixelsY, inputPixelsW, inputPixelsH,
        textureData->PixelDataFormat, textureData->PixelDataType, pixelsV); CHECK_GL();
    return 0;
}
PUBLIC STATIC void     GLRenderer::UnlockTexture(Texture* texture) {

}
PUBLIC STATIC void     GLRenderer::DisposeTexture(Texture* texture) {
    GL_TextureData* textureData = (GL_TextureData*)texture->DriverData;
    if (!textureData)
        return;

    if (texture->Access == SDL_TEXTUREACCESS_TARGET) {
        glDeleteFramebuffers(1, &textureData->FBO); CHECK_GL();
        #ifdef GL_SUPPORTS_RENDERBUFFER
        glDeleteRenderbuffers(1, &textureData->RBO); CHECK_GL();
        #endif
    }
    else if (texture->Access == SDL_TEXTUREACCESS_STREAMING) {
        // free(texture->Pixels);
    }
    if (textureData->YUV) {
        glDeleteTextures(1, &textureData->TextureU); CHECK_GL();
        glDeleteTextures(1, &textureData->TextureV); CHECK_GL();
    }
    glDeleteTextures(1, &textureData->TextureID); CHECK_GL();
    Memory::Free(textureData);
}

// Viewport and view-related functions
PUBLIC STATIC void     GLRenderer::SetRenderTarget(Texture* texture) {
    if (texture == NULL) {
        glBindFramebuffer(GL_FRAMEBUFFER, DefaultFramebuffer); CHECK_GL();

        #ifdef GL_SUPPORTS_RENDERBUFFER
        glBindRenderbuffer(GL_RENDERBUFFER, DefaultRenderbuffer); CHECK_GL();
        #endif
    }
    else {
        GL_TextureData* textureData = (GL_TextureData*)texture->DriverData;
        if (!textureData->Framebuffer) {
            Log::Print(Log::LOG_WARN, "Cannot render to non-framebuffer texture!");
            return;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, textureData->FBO); CHECK_GL();
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureData->TextureTarget, textureData->TextureID, 0); CHECK_GL();

        #ifdef GL_SUPPORTS_RENDERBUFFER
        glBindRenderbuffer(GL_RENDERBUFFER, textureData->RBO); CHECK_GL();
        // glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, textureData->RBO); CHECK_GL();
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, textureData->RBO); CHECK_GL();
        #endif

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            Log::Print(Log::LOG_ERROR, "glFramebufferTexture2D() failed");
        }
        CHECK_GL();
    }
}
PUBLIC STATIC void     GLRenderer::UpdateWindowSize(int width, int height) {
    GLRenderer::UpdateViewport();
}
PUBLIC STATIC void     GLRenderer::UpdateViewport() {
    Viewport* vp = &Graphics::CurrentViewport;
    if (Graphics::CurrentRenderTarget) {
        glViewport(vp->X * RetinaScale, vp->Y * RetinaScale, vp->Width * RetinaScale, vp->Height * RetinaScale); CHECK_GL();
    }
    else {
        int h; SDL_GetWindowSize(Application::Window, NULL, &h);
        glViewport(vp->X * RetinaScale, (h - vp->Y - vp->Height) * RetinaScale, vp->Width * RetinaScale, vp->Height * RetinaScale); CHECK_GL();
    }

    // NOTE: According to SDL2 we should be setting projection matrix here.
    // GLRenderer::UpdateOrtho(vp->Width, vp->Height);
    GLRenderer::UpdateProjectionMatrix();
}
PUBLIC STATIC void     GLRenderer::UpdateClipRect() {
    ClipArea clip = Graphics::CurrentClip;
    if (Graphics::CurrentClip.Enabled) {
        Viewport view = Graphics::CurrentViewport;

        glEnable(GL_SCISSOR_TEST); CHECK_GL();
        if (Graphics::CurrentRenderTarget) {
            glScissor((view.X + clip.X) * RetinaScale, (view.Y + clip.Y) * RetinaScale, (clip.Width) * RetinaScale, (clip.Height) * RetinaScale); CHECK_GL();
        }
        else {
            int w, h;
            float scaleW = RetinaScale, scaleH = RetinaScale;
            SDL_GetWindowSize(Application::Window, &w, &h);

            View* currentView = &Scene::Views[Scene::ViewCurrent];
            scaleW *= w / currentView->Width;
            scaleH *= h / currentView->Height;

            glScissor((view.X + clip.X) * scaleW, h * RetinaScale - (view.Y + clip.Y + clip.Height) * scaleH, (clip.Width) * scaleW, (clip.Height) * scaleH); CHECK_GL();
        }
    }
    else {
        glDisable(GL_SCISSOR_TEST); CHECK_GL();
    }
}
PUBLIC STATIC void     GLRenderer::UpdateOrtho(float left, float top, float right, float bottom) {
    // if (Graphics::CurrentRenderTarget)
    //     Matrix4x4::Ortho(Scene::Views[Scene::ViewCurrent].BaseProjectionMatrix, left, right, bottom, top, -500.0f, 500.0f);
    // else
        Matrix4x4::Ortho(Scene::Views[Scene::ViewCurrent].BaseProjectionMatrix, left, right, top, bottom, -500.0f, 500.0f);

    Matrix4x4::Copy(Scene::Views[Scene::ViewCurrent].ProjectionMatrix, Scene::Views[Scene::ViewCurrent].BaseProjectionMatrix);
}
PUBLIC STATIC void     GLRenderer::UpdatePerspective(float fovy, float aspect, float nearv, float farv) {
    Matrix4x4::Perspective(Scene::Views[Scene::ViewCurrent].BaseProjectionMatrix, fovy, aspect, nearv, farv);
    Matrix4x4::Copy(Scene::Views[Scene::ViewCurrent].ProjectionMatrix, Scene::Views[Scene::ViewCurrent].BaseProjectionMatrix);
}
PUBLIC STATIC void     GLRenderer::UpdateProjectionMatrix() {

}

// Shader-related functions
PUBLIC STATIC void     GLRenderer::UseShader(void* shader) {
    // Override shader
    // if (Graphics::CurrentShader)
    //     shader = Graphics::CurrentShader;

    if (GLRenderer::CurrentShader != (GLShader*)shader) {
        GLRenderer::CurrentShader = (GLShader*)shader;
        GLRenderer::CurrentShader->Use();
        // glEnableVertexAttribArray(CurrentShader->LocTexCoord);

        glActiveTexture(GL_TEXTURE0); CHECK_GL();
        glUniform1i(GLRenderer::CurrentShader->LocTexture, 0); CHECK_GL();
    }
}
PUBLIC STATIC void     GLRenderer::SetUniformF(int location, int count, float* values) {
    switch (count) {
        case 1: glUniform1f(location, values[0]); CHECK_GL(); break;
        case 2: glUniform2f(location, values[0], values[1]); CHECK_GL(); break;
        case 3: glUniform3f(location, values[0], values[1], values[2]); CHECK_GL(); break;
        case 4: glUniform4f(location, values[0], values[1], values[2], values[3]); CHECK_GL(); break;
    }
}
PUBLIC STATIC void     GLRenderer::SetUniformI(int location, int count, int* values) {
    glUniform1iv(location, count, values); CHECK_GL();
}
PUBLIC STATIC void     GLRenderer::SetUniformTexture(Texture* texture, int uniform_index, int slot) {
    GL_TextureData* textureData = (GL_TextureData*)texture->DriverData;
    glActiveTexture(GL_TEXTURE0 + slot); CHECK_GL();
    glUniform1i(uniform_index, slot); CHECK_GL();
    glBindTexture(GL_TEXTURE_2D, textureData->TextureID); CHECK_GL();
}

// These guys
PUBLIC STATIC void     GLRenderer::Clear() {
    if (UseDepthTesting) {
        #ifdef GL_ES
        glClearDepthf(1.0f); CHECK_GL();
        #else
        glClearDepth(1.0f); CHECK_GL();
        #endif
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); CHECK_GL();
    }
    else {
        glClear(GL_COLOR_BUFFER_BIT); CHECK_GL();
    }
}
PUBLIC STATIC void     GLRenderer::Present() {
	SDL_GL_SwapWindow(Application::Window); CHECK_GL();
}

// Draw mode setting functions
PUBLIC STATIC void     GLRenderer::SetBlendColor(float r, float g, float b, float a) {

}
PUBLIC STATIC void     GLRenderer::SetBlendMode(int srcC, int dstC, int srcA, int dstA) {
    glBlendFuncSeparate(
        GL_GetBlendFactorFromHatchEnum(srcC), GL_GetBlendFactorFromHatchEnum(dstC),
        GL_GetBlendFactorFromHatchEnum(srcA), GL_GetBlendFactorFromHatchEnum(dstA)); CHECK_GL();
}
PUBLIC STATIC void     GLRenderer::SetLineWidth(float n) {
    glLineWidth(n); CHECK_GL();
}

// Primitive drawing functions
PUBLIC STATIC void     GLRenderer::StrokeLine(float x1, float y1, float x2, float y2) {
    // Graphics::Save();
        GL_Predraw(NULL);

        float v[6];
        v[0] = x1; v[1] = y1; v[2] = 0.0f;
        v[3] = x2; v[4] = y2; v[5] = 0.0f;

        glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL();
        glVertexAttribPointer(CurrentShader->LocPosition, 3, GL_FLOAT, GL_FALSE, 0, v); CHECK_GL();
        glDrawArrays(GL_LINES, 0, 2); CHECK_GL();
    // Graphics::Restore();
}
PUBLIC STATIC void     GLRenderer::StrokeCircle(float x, float y, float rad) {
    Graphics::Save();
    Graphics::Translate(x, y, 0.0f);
    Graphics::Scale(rad, rad, 1.0f);
        GL_Predraw(NULL);

        glBindBuffer(GL_ARRAY_BUFFER, BufferCircleStroke); CHECK_GL();
        glVertexAttribPointer(CurrentShader->LocPosition, 3, GL_FLOAT, GL_FALSE, 0, 0); CHECK_GL();
        glDrawArrays(GL_LINE_STRIP, 0, 361); CHECK_GL();
    Graphics::Restore();
}
PUBLIC STATIC void     GLRenderer::StrokeEllipse(float x, float y, float w, float h) {
    Graphics::Save();
    Graphics::Translate(x + w / 2, y + h / 2, 0.0f);
    Graphics::Scale(w / 2, h / 2, 1.0f);
        GL_Predraw(NULL);

        glBindBuffer(GL_ARRAY_BUFFER, BufferCircleStroke); CHECK_GL();
        glVertexAttribPointer(CurrentShader->LocPosition, 3, GL_FLOAT, GL_FALSE, 0, 0); CHECK_GL();
        glDrawArrays(GL_LINE_STRIP, 0, 361); CHECK_GL();
    Graphics::Restore();
}
PUBLIC STATIC void     GLRenderer::StrokeRectangle(float x, float y, float w, float h) {
    StrokeLine(x, y, x + w, y);
    StrokeLine(x, y + h, x + w, y + h);

    StrokeLine(x, y, x, y + h);
    StrokeLine(x + w, y, x + w, y + h);
}
PUBLIC STATIC void     GLRenderer::FillCircle(float x, float y, float rad) {
    #ifdef GL_SUPPORTS_SMOOTHING
        if (Graphics::SmoothFill) {
            glEnable(GL_POLYGON_SMOOTH); CHECK_GL();
        }
    #endif

    Graphics::Save();
    Graphics::Translate(x, y, 0.0f);
    Graphics::Scale(rad, rad, 1.0f);
        GL_Predraw(NULL);

        glBindBuffer(GL_ARRAY_BUFFER, BufferCircleFill); CHECK_GL();
        glVertexAttribPointer(CurrentShader->LocPosition, 3, GL_FLOAT, GL_FALSE, 0, 0); CHECK_GL();
        glDrawArrays(GL_TRIANGLE_FAN, 0, 362); CHECK_GL();
    Graphics::Restore();

    #ifdef GL_SUPPORTS_SMOOTHING
        if (Graphics::SmoothFill) {
            glDisable(GL_POLYGON_SMOOTH); CHECK_GL();
        }
    #endif
}
PUBLIC STATIC void     GLRenderer::FillEllipse(float x, float y, float w, float h) {
    #ifdef GL_SUPPORTS_SMOOTHING
        if (Graphics::SmoothFill) {
            glEnable(GL_POLYGON_SMOOTH); CHECK_GL();
        }
    #endif

    Graphics::Save();
    Graphics::Translate(x + w / 2, y + h / 2, 0.0f);
    Graphics::Scale(w / 2, h / 2, 1.0f);
        GL_Predraw(NULL);

        glBindBuffer(GL_ARRAY_BUFFER, BufferCircleFill); CHECK_GL();
        glVertexAttribPointer(CurrentShader->LocPosition, 3, GL_FLOAT, GL_FALSE, 0, 0); CHECK_GL();
        glDrawArrays(GL_TRIANGLE_FAN, 0, 362); CHECK_GL();
    Graphics::Restore();

    #ifdef GL_SUPPORTS_SMOOTHING
        if (Graphics::SmoothFill) {
            glDisable(GL_POLYGON_SMOOTH); CHECK_GL();
        }
    #endif
}
PUBLIC STATIC void     GLRenderer::FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3) {
    #ifdef GL_SUPPORTS_SMOOTHING
        if (Graphics::SmoothFill) {
            glEnable(GL_POLYGON_SMOOTH); CHECK_GL();
        }
    #endif

    GL_Vec2 v[3];
    v[0] = GL_Vec2 { x1, y1 };
    v[1] = GL_Vec2 { x2, y2 };
    v[2] = GL_Vec2 { x3, y3 };

    GL_Predraw(NULL);
    glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL();
    glVertexAttribPointer(CurrentShader->LocPosition, 2, GL_FLOAT, GL_FALSE, 0, v); CHECK_GL();
    glDrawArrays(GL_TRIANGLES, 0, 3); CHECK_GL();

    #ifdef GL_SUPPORTS_SMOOTHING
        if (Graphics::SmoothFill) {
            glDisable(GL_POLYGON_SMOOTH); CHECK_GL();
        }
    #endif
}
PUBLIC STATIC void     GLRenderer::FillRectangle(float x, float y, float w, float h) {
    #ifdef GL_SUPPORTS_SMOOTHING
        if (Graphics::SmoothFill) {
            glEnable(GL_POLYGON_SMOOTH); CHECK_GL();
        }
    #endif

    Graphics::Save();
    // Graphics::Translate(x, y, 0.0f);
    // Graphics::Scale(w, h, 1.0f);
        GL_Predraw(NULL);

        GL_Vec2 v[4];
        v[0] = GL_Vec2 { x, y };
        v[1] = GL_Vec2 { x + w, y };
        v[2] = GL_Vec2 { x, y + h };
        v[3] = GL_Vec2 { x + w, y + h };
        glBindBuffer(GL_ARRAY_BUFFER, 0); CHECK_GL();
        glVertexAttribPointer(GLRenderer::CurrentShader->LocPosition, 2, GL_FLOAT, GL_FALSE, 0, v); CHECK_GL();
        // glBindBuffer(GL_ARRAY_BUFFER, BufferSquareFill); CHECK_GL();
        // glVertexAttribPointer(GLRenderer::CurrentShader->LocPosition, 2, GL_FLOAT, GL_FALSE, 0, 0); CHECK_GL();

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); CHECK_GL();
    Graphics::Restore();

    #ifdef GL_SUPPORTS_SMOOTHING
        if (Graphics::SmoothFill) {
            glDisable(GL_POLYGON_SMOOTH); CHECK_GL();
        }
    #endif
}
PUBLIC STATIC Uint32   GLRenderer::CreateTexturedShapeBuffer(float* data, int vertexCount) {
    // x, y, z, u, v
    Uint32 bufferID;
    glGenBuffers(1, &bufferID); CHECK_GL();
    glBindBuffer(GL_ARRAY_BUFFER, bufferID); CHECK_GL();
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertexCount * 5, data, GL_STATIC_DRAW); CHECK_GL();
    return bufferID;
}
PUBLIC STATIC void     GLRenderer::DrawTexturedShapeBuffer(Texture* texture, Uint32 bufferID, int vertexCount) {
    GL_Predraw(texture);

    // glEnableVertexAttribArray(GLRenderer::CurrentShader->LocTexCoord);

        glBindBuffer(GL_ARRAY_BUFFER, bufferID); CHECK_GL();
        glVertexAttribPointer(GLRenderer::CurrentShader->LocPosition, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (GLvoid*)0); CHECK_GL();
        glVertexAttribPointer(GLRenderer::CurrentShader->LocTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (GLvoid*)12); CHECK_GL();
        glDrawArrays(GL_TRIANGLES, 0, vertexCount); CHECK_GL();

    // glDisableVertexAttribArray(GLRenderer::CurrentShader->LocTexCoord);
}

// Texture drawing functions
PUBLIC STATIC void     GLRenderer::DrawTexture(Texture* texture, float sx, float sy, float sw, float sh, float x, float y, float w, float h) {
    x *= RetinaScale;
    y *= RetinaScale;
    w *= RetinaScale;
    h *= RetinaScale;
    GL_DrawTexture(texture, sx, sy, sw, sh, x, y, w, h);
}
PUBLIC STATIC void     GLRenderer::DrawSprite(ISprite* sprite, int animation, int frame, int x, int y, bool flipX, bool flipY, float scaleW, float scaleH, float rotation) {
    if (Graphics::SpriteRangeCheck(sprite, animation, frame)) return;

    // /*
    AnimFrame animframe = sprite->Animations[animation].Frames[frame];
    Graphics::Save();
        Graphics::Translate(x, y, 0.0f);
        GL_DrawTextureBuffered(sprite->Spritesheets[animframe.SheetNumber], animframe.ID, ((int)flipY << 1) | (int)flipX);
    Graphics::Restore();
    //*/

    // AnimFrame animframe = sprite->Animations[animation].Frames[frame];
    // float fX = flipX ? -1.0 : 1.0;
    // float fY = flipY ? -1.0 : 1.0;
    // float sw  = animframe.Width;
    // float sh  = animframe.Height;
    //
    // GLRenderer::DrawTexture(sprite->Spritesheets[animframe.SheetNumber],
    //     animframe.X, animframe.Y, sw, sh,
    //     x + fX * animframe.OffsetX,
    //     y + fY * animframe.OffsetY, fX * sw, fY * sh);
}
PUBLIC STATIC void     GLRenderer::DrawSpritePart(ISprite* sprite, int animation, int frame, int sx, int sy, int sw, int sh, int x, int y, bool flipX, bool flipY, float scaleW, float scaleH, float rotation) {
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

    GLRenderer::DrawTexture(sprite->Spritesheets[animframe.SheetNumber],
        animframe.X + sx, animframe.Y + sy,
        sw, sh,
        x + fX * (sx + animframe.OffsetX),
        y + fY * (sy + animframe.OffsetY), fX * sw, fY * sh);
}

PUBLIC STATIC void     GLRenderer::MakeFrameBufferID(ISprite* sprite, AnimFrame* frame) {
    frame->ID = 0;

    float fX[4], fY[4];
    fX[0] = 1.0;    fY[0] = 1.0;
    fX[1] = -1.0;   fY[1] = 1.0;
    fX[2] = 1.0;    fY[2] = -1.0;
    fX[3] = -1.0;   fY[3] = -1.0;

    GL_AnimFrameVert vertices[16];
    GL_AnimFrameVert* vert = &vertices[0];

    if (frame->SheetNumber >= sprite->SpritesheetCount)
        return;
    if (!sprite->Spritesheets[frame->SheetNumber])
        return;

    float texWidth = sprite->Spritesheets[frame->SheetNumber]->Width;
    float texHeight = sprite->Spritesheets[frame->SheetNumber]->Height;

    float ffU0 = frame->X / texWidth;
    float ffV0 = frame->Y / texHeight;
    float ffU1 = (frame->X + frame->Width) / texWidth;
    float ffV1 = (frame->Y + frame->Height) / texHeight;

    float _fX, _fY, ffX0, ffY0, ffX1, ffY1;
    for (int f = 0; f < 4; f++) {
        _fX = fX[f];
        _fY = fY[f];
        ffX0 = _fX * frame->OffsetX;
        ffY0 = _fY * frame->OffsetY;
        ffX1 = _fX * (frame->OffsetX + frame->Width);
        ffY1 = _fY * (frame->OffsetY + frame->Height);
        vert[0] = GL_AnimFrameVert { ffX0, ffY0, ffU0, ffV0 };
        vert[1] = GL_AnimFrameVert { ffX1, ffY0, ffU1, ffV0 };
        vert[2] = GL_AnimFrameVert { ffX0, ffY1, ffU0, ffV1 };
        vert[3] = GL_AnimFrameVert { ffX1, ffY1, ffU1, ffV1 };
        vert += 4;
    }
    glGenBuffers(1, (GLuint*)&frame->ID); CHECK_GL();
    glBindBuffer(GL_ARRAY_BUFFER, frame->ID); CHECK_GL();
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW); CHECK_GL();
}

#endif /* USING_OPENGL */
