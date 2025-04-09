#ifndef ENGINE_RESOURCETYPES_SCENEFORMATS_RSDKSCENEREADER_H
#define ENGINE_RESOURCETYPES_SCENEFORMATS_RSDKSCENEREADER_H

#include <Engine/IO/Stream.h>
#include <Engine/Types/Entity.h>

namespace RSDKSceneReader {
//private:
	void LoadObjectList();
	void LoadPropertyList();
	SceneLayer ReadLayer(Stream* r);
	bool LoadTileset(const char* parentFolder);

//public:
	extern Uint32 Magic;
	extern bool Initialized;

	void StageConfig_GetColors(const char* filename);
	void GameConfig_GetColors(const char* filename);
	bool Read(const char* filename, const char* parentFolder);
	bool ReadObjectDefinition(Stream* r, Entity** objSlots, const int maxObjSlots);
	bool Read(Stream* r, const char* parentFolder);
};

#endif /* ENGINE_RESOURCETYPES_SCENEFORMATS_RSDKSCENEREADER_H */
