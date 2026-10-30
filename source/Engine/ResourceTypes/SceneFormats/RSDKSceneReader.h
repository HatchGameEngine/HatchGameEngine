#ifndef ENGINE_RESOURCETYPES_SCENEFORMATS_RSDKSCENEREADER_H
#define ENGINE_RESOURCETYPES_SCENEFORMATS_RSDKSCENEREADER_H

#include <Engine/IO/Stream.h>
#include <Engine/Scene/TileLayer.h>
#include <Engine/Scene/SceneLoader.h>
#include <Engine/Types/Entity.h>

class RSDKSceneReader : public SceneLoader {
private:
	SceneLoaderObject** ObjSlots = NULL;

	char Filename16x16Tiles[MAX_RESOURCE_PATH_LENGTH];
	char StageConfigFilename[MAX_RESOURCE_PATH_LENGTH];

	static void LoadObjectList();
	static void LoadPropertyList();

	TileLayer* ReadLayer(Stream* r);
	bool ReadObjectDefinition(Stream* r);
	bool LoadTileset();

	void LoadTilesetPalette();

public:
	static Uint32 Magic;
	static bool Initialized;

	bool Read(Stream* r, const char* parentFolder);
	bool LoadIntoScene();
	bool LoadPalettes();
	void Dispose();

	// Used in StandardLibrary
	static void StageConfig_GetColors(const char* filename);
	static void GameConfig_GetColors(const char* filename);
};

#endif /* ENGINE_RESOURCETYPES_SCENEFORMATS_RSDKSCENEREADER_H */
