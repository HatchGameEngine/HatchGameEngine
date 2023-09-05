#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/Texture.h>

class GameTexture {
public:
    Texture* TexturePtr = nullptr;
    int      UnloadPolicy;
    bool     OwnsTexture = true;
};
#endif

#include <Engine/Rendering/GameTexture.h>

#include <Engine/Graphics.h>

PUBLIC GameTexture::GameTexture() {

}

PUBLIC GameTexture::GameTexture(Uint32 width, Uint32 height, int unloadPolicy) {
    TexturePtr = Texture::New(SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, width, height);
    UnloadPolicy = unloadPolicy;
}

PUBLIC VIRTUAL Texture* GameTexture::GetTexture() {
    return TexturePtr;
}

PUBLIC VIRTUAL int GameTexture::GetID() {
    if (TexturePtr)
        return TexturePtr->ID;

    return 0;
}

PUBLIC GameTexture::~GameTexture() {
    if (TexturePtr && OwnsTexture)
        Graphics::DisposeTexture(TexturePtr);
}
