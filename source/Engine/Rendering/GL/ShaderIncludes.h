#ifndef GL_SHADERINCLUDES_H
#define GL_SHADERINCLUDES_H

struct GLShaderLinkage {
	bool link_position;
	bool link_uv;
	bool link_color;
};

struct GLShaderUniforms {
	bool u_matrix;
	bool u_color;
	bool u_materialColors;
	bool u_texture;
	bool u_palette;
	bool u_yuv;
	bool u_fog_linear;
	bool u_fog_exp;
};

#endif
