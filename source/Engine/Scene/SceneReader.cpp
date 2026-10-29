#include <Engine/Diagnostics/Log.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/Scene/SceneReader.h>

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
	if (Properties != NULL) {
		if (Scene::Properties == NULL) {
			Scene::Properties = new HashMap<Property>(NULL, 4);
		}

		Properties->WithAll([](Uint32 hash, Property property) -> void {
			Scene::Properties->Put(hash, property);
		});
		Properties->Clear();
	}

	// Add tilesets and tile sprite infos
	for (size_t i = 0; i < Tilesets.size(); i++) {
		Scene::Tilesets.push_back(Tilesets[i]);
	}
	for (size_t i = 0; i < TileSpriteInfos.size(); i++) {
		Scene::TileSpriteInfos.push_back(TileSpriteInfos[i]);
	}

	// Load palettes
	LoadPalettes();

	// Add layers
	for (size_t i = 0; i < Layers.size(); i++) {
		Scene::AddLayer(Layers[i]);
	}
	Layers.clear();

	// Spawn objects
	for (size_t i = 0; i < Objects.size(); i++) {
		SpawnObject(Objects[i]);
	}

	return true;
}

bool SceneReader::LoadPalettes() {
	return true;
}

Entity* SceneReader::SpawnObject(SceneReaderObject* object) {
	ObjectList* objectList = Scene::GetStaticObjectList(object->Name.c_str());
	if (!objectList) {
		return nullptr;
	}

	Entity* entity = Scene::TrySpawnObject(objectList, object->X, object->Y);
	if (!entity) {
		return nullptr;
	}

	Scene::AddStatic(objectList, entity);

	entity->InitProperties();
	object->Properties->WithAll([entity](Uint32 hash, Property property) -> void {
		entity->Properties->Put(hash, property);
	});
	object->Properties->Clear();

	entity->SlotID = object->SlotID + Scene::ReservedSlotIDs;
	entity->Filter = object->Filter;

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

	if (Properties) {
		Properties->ForAll([](Uint32, Property property) -> void {
			Property::Delete(property);
		});
		delete Properties;
	}
}
