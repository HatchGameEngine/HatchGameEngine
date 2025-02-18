#ifndef ENGINE_TYPES_ENTITY_H
#define ENGINE_TYPES_ENTITY_H
class ObjectList;
class ObjectRegistry;
class DrawGroupList;

#include <Engine/Includes/Standard.h>
#include <Engine/Application.h>
#include <Engine/Graphics.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/Scene.h>
#include <Engine/Types/EntityTypes.h>
#include <Engine/Includes/HashMap.h>

class Entity {
public:
    float InitialX = 0;
    float InitialY = 0;
    int Active = true;
    int Pauseable = true;
    int Interactable = true;
    int Persistence = Persistence_NONE;
    int Activity = ACTIVE_BOUNDS;
    int UpdatePriority = 0;
    int InRange = false;
    bool Created = false;
    bool PostCreated = false;
    bool Dynamic = false;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float XSpeed = 0.0f;
    float YSpeed = 0.0f;
    float GroundSpeed = 0.0f;
    float Gravity = 0.0f;
    int Ground = false;
    int WasOffScreen = false;
    int OnScreen = true;
    float OnScreenHitboxW = 0.0f;
    float OnScreenHitboxH = 0.0f;
    float OnScreenRegionTop = 0.0f;
    float OnScreenRegionLeft = 0.0f;
    float OnScreenRegionRight = 0.0f;
    float OnScreenRegionBottom = 0.0f;
    int Visible = true;
    int ViewRenderFlag = 0xFFFFFFFF;
    int ViewOverrideFlag = 0;
    float RenderRegionW = 0.0f;
    float RenderRegionH = 0.0f;
    float RenderRegionTop = 0.0f;
    float RenderRegionLeft = 0.0f;
    float RenderRegionRight = 0.0f;
    float RenderRegionBottom = 0.0f;
    int Angle = 0;
    int AngleMode = 0;
    float ScaleX = 1.0;
    float ScaleY = 1.0;
    float Rotation = 0.0;
    float Alpha = 1.0;
    int AutoPhysics = false;
    int Priority = 0;
    int PriorityListIndex = -1;
    int PriorityOld = -1;
    float Depth = 0.0f;
    float OldDepth = 0.0f;
    float ZDepth = 0.0;
    int Sprite = -1;
    int CurrentAnimation = -1;
    int CurrentFrame = -1;
    int CurrentFrameCount = 0;
    float AnimationSpeedMult = 1.0;
    int AnimationSpeedAdd = 0;
    int AutoAnimate = true;
    float AnimationSpeed = 0.0;
    float AnimationTimer = 0.0;
    int AnimationFrameDuration = 0;
    int AnimationLoopIndex = 0;
    EntityHitbox Hitbox;
    int FlipFlag = 0;
    float SensorX = 0.0f;
    float SensorY = 0.0f;
    int SensorCollided = false;
    int SensorAngle = 0;
    float VelocityX = 0.0f;
    float VelocityY = 0.0f;
    float GroundVel = 0.0f;
    float GravityStrength = 0.0f;
    int OnGround = false;
    int Direction = 0;
    int TileCollisions = false;
    int CollisionLayers = 0;
    int CollisionPlane = 0;
    int CollisionMode = 0;
    int SlotID = -1;
    bool Removed = false;
    Entity* PrevEntity = NULL;
    Entity* NextEntity = NULL;
    ObjectList* List = NULL;
    Entity* PrevEntityInList = NULL;
    Entity* NextEntityInList = NULL;
    Entity* PrevSceneEntity = NULL;
    Entity* NextSceneEntity = NULL;

    virtual ~Entity() = default;
    void ApplyMotion();
    void Animate();
    void SetAnimation(int animation, int frame);
    void ResetAnimation(int animation, int frame);
    bool BasicCollideWithObject(Entity* other);
    bool CollideWithObject(Entity* other);
    int SolidCollideWithObject(Entity* other, int flag);
    bool TopSolidCollideWithObject(Entity* other, int flag);
    void Copy(Entity* other);
    void CopyFields(Entity* other);
    void ApplyPhysics();
    virtual void Initialize();
    virtual void Create();
    virtual void PostCreate();
    virtual void UpdateEarly();
    virtual void Update();
    virtual void UpdateLate();
    virtual void OnAnimationFinish();
    virtual void OnSceneLoad();
    virtual void OnSceneRestart();
    virtual void GameStart();
    virtual void RenderEarly();
    virtual void Render(int CamX, int CamY);
    virtual void RenderLate();
    virtual void Remove();
    virtual void Dispose();
};

#endif /* ENGINE_TYPES_ENTITY_H */
