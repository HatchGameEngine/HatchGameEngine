#ifdef USING_OPENGL

#include <Engine/Rendering/GL/GLShaderBuilder.h>

void GLShaderBuilder::AddUniformsToShaderText(std::string& shaderText, GLShaderUniforms uniforms) {
	if (uniforms.u_matrix) {
		shaderText += "uniform mat4 u_projectionMatrix;\n";
		shaderText += "uniform mat4 u_modelViewMatrix;\n";
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
		shaderText += "uniform sampler2D u_texture;\n";
	}
	if (uniforms.u_palette) {
		shaderText += "uniform sampler2D u_paletteTexture;\n";
	}
	if (uniforms.u_yuv) {
		shaderText += "uniform sampler2D u_texture;\n";
		shaderText += "uniform sampler2D u_textureU;\n";
		shaderText += "uniform sampler2D u_textureV;\n";
	}
	if (uniforms.u_fog_exp || uniforms.u_fog_linear) {
		shaderText += "uniform vec4 u_fogColor;\n";
		shaderText += "uniform float u_fogTable[256];\n";
	}
	if (uniforms.u_fog_linear) {
		shaderText += "uniform float u_fogLinearStart;\n";
		shaderText += "uniform float u_fogLinearEnd;\n";
	}
	if (uniforms.u_fog_exp) {
		shaderText += "uniform float u_fogDensity;\n";
	}
}
void GLShaderBuilder::AddInputsToVertexShaderText(std::string& shaderText, GLShaderLinkage inputs) {
	if (inputs.link_position) {
		shaderText += "attribute vec3 i_position;\n";
	}
	if (inputs.link_uv) {
		shaderText += "attribute vec2 i_uv;\n";
	}
	if (inputs.link_color) {
		shaderText += "attribute vec4 i_color;\n";
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
	std::string shaderText = "";

	if (uniforms.u_fog_linear) {
		shaderText +=
			"float doFogCalc(float coord, float start, float end) {\n"
			"    float invZ = 1.0 / (coord / 192.0);\n"
			"    float fogValue = (end - (1.0 - invZ)) / (end - start);\n"
			"    int result = int(clamp(fogValue * 255.0, 0.0, 255.0));\n"
			"    return 1.0 - clamp(u_fogTable[result], 0.0, 1.0);\n"
			"}\n";
	}
	else if (uniforms.u_fog_exp) {
		shaderText +=
			"float doFogCalc(float coord, float density) {\n"
			"    float fogValue = exp(-density * (coord / 192.0));\n"
			"    int result = int(clamp(fogValue * 255.0, 0.0, 255.0));\n"
			"    return 1.0 - clamp(u_fogTable[result], 0.0, 1.0);\n"
			"}\n";
	}

	shaderText += "void main() {\n";
	shaderText += "vec4 finalColor;\n";

	if (uniforms.u_texture) {
		if (inputs.link_color) {
			shaderText += "if (o_color.a == 0.0) discard;\n";
			shaderText += "vec4 base = texture2D(u_texture, o_uv);\n";
			if (uniforms.u_palette) {
				shaderText += "if (base.r == 0.0) discard;\n";
				shaderText +=
					"base = texture2D(u_paletteTexture, vec2(base.r, 0.0));\n";
			}
			shaderText += "if (base.a == 0.0) discard;\n";
			shaderText += "finalColor = base * o_color;\n";
			if (uniforms.u_materialColors) {
				shaderText += "finalColor *= u_diffuseColor;\n";
			}
		}
		else {
			shaderText += "vec4 base = texture2D(u_texture, o_uv);\n";
			if (uniforms.u_palette) {
				shaderText += "if (base.r == 0.0) discard;\n";
				shaderText +=
					"base = texture2D(u_paletteTexture, vec2(base.r, 0.0));\n";
			}
			else {
				shaderText += "if (base.a == 0.0) discard;\n";
			}
			shaderText += "finalColor = base * u_color;\n";
		}
	}
	else {
		if (inputs.link_color) {
			shaderText += "if (o_color.a == 0.0) discard;\n";
			shaderText += "finalColor = o_color;\n";
			if (uniforms.u_materialColors) {
				shaderText += "finalColor *= u_diffuseColor;\n";
			}
		}
		else {
			shaderText += "finalColor = u_color;\n";
		}
	}

	if (uniforms.u_fog_linear || uniforms.u_fog_exp) {
		shaderText +=
			"finalColor = mix(finalColor, u_fogColor, doFogCalc(abs(o_position.z / o_position.w), ";
		if (uniforms.u_fog_linear) {
			shaderText += "u_fogLinearStart, u_fogLinearEnd";
		}
		else if (uniforms.u_fog_exp) {
			shaderText += "u_fogDensity";
		}
		shaderText += "));\n";
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
	shaderText += "precision mediump float;\n"
#endif

		AddInputsToVertexShaderText(shaderText, inputs);
	AddOutputsToVertexShaderText(shaderText, outputs);
	AddUniformsToShaderText(shaderText, uniforms);

	shaderText += "void main() {\n";
	shaderText +=
		"gl_Position = u_projectionMatrix * u_modelViewMatrix * vec4(i_position, 1.0);\n";
	if (outputs.link_position) {
		shaderText += "o_position = u_modelViewMatrix * vec4(i_position, 1.0);\n";
	}
	if (outputs.link_color) {
		shaderText += "o_color = i_color;\n";
	}
	if (outputs.link_uv) {
		shaderText += "o_uv = i_uv;\n";
	}
	shaderText += "}";

	return shaderText;
}
string GLShaderBuilder::Fragment(GLShaderLinkage& inputs,
	GLShaderUniforms& uniforms,
	std::string mainText) {
	std::string shaderText = "";

#ifdef GL_ES
	shaderText += "precision mediump float;\n"
#endif

		AddInputsToFragmentShaderText(shaderText, inputs);
	AddUniformsToShaderText(shaderText, uniforms);

	shaderText += mainText;

	return shaderText;
}
string GLShaderBuilder::Fragment(GLShaderLinkage& inputs, GLShaderUniforms& uniforms) {
	return Fragment(inputs, uniforms, BuildFragmentShaderMainFunc(inputs, uniforms));
}

#endif /* USING_OPENGL */
