#include <Engine/Types/Entity.h>

void Entity::ApplyMotion() {
    YSpeed += Gravity;
    X += XSpeed;
    Y += YSpeed;
}
void Entity::Animate() {
    ResourceType* resource = Scene::GetSpriteResource(Sprite);
    if (!resource)
        return;

    ISprite* sprite = resource->AsSprite;
    if (!sprite || CurrentAnimation < 0 || (size_t)CurrentAnimation >= sprite->Animations.size())
        return;

#ifdef USE_RSDK_ANIMATE
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
#else
    if ((float)AnimationFrameDuration - AnimationTimer > 0.0f) {
        AnimationTimer += (AnimationSpeed * AnimationSpeedMult + AnimationSpeedAdd);
        if ((float)AnimationFrameDuration - AnimationTimer <= 0.0f) {
            CurrentFrame++;
            if (CurrentFrame >= CurrentFrameCount) {
                CurrentFrame = AnimationLoopIndex;
                OnAnimationFinish();

                // Sprite may have changed after a call to OnAnimationFinish
                ResourceType* resource = Scene::GetSpriteResource(Sprite);
                if (resource)
                    sprite = Scene::GetSpriteResource(Sprite)->AsSprite;
                else
                    sprite = nullptr;
            }

            // Do a basic range check, for strange loop points
            // (or just in case CurrentAnimation happens to be invalid, which is very possible)
            if (sprite && CurrentFrame < CurrentFrameCount && CurrentAnimation >= 0 && CurrentAnimation < sprite->Animations.size()) {
                AnimationFrameDuration = sprite->Animations[CurrentAnimation].Frames[CurrentFrame].Duration;
            }
            else {
                AnimationFrameDuration = 1.0f;
            }

            AnimationTimer = 0.0f;
        }
    }
    else {
        AnimationTimer = 0.0f;
    }
#endif
}
void Entity::SetAnimation(int animation, int frame) {
    if (CurrentAnimation != animation)
        ResetAnimation(animation, frame);
}
void Entity::ResetAnimation(int animation, int frame) {
    ResourceType* resource = Scene::GetSpriteResource(Sprite);
    if (!resource)
        return;

    ISprite* sprite = resource->AsSprite;
    if (!sprite || animation < 0 || (size_t)animation >= sprite->Animations.size())
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
bool Entity::BasicCollideWithObject(Entity* other) {
    float otherHitboxW = other->Hitbox.Width;
    float otherHitboxH = other->Hitbox.Height;

    if (otherHitboxW == 0.0f || otherHitboxH == 0.0f)
        return false;

    return
        other->X + other->Hitbox.GetLeft()  >= X + Hitbox.GetLeft() &&
        other->Y + other->Hitbox.GetTop()   >= Y + Hitbox.GetTop() &&
        other->X + other->Hitbox.GetRight()  < X + Hitbox.GetRight() &&
        other->Y + other->Hitbox.GetBottom() < Y + Hitbox.GetBottom();
}
bool Entity::CollideWithObject(Entity* other) {
    float sourceFlipX = (this->FlipFlag & 1) ? -1.0 : 1.0;
    float sourceFlipY = (this->FlipFlag & 2) ? -1.0 : 1.0;
    float otherFlipX = (other->FlipFlag & 1) ? -1.0 : 1.0;
    float otherFlipY = (other->FlipFlag & 2) ? -1.0 : 1.0;

    float sourceX = std::floor(this->X + this->Hitbox.OffsetX * sourceFlipX);
    float sourceY = std::floor(this->Y + this->Hitbox.OffsetY * sourceFlipY);
    float otherX = std::floor(other->X + other->Hitbox.OffsetX * otherFlipX);
    float otherY = std::floor(other->Y + other->Hitbox.OffsetY * otherFlipY);

    float otherHitboxW = other->Hitbox.Width * 0.5;
    float otherHitboxH = other->Hitbox.Height * 0.5;
    float sourceHitboxW = this->Hitbox.Width * 0.5;
    float sourceHitboxH = this->Hitbox.Height * 0.5;

    return !(otherY + otherHitboxH < sourceY - sourceHitboxH ||
        otherY - otherHitboxH > sourceY + sourceHitboxH ||
        sourceX - sourceHitboxW > otherX + otherHitboxW ||
        sourceX + sourceHitboxW < otherX - otherHitboxW);
}
int  Entity::SolidCollideWithObject(Entity* other, int flag) {
    // NOTE: "flag" is setValues
    float initialOtherX = (other->X);
    float initialOtherY = (other->Y);
    int sourceX = this->X;
    int sourceY = this->Y;
    int otherX = initialOtherX;
    int otherY = initialOtherY;
    float otherNewX = initialOtherX;
    int collideSideHori = 0;
    int collideSideVert = 0;

    int sLeft = this->Hitbox.GetLeft(this->FlipFlag & 1);
    int sRight = this->Hitbox.GetRight(this->FlipFlag & 1);
    int sTop = this->Hitbox.GetTop(this->FlipFlag & 2);
    int sBottom = this->Hitbox.GetBottom(this->FlipFlag & 2);

    int oLeft = other->Hitbox.GetLeft(other->FlipFlag & 1);
    int oRight = other->Hitbox.GetRight(other->FlipFlag & 1);
    int oTop = other->Hitbox.GetTop(other->FlipFlag & 2);
    int oBottom = other->Hitbox.GetBottom(other->FlipFlag & 2);

    oTop++;
    oBottom--;

    // Check squeezed vertically
    if (otherX <= (sRight + sLeft + 2 * sourceX) >> 1) {
        if (otherX + oRight >= sourceX + sLeft && sourceY + sTop < otherY + oBottom
            && sourceY + sBottom > otherY + oTop) {
            collideSideHori = C_LEFT;
            otherNewX = this->X + (sLeft - oRight);
        }
    }
    else {
        if (otherX + oLeft < sourceX + sRight && sourceY + sTop < otherY + oBottom
            && sourceY + sBottom > otherY + oTop) {
            collideSideHori = C_RIGHT;
            otherNewX = this->X + (sRight - oLeft);
        }
    }

    oTop--;
    oBottom++;

    oLeft++;
    oRight--;

    // Check squeezed horizontally
    if (otherY < (sTop + sBottom + 2 * sourceY) >> 1) {
        if (otherY + oBottom >= sourceY + sTop && sourceX + sLeft < otherX + oRight
            && sourceX + sRight > otherX + oLeft) {
            collideSideVert = C_TOP;
            otherY = this->Y + (sTop - oBottom);
        }
    }
    else {
        if (otherY + oTop < sourceY + sBottom && sourceX + sLeft < otherX + oRight) {
            if (otherX + oLeft < sourceX + sRight) {
                collideSideVert = C_BOTTOM;
                otherY = this->Y + (sBottom - oTop);
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
bool Entity::TopSolidCollideWithObject(Entity* other, int flag) {
    // NOTE: "flag" might be "UseGroundSpeed"
    float initialOtherX = (other->X);
    float initialOtherY = (other->Y);
    float sourceX = std::floor(this->X);
    float sourceY = std::floor(this->Y);
    float otherX = std::floor(initialOtherX);
    float otherY = std::floor(initialOtherY);
    float otherYMinusYSpeed = std::floor(initialOtherY - other->YSpeed);

    float otherHitboxW = other->Hitbox.Width * 0.5;
    float otherHitboxH = other->Hitbox.Height * 0.5;
    float sourceHitboxW = this->Hitbox.Width * 0.5;
    float sourceHitboxH = this->Hitbox.Height * 0.5;

    float sourceHitboxOffX = (this->FlipFlag & 1) ? -this->Hitbox.OffsetX : this->Hitbox.OffsetX;
    float sourceHitboxOffY = (this->FlipFlag & 2) ? -this->Hitbox.OffsetY : this->Hitbox.OffsetY;
    float otherHitboxOffX = (other->FlipFlag & 1) ? -other->Hitbox.OffsetX : other->Hitbox.OffsetX;
    float otherHitboxOffY = (other->FlipFlag & 2) ? -other->Hitbox.OffsetY : other->Hitbox.OffsetY;

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

void Entity::Copy(Entity* other) {
    // Add the other entity to this object's list
    if (List != NULL && other->List != List) {
        if (other->List != NULL)
            other->List->Remove(other);
        other->List = List;
        other->List->Add(other);
    }

    for (int l = 0; l < Scene::PriorityPerLayer; l++) {
        if (Scene::PriorityLists[l].Contains(this)) {
            // If the other object isn't in this priority list already, it's added into it
            if (!Scene::PriorityLists[l].Contains(other))
                Scene::PriorityLists[l].Add(other);
        }
        else {
            // If this object isn't in this priority list, the other object is removed from it
            Scene::PriorityLists[l].Remove(other);
        }
    }

    CopyFields(other);
}

void Entity::CopyFields(Entity* other) {
#define COPY(which) other->which = which
    COPY(InitialX);
    COPY(InitialY);
    COPY(Active);
    COPY(Pauseable);
    COPY(Persistence);
    COPY(Interactable);
    COPY(Activity);
    COPY(InRange);

    COPY(X);
    COPY(Y);
    COPY(Z);
    COPY(XSpeed);
    COPY(YSpeed);
    COPY(GroundSpeed);
    COPY(Gravity);
    COPY(Ground);

    COPY(WasOffScreen);
    COPY(OnScreen);
    COPY(OnScreenHitboxW);
    COPY(OnScreenHitboxH);
    COPY(OnScreenRegionTop);
    COPY(OnScreenRegionLeft);
    COPY(OnScreenRegionRight);
    COPY(OnScreenRegionBottom);
    COPY(Visible);
    COPY(ViewRenderFlag);
    COPY(ViewOverrideFlag);
    COPY(RenderRegionW);
    COPY(RenderRegionH);
    COPY(RenderRegionTop);
    COPY(RenderRegionLeft);
    COPY(RenderRegionRight);
    COPY(RenderRegionBottom);

    COPY(Angle);
    COPY(AngleMode);
    COPY(ScaleX);
    COPY(ScaleY);
    COPY(Rotation);
    COPY(Alpha);
    COPY(AutoPhysics);

    COPY(Priority);
    COPY(PriorityListIndex);
    COPY(PriorityOld);

    COPY(Depth);
    COPY(OldDepth);
    COPY(ZDepth);

    COPY(Sprite);
    COPY(CurrentAnimation);
    COPY(CurrentFrame);
    COPY(CurrentFrameCount);
    COPY(AnimationSpeedMult);
    COPY(AnimationSpeedAdd);
    COPY(AutoAnimate);
    COPY(AnimationSpeed);
    COPY(AnimationTimer);
    COPY(AnimationFrameDuration);
    COPY(AnimationLoopIndex);

    COPY(Hitbox);
    COPY(FlipFlag);

    COPY(SensorX);
    COPY(SensorY);
    COPY(SensorCollided);
    COPY(SensorAngle);

    COPY(VelocityX);
    COPY(VelocityY);
    COPY(GroundVel);
    COPY(GravityStrength);
    COPY(OnGround);

    COPY(CollisionLayers);
    COPY(CollisionPlane);
    COPY(CollisionMode);

    COPY(SlotID);

    COPY(Removed);
#undef COPY
}

void Entity::ApplyPhysics() {

}

void Entity::Initialize() {

}
void Entity::Create() {

}
void Entity::PostCreate() {

}

void Entity::UpdateEarly() {
}
void Entity::Update() {
}
void Entity::UpdateLate() {
}

void Entity::OnAnimationFinish() {

}

void Entity::OnSceneLoad() {

}

void Entity::OnSceneRestart() {

}

void Entity::GameStart() {

}

void Entity::RenderEarly() {

}

void Entity::Render(int CamX, int CamY) {

}

void Entity::RenderLate() {

}

void Entity::Remove() {

}

void Entity::Dispose() {

}
