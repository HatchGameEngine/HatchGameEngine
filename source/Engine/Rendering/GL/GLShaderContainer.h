#ifndef ENGINE_RENDERING_GL_GLSHADERCONTAINER_H
#define ENGINE_RENDERING_GL_GLSHADERCONTAINER_H
class GLShader;

#include <Engine/Rendering/GL/GLShader.h>
#include <Engine/Rendering/GL/ShaderIncludes.h>

#define SHADER_FEATURE_BLENDING (1 << 0)
#define SHADER_FEATURE_TEXTURE (1 << 1)
#define SHADER_FEATURE_PALETTE (1 << 2)
#define SHADER_FEATURE_MATERIALS (1 << 3)
#define SHADER_FEATURE_VERTEXCOLORS (1 << 4)
#define SHADER_FEATURE_FOG_LINEAR (1 << 5)
#define SHADER_FEATURE_FOG_EXP (1 << 6)
#define SHADER_FEATURE_FILTER_BW (1 << 7)
#define SHADER_FEATURE_FILTER_INVERT (1 << 8)
#define SHADER_FEATURE_TINTING (1 << 9)
#define SHADER_FEATURE_TINT_DEST (1 << 10)
#define SHADER_FEATURE_TINT_BLEND (1 << 11)

#define NUM_SHADER_FEATURES 12

#define SHADER_FEATURE_FILTER_FLAGS (SHADER_FEATURE_FILTER_BW | SHADER_FEATURE_FILTER_INVERT)
#define SHADER_FEATURE_TINT_FLAGS \
	(SHADER_FEATURE_TINTING | SHADER_FEATURE_TINT_DEST | SHADER_FEATURE_TINT_BLEND)
#define SHADER_FEATURE_FOG_FLAGS (SHADER_FEATURE_FOG_LINEAR | SHADER_FEATURE_FOG_EXP)

#define SHADER_FEATURE_ALL_MASK ((1 << NUM_SHADER_FEATURES) - 1)

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

	static Uint32 ValidateFeatures(Uint32 features);

	static std::string GetFeatureName(Uint32 featureFlags);
	static std::string GetFeaturesString(Uint32 features);
	static void PrintFeaturesWarning(Uint32 oldFeatures, Uint32 newFeatures);
};

#endif /* ENGINE_RENDERING_GL_GLSHADERCONTAINER_H */
