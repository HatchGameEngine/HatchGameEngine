#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/GL/Includes.h>
#include <Engine/Math/Matrix4x4.h>

#include <Engine/IO/Stream.h>

class GLShader {
public:
    GLuint ProgramID;
    GLuint VertexProgramID;
    GLuint FragmentProgramID;

    GLint  LocProjectionMatrix;
    GLint  LocModelViewMatrix;
    GLint  LocPosition;
    GLint  LocTexCoord;
    GLint  LocTexture;
    GLint  LocTextureU;
    GLint  LocTextureV;
    GLint  LocColor;

    char   FilenameV[256];
    char   FilenameF[256];

    // Cache stuff
    float      CachedBlendColors[4];
    Matrix4x4* CachedProjectionMatrix = NULL;
    Matrix4x4* CachedModelViewMatrix = NULL;
};
#endif

#ifdef USING_OPENGL

#include <Engine/Rendering/GL/GLShader.h>

#include <Engine/Diagnostics/Log.h>

#define CHECK_GL() \
    GLShader::CheckGLError(__LINE__)

PUBLIC        GLShader::GLShader(const GLchar** vertexShaderSource, size_t vsSZ, const GLchar** fragmentShaderSource, size_t fsSZ) {
    GLint compiled = GL_FALSE;
    ProgramID = glCreateProgram(); CHECK_GL();

    VertexProgramID = glCreateShader(GL_VERTEX_SHADER); CHECK_GL();
    glShaderSource(VertexProgramID, vsSZ / sizeof(GLchar*), vertexShaderSource, NULL); CHECK_GL();
    glCompileShader(VertexProgramID); CHECK_GL();
    glGetShaderiv(VertexProgramID, GL_COMPILE_STATUS, &compiled); CHECK_GL();
    if (compiled != GL_TRUE) {
        Log::Print(Log::LOG_ERROR, "Unable to compile vertex shader %d!", VertexProgramID);
        CheckShaderError(VertexProgramID);
        return;
    }

    FragmentProgramID = glCreateShader(GL_FRAGMENT_SHADER); CHECK_GL();
    glShaderSource(FragmentProgramID, fsSZ / sizeof(GLchar*), fragmentShaderSource, NULL); CHECK_GL();
    glCompileShader(FragmentProgramID); CHECK_GL();
    glGetShaderiv(FragmentProgramID, GL_COMPILE_STATUS, &compiled); CHECK_GL();
    if (compiled != GL_TRUE) {
        Log::Print(Log::LOG_ERROR, "Unable to compile fragment shader %d!", FragmentProgramID);
        CheckShaderError(FragmentProgramID);
        return;
    }

    AttachAndLink();
}
PUBLIC        GLShader::GLShader(Stream* streamVS, Stream* streamFS) {
    GLint compiled = GL_FALSE;
    ProgramID = glCreateProgram(); CHECK_GL();

    size_t lenVS = streamVS->Length();
    size_t lenFS = streamFS->Length();

    char* sourceVS = (char*)malloc(lenVS);
    char* sourceFS = (char*)malloc(lenFS);

    // const char* sourceVSMod[2];
    const char* sourceFSMod[2];

    streamVS->ReadBytes(sourceVS, lenVS);
    sourceVS[lenVS] = 0;

    streamFS->ReadBytes(sourceFS, lenFS);
    sourceFS[lenFS] = 0;

    #if GL_ES_VERSION_2_0 || GL_ES_VERSION_3_0
        sourceFSMod[0] = "precision mediump float;\n";
    #else
        sourceFSMod[0] = "\n";
    #endif
    sourceFSMod[1] = sourceFS;

    VertexProgramID = glCreateShader(GL_VERTEX_SHADER); CHECK_GL();

    glShaderSource(VertexProgramID, 1, &sourceVS, NULL); CHECK_GL();
    glCompileShader(VertexProgramID); CHECK_GL();
    glGetShaderiv(VertexProgramID, GL_COMPILE_STATUS, &compiled); CHECK_GL();
    if (compiled != GL_TRUE) {
        Log::Print(Log::LOG_ERROR, "Unable to compile vertex shader %d!", VertexProgramID);
        CheckShaderError(VertexProgramID);
        CheckGLError(__LINE__);

        free(sourceVS);
        free(sourceFS);
        return;
    }

    FragmentProgramID = glCreateShader(GL_FRAGMENT_SHADER); CHECK_GL();
    glShaderSource(FragmentProgramID, 2, sourceFSMod, NULL); CHECK_GL();
    glCompileShader(FragmentProgramID); CHECK_GL();
    glGetShaderiv(FragmentProgramID, GL_COMPILE_STATUS, &compiled); CHECK_GL();
    if (compiled != GL_TRUE) {
        Log::Print(Log::LOG_ERROR, "Unable to compile fragment shader %d!", FragmentProgramID);
        CheckShaderError(FragmentProgramID);
        CheckGLError(__LINE__);

        free(sourceVS);
        free(sourceFS);
        return;
    }

    free(sourceVS);
    free(sourceFS);

    AttachAndLink();
}
PRIVATE void  GLShader::AttachAndLink() {
    glAttachShader(ProgramID, VertexProgramID);                 CHECK_GL();
    glAttachShader(ProgramID, FragmentProgramID);               CHECK_GL();

    glBindAttribLocation(ProgramID, 0, "i_position");           CHECK_GL();
    glBindAttribLocation(ProgramID, 1, "i_uv");                 CHECK_GL();

    glLinkProgram(ProgramID);                                   CHECK_GL();

    GLint isLinked = GL_FALSE;
    glGetProgramiv(ProgramID, GL_LINK_STATUS, (int*)&isLinked); CHECK_GL();
    if (isLinked != GL_TRUE) {
        CheckProgramError(ProgramID);
    }

    LocProjectionMatrix = GetUniformLocation("u_projectionMatrix");
    LocModelViewMatrix = GetUniformLocation("u_modelViewMatrix");

    LocPosition = GetAttribLocation("i_position");
    LocTexCoord = GetAttribLocation("i_uv");
    LocTexture = GetUniformLocation("u_texture");
    LocTextureU = GetUniformLocation("u_textureU");
    LocTextureV = GetUniformLocation("u_textureV");
    LocColor = GetUniformLocation("u_color");

    // printf("Shader:\n");
    // printf("LocPosition: %d\n", LocPosition);
    // printf("LocTexCoord: %d\n", LocTexCoord);
    // printf("LocTexture: %d\n", LocTexture);
    // printf("LocTextureU: %d\n", LocTextureU);
    // printf("LocTextureV: %d\n", LocTextureV);
    // printf("LocColor: %d\n", LocColor);
    // printf("\n");

    CachedBlendColors[0] = CachedBlendColors[1] = CachedBlendColors[2] = CachedBlendColors[3] = 0.0;
}


PUBLIC bool   GLShader::CheckShaderError(GLuint shader) {
    int infoLogLength = 0;
    int maxLength = infoLogLength;

    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength); CHECK_GL();

    char* infoLog = new char[maxLength];
    glGetShaderInfoLog(shader, maxLength, &infoLogLength, infoLog); CHECK_GL();
    infoLog[strlen(infoLog) - 1] = 0;

    if (infoLogLength > 0)
        Log::Print(Log::LOG_ERROR, "%s", infoLog);

    delete[] infoLog;
    return false;
}
PUBLIC bool   GLShader::CheckProgramError(GLuint prog) {
    int infoLogLength = 0;
    int maxLength = infoLogLength;

    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &maxLength); CHECK_GL();

    char* infoLog = new char[maxLength];
    glGetProgramInfoLog(prog, maxLength, &infoLogLength, infoLog); CHECK_GL();
    infoLog[strlen(infoLog) - 1] = 0;

    if (infoLogLength > 0)
        Log::Print(Log::LOG_ERROR, "%s", infoLog);

    delete[] infoLog;
    return false;
}

PUBLIC GLuint GLShader::Use() {
    glUseProgram(ProgramID); CHECK_GL();

    // glEnableVertexAttribArray(1); CHECK_GL();
    return ProgramID;
}

PUBLIC GLint  GLShader::GetAttribLocation(const GLchar* identifier) {
    GLint value = glGetAttribLocation(ProgramID, identifier); CHECK_GL();
    return value;
}
PUBLIC GLint  GLShader::GetUniformLocation(const GLchar* identifier) {
    GLint value = glGetUniformLocation(ProgramID, identifier); CHECK_GL();
    return value;
}

PUBLIC void   GLShader::Dispose() {
    glDeleteProgram(ProgramID); CHECK_GL();
}

PUBLIC STATIC bool GLShader::CheckGLError(int line) {
    const char* errstr = NULL;
    GLenum error = glGetError();
    switch (error) {
        case GL_NO_ERROR: errstr = "no error"; break;
        case GL_INVALID_ENUM: errstr = "invalid enumerant"; break;
        case GL_INVALID_VALUE: errstr = "invalid value"; break;
        case GL_INVALID_OPERATION: errstr = "invalid operation"; break;
        case GL_OUT_OF_MEMORY: errstr = "out of memory"; break;
        #ifdef GL_STACK_OVERFLOW
        case GL_STACK_OVERFLOW: errstr = "stack overflow"; break;
        case GL_STACK_UNDERFLOW: errstr = "stack underflow"; break;
        case GL_TABLE_TOO_LARGE: errstr = "table too large"; break;
        #endif
        #ifdef GL_EXT_framebuffer_object
        case GL_INVALID_FRAMEBUFFER_OPERATION_EXT: errstr = "invalid framebuffer operation"; break;
        #endif
        #if GLU_H
        case GLU_INVALID_ENUM: errstr = "invalid enumerant"; break;
        case GLU_INVALID_VALUE: errstr = "invalid value"; break;
        case GLU_OUT_OF_MEMORY: errstr = "out of memory"; break;
        case GLU_INCOMPATIBLE_GL_VERSION: errstr = "incompatible gl version"; break;
        // case GLU_INVALID_OPERATION: errstr = "invalid operation"; break;
        #endif
        default:
            errstr = "idk";
            break;
    }
    if (error != GL_NO_ERROR) {
        Log::Print(Log::LOG_ERROR, "OpenGL error on line %d: %s", line, errstr);
        return true;
    }
    return false;
}
#undef CHECK_GL

#endif /* USING_OPENGL */
