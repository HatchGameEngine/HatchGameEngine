#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/ArrayImpl.h>
#include <Engine/Bytecode/TypeImpl/EntityImpl.h>
#include <Engine/Bytecode/TypeImpl/FontImpl.h>
#include <Engine/Bytecode/TypeImpl/FunctionImpl.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/MapImpl.h>
#include <Engine/Bytecode/TypeImpl/MaterialImpl.h>
#include <Engine/Bytecode/TypeImpl/ShaderImpl.h>
#include <Engine/Bytecode/TypeImpl/StreamImpl.h>
#include <Engine/Bytecode/TypeImpl/StringImpl.h>
#include <Engine/Bytecode/TypeImpl/TextureImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

void TypeImpl::Init() {
	ArrayImpl::Init();
	EntityImpl::Init();
	FontImpl::Init();
	FunctionImpl::Init();
	MaterialImpl::Init();
	MapImpl::Init();
	ShaderImpl::Init();
	StreamImpl::Init();
	StringImpl::Init();
	TextureImpl::Init();
}

void TypeImpl::RegisterClass(ObjClass* klass) {
	ScriptManager::ClassImplList.push_back(klass);
}

void TypeImpl::ExposeClass(ObjClass* klass) {
	ScriptManager::Globals->Put(klass->Name, OBJECT_VAL(klass));
}
