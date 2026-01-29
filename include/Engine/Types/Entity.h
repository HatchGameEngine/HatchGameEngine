#ifndef ENGINE_TYPES_ENTITY_H
#define ENGINE_TYPES_ENTITY_H
class Entity;
class ObjectList;

#define SCRIPTABLE_ENTITY

#include <Engine/Includes/HashMap.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/Enums.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/Scene/SceneEnums.h>
#include <Engine/Types/EntityTypes.h>
#include <Engine/Types/Property.h>

#ifdef SCRIPTABLE_ENTITY
#define ENTITY_PARENT_CLASS ScriptEntity
#else
#define ENTITY_PARENT_CLASS Entity
#endif

typedef Entity* (*EntitySpawnFunction)();
typedef Entity* (*NamedEntitySpawnFunction)(const char*);

#define ENTITY_INIT(name) name::ClassLoad()
#define ENTITY_DEINIT(name) name::ClassUnload()

#ifdef SCRIPTABLE_ENTITY
#define REGISTER_ENTITY(name) \
	{ \
		SpawnFunctions->Put(#name, name::Spawn); \
		name##_Class = NewClass(#name); \
		ScriptManager::Globals->Put(#name, OBJECT_VAL(name##_Class)); \
	}
#define UNREGISTER_ENTITY(name) name##_Class = nullptr
#define ENTITY_SPAWN(entity, name) ScriptEntity::SpawnForClass(entity, #name)
#define DECLARE_ENTITY_FIELD(name, field) Uint32 name##_Hash_##field = 0
#define REGISTER_ENTITY_FIELD(name, field) name##_Hash_##field = Murmur::EncryptString(#field)
#define REGISTER_ENTITY_GETTER(name, entity) entity->Instance->Object.PropertyGet = name##_VM_PropertyGet
#define REGISTER_ENTITY_SETTER(name, entity) entity->Instance->Object.PropertySet = name##_VM_PropertySet
#define ENTITY_GET_FIELD(name, field) \
	{ \
		if (name##_Hash_##field == hash) { \
			if (result) { \
				*result = name##_FieldGet_##field(entity, threadID); \
			} \
			return true; \
		} \
	}
#define ENTITY_SET_FIELD(name, field) \
	{ \
		if (name##_Hash_##field == hash) { \
			name##_FieldSet_##field(entity, value, threadID); \
			return true; \
		} \
	}
#else
#define REGISTER_ENTITY(name) SpawnFunctions->Put(#name, name::Spawn)
#define UNREGISTER_ENTITY(name)
#define DECLARE_ENTITY_FIELD(name, field)
#define REGISTER_ENTITY_FIELD(name, field)
#define REGISTER_ENTITY_GETTER(name, entity)
#define REGISTER_ENTITY_SETTER(name, entity)
#endif

class Entity {
public:
	float InitialX = 0.0f;
	float InitialY = 0.0f;
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
	float SpeedX = 0.0f;
	float SpeedY = 0.0f;
	float GroundSpeed = 0.0f;
	float GravitySpeed = 0.0f;
	int OnGround = false;
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
	int BlendMode = BlendMode_NORMAL;
	int AutoPhysics = false;
	int Priority = 0;
	int PriorityListIndex = -1;
	int PriorityOld = -1;
	float Depth = 0.0f;
	float OldDepth = 0.0f;
	int Sprite = -1;
	int CurrentAnimation = -1;
	int CurrentFrame = -1;
	int CurrentFrameCount = 0;
	float AnimationSpeedMult = 1.0;
	int AnimationSpeedAdd = 0;
	int PrevAnimation = -1;
	int AutoAnimate = true;
	float AnimationSpeed = 0.0;
	float AnimationTimer = 0.0;
	int AnimationFrameDuration = 0;
	int AnimationLoopIndex = 0;
	int RotationStyle = ROTSTYLE_NONE;
	EntityHitbox Hitbox;
	int Direction = 0;
	float SensorX = 0.0f;
	float SensorY = 0.0f;
	int SensorCollided = false;
	int SensorAngle = 0;
	int TileCollisions = TILECOLLISION_NONE;
	int CollisionLayers = 0;
	int CollisionPlane = 0;
	int CollisionMode = CMODE_FLOOR;
	int SlotID = -1;
	int Filter = 0xFF;
	bool Removed = false;
	Entity* PrevEntity = NULL;
	Entity* NextEntity = NULL;
	ObjectList* List = NULL;
	Entity* PrevEntityInList = NULL;
	Entity* NextEntityInList = NULL;
	Entity* PrevSceneEntity = NULL;
	Entity* NextSceneEntity = NULL;

	HashMap<Property>* Properties = NULL;

	static HashMap<EntitySpawnFunction>* SpawnFunctions;
	static bool DisableAutoAnimate;

	static void InitAll();
	static void UnloadAll();

	static Entity* Spawn();
	static Entity* SpawnNamed(const char* objectName);

	virtual ~Entity() = default;
	void SetDrawGroup(int index);
	void CheckDrawGroupChanges();
	void CheckDepthChanges();
	void Copy(Entity* other);
	void CopyFields(Entity* other);
	void InitProperties();
	int GetIDWithinClass();
	virtual void ApplyMotion();
	virtual void Animate();
	virtual void SetAnimation(int animation, int frame);
	virtual void ResetAnimation(int animation, int frame);
	virtual void SetUpdatePriority(int priority);
	virtual bool BasicCollideWithObject(Entity* other);
	virtual bool CollideWithObject(Entity* other);
	virtual int SolidCollideWithObject(Entity* other, int flag);
	virtual bool TopSolidCollideWithObject(Entity* other, int flag);
	virtual void ApplyPhysics();
	virtual void Initialize();
	virtual void Create();
	virtual void PostCreate();
	virtual void UpdateEarly();
	virtual void Update();
	virtual void UpdateLate();
	virtual void FixedUpdateEarly();
	virtual void FixedUpdate();
	virtual void FixedUpdateLate();
	virtual void OnAnimationFinish();
	virtual void OnSceneLoad();
	virtual void OnSceneRestart();
	virtual void GameStart();
	virtual void RenderEarly();
	virtual void Render();
	virtual void RenderLate();
	virtual void Remove();
	virtual void Dispose();
};

#endif /* ENGINE_TYPES_ENTITY_H */
