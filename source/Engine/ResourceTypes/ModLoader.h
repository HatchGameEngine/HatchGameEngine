#ifndef ENGINE_RESOURCETYPES_MODLOADER_H
#define ENGINE_RESOURCETYPES_MODLOADER_H

#include <Engine/Includes/Standard.h>

#define MODLOADER_DIR_NAME "mods"
#define MODLOADER_CONFIG_NAME "modconfig.ini"
#define MODLOADER_INFO_NAME "mod.ini"
#define MODLOADER_RESOURCES_DIR_NAME "Resources"
#define MODLOADER_PACK_PATTERN "*.hatch" // For packed data files

class VFSProvider;

struct ModInfo {
	std::string Path;
	std::string ID;
	std::string Name;
	std::string Description;
	std::string Author;
	std::string Version;
	std::string FolderName;
	bool Active = false;
	bool RedirectSave = false;
	std::map<std::string, std::string> FileMap;
	std::vector<VFSProvider*> Packs;
};

class ModLoader {
public:
	static std::vector<ModInfo> Mods;
	static std::string ModsPath;
	static std::string CustomUserFileDir;

	static void Init();
	static void LoadMods(bool newOnly);
	static bool LoadMod(ModInfo* info, std::string modsPath, std::string folder, bool active);
	static bool ScanModFolder(ModInfo* info);
	static void RescanMods();
	static void OpenPacks(ModInfo* info);
	static void ClosePacks(ModInfo* info);
	static void LoadModSettings();
	static void SaveMods();
	static void UnloadMods();
	static bool FileExists(const char* filename);
	static bool LoadFile(const char* filename, Uint8** out, size_t* size);
	static bool LoadModFile(ModInfo* mod, const char* filename, Uint8** out, size_t* size);
	static void AddFile(ModInfo* mod, const char* filename, std::string path);
	static int GetModCount(bool active);
	static void Dispose();
};

#endif /* ENGINE_RESOURCETYPES_MODLOADER_H */
