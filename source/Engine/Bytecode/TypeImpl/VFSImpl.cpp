#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/StreamImpl.h>
#include <Engine/Bytecode/TypeImpl/VFSImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/IO/VirtualFileStream.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/VFS/VirtualFileSystem.h>
#include <Engine/ResourceTypes/ResourceManager.h>

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
	ScriptManager::DefineNative(Class, "GetMountCount", VM_GetMountCount);
	ScriptManager::DefineNative(Class, "GetMountNameByIndex", VM_GetMountNameByIndex);
	ScriptManager::DefineNative(Class, "GetMountPoint", VM_GetMountPoint);
	ScriptManager::DefineNative(Class, "GetMountIndex", VM_GetMountIndex);
	ScriptManager::DefineNative(Class, "EnumerateFiles", VM_EnumerateFiles);
	ScriptManager::DefineNative(Class, "FileExists", VM_FileExists);
	ScriptManager::DefineNative(Class, "OpenReadStream", VM_OpenReadStream);
	ScriptManager::DefineNative(Class, "OpenWriteStream", VM_OpenWriteStream);
	ScriptManager::DefineNative(Class, "CreateFile", VM_CreateFile);
	ScriptManager::DefineNative(Class, "RenameFile", VM_RenameFile);
	ScriptManager::DefineNative(Class, "CopyFile", VM_CopyFile);
	ScriptManager::DefineNative(Class, "DeleteFile", VM_DeleteFile);
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
	char* mountPointPath = nullptr;
	if (argCount >= 4 && !IS_NULL(args[3])) {
		mountPointPath = GET_ARG(3, GetString);
	}
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
 * \method GetMountCount
 * \desc Gets the amount of mounts.
 * \return integer Returns an integer value.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_GetMountCount(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	ObjInstance* objVfs = AS_VFS(args[0]);

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	return INTEGER_VAL((int)vfs->GetMountCount());
}
/***
 * \method GetMountNameByIndex
 * \desc Gets the name of the mount of the given index.
 * \param index (string): The mount index.
 * \return string Returns a string value.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_GetMountNameByIndex(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjInstance* objVfs = AS_VFS(args[0]);
	int index = GET_ARG(1, GetInteger);

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	if (index < 0 || index >= (int)vfs->GetMountCount()) {
		char error[128];
		snprintf(error, sizeof error, "Mount index %d out of range. (0 - %d)", index, vfs->GetMountCount() - 1);
		throw ScriptException(error);
	}

	const char* name = vfs->GetMountName(index);

	if (ScriptManager::Lock()) {
		ObjString* string = CopyString(name);
		ScriptManager::Unlock();
		return OBJECT_VAL(string);
	}

	return NULL_VAL;
}
/***
 * \method GetMountPoint
 * \desc Gets the mount point of a mount.
 * \param name (string): The mount name.
 * \return string Returns a string value, or `null` if there is no mount point.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_GetMountPoint(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjInstance* objVfs = AS_VFS(args[0]);
	char* name = GET_ARG(1, GetString);

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	std::string mountPoint = vfs->GetMountPoint(name);
	if (mountPoint.size() == 0) {
		return NULL_VAL;
	}

	if (ScriptManager::Lock()) {
		ObjString* string = CopyString(mountPoint);
		ScriptManager::Unlock();
		return OBJECT_VAL(string);
	}

	return NULL_VAL;
}
/***
 * \method GetMountIndex
 * \desc Gets the index of the mount of the given name.
 * \param name (string): The mount name.
 * \return integer Returns an integer value, or `-1` if there is no mount with the given name.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_GetMountIndex(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjInstance* objVfs = AS_VFS(args[0]);
	char* name = GET_ARG(1, GetString);

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	return INTEGER_VAL(vfs->GetMountIndex(name));
}
/***
 * \method EnumerateFiles
 * \desc Gets the paths of all the files.
 * \paramOpt path (string): The path of the folder to find files in. (default: `/`)
 * \paramOpt pattern (string): The search pattern for the files. (ex: "*" for any file, "*.*" any file name with any file type, "*.png" any PNG file) (default: `*`)
 * \paramOpt allDirs (boolean): Whether to search into all folders in the directory. (default: `true`)
 * \return array Returns an array containing the filepaths as strings.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_EnumerateFiles(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 1);

	ObjInstance* objVfs = AS_VFS(args[0]);
	char* directory = GET_ARG_OPT(1, GetString, nullptr);
	char* pattern = GET_ARG_OPT(2, GetString, nullptr);
	int allDirs = GET_ARG_OPT(3, GetInteger, true);

	if (directory && (directory[0] == '\0' || strcmp(directory, "/") == 0)) {
		directory = nullptr;
	}
	if (pattern && strcmp(pattern, "*") == 0) {
		pattern = nullptr;
	}

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	if (!ScriptManager::Lock()) {
		return NULL_VAL;
	}

	ObjArray* array = NewArray();

	VFSEnumerationOptions options;
	options.OnlyCurrentDirectory = !allDirs;

	for (size_t i = 0, sz = vfs->GetMountCount(); i < sz; i++) {
		VFSProvider* provider = vfs->Get(i);

		const char* realDir = nullptr;

		// Get correct path
		std::string mountPoint = vfs->GetMountPoint(i);

		if (directory != nullptr) {
			size_t mountPointLength = mountPoint.size();
			if (mountPointLength == 0) {
				realDir = directory;
			}
			else if (StringUtils::StartsWith(directory, mountPoint.c_str())) {
				realDir = directory + mountPointLength;
			}
			else {
				continue;
			}
		}

		VFSEnumeration enumeration = provider->EnumerateFiles(realDir, options);
		VFSEnumerationResult result = enumeration.Result;
		if (result != VFSEnumerationResult::SUCCESS) {
			continue;
		}

		for (size_t j = 0; j < enumeration.Entries.size(); j++) {
			std::string entryName = enumeration.Entries[j];

			if (mountPoint.size() > 0) {
				entryName = mountPoint + entryName;
			}

			if (pattern && !StringUtils::WildcardMatch(entryName.c_str(), pattern)) {
				continue;
			}

			ObjString* path = CopyString(entryName);
			array->Values->push_back(OBJECT_VAL(path));
		}
	}

	ScriptManager::Unlock();

	return OBJECT_VAL(array);
}
/***
 * \method FileExists
 * \desc Checks to see if a file exists with the given filename.
 * \param filename (string): The given filename.
 * \return boolean Returns whether the file exists.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_FileExists(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjInstance* objVfs = AS_VFS(args[0]);
	char* filename = GET_ARG(1, GetString);

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	if (vfs->FileExists(filename)) {
		return INTEGER_VAL(true);
	}

	return INTEGER_VAL(false);
}
/***
 * \method OpenReadStream
 * \desc Opens a stream for reading, if the file exists and is readable.
 * \param filename (string): The given filename.
 * \return stream Returns the newly opened stream, or `null`.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_OpenReadStream(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjInstance* objVfs = AS_VFS(args[0]);
	char* filename = GET_ARG(1, GetString);

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	Stream* stream = VirtualFileStream::New(vfs, filename, VirtualFileStream::READ_ACCESS);
	if (!stream) {
		return NULL_VAL;
	}

	ObjStream* objStream = StreamImpl::New((void*)stream);
	return OBJECT_VAL(objStream);
}
/***
 * \method OpenWriteStream
 * \desc Opens a stream for writing, if the file exists and is writable.
 * \param filename (string): The given filename.
 * \return stream Returns the newly opened stream, or `null`.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_OpenWriteStream(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjInstance* objVfs = AS_VFS(args[0]);
	char* filename = GET_ARG(1, GetString);

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	Stream* stream = VirtualFileStream::New(vfs, filename, VirtualFileStream::WRITE_ACCESS);
	if (!stream) {
		return NULL_VAL;
	}

	ObjStream* objStream = StreamImpl::New((void*)stream);
	return OBJECT_VAL(objStream);
}
/***
 * \method CreateFile
 * \desc Creates a file with the given filename. If the file already exists, this does nothing and returns `true`.<br/>\
If the path is an existing directory, this does nothing and returns `false`.
 * \param filename (string): The file to create.
 * \return boolean Returns `true` if the file was created, `false` if otherwise.
 * \ns VirtualFileSystem
 */
/***
 * \method CreateFile
 * \desc Creates a file with the given filename, copying the contents of the stream from its current position into the file.<br/>\
If the file already exists, this overwrites the file.<br/>\
If the path is an existing directory, this does nothing and returns `false`.
 * \param filename (string): The file to create.
 * \param stream (stream): The stream to copy the contents of.
 * \paramOpt offset (integer): Where to start copying the contents of the stream. By default, starts copying from the stream's current position.
 * \paramOpt length (integer): How many bytes to copy. By default, copies how many bytes are remaining in the stream.
 * \return boolean Returns `true` if the file was created, `false` if otherwise.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_CreateFile(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 2);

	ObjInstance* objVfs = AS_VFS(args[0]);
	char* filename = GET_ARG(1, GetString);
	ObjStream* objStream = GET_ARG_OPT(2, GetStream, nullptr);
	int offsetArg = GET_ARG_OPT(3, GetInteger, 0);
	int lengthArg = GET_ARG_OPT(4, GetInteger, 0);

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	Stream* stream = nullptr;
	size_t length = 0;

	if (argCount >= 3) {
		stream = (Stream*)ScriptManager::RegistryGet((Obj*)objStream);
		if (stream == nullptr) {
			throw ScriptException("Cannot read closed stream!");
		}
		if (!stream->IsReadable()) {
			throw ScriptException("Stream is not readable!");
		}

		if (offsetArg < 0) {
			throw ScriptException("Offset must be a positive integer.");
		}
		if (lengthArg < 0) {
			throw ScriptException("Length must be a positive integer.");
		}

		size_t streamLength = stream->Length();

		size_t offset = (size_t)offsetArg;
		if (argCount < 4) {
			offset = stream->Position();
		}
		else {
			if (offset > streamLength) {
				offset = streamLength;
			}

			stream->Seek(offset);
		}

		size_t maxLength = streamLength - offset;

		length = (size_t)lengthArg;
		if (argCount < 5) {
			length = maxLength;
		}
		else if (length > maxLength) {
			length = maxLength;
		}
	}

	if (!vfs->CreateFile(filename)) {
		return INTEGER_VAL(false);
	}

	if (length > 0 && stream != nullptr) {
		Stream* outStream = vfs->OpenWriteStream(filename);
		if (outStream) {
			stream->CopyTo(outStream, length);
			vfs->CloseStream(outStream);
		}
	}

	return INTEGER_VAL(true);
}
/***
 * \method RenameFile
 * \desc Renames file from <param oldFilename> to <param newFilename>.<br/>\
If a file with the name of <param newFilename> already exists, this does nothing and returns `false`.<br/>\
If either path is a directory, this does nothing and returns `false`.<br/>\
If both paths are equal, this does nothing and returns `false`.
 * \param oldFilename (string): The file to rename.
 * \param newFilename (string): The new filename.
 * \return boolean Returns `true` if the file was renamed, `false` if otherwise.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_RenameFile(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ObjInstance* objVfs = AS_VFS(args[0]);
	char* oldFilename = GET_ARG(1, GetString);
	char* newFilename = GET_ARG(2, GetString);

	if (strcmp(oldFilename, newFilename) == 0) {
		return INTEGER_VAL(false);
	}

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	return INTEGER_VAL(vfs->RenameFile(oldFilename, newFilename));
}
/***
 * \method CopyFile
 * \desc Copies file <param sourceFilename> into <param destFilename>.<br/>\
If a file with the name of <param destFilename> already exists, this overwrites said file.<br/>\
If either path is a directory, this does nothing and returns `false`.<br/>\
If both paths are equal, this does nothing and returns `false`.
 * \param sourceFilename (string): The source file.
 * \param destFilename (string): The destination file.
 * \return boolean Returns `true` if the file was renamed, `false` if otherwise.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_CopyFile(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ObjInstance* objVfs = AS_VFS(args[0]);
	char* sourceFilename = GET_ARG(1, GetString);
	char* destFilename = GET_ARG(2, GetString);

	if (strcmp(sourceFilename, destFilename) == 0) {
		return INTEGER_VAL(false);
	}

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	return INTEGER_VAL(vfs->CopyFile(sourceFilename, destFilename));
}
/***
 * \method DeleteFile
 * \desc Deletes a file with the given filename.<br/>\
If the path is an existing directory, this does nothing and returns `false`.
 * \param filename (string): The file to delete.
 * \return boolean Returns `true` if the file was deleted, `false` if otherwise.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_DeleteFile(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjInstance* objVfs = AS_VFS(args[0]);
	char* filename = GET_ARG(1, GetString);

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	return INTEGER_VAL(vfs->DeleteFile(filename));
}
/***
 * \method Delete
 * \desc Deletes the virtual file system.<br/>\
All mounts are flushed (if needed), then all open streams are closed.<br/>\
The VFS can no longer be used after this function is called.
 * \ns VirtualFileSystem
 */
VMValue VFSImpl::VM_Delete(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	ObjInstance* objVfs = AS_VFS(args[0]);

	VirtualFileSystem* vfs = (VirtualFileSystem*)GetVFS(objVfs);
	CHECK_EXISTS(vfs);

	// Obviously, we must never let a script delete the main VFS.
	if (vfs == ResourceManager::GetVFS()) {
		throw ScriptException("Cannot delete the main virtual file system!");
	}

	ScriptManager::RegistryRemove((void*)vfs);
	delete vfs;

	return NULL_VAL;
}

#undef CHECK_EXISTS
#undef GET_ARG
#undef GET_ARG_OPT
