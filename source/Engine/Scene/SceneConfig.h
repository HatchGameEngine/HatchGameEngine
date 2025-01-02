#ifndef ENGINE_SCENE_SCENECONFIG_H
#define ENGINE_SCENE_SCENECONFIG_H

#include <Engine/Includes/HashMap.h>

struct SceneListEntry {
    char*           Name = nullptr;
    char*           Folder = nullptr;
    char*           ID = nullptr;
    char*           Path = nullptr;
    char*           ResourceFolder = nullptr;
    char*           Filetype = nullptr;
    int             Filter = 0;

    HashMap<char*>* Properties = nullptr;

    void Dispose() {
        Log::Print(Log::LOG_VERBOSE, "Reaching list entry clear 1");
        if (Properties) {
            Log::Print(Log::LOG_VERBOSE, "Reaching list entry clear 2");
            Properties->WithAll([](Uint32 hash, char* string) -> void {
                Log::Print(Log::LOG_VERBOSE, "Reaching list entry clear 3");
                Memory::Free(string);
            });
            Log::Print(Log::LOG_VERBOSE, "Reaching list entry clear 4");
            delete Properties;
            Log::Print(Log::LOG_VERBOSE, "Reaching list entry clear 5");
            Properties = nullptr;
        }
        Log::Print(Log::LOG_VERBOSE, "Reaching list entry clear 6");

        // Name, Folder, etc. don't need to be freed because they are contained in Properties.
    }
};

struct SceneListCategory {
    char*           Name = nullptr;

    vector<SceneListEntry> Entries;

    HashMap<char*>* Properties = nullptr;

    void Dispose() {
        Log::Print(Log::LOG_VERBOSE, "Reaching category entry clear 1");
        for (size_t i = 0; i < Entries.size(); i++) {
            Entries[i].Dispose();
        }
        Entries.clear();
        Log::Print(Log::LOG_VERBOSE, "Reaching category entry clear 2");

        if (Properties) {
            Properties->WithAll([](Uint32 hash, char* string) -> void {
                Memory::Free(string);
            });

            delete Properties;

            Properties = nullptr;
        }
        Log::Print(Log::LOG_VERBOSE, "Reaching category entry clear 3");

        // Name doesn't need to be freed because it's contained in Properties.
    }
};

#endif /* ENGINE_SCENE_SCENECONFIG_H */
