#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

HashMap<const char*>* TypeImpl::PrintableClassNames;

void TypeImpl::Init() {
	PrintableClassNames = new HashMap<const char*>(NULL, 8);
}

void TypeImpl::Dispose() {
	if (PrintableClassNames) {
		PrintableClassNames->Clear();
		delete PrintableClassNames;
		PrintableClassNames = nullptr;
	}
}

void TypeImpl::RegisterClass(ScriptManager* manager, ObjClass* klass) {
#ifdef HSL_VM
	manager->ImplClasses->Put(klass->Hash, klass);
#endif
}

void TypeImpl::ExposeClass(ScriptManager* manager, const char* name, ObjClass* klass) {
#ifdef HSL_VM
	manager->Globals->Put(name, OBJECT_VAL(klass));
#endif
}

void TypeImpl::DefinePrintableName(ObjClass* klass, const char* name) {
	PrintableClassNames->Put(klass->Hash, name);
}

const char* TypeImpl::GetPrintableName(ObjClass* klass) {
	const char* name;

	if (klass && PrintableClassNames->GetIfExists(klass->Hash, &name)) {
		return name;
	}

	return nullptr;
}
