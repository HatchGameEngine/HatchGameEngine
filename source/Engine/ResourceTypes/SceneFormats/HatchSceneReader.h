#ifndef ENGINE_RESOURCETYPES_SCENEFORMATS_HATCHSCENEREADER_H
#define ENGINE_RESOURCETYPES_SCENEFORMATS_HATCHSCENEREADER_H

#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/SceneFormats/HatchSceneTypes.h>
#include <Engine/Scene/TileLayer.h>

class HatchSceneReader {
private:
	static TileLayer* ReadLayer(Scene* scene, Stream* r);
	static void ReadTileData(Stream* r, TileLayer* layer);
	static void ConvertTileData(Scene* scene, TileLayer* layer);
	static void ReadScrollData(Stream* r, TileLayer* layer);
	static SceneClass* FindClass(SceneHash hash);
	static SceneClassProperty* FindProperty(SceneClass* scnClass, SceneHash hash);
	static void HashString(char* string, SceneHash* hash);
	static void ReadClasses(Stream* r);
	static void FreeClasses();
	static bool LoadTileset(Scene* scene, const char* parentFolder);
	static void ReadEntities(Scene* scene, Stream* r);
	static void SkipEntityProperties(Stream* r, Uint8 numProps);
	static void SkipProperty(Stream* r, Uint8 varType);

public:
	static Uint32 Magic;

	static bool Read(Scene* scene, const char* filename, const char* parentFolder);
	static bool Read(Scene* scene, Stream* r, const char* parentFolder);
};

#endif /* ENGINE_RESOURCETYPES_SCENEFORMATS_HATCHSCENEREADER_H */
