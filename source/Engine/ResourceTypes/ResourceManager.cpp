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

bool ResourceManager::UsingDataFolder = true;
bool ResourceManager::UsingModPack = false;

void ResourceManager::PrefixParentPath(char* out, size_t outSize, const char* path) {
#if defined(SWITCH_ROMFS)
	snprintf(out, outSize, "romfs:/%s", path);
#else
	snprintf(out, outSize, "%s", path);
#endif
}

void ResourceManager::Init(const char* filename) {
	if (filename == NULL) {
		filename = "Data.hatch";
	}

	if (File::Exists(filename)) {
		ResourceManager::UsingDataFolder = false;

		Log::Print(Log::LOG_INFO, "Using \"%s\"", filename);
		ResourceManager::Load(filename, VFSType::HATCH);
	}
	else {
		Log::Print(Log::LOG_WARN, "Cannot find \"%s\".", filename);
	}

	if (ResourceManager::UsingDataFolder) {
		ResourceManager::Load(RESOURCES_PATH, VFSType::FILESYSTEM);
	}

	char modpacksString[1024];
	if (Application::Settings->GetString(
		    "game", "modpacks", modpacksString, sizeof modpacksString)) {
		if (File::Exists(modpacksString)) {
			ResourceManager::UsingModPack = true;

			Log::Print(Log::LOG_IMPORTANT, "Using \"%s\"", modpacksString);
			ResourceManager::Load(modpacksString, VFSType::HATCH);
		}
	}
}
void ResourceManager::Load(const char* filename, VFSType type) {
	VirtualFileSystem* vfs = nullptr;

	if (type == VFSType::FILESYSTEM) {
		FileSystemVFS* fsVfs = new FileSystemVFS(VFS_READABLE);
		fsVfs->Open(filename);
		vfs = fsVfs;
	}
	else {
		char resourcePath[4096];
		ResourceManager::PrefixParentPath(resourcePath, sizeof resourcePath, filename);

		SDLStream* stream = SDLStream::New(resourcePath, SDLStream::READ_ACCESS);
		if (!stream) {
			Log::Print(Log::LOG_ERROR, "Resource file \"%s\" not found!", filename);
			return;
		}

		switch (type) {
		case VFSType::HATCH: {
			HatchVFS *hatchVfs = new HatchVFS(VFS_READABLE);
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
		return;
	}

	LoadedVFS.push_front(vfs);
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
