#ifdef USING_OPENGL

#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/GL/GLShaderBuilder.h>
#include <Engine/Rendering/GL/GLShaderContainer.h>

std::unordered_map<Uint32, std::string> FeatureNames = {{SHADER_FEATURE_BLENDING, "blending"},
	{SHADER_FEATURE_TEXTURE, "texture"},
	{SHADER_FEATURE_PALETTE, "palette"},
	{SHADER_FEATURE_MATERIALS, "materials"},
	{SHADER_FEATURE_VERTEXCOLORS, "vertex_colors"},
	{SHADER_FEATURE_FOG_LINEAR, "fog_linear"},
	{SHADER_FEATURE_FOG_EXP, "fog_exp"},
	{SHADER_FEATURE_FILTER_BW, "filter_bw"},
	{SHADER_FEATURE_FILTER_INVERT, "filter_invert"},
	{SHADER_FEATURE_TINTING, "tinting"},
	{SHADER_FEATURE_TINT_DEST, "tint_dest"},
	{SHADER_FEATURE_TINT_BLEND, "tint_blend"}};

void GLShaderContainer::Init() {
	Translation[0] = 0;
	Shaders[0] = Generate(0);
}

void GLShaderContainer::Precompile() {
	Log::Print(Log::LOG_VERBOSE, "Compiling shaders...");

	double elapsed = Clock::GetTicks();

	for (size_t flags = 0; flags <= SHADER_FEATURE_ALL_MASK; flags++) {
		// Some features make no sense when combined, so those combinations are skipped.
		if ((flags & SHADER_FEATURE_FILTER_FLAGS) == SHADER_FEATURE_FILTER_FLAGS) {
			continue;
		}
		if ((flags & SHADER_FEATURE_FOG_FLAGS) == SHADER_FEATURE_FOG_FLAGS) {
			continue;
		}

		// SHADER_FEATURE_PALETTE isn't very useful without SHADER_FEATURE_TEXTURE
		if ((flags & (SHADER_FEATURE_TEXTURE | SHADER_FEATURE_PALETTE)) == SHADER_FEATURE_PALETTE) {
			continue;
		}

		// Likewise, tint flags without SHADER_FEATURE_TINTING don't do anything useful
		if ((flags & SHADER_FEATURE_TINT_FLAGS) != 0 && (flags & SHADER_FEATURE_TINTING) == 0) {
			continue;
		}

		Get(flags);
	}

	elapsed = Clock::GetTicks() - elapsed;

	Log::Print(Log::LOG_VERBOSE, "Compiled %d shaders, took %.1f ms", NumShaders, elapsed);
}

GLShader* GLShaderContainer::Generate(Uint32 features) {
	GLShaderLinkage vsIn = {0};
	GLShaderLinkage vsOut = {0};
	GLShaderLinkage fsIn = {0};
	GLShaderUniforms vsUni = {0};
	GLShaderUniforms fsUni = {0};
	GLShaderOptions options = {0};

	bool useBlending = features & SHADER_FEATURE_BLENDING;
	bool useTexturing = features & SHADER_FEATURE_TEXTURE;
	bool useVertexColors = features & SHADER_FEATURE_VERTEXCOLORS;
	bool useMaterials = features & SHADER_FEATURE_MATERIALS;

	Uint32 filterFlags = features & SHADER_FEATURE_FILTER_FLAGS;
	Uint32 tintFlags = features & SHADER_FEATURE_TINT_FLAGS;

	vsIn.link_position = true;
	vsUni.u_matrix = true;
	fsUni.u_color = useBlending;
	fsUni.u_tintColor = tintFlags != 0;
	fsUni.u_materialColors = useMaterials;

	switch (filterFlags) {
	case SHADER_FEATURE_FILTER_BW:
		options.Filter = Filter_BLACK_AND_WHITE;
		break;
	case SHADER_FEATURE_FILTER_INVERT:
		options.Filter = Filter_INVERT;
		break;
	}

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

	if (features & SHADER_FEATURE_FOG_FLAGS) {
		options.FogEnabled = true;

		vsOut.link_position = fsIn.link_position = true;

		if (features & SHADER_FEATURE_FOG_LINEAR) {
			fsUni.u_fog_linear = true;
		}
		else if (features & SHADER_FEATURE_FOG_EXP) {
			fsUni.u_fog_exp = true;
		}
	}

	GLShaderBuilder vs = GLShaderBuilder::Vertex(vsIn, vsOut, vsUni, options);
	GLShaderBuilder fs = GLShaderBuilder::Fragment(fsIn, fsUni, options);

	// Be aware that this may throw an exception.
	GLShader* shader = new GLShader(vs.GetText(), fs.GetText());

	return shader;
}

void GLShaderContainer::PrintFeaturesWarning(Uint32 oldFeatures, Uint32 newFeatures) {
	std::string text;
	Log::Print(Log::LOG_WARN, "The following shader feature combination:");

	text = GetFeaturesString(oldFeatures);
	Log::Print(Log::LOG_WARN, "    %s", text.c_str());

	Log::Print(Log::LOG_WARN, "was not valid. Compiling the shader with the following features instead:");

	text = GetFeaturesString(newFeatures);
	Log::Print(Log::LOG_WARN, "    %s", text.c_str());
}

GLShader* GLShaderContainer::Compile(Uint32& features) {
	do {
		try {
			return Generate(features);
		} catch (const std::runtime_error& error) {
			Log::Print(Log::LOG_ERROR,
				"Could not compile shader! Error:\n%s",
				error.what());

#define REMOVE(flags) \
	if (features & flags) { \
		Uint32 newFeatures = features & ~flags; \
		PrintFeaturesWarning(features, newFeatures); \
		features = newFeatures; \
		continue; \
	}

			// Attempt to remove problematic features until the shader compiles.
			REMOVE(SHADER_FEATURE_TINT_DEST)
			REMOVE(SHADER_FEATURE_TINT_BLEND)
			REMOVE(SHADER_FEATURE_TINTING)
			REMOVE(SHADER_FEATURE_FILTER_FLAGS)
			REMOVE(SHADER_FEATURE_FOG_FLAGS)
			REMOVE(SHADER_FEATURE_VERTEXCOLORS)
			REMOVE(SHADER_FEATURE_MATERIALS)
			REMOVE(SHADER_FEATURE_PALETTE)
			REMOVE(SHADER_FEATURE_TEXTURE)
			REMOVE(SHADER_FEATURE_BLENDING)

#undef REMOVE

			throw;
		}
	} while (true);
}
GLShader* GLShaderContainer::CompileNoFeatures() {
	try {
		return Generate(0);
	} catch (const std::runtime_error& error) {
		Log::Print(Log::LOG_ERROR, "Could not compile shader! Error:\n%s", error.what());
	}

	return nullptr;
}

Uint32 GLShaderContainer::ValidateFeatures(Uint32 features) {
	if ((features & SHADER_FEATURE_FILTER_FLAGS) == SHADER_FEATURE_FILTER_FLAGS) {
		features &= ~SHADER_FEATURE_FILTER_FLAGS;
	}
	if ((features & SHADER_FEATURE_FOG_FLAGS) == SHADER_FEATURE_FOG_FLAGS) {
		features &= ~SHADER_FEATURE_FOG_FLAGS;
	}
	if ((features & (SHADER_FEATURE_TEXTURE | SHADER_FEATURE_PALETTE)) == SHADER_FEATURE_PALETTE) {
		features &= ~SHADER_FEATURE_PALETTE;
	}
	if ((features & SHADER_FEATURE_TINT_FLAGS) != 0 && (features & SHADER_FEATURE_TINTING) == 0) {
		features &= ~SHADER_FEATURE_TINT_FLAGS;
	}

	return features;
}

std::string GLShaderContainer::GetFeatureName(Uint32 featureFlags) {
	std::unordered_map<Uint32, std::string>::iterator it = FeatureNames.find(featureFlags);
	if (it != FeatureNames.end()) {
		return it->second;
	}

	return "unknown";
}

std::string GLShaderContainer::GetFeaturesString(Uint32 features) {
	std::string text = "";

	for (size_t i = 0, j = 0; i < NUM_SHADER_FEATURES; i++) {
		Uint32 featureFlags = 1 << i;
		if ((features & featureFlags) == 0) {
			continue;
		}

		if (j > 0) {
			text += ", ";
		}

		text += GetFeatureName(featureFlags);

		j++;
	}

	return text;
}

GLShader* GLShaderContainer::Get() {
	return Get(0);
}
GLShader* GLShaderContainer::Get(Uint32 featureFlags) {
	Uint32 features = featureFlags & SHADER_FEATURE_ALL_MASK;

	std::unordered_map<Uint32, Uint32>::iterator it = Translation.find(features);
	if (it != Translation.end()) {
		return Shaders[it->second];
	}

#ifdef SHADER_DEBUG
	std::string featuresText = GetFeaturesString(features);
	Log::Print(Log::LOG_VERBOSE, "Compiling shader \"%s\"...", featuresText.c_str());
#endif

	GLShader* shader = nullptr;
	Uint32 actualFeatures = ValidateFeatures(features);

	if (actualFeatures != features) {
		PrintFeaturesWarning(features, actualFeatures);
	}

	try {
		shader = Compile(actualFeatures);
	} catch (const std::runtime_error& error) {
		shader = CompileNoFeatures();
		actualFeatures = 0;

		if (shader == nullptr) {
			Log::Print(Log::LOG_ERROR, "No variant of this shader was valid!");
		}
	}

	Shaders[actualFeatures] = shader;
	Translation[features] = actualFeatures;

	if (shader != nullptr) {
		NumShaders++;
	}

	return shader;
}

GLShaderContainer::~GLShaderContainer() {
	for (auto it = Shaders.begin(); it != Shaders.end(); it++) {
		delete it->second;
	}
}

#endif /* USING_OPENGL */
