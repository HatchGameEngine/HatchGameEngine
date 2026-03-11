#ifndef ENGINE_RESOURCETYPES_RESOURCEMANAGER_H
#define ENGINE_RESOURCETYPES_RESOURCEMANAGER_H

#include <Engine/Filesystem/VFS/VFSProvider.h>
#include <Engine/Filesystem/VFS/VirtualFileSystem.h>
#include <Engine/Includes/Standard.h>

struct DataFileCandidate {
	std::string Path;
	bool Valid;
};

// Resource preloading behaviors.
enum {
	// Do not preload anything.
	PRELOAD_NONE,

	// Use default preloading behavior.
	PRELOAD_DEFAULT,

	// Preload everything.
	PRELOAD_ALL
};

class ResourceManager {
private:
	static std::vector<std::string> DataFilePaths;

	static void InitDataFileList();
	static std::vector<DataFileCandidate> FindDataFiles();
	static char* GetDataFileCandidate(std::vector<DataFileCandidate> candidates);
	static VFSType DetectVFSTypeByFilename(const char* filename);

public:
	static std::vector<std::string> PreloadList;
	static int PreloadResources;
	static bool UsingDataFolder;

	static bool Init(const char* dataFilePath);
	static bool Mount(const char* name,
		const char* filename,
		const char* mountPoint,
		VFSType type,
		Uint16 flags);
	static bool Unmount(const char* name);
	static bool Preload(std::vector<std::string> filenames);
	static VirtualFileSystem* GetVFS();
	static VFSProvider* GetMainResource();
	static void SetMainResourceWritable(bool writable);
	static bool LoadResource(const char* filename, Uint8** out, size_t* size);
	static bool ResourceExists(const char* filename);
	static void Dispose();
};

#endif /* ENGINE_RESOURCETYPES_RESOURCEMANAGER_H */
