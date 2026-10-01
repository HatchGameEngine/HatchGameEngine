#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/VFSImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/VFS/VirtualFileSystem.h>

/***
* \class VirtualFileSystem
* \desc A representation of a filesystem.
The files in a VFS (Virtual File System) may or may not actually come from the OS's file system (hence the "virtual" in the name.)
For example, one directory might be mounted to a .hatch file, and another directory to a path in the real file system.
*/

ObjClass* VFSImpl::Class = nullptr;

void VFSImpl::Init() {
	Class = NewClass(CLASS_VFS);
	Class->NewFn = Constructor;
	Class->Initializer = OBJECT_VAL(NewNative(VM_Initializer));

	ScriptManager::DefineNative(Class, "Mount", VM_Mount);
	ScriptManager::DefineNative(Class, "Unmount", VM_Unmount);
	ScriptManager::DefineNative(Class, "IsMounted", VM_IsMounted);
	ScriptManager::DefineNative(Class, "Delete", VM_Delete);

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(Class);
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))
#define GET_ARG_OPT(argIndex, argFunction, argDefault) \
	(argIndex < argCount ? GET_ARG(argIndex, StandardLibrary::argFunction) : argDefault)

Obj* VFSImpl::Constructor() {
	ObjInstance* objVfs = (ObjInstance*)NewNativeInstance(sizeof(ObjInstance));
	Memory::Track(objVfs, "NewVFS");
	objVfs->Object.Class = Class;
	objVfs->Destructor = Dispose;
	return (Obj*)objVfs;
}

/***
 * \constructor
 * \desc Creates a virtual file system.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_Initializer(int argCount, VMValue* args, Uint32 threadID) {
	ObjInstance* objVfs = AS_VFS(args[0]);

	StandardLibrary::CheckArgCount(argCount, 1);

	VirtualFileSystem* vfs = new VirtualFileSystem();

	ScriptManager::RegistryAdd(vfs, (Obj*)objVfs);

	return OBJECT_VAL(objVfs);
}
void VFSImpl::Dispose(Obj* object) {
	// Yes, this leaks memory.
	// Use Delete() in your script for a VFS you no longer need!
	InstanceImpl::Dispose(object);
}

void* VFSImpl::GetVFS(ObjInstance* object) {
	return ScriptManager::RegistryGet((Obj*)object);
}
ObjInstance* VFSImpl::GetVFSObject(void* vfs) {
	if (vfs == nullptr) {
		return nullptr;
	}

	Obj* obj = ScriptManager::RegistryGet(vfs);
	if (obj == nullptr) {
		obj = ScriptManager::RegistryAdd(vfs, VFSImpl::Constructor());
	}

	return (ObjInstance*)obj;
}

#define CHECK_EXISTS(ptr) \
	if (ptr == nullptr) { \
		throw ScriptException("Virtual file system is no longer valid!"); \
	}

/***
 * \method Mount
 * \desc Mounts <param path> into <param mountPoint>, as <param name>. <param path> may be an URL.<br/>\
If <param path> is a file, it will attempt to mount <param path> as an archive. Otherwise, it must be a directory, and it will attempt to mount <param path> as a directory.
 * \param name (string): The name of the mount.
 * \param path (string): The file or directory to mount.
 * \paramOpt mountPoint (string): The mount point. (default: `null`)
 * \paramOpt isWritable (boolean): Whether the mount is writable. (default: `false`)
 * \return boolean Returns whether <param path> was mounted.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_Mount(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 3);

	ObjInstance* objVfs = AS_VFS(args[0]);
	char* name = GET_ARG(1, GetString);
	char* path = GET_ARG(2, GetString);
	char* mountPointPath = GET_ARG_OPT(3, GetString, nullptr);
	bool isWritable = GET_ARG_OPT(4, GetInteger, false);

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	// Check if there is already a mount with the provided name.
	if (vfs->IsMounted(name)) {
		std::string errorString;

		size_t bufferSize = strlen(name) + 24 + 1;
		char* buffer = (char*)Memory::Malloc(bufferSize);
		if (buffer) {
			snprintf(buffer, bufferSize, "Mount \"%s\" already exists!", name);
			errorString = std::string(buffer);
			Memory::Free(buffer);
		}
		else {
			errorString = "Mount already exists!";
		}

		throw ScriptException(errorString);
	}

	const char* realPath = nullptr;

	std::string resolved = "";
	if (Path::FromURL(path, resolved)) {
		realPath = resolved.c_str();
	}
	else {
		return INTEGER_VAL(false);
	}

	// '/' or an empty string is the same as no mount point
	// (That is, we use DEFAULT_MOUNT_POINT instead)
	std::string mountPoint;
	if (mountPointPath == nullptr || strcmp(mountPointPath, "/") == 0 || mountPointPath[0] == '\0') {
		mountPoint = DEFAULT_MOUNT_POINT;
	}
	else {
		// Add '/' at the end
		// VirtualFileSystem::Mount normalizes the path already, so this is fine.
		mountPoint = std::string(mountPointPath) + "/";
	}

	Uint16 flags = VFS_READABLE;
	if (isWritable) {
		flags |= VFS_WRITABLE;
	}

	VFSType type;
	if (Directory::Exists(realPath)) {
		type = VFSType::FILESYSTEM;
	}
	else {
		type = VFSType::HATCH;
	}

	VFSMountStatus status = vfs->Mount(name, realPath, mountPoint.c_str(), type, flags);

	if (status == VFSMountStatus::MOUNTED) {
		return INTEGER_VAL(true);
	}

	return INTEGER_VAL(false);
}
/***
 * \method Unmount
 * \desc Unmounts the mount of the given name.
 * \param name (string): The mount to unmount.
 * \return boolean Returns whether <param name> was unmounted.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_Unmount(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjInstance* objVfs = AS_VFS(args[0]);
	char* name = GET_ARG(1, GetString);

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	VFSMountStatus status = vfs->Unmount(name);

	if (status == VFSMountStatus::UNMOUNTED) {
		return INTEGER_VAL(true);
	}

	return INTEGER_VAL(false);
}
/***
 * \method IsMounted
 * \desc Checks whether the mount of the given name is mounted.
 * \param name (string): The mount to check.
 * \return boolean Returns whether <param name> is mounted.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_IsMounted(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjInstance* objVfs = AS_VFS(args[0]);
	char* name = GET_ARG(1, GetString);

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	if (vfs->IsMounted(name)) {
		return INTEGER_VAL(true);
	}

	return INTEGER_VAL(false);
}
/***
 * \method Delete
 * \desc Deletes the virtual file system. It can no longer be used after this function is called.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_Delete(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	ObjInstance* objVfs = AS_VFS(args[0]);

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	if (vfs) {
		ScriptManager::RegistryRemove((void*)vfs);
		delete vfs;
	}

	return NULL_VAL;
}

#undef CHECK_EXISTS
#undef GET_ARG
#undef GET_ARG_OPT
