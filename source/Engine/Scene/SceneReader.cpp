#include <Engine/Diagnostics/Log.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/SceneFormats/HatchSceneReader.h>
#include <Engine/ResourceTypes/SceneFormats/RSDKSceneReader.h>
#include <Engine/ResourceTypes/SceneFormats/TiledMapReader.h>
#include <Engine/Scene/SceneReader.h>
#include <Engine/Graphics.h>

SceneReader* SceneReader::New(Stream* stream, const char* filename) {
	SceneReader* reader = nullptr;

	// Guess from the header first
	Uint32 magic = stream->ReadUInt32();
	stream->Skip(-4);

	if (magic == HatchSceneReader::Magic) {
		reader = new HatchSceneReader();
	}
	else if (magic == RSDKSceneReader::Magic) {
		reader = new RSDKSceneReader();
	}
	else if (filename != nullptr) {
		// Guess from the filename
		if (StringUtils::StrCaseStr(filename, ".bin")) {
			reader = new RSDKSceneReader();
		}
		else if (StringUtils::StrCaseStr(filename, ".hcsn")) {
			reader = new HatchSceneReader();
		}
	}

	// Assume it's a Tiled map
	if (reader == nullptr) {
		reader = new TiledMapReader();
	}

	return reader;
}

bool SceneReader::Read(Stream* stream, const char* parentFolder) {
	throw std::runtime_error("SceneReader::Read() called without an implementation");
}

bool SceneReader::Read(const char* filename, const char* parentFolder) {
	Stream* stream = ResourceStream::New(filename);
	if (!stream) {
		Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", filename);
		return false;
	}

	bool result = Read(stream, parentFolder);

	stream->Close();

	return result;
}

bool SceneReader::LoadIntoScene() {
	Scene::SceneType = SceneType;

	Scene::TileWidth = TileWidth;
	Scene::TileHeight = TileHeight;
	Scene::EmptyTile = EmptyTile;

	Scene::FreePriorityLists();
	Scene::PriorityPerLayer = PriorityPerLayer;
	Scene::InitPriorityLists();

	if (RefreshTileAnimations) {
		Scene::RefreshTileAnimations = true;
	}

	// Add properties
	AddPropertiesToScene();

	// Add tilesets and tile sprite infos
	Scene::Tilesets.insert(Scene::Tilesets.end(),
		std::make_move_iterator(Tilesets.begin()),
		std::make_move_iterator(Tilesets.end()));
	Tilesets.clear();

	Scene::TileSpriteInfos.insert(Scene::TileSpriteInfos.end(),
		std::make_move_iterator(TileSpriteInfos.begin()),
		std::make_move_iterator(TileSpriteInfos.end()));
	TileSpriteInfos.clear();

	// Load palettes
	LoadPalettes();

	// Add layers
	for (size_t i = 0; i < Layers.size(); i++) {
		AddLayerToScene(Layers[i]);
	}
	Layers.clear();

	// Spawn objects
	for (size_t i = 0; i < Objects.size(); i++) {
		SceneReaderObject* object = Objects[i];

		float spawnX = object->X;
		float spawnY = object->Y;

		SpawnObject(Objects[i], spawnX, spawnY);
	}

	return true;
}

void SceneReader::AddPropertiesToScene() {
	if (Properties == NULL) {
		return;
	}

	if (Scene::Properties == NULL) {
		Scene::Properties = new HashMap<Property>(NULL, 4);
	}

	Properties->WithAll([](Uint32 hash, Property property) -> void {
		Scene::Properties->Put(hash, Property::Duplicate(property));
	});
}

bool SceneReader::LoadPalettes() {
	return true;
}

bool SceneReader::AddToScene(bool keepLayers, bool keepObjects, bool mergeLayers,
	std::vector<std::string>* layersToAdd, std::unordered_set<std::string>* objectsToAdd,
	int offsetX, int offsetY, bool loadPalettes, bool loadProperties) {
	if (TileWidth != Scene::TileWidth || TileHeight != Scene::TileHeight) {
		return false;
	}

	if (RefreshTileAnimations) {
		Scene::RefreshTileAnimations = true;
	}

	// Add properties
	if (loadProperties) {
		AddPropertiesToScene();
	}

	// Add tilesets and tile sprite infos
	LoadTilesets();

	// Load palettes
	if (loadPalettes) {
		LoadPalettes();
	}

	// Add layers
	if (layersToAdd != nullptr) {
		for (size_t i = 0; i < layersToAdd->size(); i++) {
			for (size_t l = 0; l < Layers.size(); l++) {
				SceneLayer* layer = Layers[i];

				if (strcmp(layer->Name, (*layersToAdd)[i].c_str()) != 0) {
					continue;
				}

				AddLayerToSceneDynamic(layer, mergeLayers, !keepLayers, offsetX, offsetY);
			}
		}
	}
	else {
		for (size_t i = 0; i < Layers.size(); i++) {
			AddLayerToSceneDynamic(Layers[i], mergeLayers, !keepLayers, offsetX, offsetY);
		}
	}
	Layers.clear();

	// Spawn objects
	for (size_t i = 0; i < Objects.size(); i++) {
		SceneReaderObject* object = Objects[i];

		if (objectsToAdd != nullptr && objectsToAdd->find(object->Name) == objectsToAdd->end()) {
			continue;
		}

		float spawnX = object->X + (offsetX * TileWidth);
		float spawnY = object->Y + (offsetY * TileHeight);

		SpawnObject(object, spawnX, spawnY, true, keepObjects);
	}

	return true;
}

void SceneReader::LoadTilesets() {
	for (size_t i = 0; i < Tilesets.size(); i++) {
		AddTileset(i);
	}
}
int SceneReader::AddTileset(size_t index) {
	int found = GetLoadedTilesetIndex(index);
	if (found != -1) {
		return found;
	}

	int tileOffset = 0;

	Tileset& otherTileset = Tilesets[index];
	found = Scene::FindMatchingTileset(&otherTileset);

	if (found == -1) {
		// Add new tileset
		found = Scene::Tilesets.size();

		size_t startTile = otherTileset.StartTile;
		tileOffset = (int)Scene::TileSpriteInfos.size() - startTile;
		otherTileset.StartTile = Scene::TileSpriteInfos.size();

		for (size_t t = startTile; t < startTile + otherTileset.TileCount; t++) {
			TileSpriteInfo info = TileSpriteInfos[t];
			info.TilesetID = found;
			Scene::TileSpriteInfos.push_back(info);
		}

		Scene::AddTilesetDirect(otherTileset);
	}
	else {
		// Found a matching tileset
		tileOffset = (int)Scene::Tilesets[found].StartTile - (int)otherTileset.StartTile;

		otherTileset.Dispose();
	}

	TileOffsets[index] = tileOffset;
	LoadedTilesets[index] = found;

	return found;
}
int SceneReader::GetLoadedTilesetIndex(size_t index) {
	if (index >= 0 && index < Tilesets.size()) {
		return LoadedTilesets[index];
	}

	return -1;
}
Tileset* SceneReader::GetTileset(size_t index) {
	if (index < 0 || index >= Tilesets.size()) {
		return nullptr;
	}

	return &Tilesets[index];
}
bool SceneReader::IsValidTilesetIndex(size_t index) {
	return index >= 0 && index < (int)Tilesets.size();
}
int SceneReader::GetTilesetCount() {
	return (int)Tilesets.size();
}

size_t SceneReader::AddLayerToScene(SceneLayer* layer) {
	if (layer->Type == SceneLayer::TYPE_TILE) {
		((TileLayer*)layer)->MakeTilesBackup();
	}

	return Scene::AddLayer(layer);
}
size_t SceneReader::AddLayerToSceneDynamic(SceneLayer* layer, bool mergeLayers, bool temporary, int offsetX, int offsetY) {
	if (mergeLayers) {
		int index = Scene::FindLayerIndex(layer->Name);
		if (index != -1 && TryMergeLayerWhenAdding(layer, Scene::Layers[index], offsetX, offsetY)) {
			return (size_t)index;
		}
	}

	size_t index = AddLayerToScene(layer);
	layer->OffsetX -= offsetX * TileWidth;
	layer->OffsetY -= offsetY * TileHeight;
	layer->Temporary = temporary;

	if (layer->Type == SceneLayer::TYPE_TILE && Graphics::LayerTileBufferingEnabled) {
		Graphics::MakeLayerTileBuffers(layer);
	}

	return index;
}

bool SceneReader::AttemptMergeNonTileLayers(SceneLayer* destLayer, SceneLayer* srcLayer, int offsetX, int offsetY) {
	if (destLayer->Type == SceneLayer::TYPE_IMAGE) {
		ImageLayer* destImageLayer = (ImageLayer*)destLayer;
		if (!destImageLayer->ImagePtr) {
			return false;
		}

		ImageLayer* srcImageLayer = (ImageLayer*)srcLayer;
		if (!srcImageLayer->ImagePtr) {
			return false;
		}

		if (strcmp(destImageLayer->ImagePtr->Filename, srcImageLayer->ImagePtr->Filename) == 0) {
			return true;
		}
	}

	return false;
}
void SceneReader::MergeTileLayers(TileLayer* destTileLayer, TileLayer* srcTileLayer, int offsetX, int offsetY) {
	// Delete buffers
	if (Graphics::LayerTileBufferingEnabled) {
		Graphics::DeleteLayerTileBuffers(destTileLayer);
	}

	int width = std::max(destTileLayer->Width, srcTileLayer->Width + offsetX);
	int height = std::max(destTileLayer->Height, srcTileLayer->Height + offsetY);

	// Resize layer to the largest of the two
	if (width != destTileLayer->Width || height != destTileLayer->Height) {
		if (destTileLayer->TilesBackup) {
			Memory::Free(destTileLayer->TilesBackup);
			destTileLayer->TilesBackup = nullptr;
		}

		destTileLayer->Resize(width, height);
	}

	// Copy non-empty tiles
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int destX = x + offsetX;
			int destY = y + offsetY;
			if (destX >= 0 && destX < width && destY >= 0 && destY < height) {
				Uint32 tile = srcTileLayer->Tiles[x + (y << srcTileLayer->WidthInBits)];
				if (tile == EmptyTile) {
					continue;
				}

				int tileID = RemapTileID(tile & TILE_IDENT_MASK);
				tile &= ~TILE_IDENT_MASK;
				tile |= tileID;

				if (tile != Scene::EmptyTile) {
					destTileLayer->Tiles[destX + (destY << destTileLayer->WidthInBits)] = tile;
				}
			}
		}
	}

	destTileLayer->MakeTilesBackup();

	// Copy scroll info
	Memory::Free(destTileLayer->ScrollIndexes);
	Memory::Free(destTileLayer->ScrollInfos);

	destTileLayer->ScrollIndexes = srcTileLayer->ScrollIndexes;
	destTileLayer->ScrollInfos = srcTileLayer->ScrollInfos;

	srcTileLayer->ScrollIndexes = nullptr;
	srcTileLayer->ScrollInfos = nullptr;

	destTileLayer->ScrollIndexCount = srcTileLayer->ScrollIndexCount;
	destTileLayer->DeformOffsetA = srcTileLayer->DeformOffsetA;
	destTileLayer->DeformOffsetB = srcTileLayer->DeformOffsetB;
	memcpy(destTileLayer->DeformSetA, srcTileLayer->DeformSetA, sizeof(destTileLayer->DeformSetA));
	memcpy(destTileLayer->DeformSetB, srcTileLayer->DeformSetB, sizeof(destTileLayer->DeformSetB));
	destTileLayer->DeformSplitLine = srcTileLayer->DeformSplitLine;

	// Remake buffers
	if (Graphics::LayerTileBufferingEnabled) {
		Graphics::MakeLayerTileBuffers(destTileLayer);
	}
}
void SceneReader::RemapLayerTileIDs(TileLayer* layer) {
	for (size_t i = 0, iH = 0; i < layer->Height; i++) {
		Uint32* tileRow = &layer->Tiles[iH];

		for (size_t ii = 0; ii < layer->Width; ii++) {
			int tileID = tileRow[ii] & TILE_IDENT_MASK;
			tileRow[ii] &= ~TILE_IDENT_MASK;
			tileRow[ii] |= RemapTileID(tileID);
		}

		iH += layer->WidthData;
	}
}
int SceneReader::RemapTileID(int tileID) {
	// Remap empty or invalid tile
	if (tileID == EmptyTile || tileID >= (int)TileSpriteInfos.size()) {
		return Scene::EmptyTile;
	}

	TileSpriteInfo& info = TileSpriteInfos[tileID];

	// Tileset wasn't loaded
	if (LoadedTilesets[info.TilesetID] == -1) {
		return Scene::EmptyTile;
	}

	return tileID + TileOffsets[info.TilesetID];
}

SceneReaderObject* SceneReader::GetObject(size_t index) {
	if (index < 0 || index >= (int)Objects.size()) {
		return nullptr;
	}

	return Objects[index];
}
int SceneReader::GetObjectCount() {
	return (int)Objects.size();
}
int SceneReader::GetObjectCount(const char* name) {
	int count = 0;

	for (size_t i = 0; i < Objects.size(); i++) {
		SceneReaderObject* object = Objects[i];

		if (strcmp(object->Name.c_str(), name) == 0) {
			count++;
		}
	}

	return count;
}

Entity* SceneReader::CreateObject(SceneReaderObject* object, float x, float y) {
	ObjectList* objectList = Scene::GetStaticObjectList(object->Name.c_str());
	if (objectList == nullptr) {
		return nullptr;
	}

	Entity* entity = Scene::TrySpawnObject(objectList, x, y);
	if (entity == nullptr) {
		return nullptr;
	}

	entity->InitProperties();
	object->Properties->WithAll([entity](Uint32 hash, Property property) -> void {
		entity->Properties->Put(hash, Property::Duplicate(property));
	});

	entity->SlotID = object->SlotID;
	entity->Filter = object->Filter;

	return entity;
}

Entity* SceneReader::SpawnObject(SceneReaderObject* object, float x, float y, bool dynamic, bool keepDynamic) {
	Entity* entity = CreateObject(object, x, y);
	if (entity == nullptr) {
		return nullptr;
	}

	if (dynamic) {
		// Call the initializer
		entity->Initialize();

		// Add it to the scene
		if (keepDynamic) {
			Scene::AddStatic(entity->List, entity);
		}
		else {
			Scene::AddDynamic(entity->List, entity);

			// Add to proper list
			if (entity->List) {
				entity->List->Add(entity);
			}
		}

		entity->Create();
		if (!Scene::Initializing) {
			entity->PostCreate();
		}
	}
	else {
		Scene::AddStatic(entity->List, entity);
	}

	return entity;
}

SceneLayer* SceneReader::GetLayer(size_t index) {
	if (index < 0 || index >= (int)Layers.size()) {
		return nullptr;
	}

	return Layers[index];
}
int SceneReader::GetLayerCount() {
	return (int)Layers.size();
}
int SceneReader::AddLayer(SceneLayer* layer, bool keepInScene, bool merge, int offsetX, int offsetY) {
	if (merge) {
		int index = Scene::FindLayerIndex(layer->Name);
		if (index != -1 && TryMergeLayerWhenAdding(layer, Scene::Layers[index], offsetX, offsetY)) {
			return index;
		}
	}

	SceneLayer* copy = layer->Duplicate();

	if (layer->Type == SceneLayer::TYPE_TILE) {
		RemapLayerTileIDs((TileLayer*)copy);
	}

	return AddLayerToSceneDynamic(copy, false, !keepInScene, offsetX, offsetY);
}
bool SceneReader::AddLayerIfCanMerge(SceneLayer* srcLayer, SceneLayer* destLayer, int offsetX, int offsetY) {
	return TryMergeLayerWhenAdding(srcLayer, destLayer, offsetX, offsetY);
}
bool SceneReader::TryMergeLayerWhenAdding(SceneLayer* srcLayer, SceneLayer* destLayer, int offsetX, int offsetY) {
	if (destLayer->Type != srcLayer->Type) {
		return false;
	}

	if (srcLayer->Type == SceneLayer::TYPE_TILE) {
		MergeTileLayers((TileLayer*)destLayer, (TileLayer*)srcLayer, offsetX, offsetY);
	}
	else if (!AttemptMergeNonTileLayers(destLayer, srcLayer, offsetX, offsetY)) {
		return false;
	}

	return true;
}

void SceneReader::AddTilesetToList(Tileset tileset) {
	Tilesets.push_back(tileset);
	TileOffsets.push_back(0);
	LoadedTilesets.push_back(-1);
}

void SceneReader::Dispose() {}

SceneReader::~SceneReader() {
	Dispose();

	for (size_t i = 0; i < Layers.size(); i++) {
		delete Layers[i];
	}

	for (size_t i = 0; i < Objects.size(); i++) {
		delete Objects[i];
	}

	for (size_t i = 0; i < Tilesets.size(); i++) {
		Tilesets[i].Dispose();
	}

	if (Properties) {
		Properties->ForAll([](Uint32, Property property) -> void {
			Property::Delete(property);
		});
		delete Properties;
	}
}
