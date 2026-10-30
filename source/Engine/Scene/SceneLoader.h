#ifndef ENGINE_RESOURCETYPES_SCENE_SCENELOADER_H
#define ENGINE_RESOURCETYPES_SCENE_SCENELOADER_H

#include <Engine/Includes/HashMap.h>
#include <Engine/Scene/SceneEnums.h>
#include <Engine/Scene/SceneLayer.h>
#include <Engine/Scene/TileSpriteInfo.h>
#include <Engine/Types/ObjectList.h>
#include <Engine/Types/Property.h>
#include <Engine/Types/Tileset.h>
#include <Engine/Scene.h>

#include <unordered_set>

class SceneLoaderObject {
public:
	std::string Name;
	float X = 0.0f;
	float Y = 0.0f;
	int SlotID = -1;
	int Filter = 0xFF;
	HashMap<Property>* Properties = nullptr;

	SceneLoaderObject(std::string name, float x, float y) {
		Name = name;
		X = x;
		Y = y;
		Properties = new HashMap<Property>(nullptr, 4);
	}

	SceneLoaderObject(std::string name, float x, float y, HashMap<Property>* properties) {
		Name = name;
		X = x;
		Y = y;
		Properties = properties;
	}

	~SceneLoaderObject() {
		Properties->ForAll([](Uint32, Property property) -> void {
			Property::Delete(property);
		});
		delete Properties;
	}
};

class SceneLoader {
private:
	std::vector<std::string> ObjectClasses;

	Entity* SpawnObject(SceneLoaderObject* object, float x, float y, bool dynamic = false, bool keepDynamic = false);
	size_t AddLayerToScene(SceneLayer* layer);
	size_t AddLayerToSceneDynamic(SceneLayer* layer, bool mergeLayers, bool temporary, int offsetX, int offsetY);
	bool TryMergeLayerWhenAdding(SceneLayer* srcLayer, SceneLayer* destLayer, int offsetX, int offsetY);
	bool AttemptMergeNonTileLayers(SceneLayer* destLayer, SceneLayer* srcLayer, int offsetX, int offsetY);
	void MergeTileLayers(TileLayer* destTileLayer, TileLayer* srcTileLayer, int offsetX, int offsetY);
	void RemapLayerTileIDs(TileLayer* layer);
	int RemapTileID(int tileID);
	void AddPropertiesToScene();

protected:
	int SceneType;
	HashMap<Property>* Properties;
	std::vector<SceneLoaderObject*> Objects;
	std::vector<SceneLayer*> Layers;
	std::vector<Tileset> Tilesets;
	std::vector<TileSpriteInfo> TileSpriteInfos;
	int EmptyTile = 0;
	int PriorityPerLayer = BASE_PRIORITY_PER_LAYER;
	bool RefreshTileAnimations = false;
	std::vector<int> TileOffsets;
	std::vector<int> LoadedTilesets;

	void AddTilesetToList(Tileset tileset);

public:
	int TileWidth = 16;
	int TileHeight = 16;
	int Filter = 0xFF;
	int ReservedSlotIDs = 0;
	bool UseSceneObjectLists = false;

	static SceneLoader* New(Stream* stream, const char* filename);
	virtual bool Read(Stream* stream, const char* parentFolder);
	virtual bool Read(const char* sourceF, const char* parentFolder);

	bool AddObjectClass(const char* objectName);
	bool HasObjectClass(const char* objectName);
	std::string GetObjectClass(size_t index);
	int GetObjectClassCount();
	bool IsObjectClassValid(size_t index);

	virtual bool LoadIntoScene();
	virtual bool LoadPalettes();

	void LoadTilesets();
	int AddTileset(size_t index);
	int GetLoadedTilesetIndex(size_t index);
	Tileset* GetTileset(size_t index);
	bool IsValidTilesetIndex(size_t index);
	int GetTilesetCount();

	bool AddToScene(bool keepLayers, bool keepObjects, bool mergeLayers,
		std::vector<std::string>* layersToAdd, std::unordered_set<std::string>* objectsToAdd,
		int offsetX, int offsetY, bool loadPalettes, bool loadProperties);

	SceneLoaderObject* GetObject(size_t index);
	int GetObjectCount();
	int GetObjectCount(const char* name);
	Entity* CreateObject(SceneLoaderObject* object, float x, float y);

	SceneLayer* GetLayer(size_t index);
	int GetLayerCount();
	int AddLayer(SceneLayer* layer, bool keepInScene, bool merge, int offsetX, int offsetY);
	bool AddLayerIfCanMerge(SceneLayer* srcLayer, SceneLayer* destLayer, int offsetX, int offsetY);

	virtual void Dispose();

	~SceneLoader();
};

#endif /* ENGINE_RESOURCETYPES_SCENE_SCENELOADER_H */
