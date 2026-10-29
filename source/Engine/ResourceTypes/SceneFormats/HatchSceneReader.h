#ifndef ENGINE_RESOURCETYPES_SCENEFORMATS_HATCHSCENEREADER_H
#define ENGINE_RESOURCETYPES_SCENEFORMATS_HATCHSCENEREADER_H

#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/SceneFormats/HatchSceneTypes.h>
#include <Engine/Scene/SceneReader.h>
#include <Engine/Scene/TileLayer.h>

class HatchSceneReader : public SceneReader {
private:
	std::vector<SceneClass> SceneClasses;

	TileLayer* ReadLayer(Stream* r);
	void ReadTileData(Stream* r, TileLayer* layer);
	void ConvertTileData(TileLayer* layer);
	void ReadScrollData(Stream* r, TileLayer* layer);
	SceneClass* FindClass(SceneHash hash);
	SceneClassProperty* FindProperty(SceneClass* scnClass, SceneHash hash);
	static void HashString(char* string, SceneHash* hash);
	void ReadClasses(Stream* r);
	void FreeClasses();
	bool LoadTileset(const char* parentFolder);
	void ReadEntities(Stream* r);
	void SkipEntityProperties(Stream* r, Uint8 numProps);
	void SkipProperty(Stream* r, Uint8 varType);

public:
	static Uint32 Magic;

	bool Read(Stream* r, const char* parentFolder);
};

#endif /* ENGINE_RESOURCETYPES_SCENEFORMATS_HATCHSCENEREADER_H */
