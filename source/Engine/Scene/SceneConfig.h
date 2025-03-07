#ifndef ENGINE_SCENE_SCENECONFIG_H
#define ENGINE_SCENE_SCENECONFIG_H

#include <Engine/Includes/HashMap.h>

struct SceneListEntry {
	char* Name = nullptr;
	char* Folder = nullptr;
	char* ID = nullptr;
	char* Path = nullptr;
	char* ResourceFolder = nullptr;
	char* Filetype = nullptr;

	HashMap<char*>* Properties = nullptr;

	void Dispose() {
		if (Properties) {
			Properties->WithAll([](Uint32 hash, char* string) -> void {
				Memory::Free(string);
			});

			delete Properties;

			Properties = nullptr;
		}

		// Name, Folder, etc. don't need to be freed because
		// they are contained in Properties.
	}
};

struct SceneListCategory {
	char* Name = nullptr;

	vector<SceneListEntry> Entries;

	HashMap<char*>* Properties = nullptr;

	void Dispose() {
		for (size_t i = 0; i < Entries.size(); i++) {
			Entries[i].Dispose();
		}
		Entries.clear();

		if (Properties) {
			Properties->WithAll([](Uint32 hash, char* string) -> void {
				Memory::Free(string);
			});

			delete Properties;

			Properties = nullptr;
		}

		// Name doesn't need to be freed because it's contained
		// in Properties.
	}
};

#endif /* ENGINE_SCENE_SCENECONFIG_H */
