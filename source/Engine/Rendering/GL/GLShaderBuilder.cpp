#ifdef USING_OPENGL

#include <Engine/Rendering/GL/GLShaderBuilder.h>

void GLShaderBuilder::AddUniformsToShaderText(std::string& shaderText, GLShaderUniforms uniforms) {
	if (uniforms.u_matrix) {
		shaderText += "uniform mat4 u_projectionMatrix;\n";
		shaderText += "uniform mat4 u_viewMatrix;\n";
		shaderText += "uniform mat4 u_modelMatrix;\n";
	}
	if (uniforms.u_color) {
		shaderText += "uniform vec4 u_color;\n";
	}
	if (uniforms.u_materialColors) {
		shaderText += "uniform vec4 u_diffuseColor;\n";
		shaderText += "uniform vec4 u_specularColor;\n";
		shaderText += "uniform vec4 u_ambientColor;\n";
	}
	if (uniforms.u_texture) {
		shaderText += "uniform sampler2D " UNIFORM_TEXTURE ";\n";
		if (uniforms.u_palette) {
			shaderText += "uniform int u_numTexturePaletteIndices;\n";
		}
	}
#ifdef GL_HAVE_YUV
	if (uniforms.u_yuv) {
		shaderText += "uniform sampler2D " UNIFORM_TEXTURE ";\n";
		shaderText += "uniform sampler2D " UNIFORM_TEXTUREU ";\n";
		shaderText += "uniform sampler2D " UNIFORM_TEXTUREV ";\n";
	}
#endif
	if (uniforms.u_fog_exp || uniforms.u_fog_linear) {
		shaderText += "uniform vec4 u_fogColor;\n";
		shaderText += "uniform float u_fogSmoothness;\n";
		if (uniforms.u_fog_linear) {
			shaderText += "uniform float u_fogLinearStart;\n";
			shaderText += "uniform float u_fogLinearEnd;\n";
		}
		else {
			shaderText += "uniform float u_fogDensity;\n";
		}
	}
}
void GLShaderBuilder::AddInputsToVertexShaderText(std::string& shaderText, GLShaderLinkage inputs) {
	if (inputs.link_position) {
		shaderText += "attribute vec3 " ATTRIB_POSITION ";\n";
	}
	if (inputs.link_uv) {
		shaderText += "attribute vec2 " ATTRIB_UV ";\n";
	}
	if (inputs.link_color) {
		shaderText += "attribute vec4 " ATTRIB_COLOR ";\n";
	}
}
void GLShaderBuilder::AddOutputsToVertexShaderText(std::string& shaderText,
	GLShaderLinkage outputs) {
	if (outputs.link_position) {
		shaderText += "varying vec4 o_position;\n";
	}
	if (outputs.link_uv) {
		shaderText += "varying vec2 o_uv;\n";
	}
	if (outputs.link_color) {
		shaderText += "varying vec4 o_color;\n";
	}
}
void GLShaderBuilder::AddInputsToFragmentShaderText(std::string& shaderText,
	GLShaderLinkage& inputs) {
	AddOutputsToVertexShaderText(shaderText, inputs);
}
string GLShaderBuilder::BuildFragmentShaderMainFunc(GLShaderLinkage& inputs,
	GLShaderUniforms& uniforms) {
	std::string shaderText;
	std::string colorVariable = inputs.link_color ? "o_color" : "u_color";

	shaderText += "void main() {\n";
	shaderText += "if (" + colorVariable + ".a == 0.0) discard;\n";

	if (uniforms.u_texture) {
		shaderText += "vec4 texel = ";
		if (uniforms.u_palette) {
			shaderText += "hatch_sampleTexture2D(" UNIFORM_TEXTURE
				      ", o_uv, u_numTexturePaletteIndices);\n";
		}
		else {
			shaderText += "texture2D(" UNIFORM_TEXTURE ", o_uv);\n";
		}

		shaderText += "if (texel.a == 0.0) discard;\n";
		shaderText += "vec4 finalColor = texel * ";
	}
	else {
		shaderText += "vec4 finalColor = ";
	}

	shaderText += colorVariable + ";\n";

	if (uniforms.u_materialColors) {
		shaderText += "if (u_diffuseColor.a == 0.0) discard;\n";
		shaderText += "finalColor *= u_diffuseColor;\n";
	}

	if (uniforms.u_fog_linear || uniforms.u_fog_exp) {
		shaderText += "float fogCoord = abs(o_position.z / o_position.w);\n";
		shaderText += "float fogValue = ";
		if (uniforms.u_fog_linear) {
			shaderText +=
				"hatch_fogCalcLinear(fogCoord, u_fogLinearStart, u_fogLinearEnd);\n";
		}
		else {
			shaderText += "hatch_fogCalcExp(fogCoord, u_fogDensity);\n";
		}
		shaderText += "if (u_fogSmoothness != 1.0) {\n";
		shaderText += "fogValue = hatch_applyFogSmoothness(fogValue, u_fogSmoothness);\n";
		shaderText += "}\n";
		shaderText += "finalColor = mix(finalColor, u_fogColor, fogValue);\n";
	}

	shaderText += "gl_FragColor = finalColor;\n";
	shaderText += "}";

	return shaderText;
}

string GLShaderBuilder::Vertex(GLShaderLinkage& inputs,
	GLShaderLinkage& outputs,
	GLShaderUniforms& uniforms) {
	std::string shaderText = "";

#ifdef GL_ES
	shaderText += "precision mediump float;\n";
#endif

	AddInputsToVertexShaderText(shaderText, inputs);
	AddOutputsToVertexShaderText(shaderText, outputs);
	AddUniformsToShaderText(shaderText, uniforms);

	shaderText += "void main() {\n";
	shaderText += "mat4 modelViewMatrix = u_viewMatrix * u_modelMatrix;\n";
	shaderText += "gl_Position = u_projectionMatrix * modelViewMatrix * vec4(";
	shaderText += ATTRIB_POSITION;
	shaderText += ", 1.0);\n";
	if (outputs.link_position) {
		shaderText += "o_position = modelViewMatrix * vec4(" ATTRIB_POSITION ", 1.0);\n";
	}
	if (outputs.link_color) {
		shaderText += "o_color = " ATTRIB_COLOR ";\n";
	}
	if (outputs.link_uv) {
		shaderText += "o_uv = " ATTRIB_UV ";\n";
	}
	shaderText += "}";

	return shaderText;
}
string GLShaderBuilder::Fragment(GLShaderLinkage& inputs,
	GLShaderUniforms& uniforms,
	std::string mainText) {
	std::string shaderText = "";

#ifdef GL_ES
	shaderText += "precision mediump float;\n";
#endif

	if (uniforms.u_palette) {
		shaderText += "#define TEXTURE_SAMPLING_FUNCTIONS\n";
	}
	if (uniforms.u_fog_linear || uniforms.u_fog_exp) {
		shaderText += "#define FOG_FUNCTIONS\n";
	}

	AddInputsToFragmentShaderText(shaderText, inputs);
	AddUniformsToShaderText(shaderText, uniforms);

	shaderText += mainText;

	return shaderText;
}
string GLShaderBuilder::Fragment(GLShaderLinkage& inputs, GLShaderUniforms& uniforms) {
	return Fragment(inputs, uniforms, BuildFragmentShaderMainFunc(inputs, uniforms));
}

#endif /* USING_OPENGL */
