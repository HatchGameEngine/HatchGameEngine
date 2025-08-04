#ifndef ENGINE_RENDERING_GL_GLSHADERCONTAINER_H
#define ENGINE_RENDERING_GL_GLSHADERCONTAINER_H
class GLShader;

#include <Engine/Rendering/GL/GLShader.h>
#include <Engine/Rendering/GL/ShaderIncludes.h>

class GLShaderContainer {
public:
	GLShaderContainer();
	GLShaderContainer(Uint32 features);
	GLShader* Get();
	GLShader* Get(Uint32 featureFlags);
	~GLShaderContainer();

private:
	std::unordered_map<Uint32, GLShader*> Shaders;
	std::unordered_map<Uint32, Uint32> Translation;

	Uint32 BaseFeatures = 0;

	void Init();

	static GLShader* Compile(Uint32& features);
	static GLShader* CompileNoFeatures();
	static GLShader* Generate(Uint32 features);
};

#endif /* ENGINE_RENDERING_GL_GLSHADERCONTAINER_H */
