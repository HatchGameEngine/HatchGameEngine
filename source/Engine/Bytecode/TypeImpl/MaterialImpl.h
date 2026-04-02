#ifndef ENGINE_BYTECODE_TYPEIMPL_MATERIALIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_MATERIALIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

#define CLASS_MATERIAL "Material"

#define IS_MATERIAL(value) IsNativeInstance(value, CLASS_MATERIAL)
#define AS_MATERIAL(value) ((ObjMaterial*)AS_OBJECT(value))

class MaterialImpl {
public:
	static ObjClass* Class;

	static void Init();

	static VMValue Constructor();
	static ObjMaterial* New(void* materialPtr);
	static void Dispose(Obj* object);

	static VMValue VM_Initializer(int argCount, VMValue* args, Uint32 threadID);
	static bool VM_PropertyGet(VMValue instance, Uint32 hash, VMValue* result, Uint32 threadID);
	static bool VM_PropertySet(VMValue& instance, Uint32 hash, VMValue value, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_MATERIALIMPL_H */
