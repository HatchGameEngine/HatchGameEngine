#ifndef ENGINE_RESOURCETYPES_SCENEFORMATS_HATCHSCENEREADER_H
#define ENGINE_RESOURCETYPES_SCENEFORMATS_HATCHSCENEREADER_H

#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/SceneFormats/HatchSceneTypes.h>
#include <Engine/Scene/SceneReader.h>
#include <Engine/Scene/TileLayer.h>

class HatchSceneReader : public SceneReader {
private:
	Stream* StreamPtr = nullptr;

	Uint8 NumLayers = 0;
	Uint16 NumEntities = 0;

	size_t LayerDataStart = 0;
	size_t EntityDataStart = 0;

	std::vector<SceneClass> SceneClasses;

	TileLayer* ReadLayer();
	void SkipLayer();
	void ReadTileData(TileLayer* layer);
	void ConvertTileData(TileLayer* layer);
	void ReadScrollData(TileLayer* layer);
	SceneClass* FindClass(SceneHash hash);
	SceneClassProperty* FindProperty(SceneClass* scnClass, SceneHash hash);
	static void HashString(char* string, SceneHash* hash);
	void ReadClasses();
	void FreeClasses();
	static bool LoadTileset(const char* parentFolder);
	void ReadEntities();
	void SkipEntityProperties(Uint8 numProps);
	void SkipProperty(Uint8 varType);

public:
	static Uint32 Magic;

	bool Read(Stream* r);
	bool LoadIntoScene(const char* parentFolder);
	void Dispose();
};

#endif /* ENGINE_RESOURCETYPES_SCENEFORMATS_HATCHSCENEREADER_H */
