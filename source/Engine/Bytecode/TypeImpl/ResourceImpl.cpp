#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl.h>
#include <Engine/ResourceTypes/Resource.h>
#include <Engine/ResourceTypes/ResourceType.h>

ObjClass* ResourceImpl::Class = nullptr;

Uint32 Hash_Type = 0;
Uint32 Hash_Filename = 0;
Uint32 Hash_Loaded = 0;
Uint32 Hash_Scope = 0;

void ResourceImpl::Init() {
	const char* className = "Resource";

	Class = NewClass(Murmur::EncryptString(className));
	Class->Name = CopyString(className);
	Class->NewFn = VM_New;
	Class->Initializer = OBJECT_VAL(NewNative(VM_Initializer));
	Class->PropertyGet = VM_PropertyGet;

	Hash_Type = Murmur::EncryptString("Type");
	Hash_Filename = Murmur::EncryptString("Filename");
	Hash_Loaded = Murmur::EncryptString("Loaded");
	Hash_Scope = Murmur::EncryptString("Scope");

	ScriptManager::DefineNative(Class, "Reload", ResourceImpl::VM_Reload);
	ScriptManager::DefineNative(Class, "Unload", ResourceImpl::VM_Unload);

	ScriptManager::ClassImplList.push_back(Class);

	ScriptManager::Globals->Put(className, OBJECT_VAL(Class));
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))
#define GET_ARG_OPT(argIndex, argFunction, argDefault) \
	(argIndex < argCount ? GET_ARG(argIndex, argFunction) : argDefault)

Obj* ResourceImpl::VM_New() {
	// Don't worry about it.
	return nullptr;
}

VMValue ResourceImpl::VM_Initializer(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 2);
	char* filename = GET_ARG(1, GetString);
	int unloadPolicy = GET_ARG_OPT(2, GetInteger, SCOPE_GAME);

	VMValue value = NULL_VAL;

	if (filename != nullptr) {
		vector<ResourceType*>* list = Resource::GetList(unloadPolicy);
		ResourceType* resource = Resource::Load(list, RESOURCE_NONE, filename, unloadPolicy);
		if (resource != nullptr) {
			value = OBJECT_VAL(Resource::GetVMObject(resource));
		}
	}

	// Since we know args[] is in the thread's stack, this is okay to do.
	args[0] = value;

	return value;
}

bool ResourceImpl::VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID) {
	ObjResource* objResource = (ObjResource*)object;
	ResourceType* resource = (ResourceType*)objResource->ResourcePtr;

	if (hash == Hash_Type) {
		*result = INTEGER_VAL(resource->Type);
	}
	else if (hash == Hash_Filename) {
		if (ScriptManager::Lock()) {
			ObjString* string = CopyString(resource->Filename);
			ScriptManager::Unlock();
			*result = OBJECT_VAL(string);
		}
		else {
			*result = NULL_VAL;
		}
	}
	else if (hash == Hash_Loaded) {
		*result = INTEGER_VAL(resource->Loaded ? true : false);
	}
	else if (hash == Hash_Scope) {
		*result = INTEGER_VAL((int)resource->UnloadPolicy);
	}
	else {
		return false;
	}

	return true;
}

VMValue ResourceImpl::VM_Reload(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	bool success = false;

	void* ptr = GET_ARG(0, GetResource);
	if (ptr != nullptr) {
		ResourceType* resource = (ResourceType*)ptr;
		success = Resource::Reload(resource);
	}

	return INTEGER_VAL(success);
}

VMValue ResourceImpl::VM_Unload(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	void* ptr = GET_ARG(0, GetResource);
	if (ptr != nullptr) {
		ResourceType* resource = (ResourceType*)ptr;
		Resource::Unload(resource);
	}

	return NULL_VAL;
}

#undef GET_ARG
#undef GET_ARG_OPT
