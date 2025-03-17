#include <Engine/ResourceTypes/ResourceManager.h>

#include <Engine/Application.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Filesystem/VFS/FileSystemVFS.h>
#include <Engine/Filesystem/VFS/HatchVFS.h>
#include <Engine/IO/SDLStream.h>
#include <Engine/IO/Stream.h>

#define RESOURCES_PATH "Resources"

std::deque<VirtualFileSystem*> LoadedVFS;

bool ResourceManager::UsingDataFolder = false;
bool ResourceManager::UsingModPack = false;

const char* data_files[] = {
	"Data.hatch",
	"Game.hatch",
	TARGET_NAME ".hatch"
};

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
	if (filename != NULL && File::Exists(filename)) {
		Log::Print(Log::LOG_IMPORTANT, "Loading \"%s\"...", filename);
	}
	else {
		filename = FindDataFile();
	}

	if (filename != NULL && File::Exists(filename)) {
		ResourceManager::Mount(filename, nullptr, VFSType::HATCH, VFS_READABLE);
	}
	else if (ResourceManager::Mount(RESOURCES_PATH, nullptr, VFSType::FILESYSTEM, VFS_READABLE)) {
		Log::Print(Log::LOG_INFO, "Using \"%s\" folder.", RESOURCES_PATH);

		ResourceManager::UsingDataFolder = true;
	}

	char modpacksString[1024];
	if (Application::Settings->GetString(
		    "game", "modpacks", modpacksString, sizeof modpacksString)) {
		if (File::Exists(modpacksString)) {
			Log::Print(Log::LOG_IMPORTANT, "Loading modpack \"%s\"...", modpacksString);

			if (ResourceManager::Mount(modpacksString, nullptr, VFSType::HATCH, VFS_READABLE)) {
				ResourceManager::UsingModPack = true;
			}
		}
	}

	if (LoadedVFS.size() == 0) {
		Log::Print(Log::LOG_ERROR, "No resource files loaded!");

		return false;
	}

	return true;
}
bool ResourceManager::Mount(const char* filename, const char* mountPoint, VFSType type, Uint16 flags) {
	VirtualFileSystem* vfs = nullptr;

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
			Log::Print(Log::LOG_ERROR, "Resource file \"%s\" not found!", filename);
			return false;
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
		Log::Print(Log::LOG_ERROR, "Could not read resource file \"%s\"!", filename);
		return false;
	}

	LoadedVFS.push_front(vfs);

	return true;
}
bool ResourceManager::LoadResource(const char* filename, Uint8** out, size_t* size) {
	char resourcePath[4096];

	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VirtualFileSystem* vfs = LoadedVFS[i];

		vfs->TransformFilename(filename, resourcePath, sizeof resourcePath);

		if (vfs->ReadFile(resourcePath, out, size)) {
			return true;
		}
	}

	return false;
}
bool ResourceManager::ResourceExists(const char* filename) {
	char resourcePath[4096];

	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		VirtualFileSystem* vfs = LoadedVFS[i];

		vfs->TransformFilename(filename, resourcePath, sizeof resourcePath);

		if (vfs->HasFile(resourcePath)) {
			return true;
		}
	}

	return false;
}
void ResourceManager::Dispose() {
	for (size_t i = 0; i < LoadedVFS.size(); i++) {
		delete LoadedVFS[i];
	}

	LoadedVFS.clear();
}
