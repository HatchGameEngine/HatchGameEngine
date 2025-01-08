#ifndef ENGINE_RESOURCETYPES_RESOURCEMANAGER_H
#define ENGINE_RESOURCETYPES_RESOURCEMANAGER_H

#include <Engine/Includes/Standard.h>
#include <Engine/Includes/HashMap.h>
#include <Engine/TextFormats/INI/INI.h>

struct ModInfo {
    std::string Path;
    std::string ID;
    std::string Name;
    std::string Description;
    std::string Author;
    std::string Version;
    std::string FolderName;
    bool Active;
};

class ResourceManager {
public:
    static bool                 UsingDataFolder;
    static bool                 UsingModPack;
    static std::vector<ModInfo> Mods;
    static INI*                 ModConfig;

    static void PrefixResourcePath(char* out, size_t outSize, const char* path);
    static void PrefixParentPath(char* out, size_t outSize, const char* path);
    static void Init(const char* filename);
    static void Load(const char* filename);
    static bool LoadResource(const char* filename, Uint8** out, size_t* size);
    static bool ResourceExists(const char* filename);
    static void Dispose();
    static void LoadModConfig();
};

#endif /* ENGINE_RESOURCETYPES_RESOURCEMANAGER_H */