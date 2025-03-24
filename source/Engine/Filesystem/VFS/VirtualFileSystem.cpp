#include <Engine/Filesystem/VFS/VirtualFileSystem.h>

#include <Engine/Filesystem/Path.h>
#include <Engine/Filesystem/VFS/FileSystemVFS.h>
#include <Engine/Filesystem/VFS/HatchVFS.h>
#include <Engine/Filesystem/VFS/MemoryVFS.h>
#include <Engine/IO/SDLStream.h>
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

	if (type == VFSType::FILESYSTEM) {
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
		SDLStream* stream = SDLStream::New(filename, SDLStream::READ_ACCESS);
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
	size_t offset = 0;

	std::string mountPoint = mount.MountPoint;

	if (StringUtils::StartsWith(filename, mountPoint.c_str())) {
		offset = mountPoint.size();
	}

	return filename + offset;
}

bool VirtualFileSystem::LoadFile(const char* filename, Uint8** out, size_t* size) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSMount& mount = LoadedVFS[i];
		VFSProvider* vfs = mount.VFSPtr;

		if (vfs->ReadFile(GetFilename(mount, filename), out, size)) {
			return true;
		}
	}

	return false;
}
bool VirtualFileSystem::FileExists(const char* filename) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSMount& mount = LoadedVFS[i];
		VFSProvider* vfs = mount.VFSPtr;

		if (vfs->HasFile(GetFilename(mount, filename))) {
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

		Stream* stream = vfs->OpenReadStream(GetFilename(mount, filename));
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

		Stream* stream = vfs->OpenWriteStream(GetFilename(mount, filename));
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

		Stream* stream = vfs->OpenAppendStream(GetFilename(mount, filename));
		if (stream) {
			return stream;
		}
	}

	return nullptr;
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
