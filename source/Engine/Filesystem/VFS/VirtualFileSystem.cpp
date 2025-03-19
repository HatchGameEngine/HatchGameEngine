#include <Engine/Filesystem/VFS/VirtualFileSystem.h>

#include <Engine/Filesystem/VFS/FileSystemVFS.h>
#include <Engine/Filesystem/VFS/HatchVFS.h>
#include <Engine/Filesystem/VFS/MemoryVFS.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/IO/SDLStream.h>

VFSProvider* VirtualFileSystem::Get(const char* name) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSProvider* vfs = LoadedVFS[i];

		if (strcmp(vfs->GetName().c_str(), name) == 0) {
			return vfs;
		}
	}

	return nullptr;
}

VFSMountStatus VirtualFileSystem::Mount(const char* name, const char* filename,
	const char* mountPoint, VFSType type, Uint16 flags) {
	VFSProvider* vfs = nullptr;

	if (mountPoint == nullptr) {
		mountPoint = DEFAULT_MOUNT_POINT;
	}

	if (type == VFSType::FILESYSTEM) {
		FileSystemVFS* fsVfs = new FileSystemVFS(name, mountPoint, flags);
		fsVfs->Open(filename);
		vfs = fsVfs;
	}
	else if (type == VFSType::MEMORY) {
		MemoryVFS* memVfs = new MemoryVFS(name, mountPoint, flags);
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
			HatchVFS *hatchVfs = new HatchVFS(name, mountPoint, flags);
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

	LoadedVFS.push_front(vfs);

	return VFSMountStatus::MOUNTED;
}
int VirtualFileSystem::NumMounted() {
	return LoadedVFS.size();
}

bool VirtualFileSystem::LoadFile(const char* filename, Uint8** out, size_t* size) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSProvider* vfs = LoadedVFS[i];

		if (vfs->ReadFile(filename, out, size)) {
			return true;
		}
	}

	return false;
}
bool VirtualFileSystem::FileExists(const char* filename) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSProvider* vfs = LoadedVFS[i];

		if (vfs->HasFile(filename)) {
			return true;
		}
	}

	return false;
}

Stream* VirtualFileSystem::OpenReadStream(const char* filename) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSProvider* vfs = LoadedVFS[i];

		if (!vfs->IsReadable()) {
			continue;
		}

		Stream* stream = vfs->OpenReadStream(filename);
		if (stream) {
			return stream;
		}
	}

	return nullptr;
}
Stream* VirtualFileSystem::OpenWriteStream(const char* filename) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSProvider* vfs = LoadedVFS[i];

		if (!vfs->IsWritable()) {
			continue;
		}

		Stream* stream = vfs->OpenWriteStream(filename);
		if (stream) {
			return stream;
		}
	}

	return nullptr;
}
Stream* VirtualFileSystem::OpenAppendStream(const char* filename) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSProvider* vfs = LoadedVFS[i];

		if (!vfs->IsWritable()) {
			continue;
		}

		Stream* stream = vfs->OpenAppendStream(filename);
		if (stream) {
			return stream;
		}
	}

	return nullptr;
}
bool VirtualFileSystem::CloseStream(Stream* stream) {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSProvider* vfs = LoadedVFS[i];

		if (vfs->CloseStream(stream)) {
			return true;
		}
	}

	return false;
}

void VirtualFileSystem::Dispose() {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		delete LoadedVFS[i];
	}

	LoadedVFS.clear();
}

VirtualFileSystem::~VirtualFileSystem() {
	Dispose();
}
