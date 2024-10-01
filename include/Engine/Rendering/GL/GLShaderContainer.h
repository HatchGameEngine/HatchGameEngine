#ifndef ENGINE_RENDERING_GL_GLSHADERCONTAINER_H
#define ENGINE_RENDERING_GL_GLSHADERCONTAINER_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED

class GLShader;
class GLShader;
class GLShader;

#include <Engine/Rendering/GL/GLShader.h>
#include <Engine/Rendering/GL/ShaderIncludes.h>

class GLShaderContainer {
public:
    GLShader *Base = nullptr;
    GLShader *Textured = nullptr;
    GLShader *PalettizedTextured = nullptr;

    GLShaderContainer();
    GLShaderContainer(GLShaderLinkage vsIn, GLShaderLinkage vsOut, GLShaderLinkage fsIn, GLShaderUniforms vsUni, GLShaderUniforms fsUni);
    GLShader* Get(bool useTexturing, bool usePalette);
    GLShader* Get(bool useTexturing);
    GLShader* Get();
    static GLShaderContainer* Make(bool use_vertex_colors);
    static GLShaderContainer* MakeFog(int fog_type);
    static GLShaderContainer* MakeYUV();
    ~GLShaderContainer();
};

#endif /* ENGINE_RENDERING_GL_GLSHADERCONTAINER_H */
