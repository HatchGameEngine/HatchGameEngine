#ifndef ENGINE_SPRITES_TILEANIMATION_H
#define ENGINE_SPRITES_TILEANIMATION_H

#include <Engine/Scene/TileSpriteInfo.h>
#include <Engine/ResourceTypes/ISprite.h>

struct TileAnimator {
    TileSpriteInfo* TileInfo = nullptr;
    vector<Animation>* Animations = nullptr;
    ISprite* Sprite = nullptr;

    Animation* CurrentAnimation = nullptr;
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
        Animations = &sprite->Animations;
        SetAnimation(animationID, 0);
    }

    void SetAnimation(int animation, int frame) {
        if (animation < 0 || (size_t)animation >= Animations->size())
            return;
        if (frame < 0 || (size_t)frame >= (*Animations)[animation].Frames.size())
            return;

        AnimationIndex = animation;
        CurrentAnimation = &((*Animations)[AnimationIndex]);
        FrameIndex = frame;
        FrameDuration = CurrentAnimation->Frames[FrameIndex].Duration;
        FrameCount = (int)CurrentAnimation->Frames.size();
        LoopIndex = CurrentAnimation->FrameToLoop;
        Speed = CurrentAnimation->AnimationSpeed;
        Timer = 0.0;
    }

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
            if (FrameIndex >= FrameCount)
                FrameIndex = LoopIndex;

            UpdateTile();

            FrameDuration = CurrentAnimation->Frames[FrameIndex].Duration;
        }
    }
};

#endif /* ENGINE_SPRITES_TILEANIMATION_H */
