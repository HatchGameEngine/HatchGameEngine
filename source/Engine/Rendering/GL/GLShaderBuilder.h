#ifndef ENGINE_RENDERING_GL_GLSHADERBUILDER_H
#define ENGINE_RENDERING_GL_GLSHADERBUILDER_H

#include <Engine/Rendering/GL/GLShader.h>
#include <Engine/Rendering/GL/ShaderIncludes.h>

class GLShaderBuilder {
private:
	std::string Text;

	GLShaderLinkage Inputs;
	GLShaderLinkage Outputs;
	GLShaderUniforms Uniforms;
	GLShaderOptions Options;

public:
	void AddText(std::string text);
	void AddDefine(std::string name);

	void AddUniform(std::string name, Uint8 type);
	void AddAttribute(std::string name, Uint8 type);
	void AddVarying(std::string name, Uint8 type);

	void AddUniformsToShaderText();
	void AddInputsToVertexShaderText();
	void AddOutputsToVertexShaderText();
	void AddInputsToFragmentShaderText();

	void BuildVertexShaderMainFunc();
	void BuildFragmentShaderMainFunc();
#ifdef GL_HAVE_YUV
	void BuildFragmentShaderMainFuncYUV();
#endif

	std::string GetText();

	static GLShaderBuilder Vertex(GLShaderLinkage inputs,
		GLShaderLinkage outputs,
		GLShaderUniforms uniforms,
		GLShaderOptions options);
	static GLShaderBuilder
	Fragment(GLShaderLinkage inputs, GLShaderUniforms uniforms, GLShaderOptions options);
};

#endif /* ENGINE_RENDERING_GL_GLSHADERBUILDER_H */
