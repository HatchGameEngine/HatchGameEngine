#ifndef ENGINE_BYTECODE_TYPEIMPL_RESOURCEIMPL_IMAGEIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_RESOURCEIMPL_IMAGEIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class ImageImpl {
private:
	static bool IsValidField(Uint32 hash);

public:
	static ObjClass* Class;

	static void Init();
	static bool VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID);
	static bool VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_RESOURCEIMPL_IMAGEIMPL_H */
