#ifdef USING_OPENGL

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
	if (Uniforms.u_materialColors) {
		AddUniform("u_diffuseColor", Shader::DATATYPE_FLOAT_VEC4);
		AddUniform("u_specularColor", Shader::DATATYPE_FLOAT_VEC4);
		AddUniform("u_ambientColor", Shader::DATATYPE_FLOAT_VEC4);
	}
	if (Uniforms.u_texture) {
		AddUniform(UNIFORM_TEXTURE, Shader::DATATYPE_SAMPLER_2D);
		if (Uniforms.u_palette) {
			AddUniform("u_numTexturePaletteIndices", Shader::DATATYPE_INT);
		}
	}
#ifdef GL_HAVE_YUV
	if (Uniforms.u_yuv) {
		AddUniform(UNIFORM_TEXTUREU, Shader::DATATYPE_SAMPLER_2D);
		AddUniform(UNIFORM_TEXTUREV, Shader::DATATYPE_SAMPLER_2D);
	}
#endif
	if (Uniforms.u_fog_exp || Uniforms.u_fog_linear) {
		AddUniform("u_fogColor", Shader::DATATYPE_FLOAT_VEC4);
		AddUniform("u_fogSmoothness", Shader::DATATYPE_FLOAT);

		if (Uniforms.u_fog_linear) {
			AddUniform("u_fogLinearStart", Shader::DATATYPE_FLOAT);
			AddUniform("u_fogLinearEnd", Shader::DATATYPE_FLOAT);
		}
		else {
			AddUniform("u_fogDensity", Shader::DATATYPE_FLOAT);
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
	std::string colorVariable = Inputs.link_color ? "o_color" : "u_color";

	AddText("void main() {\n");
	AddText("if (" + colorVariable + ".a == 0.0) discard;\n");

	if (Uniforms.u_texture) {
		AddText("vec4 texel = ");
		if (Uniforms.u_palette) {
			AddText("hatch_sampleTexture2D(" UNIFORM_TEXTURE
				", o_uv, u_numTexturePaletteIndices);\n");
		}
		else {
			AddText("texture2D(" UNIFORM_TEXTURE ", o_uv);\n");
		}

		AddText("if (texel.a == 0.0) discard;\n");
		AddText("vec4 finalColor = texel * ");
	}
	else {
		AddText("vec4 finalColor = ");
	}

	AddText(colorVariable + ";\n");

	if (Uniforms.u_materialColors) {
		AddText("if (u_diffuseColor.a == 0.0) discard;\n");
		AddText("finalColor *= u_diffuseColor;\n");
	}

	if (Uniforms.u_fog_linear || Uniforms.u_fog_exp) {
		AddText("float fogCoord = abs(o_position.z / o_position.w);\n");
		AddText("float fogValue = ");
		if (Uniforms.u_fog_linear) {
			AddText("hatch_fogCalcLinear(fogCoord, u_fogLinearStart, u_fogLinearEnd);\n");
		}
		else {
			AddText("hatch_fogCalcExp(fogCoord, u_fogDensity);\n");
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

std::string GLShaderBuilder::GetText() {
	return Text;
}

GLShaderBuilder GLShaderBuilder::Vertex(GLShaderLinkage inputs,
	GLShaderLinkage outputs,
	GLShaderUniforms uniforms) {
	GLShaderBuilder builder;
	builder.Inputs = inputs;
	builder.Outputs = outputs;
	builder.Uniforms = uniforms;

#ifdef GL_ES
	builder.AddText("precision mediump float;\n");
#endif

	builder.AddInputsToVertexShaderText();
	builder.AddOutputsToVertexShaderText();
	builder.AddUniformsToShaderText();
	builder.BuildVertexShaderMainFunc();

	return builder;
}
GLShaderBuilder
GLShaderBuilder::Fragment(GLShaderLinkage inputs, GLShaderUniforms uniforms, std::string mainText) {
	GLShaderBuilder builder;
	builder.Inputs = inputs;
	builder.Uniforms = uniforms;

#ifdef GL_ES
	builder.AddText("precision mediump float;\n");
#endif

	if (uniforms.u_palette) {
		builder.AddDefine("TEXTURE_SAMPLING_FUNCTIONS");
	}
	if (uniforms.u_fog_linear || uniforms.u_fog_exp) {
		builder.AddDefine("FOG_FUNCTIONS");
	}

	builder.AddInputsToFragmentShaderText();
	builder.AddUniformsToShaderText();

	if (mainText != "") {
		builder.AddText(mainText);
	}
	else {
		builder.BuildFragmentShaderMainFunc();
	}

	return builder;
}
GLShaderBuilder GLShaderBuilder::Fragment(GLShaderLinkage inputs, GLShaderUniforms uniforms) {
	return Fragment(inputs, uniforms, "");
}

#endif /* USING_OPENGL */
