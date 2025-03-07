#ifndef ENGINE_RESOURCETYPES_SCENEFORMATS_RSDKSCENEREADER_H
#define ENGINE_RESOURCETYPES_SCENEFORMATS_RSDKSCENEREADER_H

#include <Engine/IO/Stream.h>
#include <Engine/Types/Entity.h>

class RSDKSceneReader {
private:
	static void LoadObjectList();
	static void LoadPropertyList();
	static SceneLayer ReadLayer(Stream* r);
	static bool LoadTileset(const char* parentFolder);

public:
	static Uint32 Magic;
	static bool Initialized;

	static void StageConfig_GetColors(const char* filename);
	static void GameConfig_GetColors(const char* filename);
	static bool Read(const char* filename, const char* parentFolder);
	static bool ReadObjectDefinition(Stream* r, Entity** objSlots, const int maxObjSlots);
	static bool Read(Stream* r, const char* parentFolder);
};

#endif /* ENGINE_RESOURCETYPES_SCENEFORMATS_RSDKSCENEREADER_H */
