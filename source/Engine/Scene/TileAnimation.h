#ifndef ENGINE_SPRITES_TILEANIMATION_H
#define ENGINE_SPRITES_TILEANIMATION_H

#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/Scene/TileSpriteInfo.h>

struct TileAnimator {
	TileSpriteInfo* TileInfo = nullptr;
	ISprite* Sprite = nullptr;

	int AnimationIndex = -1;
	int FrameIndex = -1;
	int FrameCount = 0;
	int LoopIndex = 0;

	float Speed = 0.0;
	float Timer = 0.0;
	float FrameDuration = 0.0;

	bool Paused = false;

	TileAnimator(TileSpriteInfo* tileSpriteInfo, ISprite* sprite, int animationID) {
		TileInfo = tileSpriteInfo;
		Sprite = sprite;
		SetAnimation(animationID, 0);
	}

	void SetAnimation(int animation, int frame) {
		if (animation < 0 || (size_t)animation >= Sprite->Animations.size()) {
			return;
		}
		if (frame < 0 || (size_t)frame >= Sprite->Animations[animation].Frames.size()) {
			return;
		}

		Animation* animationPtr = &Sprite->Animations[animation];
		AnimationIndex = animation;
		FrameIndex = frame;
		FrameDuration = animationPtr->Frames[FrameIndex].Duration;
		FrameCount = (int)animationPtr->Frames.size();
		LoopIndex = animationPtr->FrameToLoop;
		Speed = animationPtr->AnimationSpeed;
		Timer = 0.0;
	}

	Animation* GetCurrentAnimation() { return &Sprite->Animations[AnimationIndex]; }

	void RestartAnimation() {
		SetAnimation(AnimationIndex, 0);
		UpdateTile();
	}

	void UpdateTile() {
		TileInfo->Sprite = Sprite;
		TileInfo->AnimationIndex = AnimationIndex;
		TileInfo->FrameIndex = FrameIndex;
	}

	void Animate() {
		Timer += Speed;

		while (FrameDuration && Timer > FrameDuration) {
			FrameIndex++;

			Timer -= FrameDuration;
			if (FrameIndex >= FrameCount) {
				FrameIndex = LoopIndex;
			}

			UpdateTile();

			FrameDuration =
				Sprite->Animations[AnimationIndex].Frames[FrameIndex].Duration;
		}
	}
};

#endif /* ENGINE_SPRITES_TILEANIMATION_H */
