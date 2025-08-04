#ifndef ENGINE_RENDERING_GL_GLSHADERCONTAINER_H
#define ENGINE_RENDERING_GL_GLSHADERCONTAINER_H
class GLShader;

#include <Engine/Rendering/GL/GLShader.h>
#include <Engine/Rendering/GL/ShaderIncludes.h>

class GLShaderContainer {
public:
	void Init();
	void Precompile();

	GLShader* Get();
	GLShader* Get(Uint32 featureFlags);
	~GLShaderContainer();

private:
	std::unordered_map<Uint32, GLShader*> Shaders;
	std::unordered_map<Uint32, Uint32> Translation;

	unsigned NumShaders = 0;

	static GLShader* Compile(Uint32& features);
	static GLShader* CompileNoFeatures();
	static GLShader* Generate(Uint32 features);
};

#endif /* ENGINE_RENDERING_GL_GLSHADERCONTAINER_H */
