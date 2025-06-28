#ifndef ENGINE_BYTECODE_SCRIPTENTITY_H
#define ENGINE_BYTECODE_SCRIPTENTITY_H

#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Types/Entity.h>

#define ENTITY_FIELDS_LIST \
	ENTITY_FIELD(Create)\
	ENTITY_FIELD(PostCreate)\
	ENTITY_FIELD(Update)\
	ENTITY_FIELD(UpdateLate)\
	ENTITY_FIELD(UpdateEarly)\
	ENTITY_FIELD(RenderEarly)\
	ENTITY_FIELD(Render)\
	ENTITY_FIELD(RenderLate)\
	ENTITY_FIELD(SetAnimation)\
	ENTITY_FIELD(ResetAnimation)\
	ENTITY_FIELD(OnAnimationFinish)\
	ENTITY_FIELD(OnSceneLoad)\
	ENTITY_FIELD(OnSceneRestart)\
	ENTITY_FIELD(GameStart)\
	ENTITY_FIELD(Dispose)

class ScriptEntity : public Entity {
private:
	bool GetCallableValue(Uint32 hash, VMValue& value);

public:
	static bool DisableAutoAnimate;
	ObjEntity* Instance = NULL;
	HashMap<VMValue>* Properties;

	void Link(ObjEntity* entity);
	void LinkFields();
	void AddEntityClassMethods();
	bool RunFunction(Uint32 hash);
	bool RunCreateFunction(VMValue flag);
	bool RunInitializer();
	bool ChangeClass(const char* className);
	void Copy(ScriptEntity* other, bool copyClass);
	void CopyFields(ScriptEntity* other);
	void CopyVMFields(ScriptEntity* other);
	void Initialize();
	void Create(VMValue flag);
	void Create();
	void PostCreate();
	void UpdateEarly();
	void Update();
	void UpdateLate();
	void RenderEarly();
	void Render();
	void RenderLate();
	void SetAnimation(int animation, int frame);
	void ResetAnimation(int animation, int frame);
	void OnAnimationFinish();
	void OnSceneLoad();
	void OnSceneRestart();
	void GameStart();
	void Remove();
	void Dispose();
	bool IsValid();
	static VMValue VM_SetAnimation(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_ResetAnimation(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_Animate(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetIDWithinClass(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_AddToRegistry(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_IsInRegistry(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_RemoveFromRegistry(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_ApplyMotion(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_InView(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_CollidedWithObject(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetHitboxFromSprite(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_ReturnHitbox(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_CollideWithObject(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_SolidCollideWithObject(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_TopSolidCollideWithObject(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_ApplyPhysics(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_PropertyExists(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_PropertyGet(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_SetViewVisibility(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_SetViewOverride(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_AddToDrawGroup(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_IsInDrawGroup(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_RemoveFromDrawGroup(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_PlaySound(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_LoopSound(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_StopSound(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_StopAllSounds(int argCount, VMValue* args, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_SCRIPTENTITY_H */
