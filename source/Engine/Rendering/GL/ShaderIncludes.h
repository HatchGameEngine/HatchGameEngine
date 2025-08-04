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
	bool u_tintColor;
	bool u_materialColors;
	bool u_texture;
	bool u_palette;
	bool u_screenTexture;
	bool u_fog_linear;
	bool u_fog_exp;
#ifdef GL_HAVE_YUV
	bool u_yuv;
#endif
};

#define SCREENTEXTURESAMPLE_DISABLED 0
#define SCREENTEXTURESAMPLE_ENABLED 1
#define SCREENTEXTURESAMPLE_WITH_MASK 2

struct GLShaderOptions {
	Uint8 SampleScreenTexture;
	Uint8 TintMode;
	bool FogEnabled;
#ifdef GL_HAVE_YUV
	bool IsYUV;
#endif
};

#endif
