#ifndef ENGINE_RESOURCETYPES_SCENEFORMATS_HATCHSCENEREADER_H
#define ENGINE_RESOURCETYPES_SCENEFORMATS_HATCHSCENEREADER_H

#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/SceneFormats/HatchSceneTypes.h>
#include <Engine/Scene/SceneLayer.h>

namespace HatchSceneReader {
//private:
	SceneLayer ReadLayer(Stream* r);
	void ReadTileData(Stream* r, SceneLayer* layer);
	void ConvertTileData(SceneLayer* layer);
	void ReadScrollData(Stream* r, SceneLayer* layer);
	SceneClass* FindClass(SceneHash hash);
	SceneClassProperty* FindProperty(SceneClass* scnClass, SceneHash hash);
	void HashString(char* string, SceneHash* hash);
	void ReadClasses(Stream* r);
	void FreeClasses();
	bool LoadTileset(const char* parentFolder);
	void ReadEntities(Stream* r);
	void SkipEntityProperties(Stream* r, Uint8 numProps);
	void SkipProperty(Stream* r, Uint8 varType);

//public:
	extern Uint32 Magic;

	bool Read(const char* filename, const char* parentFolder);
	bool Read(Stream* r, const char* parentFolder);
};

#endif /* ENGINE_RESOURCETYPES_SCENEFORMATS_HATCHSCENEREADER_H */
