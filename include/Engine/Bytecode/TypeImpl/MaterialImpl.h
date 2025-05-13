#ifndef ENGINE_BYTECODE_TYPEIMPL_MATERIALIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_MATERIALIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

#define IS_MATERIAL(value) IsNativeInstance(value, CLASS_MATERIAL)
#define AS_MATERIAL(value) ((ObjMaterial*)AS_OBJECT(value))

class MaterialImpl {
public:
	static ObjClass* Class;

	static void Init();

	static Obj* New(void);
	static ObjMaterial* New(void* materialPtr);
	static void Dispose(Obj* object);

	static VMValue VM_Initializer(int argCount, VMValue* args, Uint32 threadID);
	static bool VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID);
	static bool VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_MATERIALIMPL_H */
