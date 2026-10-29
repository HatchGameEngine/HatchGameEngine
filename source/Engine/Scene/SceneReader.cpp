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
		AddLayer(Layers[i]);
	}
	Layers.clear();

	// Spawn objects
	for (size_t i = 0; i < Objects.size(); i++) {
		SpawnObject(Objects[i]);
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

bool SceneReader::AddToScene(
	std::vector<std::string>* layersToAdd,
	std::unordered_set<std::string>* objectsToAdd,
	bool loadPalettes,
	bool loadProperties,
	bool keepLayers,
	bool keepObjects,
	bool mergeLayers) {
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
	std::vector<int> tileOffsets;
	for (size_t i = 0; i < Tilesets.size(); i++) {
		Tileset& otherTileset = Tilesets[i];

		int tileOffset = 0;

		int index = Scene::FindMatchingTileset(&otherTileset);
		if (index == -1) {
			// Add new tileset
			size_t startTile = otherTileset.StartTile;

			tileOffset = (int)(Scene::TileSpriteInfos.size() - startTile);
			otherTileset.StartTile = Scene::TileSpriteInfos.size();

			Scene::AddTilesetDirect(otherTileset);

			for (size_t t = startTile; t < startTile + otherTileset.TileCount; t++) {
				Scene::TileSpriteInfos.push_back(TileSpriteInfos[t]);
			}
		}
		else {
			// Found a matching tileset
			tileOffset = (int)otherTileset.StartTile - (int)Scene::Tilesets[index].StartTile;

			otherTileset.Dispose();
		}

		tileOffsets.push_back(tileOffset);
	}
	Tilesets.clear();

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

				RemapTileIDs((TileLayer*)layer, tileOffsets);
				AddLayerDynamic(layer, mergeLayers, !keepLayers);
			}
		}
	}
	else {
		for (size_t i = 0; i < Layers.size(); i++) {
			SceneLayer* layer = Layers[i];
			RemapTileIDs((TileLayer*)layer, tileOffsets);
			AddLayerDynamic(layer, mergeLayers, !keepLayers);
		}
	}
	Layers.clear();

	// Spawn objects
	for (size_t i = 0; i < Objects.size(); i++) {
		SceneReaderObject* object = Objects[i];

		if (objectsToAdd != nullptr && objectsToAdd->find(object->Name) == objectsToAdd->end()) {
			continue;
		}

		SpawnObject(object, true, keepObjects);
	}

	return true;
}

void SceneReader::AddLayer(SceneLayer* layer) {
	if (layer->Type == SceneLayer::TYPE_TILE) {
		((TileLayer*)layer)->MakeTilesBackup();
	}

	Scene::AddLayer(layer);
}

void SceneReader::AddLayerDynamic(SceneLayer* layer, bool mergeLayers, bool temporary) {
	if (mergeLayers) {
		int index = Scene::FindLayerIndex(layer->Name);
		if (index != -1) {
			SceneLayer* otherLayer = Scene::Layers[index];
			if (AttemptMergeLayers(otherLayer, layer)) {
				delete layer;
				return;
			}
		}
	}

	AddLayer(layer);
	layer->Temporary = temporary;

	if (layer->Type == SceneLayer::TYPE_TILE && Graphics::LayerTileBufferingEnabled) {
		Graphics::MakeLayerTileBuffers(layer);
	}
}

bool SceneReader::AttemptMergeLayers(SceneLayer* destLayer, SceneLayer* srcLayer) {
	if (destLayer->Type != srcLayer->Type) {
		return false;
	}
	else if (destLayer->Type == SceneLayer::TYPE_TILE) {
		SceneReader::MergeTileLayers((TileLayer*)destLayer, (TileLayer*)srcLayer);
		return true;
	}
	else if (destLayer->Type == SceneLayer::TYPE_IMAGE) {
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

void SceneReader::MergeTileLayers(TileLayer* destTileLayer, TileLayer* srcTileLayer) {
	// Delete buffers
	if (Graphics::LayerTileBufferingEnabled) {
		Graphics::DeleteLayerTileBuffers(destTileLayer);
	}

	int width = std::max(destTileLayer->Width, srcTileLayer->Width);
	int height = std::max(destTileLayer->Height, srcTileLayer->Height);

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
			Uint32 tile = srcTileLayer->Tiles[x + (y << srcTileLayer->WidthInBits)];
			if (tile != Scene::EmptyTile) {
				destTileLayer->Tiles[x + (y << destTileLayer->WidthInBits)] = tile;
			}
		}
	}

	destTileLayer->MakeTilesBackup();

	// Copy scroll info
	Memory::Free(destTileLayer->TileBufferIndexes);
	Memory::Free(destTileLayer->ScrollIndexes);
	Memory::Free(destTileLayer->ScrollInfos);

	destTileLayer->TileBufferIndexes = srcTileLayer->TileBufferIndexes;
	destTileLayer->ScrollIndexes = srcTileLayer->ScrollIndexes;
	destTileLayer->ScrollInfos = srcTileLayer->ScrollInfos;

	srcTileLayer->TileBufferIndexes = nullptr;
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

void SceneReader::RemapTileIDs(TileLayer* layer, std::vector<int>& tileOffsets) {
	for (size_t i = 0, iH = 0; i < layer->Height; i++) {
		Uint32* tileRow = &layer->Tiles[iH];

		for (size_t ii = 0; ii < layer->Width; ii++) {
			int tileID = tileRow[ii] & TILE_IDENT_MASK;

			// Remap empty tile
			if (tileID == EmptyTile) {
				tileRow[ii] = Scene::EmptyTile;
				continue;
			}
			else if ((size_t)tileID >= Scene::TileSpriteInfos.size()) {
				// Huh?
				tileRow[ii] = Scene::EmptyTile;
				continue;
			}

			TileSpriteInfo& info = Scene::TileSpriteInfos[tileID];

			tileID += tileOffsets[info.TilesetID];

			tileRow[ii] &= ~TILE_IDENT_MASK;
			tileRow[ii] |= tileID;
		}

		iH += layer->WidthData;
	}
}

Entity* SceneReader::SpawnObject(SceneReaderObject* object, bool dynamic, bool keepDynamic) {
	ObjectList* objectList = Scene::GetStaticObjectList(object->Name.c_str());
	if (!objectList) {
		return nullptr;
	}

	Entity* entity = Scene::TrySpawnObject(objectList, object->X, object->Y);
	if (!entity) {
		return nullptr;
	}

	entity->InitProperties();
	object->Properties->WithAll([entity](Uint32 hash, Property property) -> void {
		entity->Properties->Put(hash, Property::Duplicate(property));
	});

	entity->SlotID = object->SlotID + Scene::ReservedSlotIDs;
	entity->Filter = object->Filter;

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
		Scene::AddStatic(objectList, entity);
	}

	return entity;
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
