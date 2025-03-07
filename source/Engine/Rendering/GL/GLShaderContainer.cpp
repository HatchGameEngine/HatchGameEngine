#ifdef USING_OPENGL

#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/GL/GLShaderBuilder.h>
#include <Engine/Rendering/GL/GLShaderContainer.h>

GLShaderContainer::GLShaderContainer() {}

GLShaderContainer::GLShaderContainer(GLShaderLinkage vsIn,
	GLShaderLinkage vsOut,
	GLShaderLinkage fsIn,
	GLShaderUniforms vsUni,
	GLShaderUniforms fsUni,
	bool useMaterial) {
	std::string vs, fs;

	vsIn.link_position = true;
	vsUni.u_matrix = true;
	fsUni.u_color = !fsIn.link_color;
	fsUni.u_materialColors = useMaterial;

	vs = GLShaderBuilder::Vertex(vsIn, vsOut, vsUni);
	fs = GLShaderBuilder::Fragment(fsIn, fsUni);
	Base = new GLShader(vs, fs);

	vsIn.link_uv = true;
	vsOut.link_uv = true;
	fsIn.link_uv = true;
	fsUni.u_texture = true;
	vs = GLShaderBuilder::Vertex(vsIn, vsOut, vsUni);
	fs = GLShaderBuilder::Fragment(fsIn, fsUni);
	Textured = new GLShader(vs, fs);

	fsUni.u_palette = true;
	vs = GLShaderBuilder::Vertex(vsIn, vsOut, vsUni);
	fs = GLShaderBuilder::Fragment(fsIn, fsUni);
	PalettizedTextured = new GLShader(vs, fs);
}

GLShader* GLShaderContainer::Get(bool useTexturing, bool usePalette) {
	if (useTexturing) {
		if (usePalette) {
			return PalettizedTextured;
		}
		else {
			return Textured;
		}
	}

	return Base;
}

GLShader* GLShaderContainer::Get(bool useTexturing) {
	return Get(useTexturing, false);
}

GLShader* GLShaderContainer::Get() {
	return Get(false, false);
}

GLShaderContainer* GLShaderContainer::Make(bool useMaterial, bool useVertexColors) {
	GLShaderLinkage vsIn = {0};
	GLShaderLinkage vsOut = {0};
	GLShaderLinkage fsIn = {0};
	GLShaderUniforms vsUni = {0};
	GLShaderUniforms fsUni = {0};

	vsIn.link_color = useVertexColors;
	vsOut.link_color = useVertexColors;
	fsIn.link_color = useVertexColors;

	return new GLShaderContainer(vsIn, vsOut, fsIn, vsUni, fsUni, useMaterial);
}

GLShaderContainer* GLShaderContainer::Make() {
	return Make(false, false);
}

GLShaderContainer* GLShaderContainer::MakeFog(int fogType) {
	GLShaderLinkage vsIn = {0};
	GLShaderLinkage vsOut = {0};
	GLShaderLinkage fsIn = {0};
	GLShaderUniforms vsUni = {0};
	GLShaderUniforms fsUni = {0};

	vsIn.link_color = true;
	vsOut.link_position = true;
	vsOut.link_color = true;
	fsIn.link_color = true;
	fsIn.link_position = true;

	if (fogType == FogEquation_Linear) {
		fsUni.u_fog_linear = true;
	}
	else {
		fsUni.u_fog_exp = true;
	}

	return new GLShaderContainer(vsIn, vsOut, fsIn, vsUni, fsUni, true);
}

GLShaderContainer* GLShaderContainer::MakeYUV() {
	GLShaderLinkage vsIn = {0};
	GLShaderLinkage vsOut = {0};
	GLShaderLinkage fsIn = {0};
	GLShaderUniforms vsUni = {0};
	GLShaderUniforms fsUni = {0};

	vsIn.link_position = true;
	vsIn.link_uv = true;
	vsOut.link_uv = true;
	fsIn.link_uv = true;
	vsUni.u_matrix = true;
	fsUni.u_color = true;
	fsUni.u_materialColors = false;
	fsUni.u_yuv = true;

	std::string vs = GLShaderBuilder::Vertex(vsIn, vsOut, vsUni);
	std::string fs = GLShaderBuilder::Fragment(fsIn,
		fsUni,
		"const vec3 offset = vec3(-0.0625, -0.5, -0.5);\n"
		"const vec3 Rcoeff = vec3(1.164,  0.000,  1.596);\n"
		"const vec3 Gcoeff = vec3(1.164, -0.391, -0.813);\n"
		"const vec3 Bcoeff = vec3(1.164,  2.018,  0.000);\n"

		"void main() {\n"
		"    vec3 yuv, rgb;\n"
		"    vec2 uv = o_uv;\n"

		"    yuv.x = texture2D(u_texture,  uv).r;\n"
		"    yuv.y = texture2D(u_textureU, uv).r;\n"
		"    yuv.z = texture2D(u_textureV, uv).r;\n"
		"    yuv += offset;\n"

		"    rgb.r = dot(yuv, Rcoeff);\n"
		"    rgb.g = dot(yuv, Gcoeff);\n"
		"    rgb.b = dot(yuv, Bcoeff);\n"
		"    gl_FragColor = vec4(rgb, 1.0) * u_color;\n"
		"}");

	GLShaderContainer* container = new GLShaderContainer();
	container->Textured = new GLShader(vs, fs);
	return container;
}

GLShaderContainer::~GLShaderContainer() {
	if (Base) {
		delete Base;
	}
	if (Textured) {
		delete Textured;
	}
	if (PalettizedTextured) {
		delete PalettizedTextured;
	}
}

#endif /* USING_OPENGL */
