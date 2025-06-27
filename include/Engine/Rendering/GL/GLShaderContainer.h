#ifndef ENGINE_RENDERING_GL_GLSHADERCONTAINER_H
#define ENGINE_RENDERING_GL_GLSHADERCONTAINER_H
class GLShader;
class GLShader;
class GLShader;

#include <Engine/Rendering/GL/GLShader.h>
#include <Engine/Rendering/GL/ShaderIncludes.h>

class GLShaderContainer {
public:
	GLShader* Base = nullptr;
	GLShader* Textured = nullptr;
	GLShader* PalettizedTextured = nullptr;

	GLShaderContainer();
	GLShaderContainer(GLShaderLinkage vsIn,
		GLShaderLinkage vsOut,
		GLShaderLinkage fsIn,
		GLShaderUniforms vsUni,
		GLShaderUniforms fsUni,
		bool useMaterial);
	GLShader* Get();
	GLShader* GetWithTexturing();
	GLShader* GetWithPalette();
	static GLShaderContainer* Make();
	static GLShaderContainer* Make(bool useMaterial, bool useVertexColors);
	static GLShaderContainer* MakeFog(int fogType);
	static GLShaderContainer* MakeYUV();
	~GLShaderContainer();
};

#endif /* ENGINE_RENDERING_GL_GLSHADERCONTAINER_H */
