#include <Engine/Filesystem/VFS/VirtualFileSystem.h>

#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/Filesystem/VFS/FileSystemVFS.h>
#include <Engine/Filesystem/VFS/HatchVFS.h>
#include <Engine/Filesystem/VFS/MemoryVFS.h>
#include <Engine/Utilities/StringUtils.h>

VFSProvider* VirtualFileSystem::Get(const char* name) {
	size_t index = GetIndex(name);
	if (index != -1) {
		return LoadedVFS[index].VFSPtr;
	}

	return nullptr;
}
size_t VirtualFileSystem::GetIndex(const char* name) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSMount& mount = LoadedVFS[i];

		if (strcmp(mount.Name.c_str(), name) == 0) {
			return i;
		}
	}

	return -1;
}
VFSProvider* VirtualFileSystem::Get(size_t index) {
	if (index < LoadedVFS.size()) {
		return LoadedVFS[index].VFSPtr;
	}

	return nullptr;
}

VFSMountStatus VirtualFileSystem::Mount(const char* name,
	const char* filename,
	const char* mountPoint,
	VFSType type,
	Uint16 flags) {
	VFSProvider* vfs = Get(name);
	if (vfs != nullptr) {
		return VFSMountStatus::ALREADY_EXISTS;
	}

	if (mountPoint == nullptr) {
		mountPoint = DEFAULT_MOUNT_POINT;
	}

	if (type == VFSType::FILESYSTEM && Directory::Exists(filename)) {
		FileSystemVFS* fsVfs = new FileSystemVFS(flags);
		fsVfs->Open(filename);
		vfs = fsVfs;
	}
	else if (type == VFSType::MEMORY) {
		MemoryVFS* memVfs = new MemoryVFS(flags);
		memVfs->Open();
		vfs = memVfs;
	}
	else {
		Stream* stream = File::Open(filename, File::READ_ACCESS);
		if (!stream) {
			return VFSMountStatus::NOT_FOUND;
		}

		switch (type) {
		case VFSType::HATCH: {
			HatchVFS* hatchVfs = new HatchVFS(flags);
			hatchVfs->Open(stream);
			vfs = hatchVfs;
			break;
		}
		default:
			stream->Close();
			stream = nullptr;
			break;
		}
	}

	if (vfs == nullptr || !vfs->IsOpen()) {
		return VFSMountStatus::COULD_NOT_MOUNT;
	}

	std::string mountPointPath = Path::Normalize(mountPoint);

	LoadedVFS.push_front({std::string(name), mountPointPath, vfs});

	return VFSMountStatus::MOUNTED;
}
bool VirtualFileSystem::IsMounted(const char* name) {
	size_t index = GetIndex(name);
	if (index == -1) {
		return false;
	}

	return true;
}
VFSMountStatus VirtualFileSystem::Unmount(const char* name) {
	size_t index = GetIndex(name);
	if (index == -1) {
		return VFSMountStatus::NOT_FOUND;
	}

	VFSProvider* vfs = LoadedVFS[index].VFSPtr;
	if (!vfs->CanUnmount()) {
		return VFSMountStatus::COULD_NOT_UNMOUNT;
	}

	LoadedVFS.erase(LoadedVFS.begin() + index);

	return VFSMountStatus::UNMOUNTED;
}
int VirtualFileSystem::NumMounted() {
	return LoadedVFS.size();
}

const char* VirtualFileSystem::GetFilename(VFSMount mount, const char* filename) {
	std::string mountPoint = mount.MountPoint;
	size_t length = mountPoint.size();
	if (length == 0) {
		return filename;
	}

	if (StringUtils::StartsWith(filename, mountPoint.c_str())) {
		return filename + length;
	}

	return nullptr;
}

bool VirtualFileSystem::LoadFile(const char* filename, Uint8** out, size_t* size) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSMount& mount = LoadedVFS[i];
		VFSProvider* vfs = mount.VFSPtr;

		const char* realFilename = GetFilename(mount, filename);
		if (realFilename == nullptr) {
			continue;
		}

		if (vfs->ReadFile(realFilename, out, size)) {
			return true;
		}
	}

	return false;
}
bool VirtualFileSystem::FileExists(const char* filename) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSMount& mount = LoadedVFS[i];
		VFSProvider* vfs = mount.VFSPtr;

		const char* realFilename = GetFilename(mount, filename);
		if (realFilename == nullptr) {
			continue;
		}

		if (vfs->HasFile(realFilename)) {
			return true;
		}
	}

	return false;
}
bool VirtualFileSystem::CreateFile(const char* filename) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSMount& mount = LoadedVFS[i];
		VFSProvider* vfs = mount.VFSPtr;

		const char* realFilename = GetFilename(mount, filename);
		if (realFilename == nullptr) {
			continue;
		}

		if (vfs->CreateFile(realFilename)) {
			return true;
		}
	}

	return false;
}
bool VirtualFileSystem::RenameFile(const char* oldFilename, const char* newFilename) {
	VFSProvider* vfsA = nullptr;
	VFSProvider* vfsB = nullptr;

	const char* realOldFilename = nullptr;
	const char* realNewFilename = nullptr;

	if (LoadedVFS.size() == 0) {
		return false;
	}

	// Figure out which mounts could have the given files
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSMount& mount = LoadedVFS[i];
		VFSProvider* vfs = mount.VFSPtr;

		const char* oldName = GetFilename(mount, oldFilename);

		if (oldName != nullptr && vfs->HasFile(oldName) && vfsA == nullptr) {
			realOldFilename = oldName;
			vfsA = vfs;

			// Check if the same VFS is suitable for the destination
			if (mount.MountPoint.size() > 0) {
				const char* newName = GetFilename(mount, newFilename);
				if (newName != nullptr) {
					realNewFilename = newName;
					vfsB = vfs;
					break;
				}
			}
		}
	}

	// The file that needs to be renamed doesn't exist, or the VFS is not readable
	if (vfsA == nullptr || !vfsA->IsReadable()) {
		return false;
	}

	// If there was no suitable destination VFS:
	// Try checking in the first (last in the list) mounted VFS, if it has a mount pount
	VFSMount* firstMount = &LoadedVFS.back();

	if (vfsB == nullptr && firstMount->MountPoint.size() > 0) {
		VFSProvider* vfs = firstMount->VFSPtr;

		const char* newName = GetFilename(*firstMount, newFilename);
		if (newName != nullptr) {
			realNewFilename = newName;
			vfsB = vfs;
		}
	}

	if (vfsB == nullptr) {
		// Try again with the rest of the list, if they have mount points
		for (size_t i = 0; i < LoadedVFS.size(); i++) {
			VFSMount* mount = &LoadedVFS[i];
			if (mount == firstMount || mount->MountPoint.size() == 0) {
				continue;
			}

			VFSProvider* vfs = mount->VFSPtr;

			const char* newName = GetFilename(*mount, newFilename);
			if (newName != nullptr) {
				realNewFilename = newName;
				vfsB = vfs;
				break;
			}
		}
	}

	// Try checking in the first mounted VFS, if it does not have a mount point
	if (vfsB == nullptr && firstMount->MountPoint.size() == 0) {
		VFSProvider* vfs = firstMount->VFSPtr;

		const char* newName = GetFilename(*firstMount, newFilename);
		if (newName != nullptr) {
			realNewFilename = newName;
			vfsB = vfs;
		}
	}

	if (vfsB == nullptr) {
		// Try again with the rest of the list, if they do not have mount points
		for (size_t i = 0; i < LoadedVFS.size(); i++) {
			VFSMount* mount = &LoadedVFS[i];
			if (mount == firstMount || mount->MountPoint.size() > 0) {
				continue;
			}

			VFSProvider* vfs = mount->VFSPtr;

			const char* newName = GetFilename(*mount, newFilename);
			if (newName != nullptr) {
				realNewFilename = newName;
				vfsB = vfs;
				break;
			}
		}
	}

	// No destination VFS found
	if (vfsB == nullptr) {
		return false;
	}

	// The file that needs to be renamed exists, and in the same VFS
	// Just rename the entry
	if (vfsA == vfsB) {
		// The new file already exists.
		if (vfsA->HasFile(realNewFilename)) {
			return false;
		}

		return vfsA->RenameFile(realOldFilename, realNewFilename);
	}

	// Else, the rename moves the file from a provider into another
	// Check if both filesystems are writable
	if (!vfsA->IsWritable() || !vfsB->IsWritable()) {
		return false;
	}

	// The new file already exists.
	if (vfsB->HasFile(realNewFilename)) {
		return false;
	}

	// Open both streams
	Stream* oldStream = vfsA->OpenReadStream(realOldFilename);
	if (!oldStream) {
		return false;
	}

	Stream* newStream = vfsB->OpenWriteStream(realNewFilename);
	if (!newStream) {
		vfsA->CloseStream(oldStream);
		return false;
	}

	// Copy the data
	oldStream->CopyTo(newStream);

	// Close the streams
	vfsA->CloseStream(oldStream);
	vfsB->CloseStream(newStream);

	// Delete the old file
	vfsA->DeleteFile(realOldFilename);

	return true;
}
bool VirtualFileSystem::DeleteFile(const char* filename) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSMount& mount = LoadedVFS[i];
		VFSProvider* vfs = mount.VFSPtr;

		const char* realFilename = GetFilename(mount, filename);
		if (realFilename == nullptr) {
			continue;
		}

		if (vfs->DeleteFile(realFilename)) {
			return true;
		}
	}

	return false;
}

Stream* VirtualFileSystem::OpenReadStream(const char* filename) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSMount& mount = LoadedVFS[i];
		VFSProvider* vfs = mount.VFSPtr;

		if (!vfs->IsReadable()) {
			continue;
		}

		const char* realFilename = GetFilename(mount, filename);
		if (realFilename == nullptr) {
			continue;
		}

		Stream* stream = vfs->OpenReadStream(realFilename);
		if (stream) {
			return stream;
		}
	}

	return nullptr;
}
Stream* VirtualFileSystem::OpenWriteStream(const char* filename) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSMount& mount = LoadedVFS[i];
		VFSProvider* vfs = mount.VFSPtr;

		if (!vfs->IsWritable()) {
			continue;
		}

		const char* realFilename = GetFilename(mount, filename);
		if (realFilename == nullptr) {
			continue;
		}

		Stream* stream = vfs->OpenWriteStream(realFilename);
		if (stream) {
			return stream;
		}
	}

	return nullptr;
}
Stream* VirtualFileSystem::OpenAppendStream(const char* filename) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSMount& mount = LoadedVFS[i];
		VFSProvider* vfs = mount.VFSPtr;

		if (!vfs->IsWritable()) {
			continue;
		}

		const char* realFilename = GetFilename(mount, filename);
		if (realFilename == nullptr) {
			continue;
		}

		Stream* stream = vfs->OpenAppendStream(realFilename);
		if (stream) {
			return stream;
		}
	}

	return nullptr;
}
bool VirtualFileSystem::ReplaceOpenStream(Stream* oldStream, Stream* newStream) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSMount& mount = LoadedVFS[i];
		VFSProvider* vfs = mount.VFSPtr;

		if (vfs->ReplaceOpenStream(oldStream, newStream)) {
			return true;
		}
	}

	return false;
}
bool VirtualFileSystem::CloseStream(Stream* stream) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSMount& mount = LoadedVFS[i];
		VFSProvider* vfs = mount.VFSPtr;

		if (vfs->CloseStream(stream)) {
			return true;
		}
	}

	return false;
}

void VirtualFileSystem::Dispose() {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		delete LoadedVFS[i].VFSPtr;
	}

	LoadedVFS.clear();
}

VirtualFileSystem::~VirtualFileSystem() {
	Dispose();
}
