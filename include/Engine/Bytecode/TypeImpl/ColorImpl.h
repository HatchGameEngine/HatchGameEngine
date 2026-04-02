#ifndef ENGINE_BYTECODE_TYPEIMPL_COLORIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_COLORIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class ColorImpl {
public:
	static ObjClass* Class;

	static void Init();

	static VMValue New(void);

	static VMValue VM_Initializer(int argCount, VMValue* args, Uint32 threadID);
	static bool VM_PropertyGet(VMValue instance, Uint32 hash, VMValue* result, Uint32 threadID);
	static bool VM_PropertySet(VMValue& instance, Uint32 hash, VMValue value, Uint32 threadID);
	static bool VM_ElementGet(VMValue instance, VMValue at, VMValue* result, Uint32 threadID);
	static bool VM_ElementSet(VMValue& instance, VMValue at, VMValue value, Uint32 threadID);

	static VMValue VM_Inverted(int argCount, VMValue* args, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_COLORIMPL_H */
