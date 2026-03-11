#include <Engine/Application.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Error.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Filesystem/VFS/VirtualFileSystem.h>
#include <Engine/IO/FileStream.h>
#include <Engine/ResourceTypes/ResourceManager.h>

#define RESOURCES_VFS_NAME "main"

#define RESOURCES_DIR_PATH "Resources"

VirtualFileSystem* vfs = nullptr;
VFSProvider* mainResource = nullptr;

std::vector<std::string> ResourceManager::DataFilePaths;
std::vector<std::string> ResourceManager::PreloadList;
int ResourceManager::PreloadResources = PRELOAD_DEFAULT;
bool ResourceManager::UsingDataFolder = false;

std::pair<const char*, VFSType> ExtensionToVFSType[] = {std::make_pair(".hatch", VFSType::HATCH),
	std::make_pair(".egg", VFSType::EGG),
	std::make_pair(".egg.gz", VFSType::EGG),
	std::make_pair(".egg.tar", VFSType::EGG),
	std::make_pair(".egg.tar.gz", VFSType::EGG)};

const char* BaseDataFileNames[] = {"Data", "Game", TARGET_NAME};

bool ResourceManager::Init(const char* dataFilePath) {
	bool foundDataFile = false;
	bool isDirectory = false;
	char* filename = nullptr;

	std::vector<DataFileCandidate> candidates;

	bool useResourcesFolder = true;
	UsingDataFolder = false;

	InitDataFileList();
	candidates = FindDataFiles();

	vfs = new VirtualFileSystem();

	// Note that in this case, the filename should not be an URL.
	if (dataFilePath != nullptr && dataFilePath[0] != '\0') {
		bool found = false;

		useResourcesFolder = false;

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

		VFSMountStatus status = vfs->Mount(
			RESOURCES_VFS_NAME, filename, nullptr, VFSType::FILESYSTEM, VFS_READABLE);

		if (status != VFSMountStatus::MOUNTED) {
			Log::Print(Log::LOG_ERROR, "Could not access \"%s\"!", filename);
		}
	}
	else if (foundDataFile) {
		const char* filenameOnly = StringUtils::GetFilename(filename);

		Log::Print(Log::LOG_VERBOSE, "Loading \"%s\"...", filenameOnly);

		ResourceManager::Mount(RESOURCES_VFS_NAME, filename, nullptr, VFSType::NONE, VFS_READABLE);
	}
	else if (useResourcesFolder) {
		VFSMountStatus status = vfs->Mount(RESOURCES_VFS_NAME,
			RESOURCES_DIR_PATH,
			nullptr,
			VFSType::FILESYSTEM,
			VFS_READABLE | VFS_WRITABLE);

		if (status == VFSMountStatus::MOUNTED) {
#ifdef DEVELOPER_MODE
			Log::Print(Log::LOG_INFO, "Using \"%s\" folder.", RESOURCES_DIR_PATH);
#endif

			ResourceManager::UsingDataFolder = true;
		}
		else {
			Log::Print(Log::LOG_ERROR, "Could not access \"%s\"!", RESOURCES_DIR_PATH);
		}
	}

	Memory::Free(filename);

	// If the resource wasn't found
	if (GetMainResource() == nullptr) {
#ifdef DEVELOPER_MODE
		std::string error = "No data file was found!\n";
		if (candidates.size() > 0) {
			error += "Ensure that it's named one of the following:\n";
			for (size_t i = 0; i < candidates.size(); i++) {
				std::string path = candidates[i].Path;
				if (StringUtils::EndsWith(path.c_str(), ".tar") ||
					StringUtils::EndsWith(path.c_str(), ".tar.gz")) {
					continue;
				}

				error += "* " + path;
				if (i < candidates.size() - 1) {
					error += "\n";
				}
			}
		}

		Error::FatalNoMessageBox("%s", error.c_str());
#else
		const char* datafilename = StringUtils::GetFilename(DataFilePaths[0].c_str());

#if WIN32
		Error::Fatal(
			"%s not found! Ensure that it's in the same location as the application.",
			datafilename);
#else
		std::string additionalError;
		std::string resolved = "";
		if (Path::FromURL(PATHLOCATION_GAME_URL, resolved)) {
			additionalError = "Ensure that it's in the following path:\n";
			additionalError += "* " + resolved;
		}
		else {
			additionalError =
				"Ensure that the application has read access permissions.";
		}

#if UNIX
		Error::FatalNoMessageBox("%s not found! %s", datafilename, additionalError.c_str());
#else
		Error::Fatal("%s not found! %s", datafilename, additionalError.c_str());
#endif // #if UNIX
#endif // #if WIN32
#endif // #ifdef DEVELOPER_MODE

		return false;
	}
	else if (useResourcesFolder && GetMainResource()->IsEmpty()) {
		Log::Print(Log::LOG_WARN, "\"%s\" folder is empty.", RESOURCES_DIR_PATH);
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

void ResourceManager::InitDataFileList() {
	for (std::pair<const char*, VFSType> pair : ExtensionToVFSType) {
		size_t numBaseDataFileNames =
			sizeof(BaseDataFileNames) / sizeof(BaseDataFileNames[0]);
		for (size_t i = 0; i < numBaseDataFileNames; i++) {
			std::string combined = std::string(PATHLOCATION_GAME_URL) +
				BaseDataFileNames[i] + pair.first;
			DataFilePaths.push_back(combined);
		}
	}
}

std::vector<DataFileCandidate> ResourceManager::FindDataFiles() {
	std::vector<DataFileCandidate> candidates;

	for (size_t i = 0; i < DataFilePaths.size(); i++) {
		std::string resolved = "";
		if (Path::FromURL(DataFilePaths[i].c_str(), resolved)) {
			DataFileCandidate candidate;
			candidate.Path = resolved;
			candidate.Valid = File::Exists(resolved.c_str());
			candidates.push_back(candidate);
		}
	}

	return candidates;
}

char* ResourceManager::GetDataFileCandidate(std::vector<DataFileCandidate> candidates) {
	for (size_t i = 0; i < candidates.size(); i++) {
		if (candidates[i].Valid) {
			return StringUtils::Create(candidates[i].Path);
		}
	}

	return nullptr;
}

VFSType ResourceManager::DetectVFSTypeByFilename(const char* filename) {
	for (std::pair<const char*, VFSType> pair : ExtensionToVFSType) {
		if (StringUtils::EndsWith(filename, pair.first)) {
			return pair.second;
		}
	}

	return VFSType::NONE;
}

bool ResourceManager::Preload(std::vector<std::string> filenames) {
	if (!vfs) {
		return false;
	}

	if (PreloadResources == PRELOAD_NONE) {
		return true;
	}
	else if (PreloadResources == PRELOAD_ALL) {
		filenames.clear();
		filenames.push_back("*");
	}

	if (filenames.size() == 0) {
		return true;
	}

	return vfs->PreloadFiles(filenames);
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

	PreloadList.clear();
	DataFilePaths.clear();
}
