#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/GameTexture.h>

class ViewTexture : public GameTexture {
public:
    int ViewIndex;
};
#endif

#include <Engine/Rendering/ViewTexture.h>
#include <Engine/Scene.h>

PUBLIC ViewTexture::ViewTexture(int viewIndex) {
    ViewIndex = viewIndex;
    UnloadPolicy = SCOPE_GAME;
    OwnsTexture = false;
}

PUBLIC VIRTUAL Texture* ViewTexture::GetTexture() {
    if (ViewIndex < 0 || ViewIndex >= MAX_SCENE_VIEWS)
        return nullptr;

    if (Scene::Views[ViewIndex].UseDrawTarget && Scene::Views[ViewIndex].DrawTarget)
        return Scene::Views[ViewIndex].DrawTarget;

    return nullptr;
}

PUBLIC VIRTUAL int ViewTexture::GetID() {
    // Using negative IDs makes sure they don't collide with other textures
    return -(ViewIndex + 1);
}
