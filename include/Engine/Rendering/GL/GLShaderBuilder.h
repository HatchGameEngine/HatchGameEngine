#ifndef ENGINE_RENDERING_GL_GLSHADERBUILDER_H
#define ENGINE_RENDERING_GL_GLSHADERBUILDER_H

#include <Engine/Rendering/GL/GLShader.h>
#include <Engine/Rendering/GL/ShaderIncludes.h>

namespace GLShaderBuilder {
//private:
	void AddUniformsToShaderText(std::string& shaderText, GLShaderUniforms uniforms);
	void AddInputsToVertexShaderText(std::string& shaderText, GLShaderLinkage inputs);
	void AddOutputsToVertexShaderText(std::string& shaderText, GLShaderLinkage outputs);
	void AddInputsToFragmentShaderText(std::string& shaderText, GLShaderLinkage& inputs);
	string BuildFragmentShaderMainFunc(GLShaderLinkage& inputs,
		GLShaderUniforms& uniforms);

//public:
	string
	Vertex(GLShaderLinkage& inputs, GLShaderLinkage& outputs, GLShaderUniforms& uniforms);
	string
	Fragment(GLShaderLinkage& inputs, GLShaderUniforms& uniforms, std::string mainText);
	string Fragment(GLShaderLinkage& inputs, GLShaderUniforms& uniforms);
};

#endif /* ENGINE_RENDERING_GL_GLSHADERBUILDER_H */
