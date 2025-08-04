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

	std::string GetText();

	static GLShaderBuilder
	Vertex(GLShaderLinkage inputs, GLShaderLinkage outputs, GLShaderUniforms uniforms);
	static GLShaderBuilder
	Fragment(GLShaderLinkage inputs, GLShaderUniforms uniforms, std::string mainText);
	static GLShaderBuilder Fragment(GLShaderLinkage inputs, GLShaderUniforms uniforms);
};

#endif /* ENGINE_RENDERING_GL_GLSHADERBUILDER_H */
