#include <Engine/ResourceTypes/ResourceManager.h>

#include <Engine/Application.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Filesystem/VFS/VirtualFileSystem.h>
#include <Engine/IO/FileStream.h>

#define RESOURCES_VFS_NAME "main"

#define RESOURCES_DIR_PATH "Resources"

VirtualFileSystem* vfs = nullptr;
VFSProvider* mainResource = nullptr;

bool ResourceManager::UsingDataFolder = false;

const char* data_files[] = {PATHLOCATION_GAME_URL "Data.hatch",
	PATHLOCATION_GAME_URL "Game.hatch",
	PATHLOCATION_GAME_URL TARGET_NAME ".hatch"};

struct DataFileCandidate {
	std::string Path;
	bool Valid;
};

std::vector<DataFileCandidate> FindDataFiles() {
	std::vector<DataFileCandidate> candidates;

	for (size_t i = 0; i < sizeof(data_files) / sizeof(data_files[0]); i++) {
		const char* filename = data_files[i];

		std::string resolved = "";
		if (Path::FromURL(filename, resolved)) {
			DataFileCandidate candidate;
			candidate.Path = resolved;
			candidate.Valid = File::Exists(resolved.c_str());
			candidates.push_back(candidate);
		}
	}

	return candidates;
}

char* GetDataFileCandidate(std::vector<DataFileCandidate> candidates) {
	for (size_t i = 0; i < candidates.size(); i++) {
		if (candidates[i].Valid) {
			return StringUtils::Create(candidates[i].Path);
		}
	}

	return nullptr;
}

bool ResourceManager::Init(const char* dataFilePath) {
	bool foundDataFile = false;
	bool isDirectory = false;
	char* filename = nullptr;

	std::vector<DataFileCandidate> candidates = FindDataFiles();

#ifdef DEVELOPER_MODE
	bool useResourcesFolder = true;
#endif

	UsingDataFolder = false;

	vfs = new VirtualFileSystem();

	// Note that in this case, the filename should not be an URL.
	if (dataFilePath != nullptr) {
		bool found = false;

#ifdef DEVELOPER_MODE
		useResourcesFolder = false;
#endif

		if (dataFilePath[strlen(dataFilePath) - 1] == '/' &&
			Directory::Exists(dataFilePath)) {
			found = true;
			isDirectory = true;
		}
		else if (File::Exists(dataFilePath)) {
			found = true;
		}

		if (found) {
			filename = StringUtils::Duplicate(dataFilePath);
			foundDataFile = true;
		}
		else {
			Log::Print(Log::LOG_ERROR, "Could not find file \"%s\"!", dataFilePath);
		}
	}
	else if (candidates.size() > 0) {
		char* candidate = GetDataFileCandidate(candidates);
		if (candidate != nullptr) {
			filename = candidate;
			foundDataFile = true;
		}
	}
	else {
		Log::Print(Log::LOG_ERROR, "No valid location to access data file!");
	}

	if (isDirectory) {
		size_t filenameLength = strlen(filename);
		if (filename[filenameLength - 1] == '/') {
			filename[filenameLength - 1] = '\0';
		}

		const char* filenameOnly = StringUtils::GetFilename(filename);

		Log::Print(Log::LOG_VERBOSE, "Loading \"%s\"...", filenameOnly);

		VFSMountStatus status = vfs->Mount(RESOURCES_VFS_NAME,
				filename,
				nullptr,
				VFSType::FILESYSTEM,
				VFS_READABLE);

		if (status != VFSMountStatus::MOUNTED) {
			Log::Print(Log::LOG_ERROR, "Could not access \"%s\"!", filename);
		}
	}
	else if (foundDataFile) {
		const char* filenameOnly = StringUtils::GetFilename(filename);

		Log::Print(Log::LOG_VERBOSE, "Loading \"%s\"...", filenameOnly);

		ResourceManager::Mount(RESOURCES_VFS_NAME,
			filename,
			nullptr,
			VFSType::HATCH,
			VFS_READABLE);
	}
#ifdef DEVELOPER_MODE
	else if (useResourcesFolder) {
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
			Log::Print(Log::LOG_ERROR, "Could not access \"%s\"!", RESOURCES_DIR_PATH);
		}
	}
#endif

	Memory::Free(filename);

	if (GetMainResource() == nullptr) {
		Log::Print(Log::LOG_ERROR, "No data files loaded!");

#ifdef DEVELOPER_MODE
		if (!useResourcesFolder && candidates.size() > 0) {
			Log::Print(Log::LOG_ERROR,
				"Ensure the data file is in one of the following paths:");

			for (size_t i = 0; i < candidates.size(); i++) {
				Log::Print(Log::LOG_ERROR, "* %s", candidates[i].Path.c_str());
			}
		}
#endif

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
