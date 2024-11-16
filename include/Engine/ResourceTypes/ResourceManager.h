#ifndef ENGINE_RESOURCETYPES_RESOURCEMANAGER_H
#define ENGINE_RESOURCETYPES_RESOURCEMANAGER_H

#include <Engine/Includes/Standard.h>
#include <Engine/Includes/HashMap.h>

class ResourceManager {
public:
    static bool      UsingDataFolder;
    static bool      UsingModPack;

    static void PrefixResourcePath(char* out, size_t outSize, const char* path);
    static void PrefixParentPath(char* out, size_t outSize, const char* path);
    static void Init(const char* filename);
    static void Load(const char* filename);
    static bool LoadResource(const char* filename, Uint8** out, size_t* size);
    static bool ResourceExists(const char* filename);
    static void Dispose();
};

#endif /* ENGINE_RESOURCETYPES_RESOURCEMANAGER_H */
