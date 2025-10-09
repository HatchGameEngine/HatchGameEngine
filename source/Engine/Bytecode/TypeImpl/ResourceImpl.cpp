#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/AssetImpl.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/ResourceTypes/Resource.h>
#include <Engine/ResourceTypes/ResourceType.h>

ObjClass* ResourceImpl::Class = nullptr;

Uint32 Hash_Type = 0;
Uint32 Hash_Filename = 0;
Uint32 Hash_Loaded = 0;
Uint32 Hash_Scope = 0;
Uint32 Hash_Asset = 0;

#define CLASS_RESOURCE "Resource"

void ResourceImpl::Init() {
	Class = NewClass(CLASS_RESOURCE);
	Class->NewFn = New;
	Class->Initializer = OBJECT_VAL(NewNative(VM_Initializer));

	GET_STRING_HASH(Type);
	GET_STRING_HASH(Filename);
	GET_STRING_HASH(Loaded);
	GET_STRING_HASH(Scope);
	GET_STRING_HASH(Asset);

	ScriptManager::DefineNative(Class, "IsUnique", ResourceImpl::VM_IsUnique);
	ScriptManager::DefineNative(Class, "MakeUnique", ResourceImpl::VM_MakeUnique);
	ScriptManager::DefineNative(Class, "Reload", ResourceImpl::VM_Reload);
	ScriptManager::DefineNative(Class, "Unload", ResourceImpl::VM_Unload);

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(CLASS_RESOURCE, Class);
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))
#define GET_ARG_OPT(argIndex, argFunction, argDefault) \
	(argIndex < argCount ? GET_ARG(argIndex, argFunction) : argDefault)

Obj* ResourceImpl::New() {
	// Don't worry about it.
	return nullptr;
}

Obj* ResourceImpl::New(void* resourcePtr) {
	ObjResource* resource = (ObjResource*)AllocateObject(sizeof(ObjResource), OBJ_RESOURCE);
	Memory::Track(resource, "NewResource");
	resource->Object.Class = Class;
	resource->Object.Destructor = Dispose;
	resource->Object.PropertyGet = VM_PropertyGet;
	resource->Object.PropertySet = VM_PropertySet;
	SetOwner((Obj*)resource, resourcePtr);
	return (Obj*)resource;
}

void ResourceImpl::SetOwner(Obj* obj, void* resourcePtr) {
	ObjResource* resource = (ObjResource*)obj;
	if (resourcePtr) {
		ResourceType* resourceType = (ResourceType*)resourcePtr;
		resource->ResourcePtr = resourcePtr;
		resource->GetAssetField = AssetImpl::GetGetter(resourceType->Type);
		resource->SetAssetField = AssetImpl::GetSetter(resourceType->Type);
	}
}

void ResourceImpl::Dispose(Obj* object) {
	ObjResource* resource = (ObjResource*)object;

	if (resource->ResourcePtr != nullptr) {
		Resource::ReleaseVMObject((ResourceType*)resource->ResourcePtr);
	}
}

/***
 * \constructor
 * \desc Loads a Resource by name.
 * \param filename (String): Filename of the resource.
 * \paramOpt unloadPolicy (Integer): The <linkto ref="SCOPE_*">unload policy</linkto> of the resource.
 * \paramOpt unique (Boolean): If <code>false</code> (the default), this constructor may return an already loaded Resource of the same type and filename. However, if <code>true</code>, this constructor will always return an unique Resource, and load a new Asset.
 * \ns Resource
 */
VMValue ResourceImpl::VM_Initializer(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 2);
	char* filename = GET_ARG(1, GetString);
	int unloadPolicy = GET_ARG_OPT(2, GetInteger, SCOPE_GAME);
	bool unique = GET_ARG_OPT(3, GetInteger, false);

	VMValue value = NULL_VAL;

	if (filename != nullptr) {
		ResourceType* resource = Resource::Load(ASSET_NONE, filename, unloadPolicy, unique);
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

	/***
	 * \field Type
	 * \desc The <linkto ref="ASSET_*">asset type</linkto> of the Asset.
	 * \ns Resource
 	*/
	if (hash == Hash_Type) {
		*result = INTEGER_VAL(resource->Type);
	}
	/***
	 * \field Filename
	 * \desc The filename of the Resource.
	 * \ns Resource
 	*/
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
	/***
	 * \field Loaded
	 * \desc Whether the Asset is loaded or not.
	 * \ns Resource
 	*/
	else if (hash == Hash_Loaded) {
		*result = INTEGER_VAL(resource->Loaded ? true : false);
	}
	/***
	 * \field Scope
	 * \desc The scope of the Resource.
	 * \ns Resource
 	*/
	else if (hash == Hash_Scope) {
		*result = INTEGER_VAL((int)resource->UnloadPolicy);
	}
	/***
	 * \field Asset
	 * \desc The Asset owned by this resource.
	 * \ns Resource
 	*/
	else if (hash == Hash_Asset) {
		if (resource->AsAsset) {
			*result = OBJECT_VAL(resource->AsAsset->GetVMObject());
		}
		else {
			*result = NULL_VAL;
		}
	}
	else if (objResource->GetAssetField) {
		Obj* obj = nullptr;
		if (resource->AsAsset) {
			obj = (Obj*)resource->AsAsset->GetVMObject();
		}
		return objResource->GetAssetField(obj, hash, result, threadID);
	}
	else {
		return false;
	}

	return true;
}

bool ResourceImpl::VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID) {
	ObjResource* objResource = (ObjResource*)object;
	ResourceType* resource = (ResourceType*)objResource->ResourcePtr;

	if (hash == Hash_Type || hash == Hash_Filename || hash == Hash_Loaded ||
		hash == Hash_Scope || hash == Hash_Asset) {
		VM_THROW_ERROR("Field cannot be written to!");
		return true;
	}
	else if (objResource->SetAssetField) {
		Obj* obj = nullptr;
		if (resource->AsAsset) {
			obj = (Obj*)resource->AsAsset->GetVMObject();
		}
		return objResource->SetAssetField(obj, hash, value, threadID);
	}
	else {
		return false;
	}

	return true;
}

/***
 * \method IsUnique
 * \desc Returns whether the Resource is unique or not.
 * \return Returns a Boolean value.
 * \ns Resource
 */
VMValue ResourceImpl::VM_IsUnique(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	void* ptr = GET_ARG(0, GetResource);
	if (ptr != nullptr) {
		ResourceType* resource = (ResourceType*)ptr;
		return INTEGER_VAL(resource->Unique);
	}

	return NULL_VAL;
}

/***
 * \method MakeUnique
 * \desc Makes the Resource unique, if it already wasn't.
 * \ns Resource
 */
VMValue ResourceImpl::VM_MakeUnique(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	void* ptr = GET_ARG(0, GetResource);
	if (ptr != nullptr) {
		ResourceType* resource = (ResourceType*)ptr;
		resource->Unique = true;
	}

	return NULL_VAL;
}

/***
 * \method Reload
 * \desc Reloads the Asset owned by this Resource. Returns <code>true</code> if it was reloaded successfully, and <code>false</code> if otherwise.
 * \return Returns a Boolean value.
 * \ns Resource
 */
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

/***
 * \method Unload
 * \desc Unloads the Asset owned by this Resource. Does nothing if the Resource was already unloaded.
 * \ns Resource
 */
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
