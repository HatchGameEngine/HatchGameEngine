#ifndef ENGINE_RESOURCETYPES_SCENEFORMATS_RSDKSCENEREADER_H
#define ENGINE_RESOURCETYPES_SCENEFORMATS_RSDKSCENEREADER_H

#include <Engine/IO/Stream.h>
#include <Engine/Scene/TileLayer.h>
#include <Engine/Scene/SceneReader.h>
#include <Engine/Types/Entity.h>

class RSDKSceneReader : public SceneReader {
private:
	Stream* StreamPtr = nullptr;

	static void LoadObjectList();
	static void LoadPropertyList();
	TileLayer* ReadLayer();
	static bool LoadTileset(const char* parentFolder);

public:
	static Uint32 Magic;
	static bool Initialized;

	static void StageConfig_GetColors(const char* filename);
	static void GameConfig_GetColors(const char* filename);
	bool ReadObjectDefinition(Entity** objSlots, const int maxObjSlots);

	bool Read(Stream* r);
	bool LoadIntoScene(const char* parentFolder);
};

#endif /* ENGINE_RESOURCETYPES_SCENEFORMATS_RSDKSCENEREADER_H */
