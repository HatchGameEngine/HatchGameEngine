#ifndef ENGINE_BYTECODE_SCRIPTENTITY_H
#define ENGINE_BYTECODE_SCRIPTENTITY_H

#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Types/Entity.h>

class ScriptEntity : public Entity {
private:
	bool GetCallableValue(Uint32 hash, VMValue& value);
	ObjFunction* GetCallableFunction(Uint32 hash);

public:
	static bool DisableAutoAnimate;
	ObjInstance* Instance = NULL;
	HashMap<VMValue>* Properties;

	void Link(ObjInstance* instance);
	void LinkFields();
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
	void Render(int CamX, int CamY);
	void RenderLate();
	void OnAnimationFinish();
	void OnSceneLoad();
	void OnSceneRestart();
	void GameStart();
	void Remove();
	void Dispose();
	static bool VM_Getter(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID);
	static bool VM_Setter(Obj* object, Uint32 hash, VMValue value, Uint32 threadID);
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
	static VMValue VM_ReturnHitboxFromSprite(int argCount, VMValue* args, Uint32 threadID);
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
