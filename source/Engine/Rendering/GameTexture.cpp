#include <Engine/Rendering/GameTexture.h>

#include <Engine/Graphics.h>

GameTexture::GameTexture() {}

GameTexture::GameTexture(Uint32 width, Uint32 height, int unloadPolicy) {
	TexturePtr =
		Texture::New(SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, width, height);
	UnloadPolicy = unloadPolicy;
}

Texture* GameTexture::GetTexture() {
	return TexturePtr;
}

int GameTexture::GetID() {
	if (TexturePtr) {
		return TexturePtr->ID;
	}

	return 0;
}

GameTexture::~GameTexture() {
	if (TexturePtr && OwnsTexture) {
		Graphics::DisposeTexture(TexturePtr);
	}
}
