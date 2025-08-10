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
};

#endif /* ENGINE_BYTECODE_SCRIPTENTITY_H */
