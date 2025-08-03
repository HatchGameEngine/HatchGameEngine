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
	GLShader* Get(Uint32 features);
	~GLShaderContainer();

private:
	std::unordered_map<Uint32, GLShader*> ShaderMap;

	GLShader* BaseShader = nullptr;
	Uint32 BaseFeatures = 0;

	void Init();

	static GLShader* Generate(Uint32 features);
};

#endif /* ENGINE_RENDERING_GL_GLSHADERCONTAINER_H */
