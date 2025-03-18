#include <Engine/Filesystem/VFS/VirtualFileSystem.h>

#include <Engine/Filesystem/VFS/FileSystemVFS.h>
#include <Engine/Filesystem/VFS/HatchVFS.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/IO/SDLStream.h>

VFSMountStatus VirtualFileSystem::Mount(const char* filename, const char* mountPoint, VFSType type, Uint16 flags) {
	VFSProvider* vfs = nullptr;

	if (mountPoint == nullptr) {
		mountPoint = DEFAULT_MOUNT_POINT;
	}

	if (type == VFSType::FILESYSTEM) {
		FileSystemVFS* fsVfs = new FileSystemVFS(mountPoint, flags);
		fsVfs->Open(filename);
		vfs = fsVfs;
	}
	else {
		SDLStream* stream = SDLStream::New(filename, SDLStream::READ_ACCESS);
		if (!stream) {
			return VFSMountStatus::NOT_FOUND;
		}

		switch (type) {
		case VFSType::HATCH: {
			HatchVFS *hatchVfs = new HatchVFS(mountPoint, flags);
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
	char resourcePath[MAX_PATH_LENGTH];

	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSProvider* vfs = LoadedVFS[i];

		vfs->TransformFilename(filename, resourcePath, sizeof resourcePath);

		if (vfs->ReadFile(resourcePath, out, size)) {
			return true;
		}
	}

	return false;
}
bool VirtualFileSystem::FileExists(const char* filename) {
	char resourcePath[MAX_PATH_LENGTH];

	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSProvider* vfs = LoadedVFS[i];

		vfs->TransformFilename(filename, resourcePath, sizeof resourcePath);

		if (vfs->HasFile(resourcePath)) {
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
