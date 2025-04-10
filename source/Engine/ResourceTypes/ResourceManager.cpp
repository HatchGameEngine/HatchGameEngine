#include <Engine/ResourceTypes/ResourceManager.h>

#include <Engine/Application.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Filesystem/VFS/VirtualFileSystem.h>

#define RESOURCES_VFS_NAME "main"

#define RESOURCES_DIR_PATH "Resources"

VirtualFileSystem* vfs = nullptr;
VFSProvider* mainResource = nullptr;

bool ResourceManager::UsingDataFolder = false;
bool ResourceManager::UsingModPack = false;

const char* data_files[] = {"Data.hatch", "Game.hatch", TARGET_NAME ".hatch"};

const char* FindDataFile() {
	for (size_t i = 0; i < sizeof(data_files) / sizeof(data_files[0]); i++) {
		const char* filename = data_files[i];

		if (File::Exists(filename)) {
			return filename;
		}
	}

	return nullptr;
}

bool ResourceManager::Init(const char* filename) {
	vfs = new VirtualFileSystem();

	if (filename != NULL && File::Exists(filename)) {
		Log::Print(Log::LOG_IMPORTANT, "Loading \"%s\"...", filename);
	}
	else {
		filename = FindDataFile();
	}

	if (filename != NULL) {
		ResourceManager::Mount(
			RESOURCES_VFS_NAME, filename, nullptr, VFSType::HATCH, VFS_READABLE);
	}
	else {
		VFSMountStatus status = vfs->Mount(RESOURCES_VFS_NAME,
			RESOURCES_DIR_PATH,
			nullptr,
			VFSType::FILESYSTEM,
			VFS_READABLE | VFS_WRITABLE);

		if (status == VFSMountStatus::MOUNTED) {
			Log::Print(Log::LOG_INFO, "Using \"%s\" folder.", RESOURCES_DIR_PATH);

			ResourceManager::UsingDataFolder = true;
		}
		else {
			Log::Print(Log::LOG_ERROR,
				"Could not access \"%s\" folder!",
				RESOURCES_DIR_PATH);
		}
	}

	if (GetMainResource() == nullptr) {
		Log::Print(Log::LOG_ERROR, "No resource files loaded!");

		return false;
	}

	return true;
}

bool ResourceManager::Mount(const char* name,
	const char* filename,
	const char* mountPoint,
	VFSType type,
	Uint16 flags) {
	VFSMountStatus status = vfs->Mount(name, filename, mountPoint, type, flags);

	if (status == VFSMountStatus::NOT_FOUND) {
		Log::Print(Log::LOG_ERROR, "Could not find resource \"%s\"!", filename);
	}
	else if (status != VFSMountStatus::MOUNTED) {
		Log::Print(Log::LOG_ERROR, "Could not load resource \"%s\"!", filename);
	}

	return status == VFSMountStatus::MOUNTED;
}
bool ResourceManager::Unmount(const char* name) {
	if (vfs == nullptr) {
		return false;
	}

	VFSProvider* provider = vfs->Get(name);
	if (provider == nullptr) {
		return false;
	}

	VFSMountStatus status = vfs->Unmount(name);
	if (status == VFSMountStatus::UNMOUNTED) {
		delete provider;

		return true;
	}

	return false;
}

VirtualFileSystem* ResourceManager::GetVFS() {
	return vfs;
}
VFSProvider* ResourceManager::GetMainResource() {
	mainResource = vfs->Get(RESOURCES_VFS_NAME);

	if (mainResource == nullptr || !mainResource->IsOpen()) {
		return nullptr;
	}

	return mainResource;
}
void ResourceManager::SetMainResourceWritable(bool writable) {
	VFSProvider* provider = GetMainResource();
	if (provider != nullptr) {
		provider->SetWritable(writable);
	}
}

bool ResourceManager::LoadResource(const char* filename, Uint8** out, size_t* size) {
	if (vfs) {
		return vfs->LoadFile(filename, out, size);
	}
	return false;
}
bool ResourceManager::ResourceExists(const char* filename) {
	if (vfs) {
		return vfs->FileExists(filename);
	}
	return false;
}
void ResourceManager::Dispose() {
	delete vfs;
	vfs = nullptr;
	mainResource = nullptr;
}
