#ifndef ENGINE_BYTECODE_TYPEIMPL_RESOURCEIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_RESOURCEIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

#define IS_RESOURCE(value) IsObjectType(value, OBJ_RESOURCE)
#define AS_RESOURCE(value) ((ObjResource*)AS_OBJECT(value))

class ResourceImpl {
public:
	static ObjClass* Class;

	static void Init();
	static Obj* New();
	static Obj* New(void* resourcePtr);
	static void SetOwner(Obj* obj, void* resourcePtr);
	static void Dispose(Obj* object);
	static VMValue VM_Initializer(int argCount, VMValue* args, Uint32 threadID);

	static bool VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID);
	static bool VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID);
	static VMValue VM_IsUnique(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_MakeUnique(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_Reload(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_Unload(int argCount, VMValue* args, Uint32 threadID);

	static void* NewAssetObject(void* ptr);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_RESOURCEIMPL_H */
