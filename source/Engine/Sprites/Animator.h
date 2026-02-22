#ifndef ENGINE_SPRITES_ANIMATOR_H
#define ENGINE_SPRITES_ANIMATOR_H

#include <Engine/ResourceTypes/ISprite.h>

class Animator {
public:
	vector<AnimFrame> Frames;
	ISprite* Sprite = nullptr;
	int CurrentAnimation = -1;
	int CurrentFrame = -1;
	int PrevAnimation = 0;
	int AnimationSpeed = 0;
	int AnimationTimer = 0;
	int Duration = 0;
	int FrameCount = 0;
	int LoopIndex = 0;
	int RotationStyle = 0;
	Uint32 UnloadPolicy = 0;

	void SetSprite(ISprite* newSprite) {
		if (Sprite != nullptr) {
			Sprite->Release();
			Sprite = nullptr;
		}

		if (newSprite) {
			newSprite->TakeRef();
			Sprite = newSprite;
		}
	}

	~Animator() {
		SetSprite(nullptr);
	}
};

#endif /* ENGINE_SPRITES_ANIMATOR_H */
