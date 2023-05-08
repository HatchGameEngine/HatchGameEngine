#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Application.h>
#include <Engine/Graphics.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/Scene.h>

#include <Engine/Types/EntityTypes.h>
#include <Engine/Includes/HashMap.h>

need_t ObjectList;
need_t ObjectRegistry;
need_t DrawGroupList;

class Entity {
public:
    float        InitialX = 0;
    float        InitialY = 0;
    int          Active = true;
    int          Pauseable = true;
    int          Persistent = false;
    int          Interactable = true;
    int          ActiveStatus = 4; // Active_BOUNDS
    int          InRange = false;

    float        X = 0.0f;
    float        Y = 0.0f;
    float        Z = 0.0f;

    float        XSpeed = 0.0f;
    float        YSpeed = 0.0f;
    float        GroundSpeed = 0.0f;
    float        Gravity = 0.0f;
    int          Ground = false;

    int          OnScreen = true;
    float        OnScreenHitboxW = 0.0f;
    float        OnScreenHitboxH = 0.0f;
    int          ViewRenderFlag = 0xFFFFFFFF;
    int          ViewOverrideFlag = 0;
    float        RenderRegionW = 0.0f;
    float        RenderRegionH = 0.0f;

    int          Angle = 0;
    int          AngleMode = 0;
    float        ScaleX = 1.0;
    float        ScaleY = 1.0;
    float        Rotation = 0.0;
    float        Alpha = 1.0;
    int          AutoPhysics = false;

    int          Priority = 0;
    int          PriorityListIndex = -1;
    int          PriorityOld = 0;

    float        Depth = 0.0f;
    float        OldDepth = 0.0f;
    float        ZDepth = 0.0;

    int          Sprite = -1;
    int          CurrentAnimation = -1;
    int          CurrentFrame = -1;
    int          CurrentFrameCount = 0;
    float        AnimationSpeedMult = 1.0;
    int          AnimationSpeedAdd = 0;
    int          AutoAnimate = true;
    float        AnimationSpeed = 0.0;
    float        AnimationTimer = 0.0;
    int          AnimationFrameDuration = 0;
    int          AnimationLoopIndex = 0;

    float        HitboxW = 0.0f;
    float        HitboxH = 0.0f;
    float        HitboxOffX = 0.0f;
    float        HitboxOffY = 0.0f;
    int          FlipFlag = 0;

    float        SensorX = 0.0f;
    float        SensorY = 0.0f;
    int          SensorCollided = false;
    int          SensorAngle = 0;

    float        VelocityX = 0.0f;
    float        VelocityY = 0.0f;
    float        GroundVel = 0.0f;
    float        GravityStrength = 0.0f;
    int          OnGround = false;
    int          Direction = 0;

    int          CollisionLayers = 0;
    int          CollisionPlane = 0;
    int          CollisionMode = 0;
    
    int          SlotID = 0;

    bool         Removed = false;

    Entity*      PrevEntity = NULL;
    Entity*      NextEntity = NULL;

    ObjectList*  List = NULL;
    Entity*      PrevEntityInList = NULL;
    Entity*      NextEntityInList = NULL;
};
#endif

#include <Engine/Types/Entity.h>

PUBLIC void Entity::ApplyMotion() {
    YSpeed += Gravity;
    X += XSpeed;
    Y += YSpeed;
}
PUBLIC void Entity::Animate() {
    if (Sprite < 0 || (size_t)Sprite >= Scene::SpriteList.size())
        return;

    ISprite* sprite = Scene::SpriteList[Sprite]->AsSprite;

    if (CurrentAnimation < 0 || (size_t)CurrentAnimation >= sprite->Animations.size())
        return;

    AnimationTimer += (AnimationSpeed * AnimationSpeedMult + AnimationSpeedAdd);

    while (AnimationTimer > AnimationFrameDuration) {
        CurrentFrame++;

        AnimationTimer -= AnimationFrameDuration;
        if (CurrentFrame >= CurrentFrameCount) {
            CurrentFrame = AnimationLoopIndex;
            OnAnimationFinish();
        }

        AnimationFrameDuration = sprite->Animations[CurrentAnimation].Frames[CurrentFrame].Duration;
    }
}
PUBLIC void Entity::SetAnimation(int animation, int frame) {
    if (CurrentAnimation != animation)
        ResetAnimation(animation, frame);
}
PUBLIC void Entity::ResetAnimation(int animation, int frame) {
    if (Sprite < 0 || (size_t)Sprite >= Scene::SpriteList.size())
        return;

    ISprite* sprite = Scene::SpriteList[Sprite]->AsSprite;

    if (animation < 0 || (size_t)animation >= sprite->Animations.size())
        return;

    if (frame < 0 || (size_t)frame >= sprite->Animations[animation].Frames.size())
        return;

    CurrentAnimation        = animation;
    AnimationTimer          = 0.0;
    CurrentFrame            = frame;
    CurrentFrameCount       = (int)sprite->Animations[CurrentAnimation].Frames.size();
    AnimationFrameDuration  = sprite->Animations[CurrentAnimation].Frames[CurrentFrame].Duration;
    AnimationSpeed          = sprite->Animations[CurrentAnimation].AnimationSpeed;
    AnimationLoopIndex      = sprite->Animations[CurrentAnimation].FrameToLoop;
}
PUBLIC bool Entity::CollideWithObject(Entity* other) {
    float sourceFlipX = (this->FlipFlag & 1) ? -1.0 : 1.0;
    float sourceFlipY = (this->FlipFlag & 2) ? -1.0 : 1.0;
    float otherFlipX = (other->FlipFlag & 1) ? -1.0 : 1.0;
    float otherFlipY = (other->FlipFlag & 2) ? -1.0 : 1.0;

    float sourceX = std::floor(this->X + this->HitboxOffX * sourceFlipX);
    float sourceY = std::floor(this->Y + this->HitboxOffY * sourceFlipY);
    float otherX = std::floor(other->X + other->HitboxOffX * otherFlipX);
    float otherY = std::floor(other->Y + other->HitboxOffY * otherFlipY);

    float otherHitboxW = (other->HitboxW) * 0.5;
    float otherHitboxH = (other->HitboxH) * 0.5;
    float sourceHitboxW = (this->HitboxW) * 0.5;
    float sourceHitboxH = (this->HitboxH) * 0.5;

    if (otherY + otherHitboxH < sourceY - sourceHitboxH ||
        otherY - otherHitboxH > sourceY + sourceHitboxH ||
        sourceX - sourceHitboxW > otherX + otherHitboxW ||
        sourceX + sourceHitboxW < otherX - otherHitboxW)
        return false;

    return true;
}
PUBLIC int  Entity::SolidCollideWithObject(Entity* other, int flag) {
    // NOTE: "flag" might be "UseGroundSpeed"
    float initialOtherX = (other->X);
    float initialOtherY = (other->Y);
    float sourceX = std::floor(this->X);
    float sourceY = std::floor(this->Y);
    float otherX = std::floor(initialOtherX);
    float otherY = std::floor(initialOtherY);
    float otherNewX = initialOtherX;
    int collideSideHori = 0;
    int collideSideVert = 0;

    float otherHitboxW = (other->HitboxW) * 0.5;
    float otherHitboxH = (other->HitboxH) * 0.5;
    float otherHitboxWSq = (other->HitboxW - 2.0) * 0.5;
    float otherHitboxHSq = (other->HitboxH - 2.0) * 0.5;
    float sourceHitboxW = (this->HitboxW) * 0.5;
    float sourceHitboxH = (this->HitboxH) * 0.5;

    float sourceHitboxOffX = (this->FlipFlag & 1) ? -this->HitboxOffX : this->HitboxOffX;
    float sourceHitboxOffY = (this->FlipFlag & 2) ? -this->HitboxOffY : this->HitboxOffY;
    float otherHitboxOffX = (other->FlipFlag & 1) ? -other->HitboxOffX : other->HitboxOffX;
    float otherHitboxOffY = (other->FlipFlag & 2) ? -other->HitboxOffY : other->HitboxOffY;

    // NOTE: Keep this.
    // if ( other_X <= (sourceHitbox->Right + sourceHitbox->Left + 2 * source_X) >> 1 )
    // if ( other_X <= (sourceHitbox->Right + sourceHitbox->Left) / 2 + source_X )
    // if ( other_X <= source_X + (sourceHitbox->Right + sourceHitbox->Left) / 2 )
    // if other->X <= this->X + this->HitboxCenterX

    // Check squeezed vertically
    if (sourceY + (-sourceHitboxH + sourceHitboxOffY) < initialOtherY + otherHitboxHSq &&
        sourceY + (sourceHitboxH + sourceHitboxOffY) > initialOtherY - otherHitboxHSq) {
        // if other->X <= this->X + this->HitboxCenterX
        if (otherX <= sourceX) {
            if (otherX + otherHitboxW >= sourceX - sourceHitboxW) {
                collideSideHori = 2;
                otherNewX = this->X + ((-sourceHitboxW + sourceHitboxOffX) - (otherHitboxW + otherHitboxOffX));
            }
        }
        else {
            if (otherX - otherHitboxW < sourceX + sourceHitboxW) {
                collideSideHori = 3;
                otherNewX = this->X + ((sourceHitboxW + sourceHitboxOffX) - (-otherHitboxW + otherHitboxOffX));
            }
        }
    }

    // Check squeezed horizontally
    if (sourceX - sourceHitboxW < initialOtherX + otherHitboxWSq &&
        sourceX + sourceHitboxW > initialOtherX - otherHitboxWSq) {
        // if other->Y <= this->Y + this->HitboxCenterY
        if (otherY <= sourceY) {
            if (otherY + (otherHitboxH + otherHitboxOffY) >= sourceY + (-sourceHitboxH + sourceHitboxOffY)) {
                collideSideVert = 1;
                otherY = this->Y + ((-sourceHitboxH + sourceHitboxOffY) - (otherHitboxH + otherHitboxOffY));
            }
        }
        else {
            if (otherY + (-otherHitboxH + otherHitboxOffY) < sourceY + (sourceHitboxH + sourceHitboxOffY)) {
                collideSideVert = 4;
                otherY = this->Y + ((sourceHitboxH + sourceHitboxOffY) - (-otherHitboxH + otherHitboxOffY));
            }
        }
    }

    float deltaSquaredX1 = otherNewX - (other->X);
    float deltaSquaredX2 = initialOtherX - (other->X);
    float deltaSquaredY1 = otherY - (other->Y);
    float deltaSquaredY2 = initialOtherY - (other->Y);

    deltaSquaredX1 *= deltaSquaredX1;
    deltaSquaredX2 *= deltaSquaredX2;
    deltaSquaredY1 *= deltaSquaredY1;
    deltaSquaredY2 *= deltaSquaredY2;

    int v47 = collideSideVert;
    int v46 = collideSideHori;

    if (collideSideHori != 0 || collideSideVert != 0) {
        if (deltaSquaredX1 + deltaSquaredY2 >= deltaSquaredX2 + deltaSquaredY1) {
            if (collideSideVert || !collideSideHori) {
                other->X = initialOtherX;
                other->Y = otherY;
                if (flag == 1) {
                    if (collideSideVert != 1) {
                        if (collideSideVert == 4 && other->YSpeed < 0.0) {
                            other->YSpeed = 0.0;
                            return v47;
                        }
                        return v47;
                    }

                    if (other->YSpeed > 0.0)
                        other->YSpeed = 0.0;
                    if (!other->Ground && other->YSpeed >= 0.0) {
                        other->GroundSpeed = other->XSpeed;
                        other->Angle = 0;
                        other->Ground = true;
                    }
                }
                return v47;
            }
        }
        else {
            if (collideSideVert && !collideSideHori) {
                other->X = initialOtherX;
                other->Y = otherY;
                if (flag == 1) {
                    if (collideSideVert != 1) {
                        if (collideSideVert == 4 && other->YSpeed < 0.0) {
                            other->YSpeed = 0.0;
                            return v47;
                        }
                        return v47;
                    }

                    if (other->YSpeed > 0.0)
                        other->YSpeed = 0.0;
                    if (!other->Ground && other->YSpeed >= 0.0) {
                        other->GroundSpeed = other->XSpeed;
                        other->Angle = 0;
                        other->Ground = true;
                    }
                }
                return v47;
            }
        }

        other->X = otherNewX;
        other->Y = initialOtherY;
        if (flag == 1) {
            float v50;
            if (other->Ground) {
                v50 = other->GroundSpeed;
                if (other->AngleMode == 2)
                    v50 = -v50;
            }
            else {
                v50 = other->XSpeed;
            }

            if (v46 == 2) {
                if (v50 <= 0.0)
                    return v46;
            }
            else if (v46 != 3 || v50 >= 0.0) {
                return v46;
            }

            other->GroundSpeed = 0.0;
            other->XSpeed = 0.0;
        }
        return v46;
    }
    return 0;
}
PUBLIC bool Entity::TopSolidCollideWithObject(Entity* other, int flag) {
    // NOTE: "flag" might be "UseGroundSpeed"
    float initialOtherX = (other->X);
    float initialOtherY = (other->Y);
    float sourceX = std::floor(this->X);
    float sourceY = std::floor(this->Y);
    float otherX = std::floor(initialOtherX);
    float otherY = std::floor(initialOtherY);
    float otherYMinusYSpeed = std::floor(initialOtherY - other->YSpeed);

    float otherHitboxW = (other->HitboxW) * 0.5;
    float otherHitboxH = (other->HitboxH) * 0.5;
    float sourceHitboxW = (this->HitboxW) * 0.5;
    float sourceHitboxH = (this->HitboxH) * 0.5;

    float sourceHitboxOffX = (this->FlipFlag & 1) ? -this->HitboxOffX : this->HitboxOffX;
    float sourceHitboxOffY = (this->FlipFlag & 2) ? -this->HitboxOffY : this->HitboxOffY;
    float otherHitboxOffX = (other->FlipFlag & 1) ? -other->HitboxOffX : other->HitboxOffX;
    float otherHitboxOffY = (other->FlipFlag & 2) ? -other->HitboxOffY : other->HitboxOffY;

    if ((otherHitboxH + otherHitboxOffY) + otherY            < sourceY + (-sourceHitboxH + sourceHitboxOffY) ||
        (otherHitboxH + otherHitboxOffY) + otherYMinusYSpeed > sourceY + (sourceHitboxH + sourceHitboxOffY) ||
        sourceX + (-sourceHitboxW + sourceHitboxOffX) >= otherX + (otherHitboxW + otherHitboxOffX) ||
        sourceX + (sourceHitboxW + sourceHitboxOffX)  <= otherX + (-otherHitboxW + otherHitboxOffX) ||
        other->YSpeed < 0.0)
        return false;

    other->Y = this->Y + ((-sourceHitboxH + sourceHitboxOffY) - (otherHitboxH + otherHitboxOffY));
    if (flag) {
        other->YSpeed = 0.0;
        if (!other->Ground) {
            other->GroundSpeed = other->XSpeed;
            other->Angle = 0;
            other->Ground = true;
        }
    }
    return true;
}

PUBLIC void Entity::ApplyPhysics() {

}

PUBLIC VIRTUAL void Entity::GameStart() {

}

PUBLIC VIRTUAL void Entity::Initialize() {

}
PUBLIC VIRTUAL void Entity::Create() {

}
PUBLIC VIRTUAL void Entity::Setup() {

}

PUBLIC VIRTUAL void Entity::UpdateEarly() {
}
PUBLIC VIRTUAL void Entity::Update() {
}
PUBLIC VIRTUAL void Entity::UpdateLate() {
}

PUBLIC VIRTUAL void Entity::OnAnimationFinish() {

}

PUBLIC VIRTUAL void Entity::RenderEarly() {

}

PUBLIC VIRTUAL void Entity::Render(int CamX, int CamY) {

}

PUBLIC VIRTUAL void Entity::RenderLate() {

}

PUBLIC VIRTUAL void Entity::Dispose() {
}
