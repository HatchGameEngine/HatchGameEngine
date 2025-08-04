#ifndef ENGINE_RENDERING_GL_GLSHADERBUILDER_H
#define ENGINE_RENDERING_GL_GLSHADERBUILDER_H

#include <Engine/Rendering/GL/GLShader.h>
#include <Engine/Rendering/GL/ShaderIncludes.h>

class GLShaderBuilder {
private:
	static void AddUniformsToShaderText(std::string& shaderText, GLShaderUniforms uniforms);
	static void AddInputsToVertexShaderText(std::string& shaderText, GLShaderLinkage inputs);
	static void AddOutputsToVertexShaderText(std::string& shaderText, GLShaderLinkage outputs);
	static void AddInputsToFragmentShaderText(std::string& shaderText, GLShaderLinkage& inputs);
	static std::string BuildTextureSampleCode(GLShaderUniforms& uniforms);
	static std::string BuildFragmentShaderMainFunc(GLShaderLinkage& inputs,
		GLShaderUniforms& uniforms,
		GLShaderOptions& options);

public:
	static std::string
	Vertex(GLShaderLinkage& inputs, GLShaderLinkage& outputs, GLShaderUniforms& uniforms);
	static std::string
	Fragment(GLShaderLinkage& inputs, GLShaderUniforms& uniforms, std::string mainText);
	static std::string
	Fragment(GLShaderLinkage& inputs, GLShaderUniforms& uniforms, GLShaderOptions& options);
};

#endif /* ENGINE_RENDERING_GL_GLSHADERBUILDER_H */
