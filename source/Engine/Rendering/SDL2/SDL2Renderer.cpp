#include <Engine/Rendering/SDL2/SDL2Renderer.h>

#include <Engine/Application.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Rendering/Texture.h>

#ifdef IOS
extern "C" {
#include <Engine/Rendering/SDL2/SDL2MetalFunc.h>
}
#endif

SDL_Renderer* Renderer = NULL;
float SDL2Renderer::RenderScale = 1.0f;

vector<Texture*> TextureList;

// TODO: Would it just be better to turn Graphics::TextureMap into a
// vector? The OpenGL renderer benefits from it being a map, but this
// method would make sense for the D3D renderer too.
void FindTextureID(Texture* texture) {
	for (size_t i = 0; i <= TextureList.size(); i++) {
		if (i == TextureList.size()) {
			texture->ID = TextureList.size();
			TextureList.push_back(texture);
			return;
		}
		else if (TextureList[i] == nullptr) {
			texture->ID = i;
			TextureList[i] = texture;
			break;
		}
	}
}

// Initialization and disposal functions
void SDL2Renderer::Init() {
	Log::Print(Log::LOG_INFO, "Renderer: SDL2");

	Uint32 flags = SDL_RENDERER_ACCELERATED;
	if (Graphics::VsyncEnabled) {
		flags |= SDL_RENDERER_PRESENTVSYNC;
	}
	Renderer = SDL_CreateRenderer(Application::Window, -1, flags);

	SDL_RendererInfo rendererInfo;
	SDL_GetRendererInfo(Renderer, &rendererInfo);

	Graphics::SupportsBatching = false;
	Graphics::PreferredPixelFormat = SDL_PIXELFORMAT_ARGB8888;

	Graphics::MaxTextureWidth = rendererInfo.max_texture_width;
	Graphics::MaxTextureHeight = rendererInfo.max_texture_height;

	int w, h, ww, wh;
	// SDL_GL_GetDrawableSize(Application::Window, &w, &h);
	SDL_GetRendererOutputSize(Renderer, &w, &h);
	SDL_GetWindowSize(Application::Window, &ww, &wh);

	RenderScale = 1.0f;
	if (h > wh) {
		RenderScale = h / wh;
	}
}
Uint32 SDL2Renderer::GetWindowFlags() {
	return 0;
}
void SDL2Renderer::SetVSync(bool enabled) {
	Graphics::VsyncEnabled = enabled;
	SDL_RenderSetVSync(Renderer, enabled);
}
void SDL2Renderer::SetGraphicsFunctions() {
	Graphics::PixelOffset = 0.0f;

	Graphics::Internal.Init = SDL2Renderer::Init;
	Graphics::Internal.GetWindowFlags = SDL2Renderer::GetWindowFlags;
	Graphics::Internal.SetVSync = SDL2Renderer::SetVSync;
	Graphics::Internal.Dispose = SDL2Renderer::Dispose;

	// Texture management functions
	Graphics::Internal.CreateTexture = SDL2Renderer::CreateTexture;
	Graphics::Internal.LockTexture = SDL2Renderer::LockTexture;
	Graphics::Internal.UpdateTexture = SDL2Renderer::UpdateTexture;
	Graphics::Internal.UpdateYUVTexture = SDL2Renderer::UpdateTextureYUV;
	Graphics::Internal.UnlockTexture = SDL2Renderer::UnlockTexture;
	Graphics::Internal.DisposeTexture = SDL2Renderer::DisposeTexture;

	// Viewport and view-related functions
	Graphics::Internal.SetRenderTarget = SDL2Renderer::SetRenderTarget;
	Graphics::Internal.UpdateWindowSize = SDL2Renderer::UpdateWindowSize;
	Graphics::Internal.UpdateViewport = SDL2Renderer::UpdateViewport;
	Graphics::Internal.UpdateClipRect = SDL2Renderer::UpdateClipRect;
	Graphics::Internal.UpdateOrtho = SDL2Renderer::UpdateOrtho;
	Graphics::Internal.UpdatePerspective = SDL2Renderer::UpdatePerspective;
	Graphics::Internal.UpdateProjectionMatrix = SDL2Renderer::UpdateProjectionMatrix;
	Graphics::Internal.MakePerspectiveMatrix = SDL2Renderer::MakePerspectiveMatrix;

	// Shader-related functions
	Graphics::Internal.UseShader = SDL2Renderer::UseShader;
	Graphics::Internal.SetUniformF = SDL2Renderer::SetUniformF;
	Graphics::Internal.SetUniformI = SDL2Renderer::SetUniformI;
	Graphics::Internal.SetUniformTexture = SDL2Renderer::SetUniformTexture;

	// These guys
	Graphics::Internal.Clear = SDL2Renderer::Clear;
	Graphics::Internal.Present = SDL2Renderer::Present;

	// Draw mode setting functions
	Graphics::Internal.SetBlendColor = SDL2Renderer::SetBlendColor;
	Graphics::Internal.SetBlendMode = SDL2Renderer::SetBlendMode;
	Graphics::Internal.SetTintColor = SDL2Renderer::SetTintColor;
	Graphics::Internal.SetTintMode = SDL2Renderer::SetTintMode;
	Graphics::Internal.SetTintEnabled = SDL2Renderer::SetTintEnabled;
	Graphics::Internal.SetLineWidth = SDL2Renderer::SetLineWidth;

	// Primitive drawing functions
	Graphics::Internal.StrokeLine = SDL2Renderer::StrokeLine;
	Graphics::Internal.StrokeCircle = SDL2Renderer::StrokeCircle;
	Graphics::Internal.StrokeEllipse = SDL2Renderer::StrokeEllipse;
	Graphics::Internal.StrokeRectangle = SDL2Renderer::StrokeRectangle;
	Graphics::Internal.FillCircle = SDL2Renderer::FillCircle;
	Graphics::Internal.FillEllipse = SDL2Renderer::FillEllipse;
	Graphics::Internal.FillTriangle = SDL2Renderer::FillTriangle;
	Graphics::Internal.FillRectangle = SDL2Renderer::FillRectangle;

	// Texture drawing functions
	Graphics::Internal.DrawTexture = SDL2Renderer::DrawTexture;
	Graphics::Internal.DrawSprite = SDL2Renderer::DrawSprite;
	Graphics::Internal.DrawSpritePart = SDL2Renderer::DrawSpritePart;

	Graphics::Internal.MakeFrameBufferID = SDL2Renderer::MakeFrameBufferID;
}
void SDL2Renderer::Dispose() {
	SDL_DestroyRenderer(Renderer);
}

// Texture management functions
Texture* SDL2Renderer::CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height) {
	Texture* texture = Texture::New(format, access, width, height);
	texture->DriverData = Memory::TrackedCalloc("Texture::DriverData", 1, sizeof(SDL_Texture*));

	SDL_Texture** textureData = (SDL_Texture**)texture->DriverData;

	*textureData = SDL_CreateTexture(Renderer, format, access, width, height);
	SDL_SetTextureBlendMode(*textureData, SDL_BLENDMODE_BLEND);

	FindTextureID(texture);
	Graphics::TextureMap->Put(texture->ID, texture);

	return texture;
}
int SDL2Renderer::LockTexture(Texture* texture, void** pixels, int* pitch) {
	return 0;
}
int SDL2Renderer::UpdateTexture(Texture* texture, SDL_Rect* src, void* pixels, int pitch) {
	SDL_Texture* textureData = *(SDL_Texture**)texture->DriverData;
	return SDL_UpdateTexture(textureData, src, pixels, pitch);
}
int SDL2Renderer::UpdateTextureYUV(Texture* texture,
	SDL_Rect* src,
	void* pixelsY,
	int pitchY,
	void* pixelsU,
	int pitchU,
	void* pixelsV,
	int pitchV) {
	// SDL_Texture* textureData =
	// *(SDL_Texture**)texture->DriverData;
	return 0;
}
void SDL2Renderer::UnlockTexture(Texture* texture) {}
void SDL2Renderer::DisposeTexture(Texture* texture) {
	SDL_Texture** textureData = (SDL_Texture**)texture->DriverData;
	if (!textureData) {
		return;
	}

	if (texture->ID < TextureList.size()) {
		TextureList[texture->ID] = nullptr;
	}
	Graphics::TextureMap->Remove(texture->ID);

	SDL_DestroyTexture(*textureData);
	Memory::Free(texture->DriverData);
}

// Viewport and view-related functions
void SDL2Renderer::SetRenderTarget(Texture* texture) {
	if (texture == NULL) {
		SDL_SetRenderTarget(Renderer, NULL);
	}
	else {
		SDL_SetRenderTarget(Renderer, *(SDL_Texture**)texture->DriverData);
	}
}
void SDL2Renderer::CopyScreen(void* pixels, int width, int height) {
	Viewport* vp = &Graphics::CurrentViewport;

	SDL_Rect r = {0, 0, width, height};

	SDL_RenderReadPixels(Renderer,
		&r,
		Graphics::PreferredPixelFormat,
		pixels,
		width * SDL_BYTESPERPIXEL(Graphics::PreferredPixelFormat));
}
void SDL2Renderer::UpdateWindowSize(int width, int height) {
	SDL2Renderer::UpdateViewport();
}
void SDL2Renderer::UpdateViewport() {
	Viewport* vp = &Graphics::CurrentViewport;
	SDL_Rect r = {
		(int)(vp->X * RenderScale),
		(int)(vp->Y * RenderScale),
		(int)(vp->Width * RenderScale),
		(int)(vp->Height * RenderScale),
	};
	SDL_RenderSetViewport(Renderer, &r);

	SDL2Renderer::UpdateProjectionMatrix();
}
void SDL2Renderer::UpdateClipRect() {
	ClipArea clip = Graphics::CurrentClip;
	if (Graphics::CurrentClip.Enabled) {
		SDL_Rect r = {
			(int)clip.X,
			(int)clip.Y,
			(int)clip.Width,
			(int)clip.Height,
		};
		SDL_RenderSetClipRect(Renderer, &r);
	}
	else {
		SDL_RenderSetClipRect(Renderer, NULL);
	}
}
void SDL2Renderer::UpdateOrtho(float left, float top, float right, float bottom) {}
void SDL2Renderer::UpdatePerspective(float fovy, float aspect, float nearv, float farv) {}
void SDL2Renderer::UpdateProjectionMatrix() {}
void SDL2Renderer::MakePerspectiveMatrix(Matrix4x4* out,
	float fov,
	float near,
	float far,
	float aspect) {
	Matrix4x4::Perspective(out, fov, aspect, near, far);
}

void SDL2Renderer::GetMetalSize(int* width, int* height) {
	// #ifdef IOS
	//     SDL2MetalFunc_GetMetalSize(width, height, Renderer);
	// #endif
	SDL_GetRendererOutputSize(Renderer, width, height);
}

// Shader-related functions
void SDL2Renderer::UseShader(void* shader) {}
void SDL2Renderer::SetUniformF(int location, int count, float* values) {}
void SDL2Renderer::SetUniformI(int location, int count, int* values) {}
void SDL2Renderer::SetUniformTexture(Texture* texture, int uniform_index, int slot) {}

// These guys
void SDL2Renderer::Clear() {
	SDL_RenderClear(Renderer);

	int w, h, ww, wh;
	SDL_GetRendererOutputSize(Renderer, &w, &h);
	SDL_GetWindowSize(Application::Window, &ww, &wh);

	RenderScale = 1.0f;
	if (h > wh) {
		RenderScale = h / wh;
	}
}
void SDL2Renderer::Present() {
	SDL_RenderPresent(Renderer);
}

// Draw mode setting functions
void SDL2Renderer::SetBlendColor(float r, float g, float b, float a) {
	SDL_SetRenderDrawColor(Renderer, r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}
SDL_BlendMode SDL2Renderer::GetCustomBlendMode(int srcC, int dstC, int srcA, int dstA) {
	SDL_BlendFactor blendFactorToSDL[] = {
		SDL_BLENDFACTOR_ZERO,
		SDL_BLENDFACTOR_ONE,
		SDL_BLENDFACTOR_SRC_COLOR,
		SDL_BLENDFACTOR_ONE_MINUS_SRC_COLOR,
		SDL_BLENDFACTOR_SRC_ALPHA,
		SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
		SDL_BLENDFACTOR_DST_COLOR,
		SDL_BLENDFACTOR_ONE_MINUS_DST_COLOR,
		SDL_BLENDFACTOR_DST_ALPHA,
		SDL_BLENDFACTOR_ONE_MINUS_DST_ALPHA,
	};

	return SDL_ComposeCustomBlendMode(blendFactorToSDL[srcC],
		blendFactorToSDL[dstC],
		SDL_BLENDOPERATION_ADD,
		blendFactorToSDL[srcA],
		blendFactorToSDL[srcA],
		SDL_BLENDOPERATION_ADD);
}
void SDL2Renderer::SetBlendMode(int srcC, int dstC, int srcA, int dstA) {
	SDL_BlendMode customBlendMode = GetCustomBlendMode(srcC, dstC, srcA, dstA);
	if (SDL_SetRenderDrawBlendMode(Renderer, customBlendMode) == 0) {
		return;
	}

	// That failed, so just use regular blend modes and hope they
	// match
	switch (Graphics::BlendMode) {
	case BlendMode_NORMAL:
		SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_BLEND);
		break;
	case BlendMode_ADD:
		SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_ADD);
		break;
	default:
		SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_NONE);
	}
}
void SDL2Renderer::SetTintColor(float r, float g, float b, float a) {}
void SDL2Renderer::SetTintMode(int mode) {}
void SDL2Renderer::SetTintEnabled(bool enabled) {}
void SDL2Renderer::SetLineWidth(float n) {}

// Primitive drawing functions
void SDL2Renderer::StrokeLine(float x1, float y1, float x2, float y2) {}
void SDL2Renderer::StrokeCircle(float x, float y, float rad, float thickness) {}
void SDL2Renderer::StrokeEllipse(float x, float y, float w, float h) {}
void SDL2Renderer::StrokeRectangle(float x, float y, float w, float h) {}
void SDL2Renderer::FillCircle(float x, float y, float rad) {}
void SDL2Renderer::FillEllipse(float x, float y, float w, float h) {}
void SDL2Renderer::FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3) {}
void SDL2Renderer::FillRectangle(float x, float y, float w, float h) {}
// Texture drawing functions
void SDL2Renderer::DrawTexture(Texture* texture,
	float sx,
	float sy,
	float sw,
	float sh,
	float x,
	float y,
	float w,
	float h) {
	x *= RenderScale;
	y *= RenderScale;
	w *= RenderScale;
	h *= RenderScale;
	SDL_Rect src = {
		(int)sx,
		(int)sy,
		(int)sw,
		(int)sh,
	};
	SDL_Rect dst = {
		(int)x,
		(int)y,
		(int)w,
		(int)h,
	};
	SDL_Texture** textureData = (SDL_Texture**)texture->DriverData;
	int flip = 0;
	if (dst.w < 0) {
		dst.w = -dst.w;
		flip |= SDL_FLIP_HORIZONTAL;
	}
	if (dst.h < 0) {
		dst.h = -dst.h;
		flip |= SDL_FLIP_VERTICAL;
	}
	SDL_RenderCopyEx(Renderer, *textureData, &src, &dst, 0.0, NULL, (SDL_RendererFlip)flip);
}
void SDL2Renderer::DrawSprite(ISprite* sprite,
	int animation,
	int frame,
	int x,
	int y,
	bool flipX,
	bool flipY,
	float scaleW,
	float scaleH,
	float rotation,
	unsigned paletteID) {
	if (Graphics::SpriteRangeCheck(sprite, animation, frame)) {
		return;
	}

	AnimFrame animframe = sprite->Animations[animation].Frames[frame];
	float fX = flipX ? -1.0 : 1.0;
	float fY = flipY ? -1.0 : 1.0;
	float sw = animframe.Width;
	float sh = animframe.Height;

	SDL2Renderer::DrawTexture(sprite->Spritesheets[animframe.SheetNumber],
		animframe.X,
		animframe.Y,
		sw,
		sh,
		x + fX * animframe.OffsetX,
		y + fY * animframe.OffsetY,
		fX * sw,
		fY * sh);
}
void SDL2Renderer::DrawSpritePart(ISprite* sprite,
	int animation,
	int frame,
	int sx,
	int sy,
	int sw,
	int sh,
	int x,
	int y,
	bool flipX,
	bool flipY,
	float scaleW,
	float scaleH,
	float rotation,
	unsigned paletteID) {
	if (Graphics::SpriteRangeCheck(sprite, animation, frame)) {
		return;
	}

	AnimFrame animframe = sprite->Animations[animation].Frames[frame];
	if (sx == animframe.Width) {
		return;
	}
	if (sy == animframe.Height) {
		return;
	}

	float fX = flipX ? -1.0 : 1.0;
	float fY = flipY ? -1.0 : 1.0;
	if (sw >= animframe.Width - sx) {
		sw = animframe.Width - sx;
	}
	if (sh >= animframe.Height - sy) {
		sh = animframe.Height - sy;
	}

	SDL2Renderer::DrawTexture(sprite->Spritesheets[animframe.SheetNumber],
		animframe.X + sx,
		animframe.Y + sy,
		sw,
		sh,
		x + fX * (sx + animframe.OffsetX),
		y + fY * (sy + animframe.OffsetY),
		fX * sw,
		fY * sh);
}

void SDL2Renderer::MakeFrameBufferID(ISprite* sprite) {
	sprite->ID = 0;
}
