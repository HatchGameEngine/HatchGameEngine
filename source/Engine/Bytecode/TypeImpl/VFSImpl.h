#ifndef ENGINE_BYTECODE_TYPEIMPL_VFSIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_VFSIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

#define CLASS_VFS "VirtualFileSystem"

#define IS_VFS(value) IsNativeInstance(value, CLASS_VFS)
#define AS_VFS(value) ((ObjInstance*)AS_OBJECT(value))

class VFSImpl {
public:
	static ObjClass* Class;

	static void Init();

	static Obj* Constructor();
	static VMValue VM_Initializer(int argCount, VMValue* args, Uint32 threadID);
	static void Dispose(Obj* object);

	static void* GetVFS(ObjInstance* object);
	static ObjInstance* GetVFSObject(void* texture);

	static VMValue VM_Mount(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_Unmount(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_IsMounted(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_Delete(int argCount, VMValue* args, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_VFSIMPL_H */
