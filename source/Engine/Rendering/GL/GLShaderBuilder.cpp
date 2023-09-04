#if INTERFACE
#include <Engine/Rendering/GL/GLShader.h>
#include <Engine/Rendering/GL/ShaderIncludes.h>

class GLShaderBuilder {
public:

};
#endif

#include <Engine/Rendering/GL/GLShaderBuilder.h>

PRIVATE STATIC void GLShaderBuilder::AddUniformsToShaderText(std::string& shaderText, GLShaderUniforms uniforms) {
    if (uniforms.u_matrix) {
        shaderText += "uniform mat4 u_projectionMatrix;\n";
        shaderText += "uniform mat4 u_modelViewMatrix;\n";
    }
    if (uniforms.u_color) {
        shaderText += "uniform vec4 u_color;\n";
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
        shaderText += "uniform vec4  u_fogColor;\n";
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
PRIVATE STATIC void GLShaderBuilder::AddInputsToVertexShaderText(std::string& shaderText, GLShaderLinkage inputs) {
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
PRIVATE STATIC void GLShaderBuilder::AddOutputsToVertexShaderText(std::string& shaderText, GLShaderLinkage outputs) {
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
PRIVATE STATIC void GLShaderBuilder::AddInputsToFragmentShaderText(std::string& shaderText, GLShaderLinkage& inputs) {
    AddOutputsToVertexShaderText(shaderText, inputs);
}
PRIVATE STATIC string GLShaderBuilder::BuildFragmentShaderMainFunc(GLShaderLinkage& inputs, GLShaderUniforms& uniforms) {
    std::string shaderText = "";

    if (uniforms.u_fog_linear) {
        shaderText += "float doFogCalc(float coord, float start, float end) {\n"
        "    float invZ = 1.0 / (coord / 192.0);\n"
        "    float fogValue = (end - (1.0 - invZ)) / (end - start);\n"
        "    int result = clamp(int(fogValue * 256.0), 0, 255);\n"
        "    return 1.0 - clamp(u_fogTable[result], 0.0, 1.0);\n"
        "}\n";
    }
    else if (uniforms.u_fog_exp) {
        shaderText += "float doFogCalc(float coord, float density) {\n"
        "    float fogValue = exp(-density * coord);\n"
        "    int result = clamp(int(fogValue * 255.0), 0, 255);\n"
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
                shaderText += "base = texture2D(u_palette, vec2(base.r, 0.0));\n";
            }
            shaderText += "if (base.a == 0.0) discard;\n";
            shaderText += "finalColor = base * o_color;\n";
        }
        else {
            shaderText += "vec4 base = texture2D(u_texture, o_uv);\n";
            if (uniforms.u_palette) {
                shaderText += "base = texture2D(u_palette, vec2(base.r, 0.0));\n";
            }
            shaderText += "if (base.a == 0.0) discard;\n";
            shaderText += "finalColor = base * u_color;\n";
        }
    }
    else {
        if (inputs.link_color) {
            shaderText += "if (o_color.a == 0.0) discard;\n";
            shaderText += "finalColor = o_color;";
        }
        else {
            shaderText += "finalColor = u_color;\n";
        }
    }

    if (uniforms.u_fog_linear) {
        shaderText += "finalColor = ";
        shaderText += "mix(finalColor, u_fogColor, doFogCalc(abs(o_position.z / o_position.w), u_fogLinearStart, u_fogLinearEnd));\n";
    }
    else if (uniforms.u_fog_exp) {
        shaderText += "finalColor = ";
        shaderText += "mix(finalColor, u_fogColor, doFogCalc(abs(o_position.z / o_position.w), u_fogDensity));\n";
    }

    shaderText += "gl_FragColor = finalColor;\n";
    shaderText += "}";

    return shaderText;
}

PUBLIC STATIC string GLShaderBuilder::Vertex(GLShaderLinkage& inputs, GLShaderLinkage& outputs, GLShaderUniforms& uniforms) {
    std::string shaderText = "";

#ifdef GL_ES
    shaderText += "precision mediump float;\n"
 #endif

    AddInputsToVertexShaderText(shaderText, inputs);
    AddOutputsToVertexShaderText(shaderText, outputs);
    AddUniformsToShaderText(shaderText, uniforms);

    shaderText += "void main() {\n";
    shaderText += "gl_Position = u_projectionMatrix * u_modelViewMatrix * vec4(i_position, 1.0);\n";
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
PUBLIC STATIC string GLShaderBuilder::Fragment(GLShaderLinkage& inputs, GLShaderUniforms& uniforms, std::string mainText) {
    std::string shaderText = "";

#ifdef GL_ES
    shaderText += "precision mediump float;\n"
#endif

    AddInputsToFragmentShaderText(shaderText, inputs);
    AddUniformsToShaderText(shaderText, uniforms);

    shaderText += mainText;

    return shaderText;
}
PUBLIC STATIC string GLShaderBuilder::Fragment(GLShaderLinkage& inputs, GLShaderUniforms& uniforms) {
    return Fragment(inputs, uniforms, BuildFragmentShaderMainFunc(inputs, uniforms));
}
