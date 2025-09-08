#ifdef USING_OPENGL

#include <Engine/Rendering/Enums.h>
#include <Engine/Rendering/GL/GLShaderBuilder.h>
#include <Engine/Rendering/Shader.h>

std::unordered_map<Uint8, std::string> DataTypeMap = {{Shader::DATATYPE_FLOAT, "float"},
	{Shader::DATATYPE_FLOAT_VEC2, "vec2"},
	{Shader::DATATYPE_FLOAT_VEC3, "vec3"},
	{Shader::DATATYPE_FLOAT_VEC4, "vec4"},
	{Shader::DATATYPE_INT, "int"},
	{Shader::DATATYPE_FLOAT_MAT4, "mat4"},
	{Shader::DATATYPE_SAMPLER_2D, "sampler2D"}};

void GLShaderBuilder::AddText(std::string text) {
	Text += text;
}
void GLShaderBuilder::AddDefine(std::string name) {
	AddText("#define " + name + "\n");
}

void GLShaderBuilder::AddUniform(std::string name, Uint8 type) {
	AddText("uniform " + DataTypeMap[type] + " " + name + ";\n");
}
void GLShaderBuilder::AddAttribute(std::string name, Uint8 type) {
	AddText("attribute " + DataTypeMap[type] + " " + name + ";\n");
}
void GLShaderBuilder::AddVarying(std::string name, Uint8 type) {
	AddText("varying " + DataTypeMap[type] + " " + name + ";\n");
}

void GLShaderBuilder::AddUniformsToShaderText() {
	if (Uniforms.u_matrix) {
		AddUniform("u_projectionMatrix", Shader::DATATYPE_FLOAT_MAT4);
		AddUniform("u_viewMatrix", Shader::DATATYPE_FLOAT_MAT4);
		AddUniform("u_modelMatrix", Shader::DATATYPE_FLOAT_MAT4);
	}
	if (Uniforms.u_color) {
		AddUniform("u_color", Shader::DATATYPE_FLOAT_VEC4);
	}
	if (Uniforms.u_tintColor) {
		AddUniform("u_tintColor", Shader::DATATYPE_FLOAT_VEC4);
	}
	if (Uniforms.u_materialColors) {
		AddUniform("u_diffuseColor", Shader::DATATYPE_FLOAT_VEC4);
		AddUniform("u_specularColor", Shader::DATATYPE_FLOAT_VEC4);
		AddUniform("u_ambientColor", Shader::DATATYPE_FLOAT_VEC4);
	}
	if (Uniforms.u_texture) {
		AddUniform(UNIFORM_TEXTURE, Shader::DATATYPE_SAMPLER_2D);
		if (Uniforms.u_palette) {
			AddUniform(UNIFORM_NUMPALETTECOLORS, Shader::DATATYPE_INT);
		}
	}
#ifdef GL_HAVE_YUV
	if (Uniforms.u_yuv) {
		AddUniform(UNIFORM_TEXTUREU, Shader::DATATYPE_SAMPLER_2D);
		AddUniform(UNIFORM_TEXTUREV, Shader::DATATYPE_SAMPLER_2D);
	}
#endif
	if (Options.FogEnabled) {
		AddUniform("u_fogColor", Shader::DATATYPE_FLOAT_VEC4);
		AddUniform("u_fogSmoothness", Shader::DATATYPE_FLOAT);

		if (Uniforms.u_fog_exp) {
			AddUniform("u_fogDensity", Shader::DATATYPE_FLOAT);
		}
		else {
			AddUniform("u_fogLinearStart", Shader::DATATYPE_FLOAT);
			AddUniform("u_fogLinearEnd", Shader::DATATYPE_FLOAT);
		}
	}
}
void GLShaderBuilder::AddInputsToVertexShaderText() {
	if (Inputs.link_position) {
		AddAttribute(ATTRIB_POSITION, Shader::DATATYPE_FLOAT_VEC3);
	}
	if (Inputs.link_uv) {
		AddAttribute(ATTRIB_UV, Shader::DATATYPE_FLOAT_VEC2);
	}
	if (Inputs.link_color) {
		AddAttribute(ATTRIB_COLOR, Shader::DATATYPE_FLOAT_VEC4);
	}
}
void GLShaderBuilder::AddOutputsToVertexShaderText() {
	if (Outputs.link_position) {
		AddVarying("o_position", Shader::DATATYPE_FLOAT_VEC4);
	}
	if (Outputs.link_uv) {
		AddVarying("o_uv", Shader::DATATYPE_FLOAT_VEC2);
	}
	if (Outputs.link_color) {
		AddVarying("o_color", Shader::DATATYPE_FLOAT_VEC4);
	}
}
void GLShaderBuilder::AddInputsToFragmentShaderText() {
	if (Inputs.link_position) {
		AddVarying("o_position", Shader::DATATYPE_FLOAT_VEC4);
	}
	if (Inputs.link_uv) {
		AddVarying("o_uv", Shader::DATATYPE_FLOAT_VEC2);
	}
	if (Inputs.link_color) {
		AddVarying("o_color", Shader::DATATYPE_FLOAT_VEC4);
	}
}

void GLShaderBuilder::BuildVertexShaderMainFunc() {
	AddText("void main() {\n");
	AddText("mat4 modelViewMatrix = u_viewMatrix * u_modelMatrix;\n");
	AddText("gl_Position = u_projectionMatrix * modelViewMatrix * vec4(");
	AddText(ATTRIB_POSITION);
	AddText(", 1.0);\n");
	if (Outputs.link_position) {
		AddText("o_position = modelViewMatrix * vec4(" ATTRIB_POSITION ", 1.0);\n");
	}
	if (Outputs.link_color) {
		AddText("o_color = " ATTRIB_COLOR ";\n");
	}
	if (Outputs.link_uv) {
		AddText("o_uv = " ATTRIB_UV ";\n");
	}
	AddText("}");
}
void GLShaderBuilder::BuildFragmentShaderMainFunc() {
	AddText("void main() {\n");

	if (Inputs.link_color) {
		AddText("if (o_color.a == 0.0) discard;\n");
	}

	if (Uniforms.u_color) {
		AddText("if (u_color.a == 0.0) discard;\n");
	}

	// Sample screen texture if enabled
	bool sampleScreenTexel = Options.SampleScreenTexture != SCREENTEXTURESAMPLE_DISABLED;
	bool sampleTexel = Uniforms.u_texture;

	if (sampleScreenTexel) {
		AddText("vec4 screenPixel = hatch_sampleScreenTexture();\n");
		AddText("if (screenPixel.a == 0.0) discard;\n");

		// Don't sample texel if not using the main texture as a mask
		if (Options.SampleScreenTexture != SCREENTEXTURESAMPLE_WITH_MASK) {
			sampleTexel = false;
		}
	}

	// Sample main texture if enabled
	if (sampleTexel) {
		AddText("vec4 texel = ");

		if (Uniforms.u_palette) {
			AddText("hatch_sampleTexture2D(");
			AddText(UNIFORM_TEXTURE ", o_uv, " UNIFORM_NUMPALETTECOLORS);
			AddText(");\n");
		}
		else {
			AddText("texture2D(" UNIFORM_TEXTURE ", o_uv);\n");
		}

		AddText("if (texel.a == 0.0) discard;\n");
	}

	// Determine final color
	AddText("vec4 finalColor = ");
	if (sampleScreenTexel) {
		AddText("screenPixel;\n");
		if (sampleTexel) {
			AddText("finalColor.a *= texel.a;\n");
		}
	}
	else if (sampleTexel) {
		AddText("texel;\n");
	}
	else {
		AddText("vec4(1.0, 1.0, 1.0, 1.0);\n");
	}

	// Apply filter if enabled
	if (Options.Filter != Filter_NONE) {
		switch (Options.Filter) {
		case Filter_BLACK_AND_WHITE:
			AddText("float luminance = (finalColor.r * 0.2126) + (finalColor.g * 0.7152) + (finalColor.b * 0.0722);\n");
			AddText("finalColor = vec4(vec3(luminance), finalColor.a);\n");
			break;
		case Filter_INVERT:
			AddText("vec3 inverted = 1.0 - finalColor.rgb;\n");
			AddText("finalColor = vec4(inverted, finalColor.a);\n");
			break;
		}
	}

	// Multiply with diffuse color if available
	if (Uniforms.u_materialColors) {
		AddText("if (u_diffuseColor.a == 0.0) discard;\n");
		AddText("finalColor *= u_diffuseColor;\n");
	}

	// Multiply with color input
	if (Inputs.link_color) {
		AddText("finalColor *= o_color;\n");
	}

	// Multiply with blend color
	if (Uniforms.u_color) {
		AddText("finalColor *= u_color;\n");
	}

	// Apply tint color if enabled
	if (Uniforms.u_tintColor) {
		switch (Options.TintMode) {
		case TintMode_SRC_NORMAL:
		case TintMode_DST_NORMAL:
			AddText("vec3 tintColor = finalColor.rgb * u_tintColor.rgb;\n");
			AddText("finalColor = mix(finalColor, vec4(tintColor, 1.0), u_tintColor.a);\n");
			break;
		case TintMode_SRC_BLEND:
		case TintMode_DST_BLEND:
			AddText("finalColor = mix(finalColor, vec4(u_tintColor.rgb, 1.0), u_tintColor.a);\n");
			break;
		}
	}

	// Do fog calculation if enabled
	if (Options.FogEnabled) {
		AddText("float fogCoord = abs(o_position.z / o_position.w);\n");
		AddText("float fogValue = ");
		if (Uniforms.u_fog_exp) {
			AddText("hatch_fogCalcExp(fogCoord, u_fogDensity);\n");
		}
		else {
			AddText("hatch_fogCalcLinear(fogCoord, u_fogLinearStart, u_fogLinearEnd);\n");
		}
		AddText("\
			if (u_fogSmoothness != 1.0) {\n\
				fogValue = hatch_applyFogSmoothness(fogValue, u_fogSmoothness);\n\
			}\n\
			finalColor = mix(finalColor, u_fogColor, fogValue);\n\
		");
	}

	AddText("gl_FragColor = finalColor;\n");
	AddText("}");
}
#ifdef GL_HAVE_YUV
void GLShaderBuilder::BuildFragmentShaderMainFuncYUV() {
	AddText(R"(
const vec3 offset = vec3(-0.0625, -0.5, -0.5);
const vec3 Rcoeff = vec3(1.164,  0.000,  1.596);
const vec3 Gcoeff = vec3(1.164, -0.391, -0.813);
const vec3 Bcoeff = vec3(1.164,  2.018,  0.000);

void main() {
    vec3 yuv, rgb;
    vec2 uv = o_uv;

    yuv.x = texture2D(u_texture,  uv).r;
    yuv.y = texture2D(u_textureU, uv).r;
    yuv.z = texture2D(u_textureV, uv).r;
    yuv += offset;

    rgb.r = dot(yuv, Rcoeff);
    rgb.g = dot(yuv, Gcoeff);
    rgb.b = dot(yuv, Bcoeff);

    gl_FragColor = vec4(rgb, 1.0) * u_color;
}
)");
}
#endif

std::string GLShaderBuilder::GetText() {
	return Text;
}

GLShaderBuilder GLShaderBuilder::Vertex(GLShaderLinkage inputs,
	GLShaderLinkage outputs,
	GLShaderUniforms uniforms,
	GLShaderOptions options) {
	GLShaderBuilder builder;
	builder.Inputs = inputs;
	builder.Outputs = outputs;
	builder.Uniforms = uniforms;
	builder.Options = options;

#ifdef GL_ES
	builder.AddText("precision mediump float;\n");
#endif

	builder.AddInputsToVertexShaderText();
	builder.AddOutputsToVertexShaderText();
	builder.AddUniformsToShaderText();
	builder.BuildVertexShaderMainFunc();

	return builder;
}
GLShaderBuilder GLShaderBuilder::Fragment(GLShaderLinkage inputs,
	GLShaderUniforms uniforms,
	GLShaderOptions options) {
	GLShaderBuilder builder;
	builder.Inputs = inputs;
	builder.Uniforms = uniforms;
	builder.Options = options;

#ifdef GL_ES
	builder.AddText("precision mediump float;\n");
#endif

	if (uniforms.u_palette) {
		builder.AddDefine("TEXTURE_SAMPLING_FUNCTIONS");
	}
	if (uniforms.u_screenTexture) {
		builder.AddDefine("SCREEN_TEXTURE_INCLUDES");
	}
	if (options.FogEnabled) {
		builder.AddDefine("FOG_FUNCTIONS");
	}

	builder.AddInputsToFragmentShaderText();
	builder.AddUniformsToShaderText();

#ifdef GL_HAVE_YUV
	if (options.IsYUV) {
		builder.BuildFragmentShaderMainFuncYUV();
	}
	else
#endif
	{
		builder.BuildFragmentShaderMainFunc();
	}

	return builder;
}

#endif /* USING_OPENGL */
