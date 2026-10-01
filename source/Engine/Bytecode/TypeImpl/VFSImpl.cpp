#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/VFSImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Value.h>
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

	ScriptManager::DefineNative(Class, "MountFile", VM_MountFile);
	ScriptManager::DefineNative(Class, "MountDirectory", VM_MountDirectory);
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
 * \method MountFile
 * \desc Mounts <param filename> into <param mountPoints>, as <param name>.
 * \param name (string): The name of the mount point.
 * \param mountPoint (string): The path of the mount point.
 * \param filename (string): The file to mount.
 * \param isWritable (boolean): Whether the mount point is writable.
 * \return boolean Returns whether the file was mounted.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_MountFile(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 4);

	ObjInstance* objVfs = AS_VFS(args[0]);
	char* name = GET_ARG(1, GetString);
	char* mountPoint = GET_ARG(2, GetString);
	char* filename = GET_ARG(3, GetString);
	bool isWritable = GET_ARG_OPT(4, GetInteger, false);

	std::string resolved = "";
	if (!Path::FromURL(filename, resolved)) {
		return INTEGER_VAL(false);
	}

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	Uint16 flags = VFS_READABLE;
	if (isWritable) {
		flags |= VFS_WRITABLE;
	}

	VFSMountStatus status = vfs->Mount(name, resolved.c_str(), mountPoint, VFSType::HATCH, flags);

	if (status == VFSMountStatus::MOUNTED) {
		return INTEGER_VAL(true);
	}

	return INTEGER_VAL(false);
}
/***
 * \method MountDirectory
 * \desc Mounts <param directory> into <param mountPoints>, as <param name>.
 * \param name (string): The name of the mount point.
 * \param mountPoint (string): The path of the mount point.
 * \param directory (string): The directory to mount.
 * \param isWritable (boolean): Whether the mount point is writable.
 * \return boolean Returns whether the directory was mounted.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_MountDirectory(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 4);

	ObjInstance* objVfs = AS_VFS(args[0]);
	char* name = GET_ARG(1, GetString);
	char* mountPoint = GET_ARG(2, GetString);
	char* directory = GET_ARG(3, GetString);
	bool isWritable = GET_ARG_OPT(4, GetInteger, false);

	std::string resolved = "";
	if (!Path::FromURL(directory, resolved)) {
		return INTEGER_VAL(false);
	}

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	Uint16 flags = VFS_READABLE;
	if (isWritable) {
		flags |= VFS_WRITABLE;
	}

	VFSMountStatus status = vfs->Mount(name, resolved.c_str(), mountPoint, VFSType::FILESYSTEM, flags);

	if (status == VFSMountStatus::MOUNTED) {
		return INTEGER_VAL(true);
	}

	return INTEGER_VAL(false);
}
/***
 * \method Unmount
 * \desc Unmounts <param name>.
 * \param name (string): The mount point to unmount.
 * \return boolean Returns whether the mount point was unmounted.
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
 * \desc Checks whether <param name> is mounted.
 * \param name (string): The mount point to check.
 * \return boolean Returns whether the mount point is mounted.
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
