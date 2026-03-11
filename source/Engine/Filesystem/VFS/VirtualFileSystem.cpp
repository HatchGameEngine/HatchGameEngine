#include <Engine/Filesystem/VFS/VirtualFileSystem.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/Filesystem/VFS/EggVFS.h>
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

		bool wasDetected = false;
		int eggCompressionType = EGG_COMPRESSION_TYPE_NONE;

		if (type == VFSType::NONE) {
			if (HatchVFS::IsFile(stream)) {
				type = VFSType::HATCH;
			}
			else if (EggVFS::IsFile(stream, eggCompressionType)) {
				type = VFSType::EGG;
			}

			wasDetected = true;
		}

		switch (type) {
		case VFSType::HATCH: {
			HatchVFS* hatchVfs = new HatchVFS(flags);
			if (wasDetected) {
				hatchVfs->Open(stream, stream->Position());
			}
			else {
				hatchVfs->Open(stream);
			}
			vfs = hatchVfs;
			break;
		}
		case VFSType::EGG: {
			EggVFS* eggVfs = new EggVFS(flags);
			if (wasDetected) {
				eggVfs->Open(stream, eggCompressionType, stream->Position());
			}
			else {
				eggVfs->Open(stream);
			}
			vfs = eggVfs;
			break;
		}
		default:
			stream->Close();
			stream = nullptr;
			break;
		}
	}

	if (vfs == nullptr) {
		return VFSMountStatus::COULD_NOT_MOUNT;
	}
	else if (!vfs->IsOpen()) {
		delete vfs;
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

bool VirtualFileSystem::PreloadFiles(std::vector<std::string> filenames) {
	if (filenames.size() == 0) {
		return true;
	}

	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VFSMount& mount = LoadedVFS[i];
		VFSProvider* vfs = mount.VFSPtr;

		VFSEnumeration enumeration = vfs->EnumerateFiles(nullptr);
		if (enumeration.Result != VFSEnumerationResult::SUCCESS) {
			continue;
		}

		std::vector<std::string> matches;

		for (size_t e = 0; e < enumeration.Entries.size(); e++) {
			std::string entryName = enumeration.Entries[e];
			for (size_t f = 0; f < filenames.size(); f++) {
				// Try a wildcard match. This only works with real filenames (no transformation)
				if (StringUtils::WildcardMatch(entryName.c_str(), filenames[f].c_str())) {
					matches.push_back(entryName);
				}
				else {
					// Try with the transformed filename. Won't support wildcards
					std::string transformedFilename = vfs->TransformFilename(filenames[f].c_str());
					if (transformedFilename == entryName) {
						matches.push_back(filenames[f]);
					}
					// Finally try with the entry name itself
					else if (filenames[f] == entryName) {
						matches.push_back(filenames[f]);
					}
				}
			}
		}

		if (matches.size() == 0) {
			continue;
		}

		if (!vfs->PreloadFiles(matches)) {
			return false;
		}
	}

	return true;
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
