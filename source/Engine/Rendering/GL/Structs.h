#ifndef ENGINE_RENDERER_GL_STRUCTS
#define ENGINE_RENDERER_GL_STRUCTS

struct GL_Vec3 {
	float x;
	float y;
	float z;
};
struct GL_Vec2 {
	float x;
	float y;
};
struct GL_AnimFrameVert {
	float x;
	float y;
	float u;
	float v;
};
struct GL_TextureData {
	GLuint TextureID;
	GLuint TextureU;
	GLuint TextureV;
	bool YUV;
	bool Framebuffer;
	GLuint FBO;
	GLuint RBO;
	GLenum TextureTarget;
	GLenum TextureStorageFormat;
	GLenum PixelDataFormat;
	GLenum PixelDataType;
	int Slot;
	bool Accessed;
};

#endif /* ENGINE_RENDERER_GL_STRUCTS */
