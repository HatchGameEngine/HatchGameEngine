#include <Engine/Rendering/ViewTexture.h>
#include <Engine/Scene.h>

ViewTexture::ViewTexture(int viewIndex) {
	ViewIndex = viewIndex;
	UnloadPolicy = SCOPE_GAME;
	OwnsTexture = false;
}

Texture* ViewTexture::GetTexture() {
	if (ViewIndex < 0 || ViewIndex >= MAX_SCENE_VIEWS) {
		return nullptr;
	}

	if (Scene::Views[ViewIndex].UseDrawTarget && Scene::Views[ViewIndex].DrawTarget) {
		return Scene::Views[ViewIndex].DrawTarget;
	}

	return nullptr;
}

int ViewTexture::GetID() {
	// Using negative IDs makes sure they don't collide with other
	// textures
	return -(ViewIndex + 1);
}
