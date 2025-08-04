#ifdef USING_OPENGL

#include <Engine/Diagnostics/Log.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/GL/GLShaderBuilder.h>
#include <Engine/Rendering/GL/GLShaderContainer.h>

GLShaderContainer::GLShaderContainer() {
	BaseFeatures = 0;

	Init();
}
GLShaderContainer::GLShaderContainer(Uint32 baseFeatures) {
	BaseFeatures = baseFeatures & SHADER_FEATURE_ALL;

	Init();
}

void GLShaderContainer::Init() {
	BaseShader = Generate(BaseFeatures);
}

GLShader* GLShaderContainer::Generate(Uint32 features) {
	GLShaderLinkage vsIn = {0};
	GLShaderLinkage vsOut = {0};
	GLShaderLinkage fsIn = {0};
	GLShaderUniforms vsUni = {0};
	GLShaderUniforms fsUni = {0};
	GLShaderOptions options = {0};

	bool useTexturing = features & SHADER_FEATURE_TEXTURE;
	bool useVertexColors = features & SHADER_FEATURE_VERTEXCOLORS;
	bool useMaterials = features & SHADER_FEATURE_MATERIALS;
	Uint32 tintFlags = features & SHADER_FEATURE_TINT_FLAGS;

	vsIn.link_position = true;
	vsUni.u_matrix = true;
	fsUni.u_color = !useVertexColors;
	fsUni.u_tintColor = tintFlags != 0;
	fsUni.u_materialColors = useMaterials;

	switch (tintFlags) {
	case SHADER_FEATURE_TINTING:
		options.TintMode = TintMode_SRC_NORMAL;
		break;
	case SHADER_FEATURE_TINTING | SHADER_FEATURE_TINT_DEST:
		options.TintMode = TintMode_DST_NORMAL;
		break;
	case SHADER_FEATURE_TINTING | SHADER_FEATURE_TINT_BLEND:
		options.TintMode = TintMode_SRC_BLEND;
		break;
	case SHADER_FEATURE_TINTING | SHADER_FEATURE_TINT_BLEND | SHADER_FEATURE_TINT_DEST:
		options.TintMode = TintMode_DST_BLEND;
		break;
	}

	if (tintFlags & SHADER_FEATURE_TINT_DEST) {
		options.SampleScreenTexture = SCREENTEXTURESAMPLE_WITH_MASK;
	}

	if (options.SampleScreenTexture != SCREENTEXTURESAMPLE_DISABLED) {
		fsUni.u_screenTexture = true;
	}

	vsIn.link_color = vsOut.link_color = fsIn.link_color = useVertexColors;
	vsIn.link_uv = vsOut.link_uv = fsIn.link_uv = useTexturing;
	fsUni.u_texture = useTexturing;
	fsUni.u_palette = features & SHADER_FEATURE_PALETTE;

	if (features & SHADER_FEATURE_FOG_LINEAR) {
		fsUni.u_fog_linear = true;
		vsOut.link_position = fsIn.link_position = true;
	}
	else if (features & SHADER_FEATURE_FOG_EXP) {
		fsUni.u_fog_exp = true;
		vsOut.link_position = fsIn.link_position = true;
	}

	std::string vs = GLShaderBuilder::Vertex(vsIn, vsOut, vsUni);
	std::string fs = GLShaderBuilder::Fragment(fsIn, fsUni, options);

	// Be aware that this may throw an exception.
	GLShader* shader = new GLShader(vs, fs);

	return shader;
}

GLShader* GLShaderContainer::Get() {
	return Get(0);
}
GLShader* GLShaderContainer::Get(Uint32 features) {
	features &= SHADER_FEATURE_ALL;
	if (features == 0) {
		return BaseShader;
	}

	features |= BaseFeatures;

	std::unordered_map<Uint32, GLShader*>::iterator it = ShaderMap.find(features);
	if (it != ShaderMap.end()) {
		return it->second;
	}

	try {
		GLShader* shader = Generate(features);

		ShaderMap[features] = shader;

		return shader;
	} catch (const std::runtime_error& error) {
		Log::Print(Log::LOG_ERROR, "Could not compile shader! Error:\n%s", error.what());
	}

	return BaseShader;
}

GLShaderContainer::~GLShaderContainer() {
	for (auto it = ShaderMap.begin(); it != ShaderMap.end(); it++) {
		delete it->second;
	}
}

#endif /* USING_OPENGL */
