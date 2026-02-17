#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/TypeImpl/ArrayImpl.h>
#include <Engine/Bytecode/TypeImpl/FunctionImpl.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/MapImpl.h>
#include <Engine/Bytecode/TypeImpl/StreamImpl.h>
#include <Engine/Bytecode/TypeImpl/StringImpl.h>

#ifndef HSL_STANDALONE
#include <Engine/Bytecode/TypeImpl/FontImpl.h>
#include <Engine/Bytecode/TypeImpl/EntityImpl.h>
#include <Engine/Bytecode/TypeImpl/MaterialImpl.h>
#include <Engine/Bytecode/TypeImpl/ShaderImpl.h>
#endif

std::unordered_map<ObjClass*, const char*> PrintableNames;

void TypeImpl::Init() {
	ArrayImpl::Init();
	FunctionImpl::Init();
	InstanceImpl::Init();
	MapImpl::Init();
	StreamImpl::Init();
	StringImpl::Init();

#ifndef HSL_STANDALONE
	FontImpl::Init();
	EntityImpl::Init();
	MaterialImpl::Init();
	ShaderImpl::Init();
#endif
}

void TypeImpl::RegisterClass(ObjClass* klass) {
#ifdef HSL_VM
	ScriptManager::ClassImplList.push_back(klass);
#endif
}

void TypeImpl::ExposeClass(const char* name, ObjClass* klass) {
#ifdef HSL_VM
	ScriptManager::Globals->Put(name, OBJECT_VAL(klass));
#endif
}

void TypeImpl::DefinePrintableName(ObjClass* klass, const char* name) {
	PrintableNames[klass] = name;
}

const char* TypeImpl::GetPrintableName(ObjClass* klass) {
	if (PrintableNames.find(klass) != PrintableNames.end()) {
		return PrintableNames[klass];
	}

	return nullptr;
}
