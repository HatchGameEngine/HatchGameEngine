#ifndef ENGINE_RENDERER_METAL_FUNCS
#define ENGINE_RENDERER_METAL_FUNCS

void METAL_Init();
void METAL_Dispose();
Texture* METAL_CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height);
int METAL_LockTexture(Texture* texture, void** pixels, int* pitch);
int METAL_UpdateTexture(Texture* texture, SDL_Rect* r, void* pixels, int pitch);
void METAL_UnlockTexture(Texture* texture);
void METAL_DisposeTexture(Texture* texture);
void METAL_SetRenderTarget(Texture* texture);
void METAL_UpdateWindowSize(int width, int height);
void METAL_UpdateViewport();
void METAL_UpdateClipRect();
void METAL_UpdateOrtho(float left, float top, float right, float bottom);
void METAL_UpdatePerspective(float fovy, float aspect, float nearv, float farv);
void METAL_UpdateProjectionMatrix();
void METAL_Clear();
void METAL_Present();
void METAL_SetBlendColor(float r, float g, float b, float a);
void METAL_SetBlendMode(int srcC, int dstC, int srcA, int dstA);
void METAL_StrokeLine(float x1, float y1, float x2, float y2);
void METAL_StrokeCircle(float x, float y, float rad);
void METAL_StrokeEllipse(float x, float y, float w, float h);
void METAL_StrokeRectangle(float x, float y, float w, float h);
void METAL_FillCircle(float x, float y, float rad);
void METAL_FillEllipse(float x, float y, float w, float h);
void METAL_FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
void METAL_FillRectangle(float x, float y, float w, float h);
void METAL_DrawTexture(Texture* texture,
	float sx,
	float sy,
	float sw,
	float sh,
	float x,
	float y,
	float w,
	float h);

#endif /* ENGINE_RENDERER_METAL_FUNCS */
