#ifndef ENGINE_RESOURCETYPES_SCENE_SCENEREADER_H
#define ENGINE_RESOURCETYPES_SCENE_SCENEREADER_H

#include <Engine/Includes/HashMap.h>
#include <Engine/Scene/SceneEnums.h>
#include <Engine/Scene/SceneLayer.h>
#include <Engine/Scene/TileSpriteInfo.h>
#include <Engine/Types/Property.h>
#include <Engine/Types/Tileset.h>
#include <Engine/Scene.h>

#include <unordered_set>

class SceneReaderObject {
public:
	std::string Name;
	float X = 0.0f;
	float Y = 0.0f;
	int SlotID = -1;
	int Filter = 0xFF;
	HashMap<Property>* Properties = nullptr;

	SceneReaderObject(std::string name, float x, float y) {
		Name = name;
		X = x;
		Y = y;
		Properties = new HashMap<Property>(nullptr, 4);
	}

	SceneReaderObject(std::string name, float x, float y, HashMap<Property>* properties) {
		Name = name;
		X = x;
		Y = y;
		Properties = properties;
	}

	~SceneReaderObject() {
		Properties->ForAll([](Uint32, Property property) -> void {
			Property::Delete(property);
		});
		delete Properties;
	}
};

class SceneReader {
private:
	Entity* SpawnObject(SceneReaderObject* object, bool dynamic = false, bool keepDynamic = false);
	void AddLayer(SceneLayer* layer);
	void AddLayerDynamic(SceneLayer* layer, bool mergeLayers, bool temporary);
	bool AttemptMergeLayers(SceneLayer* destLayer, SceneLayer* srcLayer);
	void MergeTileLayers(TileLayer* destTileLayer, TileLayer* srcTileLayer);
	void RemapTileIDs(TileLayer* layer, std::vector<int>& tileOffsets);

protected:
	int SceneType;
	HashMap<Property>* Properties;
	std::vector<SceneReaderObject*> Objects;
	std::vector<SceneLayer*> Layers;
	std::vector<Tileset> Tilesets;
	std::vector<TileSpriteInfo> TileSpriteInfos;
	int TileWidth = 16;
	int TileHeight = 16;
	int EmptyTile = 0;
	int PriorityPerLayer = BASE_PRIORITY_PER_LAYER;
	bool RefreshTileAnimations = false;

public:
	int Filter = 0xFF;
	int ReservedSlotIDs = 0;

	static SceneReader* New(Stream* stream, const char* filename);
	virtual bool Read(Stream* stream, const char* parentFolder);
	virtual bool Read(const char* sourceF, const char* parentFolder);
	virtual bool LoadIntoScene();
	void AddPropertiesToScene();
	virtual bool LoadPalettes();
	bool AddToScene(std::vector<std::string>* layersToAdd,
		std::unordered_set<std::string>* objectsToAdd,
		bool loadPalettes,
		bool loadProperties,
		bool keepLayers,
		bool keepObjects,
		bool mergeLayers);
	virtual void Dispose();

	~SceneReader();
};

#endif /* ENGINE_RESOURCETYPES_SCENE_SCENEREADER_H */
