#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Application.h>
#include <Engine/Graphics.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/Scene.h>

#include <Engine/Types/EntityTypes.h>
#include <Engine/Includes/HashMap.h>

need_t ObjectList;
need_t DrawGroupList;

class Entity {
public:
    float        InitialX = 0;
    float        InitialY = 0;
    int          Active = true;
    int          Pauseable = true;

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
    int          ViewRenderFlag = 0;
    float        RenderRegionW = 0.0f;
    float        RenderRegionH = 0.0f;

    int          Angle = 0;
    int          AngleMode = 0;
    float        Rotation = 0.0;
    int          AutoPhysics = false;

    int          Priority = 0;
    int          PriorityListIndex = -1;
    int          PriorityOld = 0;

    float        Depth = 0.0f;
    float        OldDepth = 0.0f;

    int          Sprite = -1;
    int          CurrentAnimation = -1;
    int          CurrentFrame = -1;
    int          CurrentFrameCount = 0;
    float        CurrentFrameTimeLeft = 0.0;
    float        AnimationSpeedMult = 1.0;
    int          AnimationSpeedAdd = 0;
    int          AutoAnimate = true;

    float        HitboxW = 0.0f;
    float        HitboxH = 0.0f;
    float        HitboxOffX = 0.0f;
    float        HitboxOffY = 0.0f;
    int          FlipFlag = 0;

    float        SensorX = 0.0f;
    float        SensorY = 0.0f;
    int          SensorCollided = false;
    int          SensorAngle = 0;

    int          Persistent = false;
    int          Interactable = true;

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
    if (Sprite < 0) return;
    if ((size_t)Sprite >= Scene::SpriteList.size()) return;

    ISprite* sprite = Scene::SpriteList[Sprite]->AsSprite;

    if (CurrentAnimation < 0) return;
    if ((size_t)CurrentAnimation >= sprite->Animations.size()) return;

    if (CurrentFrameTimeLeft > 0.0f) {
        CurrentFrameTimeLeft -= (sprite->Animations[CurrentAnimation].AnimationSpeed * AnimationSpeedMult + AnimationSpeedAdd);
        if (CurrentFrameTimeLeft <= 0.0f) {
            CurrentFrame += 1.0f;
            if ((size_t)CurrentFrame >= sprite->Animations[CurrentAnimation].Frames.size()) {
                CurrentFrame  = sprite->Animations[CurrentAnimation].FrameToLoop;
                OnAnimationFinish();
            }

            if ((size_t)CurrentFrame < sprite->Animations[CurrentAnimation].Frames.size()) {
                sprite = Scene::SpriteList[Sprite]->AsSprite;
                CurrentFrameTimeLeft = sprite->Animations[CurrentAnimation].Frames[CurrentFrame].Duration;
            }
            else {
                CurrentFrameTimeLeft = 1.0f;
            }
        }
    }
    else {
        CurrentFrameTimeLeft = sprite->Animations[CurrentAnimation].Frames[CurrentFrame].Duration;
    }
}
PUBLIC void Entity::SetAnimation(int animation, int frame) {
    if (CurrentAnimation != animation)
        ResetAnimation(animation, frame);
}
PUBLIC void Entity::ResetAnimation(int animation, int frame) {
    if (Sprite < 0) return;
    if ((size_t)Sprite >= Scene::SpriteList.size()) return;

    ISprite* sprite = Scene::SpriteList[Sprite]->AsSprite;

    if (animation < 0) return;
    if ((size_t)animation >= sprite->Animations.size()) return;

    if (frame < 0) return;
    if ((size_t)frame >= sprite->Animations[animation].Frames.size()) return;

    CurrentAnimation = animation;
    CurrentFrame = frame;
    CurrentFrameTimeLeft = sprite->Animations[CurrentAnimation].Frames[CurrentFrame].Duration;
    CurrentFrameCount = (int)sprite->Animations[CurrentAnimation].Frames.size();
    AnimationSpeedAdd = 0;
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

    if (collideSideHori && collideSideVert) {
        // print
        //     " deltaSquaredX1: " + deltaSquaredX1 +
        //     " deltaSquaredX2: " + deltaSquaredX2 +
        //     " deltaSquaredY1: " + deltaSquaredY1 +
        //     " deltaSquaredY2: " + deltaSquaredY2;
        // print
        //     " collideSideHori: " + collideSideHori +
        //     " collideSideVert: " + collideSideVert;
    }

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

int     ApplyPhysics_InnerHitboxLeft;
int     ApplyPhysics_InnerHitboxRight;
Entity* ApplyPhysics_CurrentEntity;
int     ApplyPhysics_OuterHitboxTop;
int     ApplyPhysics_OuterHitboxLeft;
int     ApplyPhysics_OuterHitboxRight;
int     ApplyPhysics_OuterHitboxBottom;
int     ApplyPhysics_CheckCenterY;
int     ApplyPhysics_MaxTileCollisionDistance;
int     ApplyPhysics_SpeedPartitionValue;
/*
// Recreate this using Mania Source
struct SensorGroup {
    Sensor LeftSensor;
    Sensor MiddleSensor;
    Sensor RightSensor;
    Sensor WallSensor;
    int InitialX;
    int InitialY;
};
SensorStruct wallSensor;
PU BLIC void Entity::CheckLeftWall(float x, float y) {
    if (this.WallSensor_Collided = this.SenseTileCollision(x, y, CollideSide_RIGHT, -this.FloorDirection_Y, -this.FloorDirection_X, -8.0, 1.0)) {
        this.WallSensor_X = Math.Floor(this.STC_X);
        this.WallSensor_Y = this.STC_Y;
        // this.WallSensor_Angle = this.STC_Angle;
        this.WallSensor_Length = this.STC_Length;
    }
    return this.WallSensor_Collided;
}
PU BLIC void Entity::CheckRightWall(float x, float y) {
    // Setting the end thing to 0.0 seems to make this work in air
    // 1.0, 0.0
    // this.FloorDirection_Y, this.FloorDirection_X
    if (this.WallSensor_Collided = this.SenseTileCollision(float x, float y, CollideSide_LEFT, this.FloorDirection_Y, this.FloorDirection_X, -8.0, 1.0)) {
        this.WallSensor_X = Math.Floor(this.STC_X);
        this.WallSensor_Y = this.STC_Y;
        // this.WallSensor_Angle = this.STC_Angle;
        this.WallSensor_Length = this.STC_Length;
    }
    return this.WallSensor_Collided;
}
PU BLIC void Entity::CheckLeftWall_OmniDir(float x, float y, int angleMode) {
    var length = 14.0;
    angleMode = (angleMode + 3) & 3;
    this.WallSensor_Collided = false;
    switch (angleMode) {
        // Checking below
        case 0:
            if (Static.SenseTileCollision(this, x, y, CollideSide_TOP, 0.0, 1.0, -16.0, 16.0, this.PlaneIndex)) {
                if (this.STC_Length >= -length && this.STC_Length < 0.0) {
                    this.WallSensor_X = Math.Floor(this.STC_X);
                    this.WallSensor_Y = Math.Floor(this.STC_Y);
                    this.WallSensor_Length = this.STC_Length;
                    this.WallSensor_Collided = true;
                    return true;
                }
            }
            return false;
        // Checking to right
        case 1:
            if (Static.SenseTileCollision(this, x, y, CollideSide_LEFT, 1.0, 0.0, -16.0, 16.0, this.PlaneIndex)) {
                if (this.STC_Length >= -length && this.STC_Length <= 0.0) {
                    this.WallSensor_X = Math.Floor(this.STC_X);
                    this.WallSensor_Y = Math.Floor(this.STC_Y);
                    this.WallSensor_Length = this.STC_Length;
                    this.WallSensor_Collided = true;
                    return true;
                }
            }
            return false;
        // Checking above
        case 2:
            if (Static.SenseTileCollision(this, x, y, CollideSide_BOTTOM, 0.0, -1.0, -16.0, 16.0, this.PlaneIndex)) {
                if (this.STC_Length >= -length && this.STC_Length < 0.0) {
                    this.WallSensor_X = Math.Floor(this.STC_X);
                    this.WallSensor_Y = Math.Floor(this.STC_Y);
                    this.WallSensor_Length = this.STC_Length;
                    this.WallSensor_Collided = true;
                    return true;
                }
            }
            return false;
        // Checking to left
        case 3:
            if (Static.SenseTileCollision(this, x, y, CollideSide_RIGHT, -1.0, 0.0, -16.0, 16.0, this.PlaneIndex)) {
                if (this.STC_Length >= -length && this.STC_Length <= 0.0) {
                    this.WallSensor_X = Math.Floor(this.STC_X);
                    this.WallSensor_Y = Math.Floor(this.STC_Y);
                    this.WallSensor_Length = this.STC_Length;
                    this.WallSensor_Collided = true;
                    return true;
                }
            }
            return false;
    }
    return false;
}
PU BLIC void Entity::CheckRightWall_OmniDir(float x, float y, int angleMode) {
    var length = 14.0;
    angleMode = (angleMode + 1) & 3;
    this.WallSensor_Collided = false;
    switch (angleMode) {
        // Checking below
        case 0:
            if (Static.SenseTileCollision(this, x, y, CollideSide_TOP, 0.0, 1.0, -16.0, 16.0, this.PlaneIndex)) {
                if (this.STC_Length >= -length && this.STC_Length < 0.0) {
                    this.WallSensor_X = Math.Floor(this.STC_X);
                    this.WallSensor_Y = Math.Floor(this.STC_Y);
                    this.WallSensor_Length = this.STC_Length;
                    this.WallSensor_Collided = true;
                    return true;
                }
            }
            return false;
        // Checking to right
        case 1:
            if (Static.SenseTileCollision(this, x, y, CollideSide_LEFT, 1.0, 0.0, -16.0, 16.0, this.PlaneIndex)) {
                if (this.STC_Length >= -length && this.STC_Length <= 0.0) {
                    this.WallSensor_X = Math.Floor(this.STC_X);
                    this.WallSensor_Y = Math.Floor(this.STC_Y);
                    this.WallSensor_Length = this.STC_Length;
                    this.WallSensor_Collided = true;
                    return true;
                }
            }
            return false;
        // Checking above
        case 2:
            if (Static.SenseTileCollision(this, x, y, CollideSide_BOTTOM, 0.0, -1.0, -16.0, 16.0, this.PlaneIndex)) {
                if (this.STC_Length >= -length && this.STC_Length < 0.0) {
                    this.WallSensor_X = Math.Floor(this.STC_X);
                    this.WallSensor_Y = Math.Floor(this.STC_Y);
                    this.WallSensor_Length = this.STC_Length;
                    this.WallSensor_Collided = true;
                    return true;
                }
            }
            return false;
        // Checking to left
        case 3:
            if (Static.SenseTileCollision(this, x, y, CollideSide_RIGHT, -1.0, 0.0, -16.0, 16.0, this.PlaneIndex)) {
                if (this.STC_Length >= -length && this.STC_Length <= 0.0) {
                    this.WallSensor_X = Math.Floor(this.STC_X);
                    this.WallSensor_Y = Math.Floor(this.STC_Y);
                    this.WallSensor_Length = this.STC_Length;
                    this.WallSensor_Collided = true;
                    return true;
                }
            }
            return false;
    }
    return false;
}
PU BLIC void Entity::CheckCeilLeftSensor(float x, float y) {
    if (this.LeftCeilSensor_Collided = this.SenseTileCollision(float x, float y, this.FloorSensor_Side ^ 5, this.FloorDirection_X, -this.FloorDirection_Y, -10.0, 0.0)) {
        this.LeftCeilSensor_X = this.STC_X;
        this.LeftCeilSensor_Y = this.STC_Y;
        this.LeftCeilSensor_Angle = this.STC_Angle;
        this.LeftCeilSensor_Length = this.STC_Length;
    }
    return this.LeftCeilSensor_Collided;
}
PU BLIC void Entity::CheckCeilRightSensor(float x, float y) {
    if (this.RightCeilSensor_Collided = this.SenseTileCollision(float x, float y, this.FloorSensor_Side ^ 5, this.FloorDirection_X, -this.FloorDirection_Y, -10.0, 0.0)) {
        this.RightCeilSensor_X = this.STC_X;
        this.RightCeilSensor_Y = this.STC_Y;
        this.RightCeilSensor_Angle = this.STC_Angle;
        this.RightCeilSensor_Length = this.STC_Length;
    }
    return this.RightCeilSensor_Collided;
}
PU BLIC void Entity::CheckFloorLeftSensor(float x, float y, int initialAngle) {
    if (this.LeftSensor_Collided = this.SenseTileCollision(float x, float y, 0x10 | this.FloorSensor_Side, this.FloorDirection_X, this.FloorDirection_Y, -14.0, this.MaxTileCollision)) {
        if (initialAngle != -1 &&
            Math.Abs(((initialAngle - this.STC_Angle + 0x80) & 0xFF) - 0x80) > 0x20) {
            return this.LeftSensor_Collided = false;
        }
        if (this.STC_Length <= -14.0)
            return this.LeftSensor_Collided = false;
        this.LeftSensor_X = this.STC_X;
        this.LeftSensor_Y = this.STC_Y;
        this.LeftSensor_Angle = this.STC_Angle;
        this.LeftSensor_Length = this.STC_Length;
    }
    return this.LeftSensor_Collided;
}
PU BLIC void Entity::CheckFloorMiddleSensor(float x, float y, int initialAngle) {
    if (this.MiddleSensor_Collided = this.SenseTileCollision(float x, float y, 0x10 | this.FloorSensor_Side, this.FloorDirection_X, this.FloorDirection_Y, -14.0, this.MaxTileCollision)) {
        if (initialAngle != -1 &&
            Math.Abs(((initialAngle - this.STC_Angle + 0x80) & 0xFF) - 0x80) > 0x20) {
            return this.MiddleSensor_Collided = false;
        }
        if (this.STC_Length <= -14.0)
            return this.MiddleSensor_Collided = false;
        this.MiddleSensor_X = this.STC_X;
        this.MiddleSensor_Y = this.STC_Y;
        this.MiddleSensor_Angle = this.STC_Angle;
        this.MiddleSensor_Length = this.STC_Length;
    }
    return this.MiddleSensor_Collided;
}
PU BLIC void Entity::CheckFloorRightSensor(float x, float y, int initialAngle) {
    if (this.RightSensor_Collided = this.SenseTileCollision(float x, float y, 0x10 | this.FloorSensor_Side, this.FloorDirection_X, this.FloorDirection_Y, -14.0, this.MaxTileCollision)) {
        if (initialAngle != -1 &&
            Math.Abs(((initialAngle - this.STC_Angle + 0x80) & 0xFF) - 0x80) > 0x20) {
            return this.RightSensor_Collided = false;
        }
        if (this.STC_Length <= -14.0)
            return this.RightSensor_Collided = false;
        this.RightSensor_X = this.STC_X;
        this.RightSensor_Y = this.STC_Y;
        this.RightSensor_Angle = this.STC_Angle;
        this.RightSensor_Length = this.STC_Length;
    }
    return this.RightSensor_Collided;
}
PU BLIC bool Entity::SenseTileCollision(float x, float y, int side, float dirX, float dirY, int startLen, int endLen, int* angle) {
    x += dirX * startLen;
    y += dirY * startLen;

    var angle;
    for (var i = startLen; i < endLen; i += 1.0) {
        angle = TileCollision.PointExtended(float x, float y, this.PlaneIndex, side);
        if (angle >= 0) {
            this.STC_X = Math.Floor(x);
            this.STC_Y = Math.Floor(y);
            this.STC_Angle = angle;
            this.STC_Length = i;
            return true;
        }

        x += dirX;
        y += dirY;
    }
    return false;
}
PU BLIC void Entity::InitGroundSensors(float initialX, float initialY, int initialAngle, int angleMode) {
    var floor_sensor_cos = this.SensorABCD_Ground_cos;
    var floor_sensor_sin = this.SensorABCD_Ground_sin;
    var wall_sensor_cos = this.SensorEF_cos;
    var wall_sensor_sin = this.SensorEF_sin;
    var simple_cos = this.simple_cos;
    var simple_sin = this.simple_sin;
    var simple_side = this.simple_side;

    this.FloorDirection_X = simple_sin[angleMode];
    this.FloorDirection_Y = simple_cos[angleMode];
    this.FloorSensor_Side = simple_side[angleMode];

    var wall_x = initialX;
    var wall_y = initialY + this.CheckCenterOffY;
    // 1 | 3
    if (angleMode & 1)
        wall_y = initialY;

    var foot_x = initialX + (this.HitboxH * 0.5 + this.HitboxOffY) * this.FloorDirection_X;
    var foot_y = initialY + (this.HitboxH * 0.5 + this.HitboxOffY) * this.FloorDirection_Y;
    this.LeftSensor_X = foot_x - floor_sensor_cos[angleMode];
    this.LeftSensor_Y = foot_y - floor_sensor_sin[angleMode];
    this.RightSensor_X = foot_x + floor_sensor_cos[angleMode];
    this.RightSensor_Y = foot_y + floor_sensor_sin[angleMode];
    this.MiddleSensor_X = foot_x;
    this.MiddleSensor_Y = foot_y;
    if (this.GroundSpeed <= 0.0) {
        this.WallSensor_X = wall_x - wall_sensor_cos[angleMode] - 1.0;
        this.WallSensor_Y = wall_y - wall_sensor_sin[angleMode];
    }
    else {
        this.WallSensor_X = wall_x + wall_sensor_cos[angleMode];
        this.WallSensor_Y = wall_y + wall_sensor_sin[angleMode];
    }
    this.LeftSensor_Angle =
        this.RightSensor_Angle =
        this.MiddleSensor_Angle = initialAngle;
    this.LeftSensor_Length =
        this.RightSensor_Length =
        this.MiddleSensor_Length = 0;
    this.LeftSensor_Collided =
        this.RightSensor_Collided =
        this.MiddleSensor_Collided =
        this.WallSensor_Collided = false;
}
PU BLIC void Entity::InitAirSensors(float initialX, float initialY) {
    var floor_sensor_cos = this.SensorEF_cos; // this.SensorABCD_Air_cos;
    var floor_sensor_sin = this.SensorEF_sin; // this.SensorABCD_Air_sin;
    var wall_sensor_cos = this.SensorEF_cos;
    var wall_sensor_sin = this.SensorEF_sin;
    var simple_cos = this.simple_cos;
    var simple_sin = this.simple_sin;
    var simple_side = this.simple_side;

    this.FloorDirection_X = 0.0;
    this.FloorDirection_Y = 1.0;
    this.FloorSensor_Side = CollideSide_TOP;

    var wall_x = initialX;
    var wall_y = initialY + this.CheckCenterOffY;
    var foot_x = initialX;
    var foot_y = initialY + (this.HitboxH * 0.5 + this.HitboxOffY);
    var head_x = initialX;
    var head_y = initialY - (this.HitboxH * 0.5 + this.HitboxOffY) - 1.0;

    // Moving right
    if (this.XSpeed >= 0.0) {
        this.WallSensor_X = wall_x + wall_sensor_cos[0];
        this.WallSensor_Y = wall_y;
    }
    // Moving left
    if (this.XSpeed <= 0.0) {
        this.WallSensor_X = wall_x - wall_sensor_cos[0] - 1.0;
        this.WallSensor_Y = wall_y;
    }

    // Moving down
    this.LeftSensor_X = foot_x - floor_sensor_cos[0] + 1;
    this.LeftSensor_Y = foot_y;
    this.RightSensor_X = foot_x + floor_sensor_cos[0] - 2;
    this.RightSensor_Y = foot_y;

    this.LeftCeilSensor_X = head_x - floor_sensor_cos[0] + 1;
    this.LeftCeilSensor_Y = head_y;
    this.RightCeilSensor_X = head_x + floor_sensor_cos[0] - 2;
    this.RightCeilSensor_Y = head_y;

    this.LeftSensor_Angle =
        this.RightSensor_Angle =
        this.LeftCeilSensor_Angle =
        this.RightCeilSensor_Angle = 0;
    this.LeftSensor_Collided =
        this.RightSensor_Collided =
        this.LeftCeilSensor_Collided =
        this.RightCeilSensor_Collided =
        this.WallSensor_Collided = false;
}
PU BLIC void Entity::Entity_GroundMovement() {
    // NOTE: More parts = more precision & more CPU usage
    //    Mania uses 4.0, we will use 4.0 until otherwise.
    var parts = 4.0;

    var groundSpeed = Math.Abs(this.GroundSpeed);
    var speedPart = Math.Floor(groundSpeed / parts);
    var speedFraction = groundSpeed - speedPart * parts;
    var speedPartRemain = speedFraction;

    var angle, angleMode, gsp, cos, sin, deltaX, deltaY;

    var initialX = this.X;
    var initialY = this.Y;
    var initialAngle = this.Angle;

    var wall_sensor_cos = this.SensorEF_cos;
    var wall_sensor_sin = this.SensorEF_sin;

    var sensorsAngle = this.sensorsAngle;
    var sensorsLength = this.sensorsLength;
    var sensorsCollided = this.sensorsCollided;

    gsp = this.GroundSpeed;

    this.InitGroundSensors(initialX, initialY, initialAngle, this.AngleMode);

    var count = 0;
    while (speedPartRemain >= 0.0) {
        angle = this.Angle;
        angleMode = this.AngleMode;
        cos = Static.HexCos(angle);
        sin = Static.HexSin(angle);
        if (speedPart > 0.0) {
            deltaX = cos * parts;
            deltaY = sin * parts;
            speedPartRemain = speedPart - 1.0;
        }
        else {
            deltaX = speedFraction * cos;
            deltaY = speedFraction * sin;
            speedPartRemain = -1.0;
        }

        if (gsp < 0.0) {
            deltaX = -deltaX;
            deltaY = -deltaY;
        }

        // if (!this.WallSensor_Collided)
        this.InitGroundSensors(initialX, initialY, initialAngle, angleMode);

        initialX += deltaX;
        initialY += deltaY;

        // TODO: Fix
        // Check Walls here
        if (gsp <= 0) {
            // CheckLeftWall, max left 0xE past hitbox
            // if (angleMode == 0) if (this.CheckLeftWall(this.WallSensor_X, this.WallSensor_Y)) {
            if (this.CheckLeftWall_OmniDir(this.WallSensor_X, this.WallSensor_Y, angleMode)) {
                speedPartRemain = -1;
                if (!(angleMode & 1))
                    deltaX = 0.0;
                else
                    deltaY = 0.0;
                if (angleMode == 0)
                    this.LeftSensor_X = this.WallSensor_X + 2.0;
            }
        }
        else {
            // CheckRightWall
            // if (angleMode == 0) if (this.CheckRightWall(this.WallSensor_X, this.WallSensor_Y)) {
            if (this.CheckRightWall_OmniDir(this.WallSensor_X, this.WallSensor_Y, angleMode)) {
                speedPartRemain = -1;
                if (!(angleMode & 1))
                    deltaX = 0.0;
                else
                    deltaY = 0.0;
                if (angleMode == 0)
                    this.RightSensor_X = this.WallSensor_X - 2.0;
            }
        }

        // Check Floor here
        this.LeftSensor_X += deltaX;
        this.MiddleSensor_X += deltaX;
        this.RightSensor_X += deltaX;
        this.LeftSensor_Y += deltaY;
        this.MiddleSensor_Y += deltaY;
        this.RightSensor_Y += deltaY;
        this.CheckFloorLeftSensor(this.LeftSensor_X, this.LeftSensor_Y, initialAngle);
        this.CheckFloorMiddleSensor(this.MiddleSensor_X, this.MiddleSensor_Y, initialAngle);
        this.CheckFloorRightSensor(this.RightSensor_X, this.RightSensor_Y, initialAngle);

        // Determine sensor to use
        var highestSensor = -1;
        sensorsAngle[0] = this.LeftSensor_Angle;
        sensorsAngle[1] = this.MiddleSensor_Angle;
        sensorsAngle[2] = this.RightSensor_Angle;
        sensorsLength[0] = this.LeftSensor_Length;
        sensorsLength[1] = this.MiddleSensor_Length;
        sensorsLength[2] = this.RightSensor_Length;
        sensorsCollided[0] = this.LeftSensor_Collided;
        sensorsCollided[1] = this.MiddleSensor_Collided;
        sensorsCollided[2] = this.RightSensor_Collided;

        // Get highest sensor
        for (var i = 0; i < 3; i++) {
            if (highestSensor == -1) {
                if (sensorsCollided[i])
                    highestSensor = i;
            }
            else if (sensorsCollided[i]) {
                var currentLen = sensorsLength[i];
                var highestLen = sensorsLength[highestSensor];

                // if (angleMode != 0) {
                //     print "sensorsAngle["+i+"]: " + sensorsAngle[i];
                //     print "sensorsLength["+i+"]: " + sensorsLength[i];
                // }

                if (currentLen < highestLen) {
                    highestSensor = i;
                }
                else if (currentLen == highestLen) {
                    var currentAngle = sensorsAngle[i];
                    if (angleMode == 0) {
                        if (currentAngle < 0x08 || currentAngle > 0xF8)
                            highestSensor = i;
                    }
                    else {
                        highestSensor = i;
                    }
                }
            }
        }

        // Set future positions
        var finalAngle = -1;
        if (highestSensor == -1) {
            finalAngle = initialAngle;
            speedPartRemain = -1;
        }
        else {
            finalAngle = sensorsAngle[highestSensor];
            initialX += sensorsLength[highestSensor] * this.FloorDirection_X;
            initialY += sensorsLength[highestSensor] * this.FloorDirection_Y;
        }

        // Change AngleMode
        // For some reason the AngleMode used in Mania and 3K is so different
        // if (3KBased) this.AngleMode = this.GetAngleMode(this.Angle ^ 0xFF) >> 6;
        // var lastAngleMode = this.AngleMode;
        if (this.AngleMode == 0) { // 0x00 +/- 0x22
            if (((finalAngle + 0x7F) & 0xFF) <= 0x5C)
                this.AngleMode = 1;
            if (((finalAngle - 0x23 + 0x100) & 0xFF) <= 0x5C)
                this.AngleMode = 3;
        }
        else if (this.AngleMode == 1) { // 0xC0 +/- 0x22
            if (finalAngle > 0xE2)
                this.AngleMode = 0;
            if (finalAngle < 0x9E)
                this.AngleMode = 2;
        }
        else if (this.AngleMode == 2) { // 0x80 +/- 0x22
            if (finalAngle > 0xA2)
                this.AngleMode = 1;
            if (finalAngle < 0x5E)
                this.AngleMode = 3;
        }
        else if (this.AngleMode == 3) { // 0x40 +/- 0x22
            if (finalAngle > 0x62)
                this.AngleMode = 2;
            if (finalAngle < 0x1E)
                this.AngleMode = 0;
        }

        // if (this.AngleMode != lastAngleMode) {
        //     print "this.AngleMode: " + lastAngleMode + " -> " + this.AngleMode;
        // }

        if (this.WallSensor_Collided)
            break;

        // if (highestSensor != -1) {
            this.Angle = finalAngle;
        // }

        speedPart = speedPartRemain;
    }

    // On floor?
    if (this.LeftSensor_Collided || this.MiddleSensor_Collided || this.RightSensor_Collided) {
        if (this.WallSensor_Collided) {
            // TODO: Make these AngleMode agnostic
            if (gsp < 0.0) {
                this.X = this.WallSensor_X + wall_sensor_cos[angleMode] + 1.0;
                this.Y = initialY;
                this.XSpeed = 0.0;
                this.GroundSpeed = 0.0;
            }
            else if (gsp > 0.0) {
                this.X = this.WallSensor_X - wall_sensor_cos[angleMode];
                this.Y = initialY;
                this.XSpeed = 0.0;
                this.GroundSpeed = 0.0;
            }
            else {
                this.Y = initialY;
                this.XSpeed = 0.0;
                this.GroundSpeed = 0.0;
            }
        }
        else {
            this.X = initialX;
            this.Y = initialY;
        }
    }
    // Not on floor
    else {
        this.Ground = false;

        this.XSpeed = this.GroundSpeed * Static.HexCos(this.Angle);
        this.YSpeed = this.GroundSpeed * Static.HexSin(this.Angle);

        this.YSpeed = Math.Clamp(this.YSpeed, -16.0, 16.0);

        this.GroundSpeed = this.XSpeed;
        this.Angle = 0;
        this.AngleMode = 0;
        if (this.WallSensor_Collided) {
            if (this.XSpeed < 0.0) {
                this.X = this.WallSensor_X + wall_sensor_cos[angleMode] + 1.0;
                this.Y += this.YSpeed;
                this.XSpeed = 0.0;
                this.GroundSpeed = 0.0;
            }
            else if (this.XSpeed > 0.0) {
                this.X = this.WallSensor_X - wall_sensor_cos[angleMode];
                this.Y += this.YSpeed;
                this.XSpeed = 0.0;
                this.GroundSpeed = 0.0;
            }
            else {
                this.Y += this.YSpeed;
                this.XSpeed = 0.0;
                this.GroundSpeed = 0.0;
            }
        }
        else {
            this.X += this.XSpeed;
            this.Y += this.YSpeed;
        }
    }
}
PU BLIC void Entity::Entity_AirMovement() {
    var movingUp = false;
    var movingDown = false;
    var movingLeft = false;
    var movingRight = false;

    var xspeed = this.XSpeed;
    var yspeed = this.YSpeed;

    // NOTE: This is a custom addition
    this.Angle = 0;
    this.AngleMode = 0;
    // this.HitboxHalfH = 15.0;

    // Determine if moving right
    if (xspeed >= 0.0) {
        movingRight = true;
    }
    if (xspeed <= 0.0) {
        movingLeft = true;
    }
    if (yspeed >= 0.0) {
        movingDown = true;
    }
    if (Math.Abs(xspeed) > 1.0 || yspeed <= 0.0) {
        movingUp = true;
    }

    // var doSensor;
    var absXSpeed = Math.Abs(xspeed);
    var absYSpeed = Math.Abs(yspeed);
    var absMaxSpeed = Math.Max(absXSpeed, absYSpeed);
    var speedDivisor = ((Number.AsInteger(absMaxSpeed) << 16) >> this.SpeedPartitionValue) + 1.0;
    var xspeedPart = xspeed / speedDivisor;
    var yspeedPart = yspeed / speedDivisor;
    var xspeedFraction = xspeed - xspeedPart * (speedDivisor - 1.0);
    var yspeedFraction = yspeed - yspeedPart * (speedDivisor - 1.0);

    var originalXSpeed = this.XSpeed;

    var speedPartRemain, deltaX, deltaY;
    if (speedDivisor > 0) {
        deltaX = deltaY = 0.0;
        while (true) {
            if (speedDivisor >= 2.0) {
                deltaX += xspeedPart;
                deltaY += yspeedPart;
            }
            else {
                deltaX += xspeedFraction;
                deltaY += yspeedFraction;
            }
            speedPartRemain = speedDivisor - 1.0;

            this.InitAirSensors(this.X + deltaX, this.Y + deltaY);

            // print "Y: " + (this.Y + deltaY);
            // print "this.LeftSensor_Y: " + this.LeftSensor_Y;
            // print "distance: " + (this.LeftSensor_Y - (this.Y + deltaY));

            // Check for right wall collision
            if (movingRight == 1) {
                if (this.CheckRightWall(this.WallSensor_X, this.WallSensor_Y)) {
                    movingRight = 2;
                }
                else if (this.XSpeed < 2.0 && this.CheckCenterOffY > 0.0) {
                    if (this.CheckRightWall(this.WallSensor_X, this.WallSensor_Y - 2.0 * this.CheckCenterOffY))
                        movingRight = 2;
                }
            }

            // Check for left wall collision
            if (movingLeft == 1) {
                if (this.CheckLeftWall(this.WallSensor_X, this.WallSensor_Y)) {
                    movingLeft = 2;
                }
                else if (this.XSpeed > -2.0 && this.CheckCenterOffY > 0.0) {
                    if (this.CheckLeftWall(this.WallSensor_X, this.WallSensor_Y - 2.0 * this.CheckCenterOffY))
                        movingLeft = 2;
                }
            }

            // If there was a right wall collision
            if (movingRight == 2) {
                this.XSpeed = 0.0;
                this.GroundSpeed = 0.0;
                this.X = this.WallSensor_X - this.SensorEF_cos[0];
                this.LeftSensor_X = this.LeftCeilSensor_X = this.X - this.SensorEF_cos[0] + 1.0;
                this.RightSensor_X = this.RightCeilSensor_X = this.X + this.SensorEF_cos[0] - 2.0;
                // print "right wall (x: " + this.X + " wx: " + this.WallSensor_X + " ef: " + this.SensorEF_cos[0] + " rx: " + this.RightSensor_X + ")";
                movingRight = 3;
                speedPartRemain = 0.0;
            }

            // If there was a left wall collision
            if (movingLeft == 2) {
                this.XSpeed = 0.0;
                this.GroundSpeed = 0.0;
                this.X = this.WallSensor_X + this.SensorEF_cos[0] + 1.0;
                this.LeftSensor_X = this.LeftCeilSensor_X = this.X - this.SensorEF_cos[0] + 1.0;
                this.RightSensor_X = this.RightCeilSensor_X = this.X + this.SensorEF_cos[0] - 2.0;
                movingLeft = 3;
                speedPartRemain = 0.0;
            }

            // If moving down
            if (movingDown == 1) {
                var gh = this.MaxTileCollision;

                this.MaxTileCollision = 0.0;

                var ls = this.LeftSensor_Y;
                var rs = this.RightSensor_Y;

                if (this.CheckFloorLeftSensor(this.LeftSensor_X, this.LeftSensor_Y, -1)) {
                    movingDown = 2;
                    speedPartRemain = 0;

                    // print "left side collision (x: " + this.LeftSensor_X + ", y: " + this.LeftSensor_Y + ", ang: " + Number.ToString(this.LeftSensor_Angle, 16) + ")";
                }
                if (this.CheckFloorRightSensor(this.RightSensor_X, this.RightSensor_Y, -1)) {
                    movingDown = 2;
                    speedPartRemain = 0;

                    // print "right side collision (x: " + this.RightSensor_X + ", y: " + this.RightSensor_Y + ", ang: " + Number.ToString(this.RightSensor_Angle, 16) + ")";
                }

                this.MaxTileCollision = gh;
            }

            // If moving up
            if (movingUp == 1) {
                if (this.CheckCeilLeftSensor(this.LeftCeilSensor_X, this.LeftCeilSensor_Y)) {
                    movingUp = 2;
                    speedPartRemain = 0;
                }
                if (this.CheckCeilRightSensor(this.RightCeilSensor_X, this.RightCeilSensor_Y)) {
                    movingUp = 2;
                    speedPartRemain = 0;
                }
            }

            speedDivisor = speedPartRemain;
            if (speedPartRemain <= 0.0)
                break;
        }
    }

    if (movingLeft == 3) {
        // print "hit left movingDown: " + movingDown +
        //     " xspeed: " + xspeed +
        //     " yspeed: " + yspeed;
    }

    // If no collisions, just move
    if (movingRight < 2 && movingLeft < 2) {
        this.X += this.XSpeed;
    }
    if (movingDown < 2 && movingUp < 2) {
        this.Y += this.YSpeed;
        return;
    }

    // If moving down and collided
    if (movingDown == 2) {
        this.Ground = true;

        var doSensor;
        if (this.LeftSensor_Collided && this.RightSensor_Collided)
            doSensor = this.LeftSensor_Length < this.RightSensor_Length ? -1 : 1;
        else if (this.LeftSensor_Collided && !this.RightSensor_Collided)
            doSensor = -1;
        else
            doSensor = 1;

        if (doSensor == -1) {
            if (movingLeft == 3 || movingRight == 3)
                this.XSpeed = originalXSpeed;
            this.Y = this.LeftSensor_Y - (this.HitboxH * 0.5 + this.HitboxOffY);
            this.Angle = this.LeftSensor_Angle;
            this.Entity_AirMovementDownwards_Angles();
        }
        else if (doSensor == 1) {
            if (movingLeft == 3 || movingRight == 3)
                this.XSpeed = originalXSpeed;
            this.Y = this.RightSensor_Y - (this.HitboxH * 0.5 + this.HitboxOffY);
            this.Angle = this.RightSensor_Angle;
            this.Entity_AirMovementDownwards_Angles();
        }
    }

    // If moving up and collided
    if (movingUp == 2) {
        var doSensor;
        if (this.LeftCeilSensor_Collided && this.RightCeilSensor_Collided)
            doSensor = this.LeftCeilSensor_Length < this.RightCeilSensor_Length ? -1 : 1;
        else if (this.LeftCeilSensor_Collided && !this.RightCeilSensor_Collided)
            doSensor = -1;
        else
            doSensor = 1;

        if (doSensor == -1) {
            // if (movingLeft == 3 || movingRight == 3)
            //     this.XSpeed = originalXSpeed;

            this.Y = this.LeftCeilSensor_Y + (this.HitboxH * 0.5 + this.HitboxOffY) + 1.0;
            this.Entity_AirMovementUpwards_Angles(this.LeftCeilSensor_Angle);
        }
        else if (doSensor == 1) {
            // if (movingLeft == 3 || movingRight == 3)
            //     this.XSpeed = originalXSpeed;

            this.Y = this.RightCeilSensor_Y + (this.HitboxH * 0.5 + this.HitboxOffY) + 1.0;
            this.Entity_AirMovementUpwards_Angles(this.RightCeilSensor_Angle);
        }
    }
}
PU BLIC void Entity::Entity_AirMovementDownwards_Angles() {
    var angle = this.Angle;
    var angleMode = this.AngleMode;
    // Setting angle modes
    if (angle > 0xA0 && angle < 0xDE && angleMode != 1) {
        this.X -= 4.0;
        this.AngleMode = 1;
    }
    if (angle > 0x22 && angle < 0x60 && angleMode != 3) {
        this.X += 4.0;
        this.AngleMode = 3;
    }

    //
    var xspeed = this.XSpeed;
    var yspeed = this.YSpeed;

    // print "xspeed: " + xspeed;

    var gspeed = xspeed;
    var absXSpeed = Math.Abs(xspeed);
    var absYSpeed = Math.Abs(yspeed);
    var absYSpeedHalf = absYSpeed * 0.5;
    var sign = Math.Sign(Static.HexSin(angle));
    if (angle >= 0x80) {
        // Full Steep
        if (angle <= 0xE0) {
            if (absXSpeed <= absYSpeed)
                gspeed = -yspeed;
        }
        // Half Steep
        else if (angle <= 0xF0) {
            if (absXSpeed <= absYSpeedHalf)
                gspeed = -yspeed * 0.5;
        }
        // Shallow
        else {
            gspeed = xspeed;
        }
    }
    else {
        // Full Steep
        if (angle >= 0x20) {
            if (absXSpeed <= absYSpeed)
                gspeed = yspeed;
        }
        // Half Steep
        else if (angle >= 0x10) {
            if (absXSpeed <= absYSpeedHalf)
                gspeed = yspeed * 0.5;
        }
        // Shallow
        else {
            gspeed = xspeed;
        }
    }

    // print "angle: " + angle;
    // print "gspeed: " + gspeed;

    // Clamp Speeds
    this.GroundSpeed = gspeed;
    this.XSpeed = gspeed;
    this.YSpeed = 0.0;
}
PU BLIC void Entity::Entity_AirMovementUpwards_Angles(int angle) {
    var xspeed = this.XSpeed;
    var yspeed = this.YSpeed;

    var gspeed = xspeed;
    var absXSpeed = Math.Abs(xspeed);
    var absYSpeed = Math.Abs(yspeed);

    // Upward Slope (Right)
    if (angle >= 0x9F && angle <= 0xC0) {
        if (yspeed < -absXSpeed) {
            this.X -= 4.0;
            this.Y -= 2.0;
            this.Ground = true;
            this.Angle = angle;
            this.AngleMode = 1;

            gspeed = -yspeed;
            if (angle <= 0x9F)
                gspeed *= 0.5;

            this.GroundSpeed = gspeed;
        }
    }
    // Upward Slope (Left)
    else if (angle >= 0x40 && angle <= 0x61) {
        if (yspeed < -absXSpeed) {
            this.X += 4.0;
            this.Y -= 2.0;
            this.Ground = true;
            this.Angle = angle;
            this.AngleMode = 3;

            gspeed = yspeed;
            if (angle >= 0x61)
                gspeed *= 0.5;

            this.GroundSpeed = gspeed;
        }
    }

    if (this.YSpeed < 0.0)
        this.YSpeed = 0.0;
}
//*/
PUBLIC void Entity::ApplyPhysics() {
    // if (this.UseGroundSpeed) {
    //     this.Angle &= 0xFF;
    //
    //     if (Math.Abs(this.GroundSpeed) >= 6.0)
    //         this.MaxTileCollision = 15;
    //     else if (this.Angle != 0)
    //         this.MaxTileCollision = 15;
    //     else
    //         this.MaxTileCollision = 8;
    //
    //     this.CheckCenterOffY = 0.0;
    //     if ((this.HitboxH * 0.5 + this.HitboxOffY) >= 14.0) {
    //         this.CheckCenterOffY = 4.0;
    //         this.SpeedPartitionValue = 19;
    //     }
    //     else {
    //         this.CheckCenterOffY = 0.0;
    //         this.SpeedPartitionValue = 17;
    //         this.MaxTileCollision = 15;
    //     }
    //
    //     if (this.Ground)
    //         this.Entity_GroundMovement();
    //     else
    //         this.Entity_AirMovement();
    //
    //     if (this.Ground) {
    //         this.XSpeed = this.GroundSpeed * Static.HexCos(this.Angle);
    //         this.YSpeed = this.GroundSpeed * Static.HexSin(this.Angle);
    //     }
    //     else {
    //         this.GroundSpeed = this.XSpeed;
    //     }
    // }
    // else {
    //     this.X += this.XSpeed;
    //     this.Y += this.YSpeed;
    // }
}

PUBLIC VIRTUAL void Entity::GameStart() {

}

PUBLIC VIRTUAL void Entity::Create(int flag) {

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
