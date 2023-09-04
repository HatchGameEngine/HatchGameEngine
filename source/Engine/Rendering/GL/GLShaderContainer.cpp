#if INTERFACE
#include <Engine/Rendering/GL/GLShader.h>
#include <Engine/Rendering/GL/ShaderIncludes.h>

class GLShaderContainer {
public:
    GLShader *Base;
    GLShader *Textured;
    GLShader *PalettizedTextured;
};
#endif

#include <Engine/Rendering/GL/GLShaderBuilder.h>
#include <Engine/Rendering/GL/GLShaderContainer.h>

PUBLIC GLShaderContainer::GLShaderContainer(GLShaderLinkage& vsIn, GLShaderLinkage& vsOut, GLShaderLinkage& fsIn, GLShaderUniforms uniforms) {
    std::string vs = GLShaderBuilder::Vertex(vsIn, vsOut, uniforms);
    std::string fs = GLShaderBuilder::Fragment(fsIn, uniforms);
    Base = new GLShader(vs, fs);

    uniforms.u_texture = true;
    vs = GLShaderBuilder::Vertex(vsIn, vsOut, uniforms);
    fs = GLShaderBuilder::Fragment(fsIn, uniforms);
    Textured = new GLShader(vs, fs);

    uniforms.u_palette = true;
    vs = GLShaderBuilder::Vertex(vsIn, vsOut, uniforms);
    fs = GLShaderBuilder::Fragment(fsIn, uniforms);
    PalettizedTextured = new GLShader(vs, fs);
}
