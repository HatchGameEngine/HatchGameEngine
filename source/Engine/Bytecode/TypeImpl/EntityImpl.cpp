#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/TypeImpl/EntityImpl.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/IO/Stream.h>

ObjClass* EntityImpl::Class = nullptr;

void EntityImpl::Init() {
	Class = NewClass(CLASS_ENTITY);

	ScriptManager::ClassImplList.push_back(Class);
}

void EntityImpl::Dispose(Obj* object) {
	ObjEntity* entity = (ObjEntity*)object;

	Scene::DeleteRemoved((Entity*)entity->EntityPtr);

	InstanceImpl::Dispose(object);
}
