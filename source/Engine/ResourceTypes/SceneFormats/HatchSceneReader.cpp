#include <Engine/ResourceTypes/SceneFormats/HatchSceneReader.h>
#include <Engine/ResourceTypes/SceneFormats/HatchSceneTypes.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Error.h>
#include <Engine/Hashing/CRC32.h>
#include <Engine/Hashing/MD5.h>
#include <Engine/Scene.h>
#include <Engine/Scene/SceneLayer.h>
#include <Engine/Scene/TileLayer.h>
#include <Engine/Types/Entity.h>

Uint32 HatchSceneReader::Magic = 0x4E435348; // HSCN

#define HSCN_EMPTY_TILE 0xFFFFFFFFU

#define HSCN_COLLA_MASK 0x0000C000U
#define HSCN_COLLB_MASK 0x00030000U
#define HSCN_FLIPX_MASK 0x00001000U
#define HSCN_FLIPY_MASK 0x00002000U
#define HSCN_IDENT_MASK 0x00000FFFU // Max. 4096 tiles
#define HSCN_FXYID_MASK (HSCN_FLIPY_MASK | HSCN_FLIPY_MASK | HSCN_IDENT_MASK)

bool HatchSceneReader::Read(Stream* r) {
	// Start reading
	if (r->ReadUInt32() != HatchSceneReader::Magic) {
		Log::Print(Log::LOG_ERROR, "Not a Hatch scene!");
		return false;
	}

	Scene::SceneType = SCENETYPE_HATCH;

	// Read scene version
	Uint8 verMajor = r->ReadByte();
	Uint8 verMinor = r->ReadByte();
	Uint8 verPatch = r->ReadUInt16();

	Log::Print(Log::LOG_VERBOSE, "Scene version: %d.%d.%d", verMajor, verMinor, verPatch);

	if (verMajor != 0) {
		Log::Print(Log::LOG_ERROR,
			"Unrecognized scene version %d.%d.%d!",
			verMajor,
			verMinor,
			verPatch);
		return false;
	}

	StreamPtr = r;

	r->ReadUInt32(); // Editor background color 1
	r->ReadUInt32(); // Editor background color 2
	if (verPatch >= 1) {
		r->ReadByte(); // completely unused value
	}

	// Unused (number of kits)
	r->ReadByte();

	// Read layer count
	NumLayers = r->ReadByte();

	LayerDataStart = r->Position();
	for (Uint32 i = 0; i < NumLayers; i++) {
		SkipLayer();
	}

	// Read classes
	ReadClasses();

	// Read entity count
	NumEntities = r->ReadUInt16();
	EntityDataStart = r->Position();

	return true;
}

bool HatchSceneReader::LoadIntoScene(const char* parentFolder) {
	Stream* r = StreamPtr;

	// Load the tileset
	HatchSceneReader::LoadTileset(parentFolder);

	Scene::FreePriorityLists();
	Scene::PriorityPerLayer = BASE_PRIORITY_PER_LAYER;
	Scene::InitPriorityLists();

	// Read layers
	StreamPtr->Seek(LayerDataStart);
	for (Uint32 i = 0; i < NumLayers; i++) {
		TileLayer* layer = ReadLayer();

#ifdef HSCN_READER_DEBUG
		Log::Print(Log::LOG_VERBOSE,
			"Layer %d (%s): %dx%d",
			i,
			layer->Name,
			layer->Width,
			layer->Height);
#endif

		Scene::AddLayer(layer);
	}

	// Read entities
	StreamPtr->Seek(EntityDataStart);
	ReadEntities();

	return true;
}

TileLayer* HatchSceneReader::ReadLayer() {
	Stream* r = StreamPtr;

	char* name = r->ReadHeaderedString();
	Uint8 drawBehavior = r->ReadByte();
	Uint8 drawGroup = r->ReadByte();
	Uint16 width = r->ReadUInt16();
	Uint16 height = r->ReadUInt16();
	Uint16 relativeY = r->ReadInt16();
	Uint16 constantY = r->ReadInt16();

	TileLayer* layer = new TileLayer(width, height);
	layer->Name = name;
	layer->Flags = SceneLayer::FLAGS_COLLIDEABLE;
	layer->Visible = true;

	// Set draw group and draw behavior
	if (drawGroup & 0x10) {
		drawGroup &= 0xF;
		layer->Visible = false;
	}

	layer->DrawGroup = drawGroup;
	layer->DrawBehavior = drawBehavior;

	layer->RelativeY = (float)relativeY / 0x100;
	layer->ConstantY = (float)constantY / 0x100;

	layer->ScrollInfoCount = r->ReadUInt16();

	if (layer->ScrollInfoCount > 0) {
		layer->ScrollInfos = (ScrollingInfo*)Memory::Malloc(
			layer->ScrollInfoCount * sizeof(ScrollingInfo));

		if (!layer->ScrollInfos) {
			Error::Fatal("Out of memory in HatchSceneReader::ReadLayer!");
		}

		layer->UsingScrollIndexes = true;
	}
	else {
		layer->ScrollInfos = nullptr;
		layer->UsingScrollIndexes = false;
	}

	// Read scroll data
	ReadScrollData(layer);

	// Read and convert tile data
	ReadTileData(layer);
	ConvertTileData(layer);

	layer->MakeTilesBackup();

	return layer;
}

void HatchSceneReader::SkipLayer() {
	Stream* r = StreamPtr;

	r->SkipHeaderedString(); // Name
	r->Skip(10);

	Uint16 scrollInfoCount = r->ReadUInt16();

	// Skip scroll info data
	for (Uint16 i = 0; i < scrollInfoCount; i++) {
		r->Skip(6);
	}

	// Skip scroll data
	Uint32 compressedSize = r->ReadUInt32();

	// This should skip (4 + (compressedSize - 4)) bytes, but the math cancels out
	r->Skip(compressedSize);

	// Skip tile data
	compressedSize = r->ReadUInt32();

	// See above
	r->Skip(compressedSize);
}

void HatchSceneReader::ReadTileData(TileLayer* layer) {
	Stream* r = StreamPtr;

	size_t streamPos = r->Position();

	r->ReadUInt32(); // compressed size
	Uint32 layerUncompSize = r->ReadUInt32BE();

	r->Seek(streamPos);

	Uint32 dataSize = layer->DataSize;
	if (layerUncompSize > dataSize) {
		Log::Print(Log::LOG_WARN,
			"Layer has more stored tile data (%u) than allocated (%u)\n",
			layerUncompSize,
			dataSize);
	}

	r->ReadCompressed(layer->Tiles, dataSize);
}

void HatchSceneReader::ConvertTileData(TileLayer* layer) {
	for (size_t i = 0; i < (size_t)layer->Width * layer->Height; i++) {
		if (layer->Tiles[i] == HSCN_EMPTY_TILE) {
			layer->Tiles[i] = Scene::EmptyTile;
			continue;
		}

		Uint32 tileID = (layer->Tiles[i] & HSCN_FXYID_MASK);
		Uint32 tile = tileID & 0xFFF;
		if (tile >= Scene::TileSpriteInfos.size()) {
			layer->Tiles[i] = Scene::EmptyTile;
			continue;
		}

		if (layer->Tiles[i] & HSCN_FLIPX_MASK) {
			tile |= TILE_FLIPX_MASK;
		}
		if (layer->Tiles[i] & HSCN_FLIPY_MASK) {
			tile |= TILE_FLIPY_MASK;
		}

		Uint32 collA = (layer->Tiles[i] & HSCN_COLLA_MASK) >> 14;
		Uint32 collB = (layer->Tiles[i] & HSCN_COLLB_MASK) >> 16;

		tile |= (collA << 28);
		tile |= (collB << 26);

		layer->Tiles[i] = tile;
	}
}

void HatchSceneReader::ReadScrollData(TileLayer* layer) {
	Stream* r = StreamPtr;

	for (int i = 0; i < layer->ScrollInfoCount; i++) {
		ScrollingInfo* info = &layer->ScrollInfos[i];

		Sint16 relativeParallax = r->ReadInt16();
		Sint16 constantParallax = r->ReadInt16();

		info->RelativeParallax = (float)relativeParallax / 0x100;
		info->ConstantParallax = (float)constantParallax / 0x100;
		info->CanDeform = (bool)r->ReadByte();

		info->Offset = 0.0f;

		r->ReadByte(); // ?
	}

	size_t streamPos = r->Position();

	r->ReadUInt32(); // compressed size
	Uint32 layerUncompSize = r->ReadUInt32BE();

	r->Seek(streamPos);

	Uint32 dataSize = layer->ScrollIndexCount * 16;
	if (layerUncompSize > dataSize) {
		Log::Print(Log::LOG_WARN,
			"Layer has more stored scroll indexes (%u) than allocated (%u)\n",
			layerUncompSize,
			dataSize);
	}

	layer->ScrollIndexes = (Uint8*)Memory::Malloc(dataSize);
	r->ReadCompressed(layer->ScrollIndexes, dataSize);
}

SceneClass* HatchSceneReader::FindClass(SceneHash hash) {
	for (size_t i = 0; i < SceneClasses.size(); i++) {
		if (SceneClasses[i].Hash == hash) {
			return &SceneClasses[i];
		}
	}

	return NULL;
}

SceneClassProperty* HatchSceneReader::FindProperty(SceneClass* scnClass, SceneHash hash) {
	for (size_t i = 0; i < scnClass->Properties.size(); i++) {
		if (scnClass->Properties[i].Hash == hash) {
			return &scnClass->Properties[i];
		}
	}

	return NULL;
}

void HatchSceneReader::HashString(char* string, SceneHash* hash) {
	Uint8 final[16];

	MD5::EncryptString(final, string);

	hash->A = final[0] + (final[1] << 8) + (final[2] << 16) + (final[3] << 24);
	hash->B = final[4] + (final[5] << 8) + (final[6] << 16) + (final[7] << 24);
	hash->C = final[8] + (final[9] << 8) + (final[10] << 16) + (final[11] << 24);
	hash->D = final[12] + (final[13] << 8) + (final[14] << 16) + (final[15] << 24);
}

void HatchSceneReader::ReadClasses() {
	Stream* r = StreamPtr;

	Uint16 numClasses = r->ReadUInt16();

	for (Uint16 i = 0; i < numClasses; i++) {
		char* className = r->ReadHeaderedString();
		Uint8 numProps = r->ReadByte();

		SceneClass scnClass;
		scnClass.Name = className;
		scnClass.Properties.clear();

		HatchSceneReader::HashString(className, &scnClass.Hash); // Create hash

#ifdef HSCN_READER_DEBUG
		Log::Print(Log::LOG_VERBOSE, "Class: %s", className);
		Log::Print(Log::LOG_VERBOSE,
			"  Hash: %08x%08x%08x%08x",
			scnClass.Hash.A,
			scnClass.Hash.B,
			scnClass.Hash.C,
			scnClass.Hash.D);
		Log::Print(Log::LOG_VERBOSE, "  Properties: %u", numProps);
#endif

		for (Uint8 j = 0; j < numProps; j++) {
			char* propName = r->ReadHeaderedString();
			Uint8 propType = r->ReadByte();

			SceneClassProperty prop;
			prop.Name = propName;
			prop.Type = propType;

			HatchSceneReader::HashString(propName, &prop.Hash); // Create hash

#ifdef HSCN_READER_DEBUG
			static const char* propTypeNames[] = {"uint8",
				"uint16",
				"uint32",
				"int8",
				"int16",
				"int32",
				"enum",
				"bool",
				"string",
				"vector2",
				"unknown",
				"color"};

			Log::Print(Log::LOG_VERBOSE, "    Name: %s", propName);
			Log::Print(Log::LOG_VERBOSE, "    Type: %s", propTypeNames[prop.Type]);
			Log::Print(Log::LOG_VERBOSE,
				"    Hash: %08x%08x%08x%08x",
				prop.Hash.A,
				prop.Hash.B,
				prop.Hash.C,
				prop.Hash.D);
#endif

			scnClass.Properties.push_back(prop);
		}

		SceneClasses.push_back(scnClass);
	}
}

void HatchSceneReader::FreeClasses() {
	for (size_t i = 0; i < SceneClasses.size(); i++) {
		SceneClass* scnClass = &SceneClasses[i];
		Memory::Free(scnClass->Name);
		for (size_t j = 0; j < scnClass->Properties.size(); j++) {
			Memory::Free(scnClass->Properties[j].Name);
		}
	}

	SceneClasses.clear();
}

bool HatchSceneReader::LoadTileset(const char* parentFolder) {
	int curTileCount = (int)Scene::TileSpriteInfos.size();

	char tilesetFile[MAX_RESOURCE_PATH_LENGTH];
	snprintf(tilesetFile, sizeof(tilesetFile), "%s/Tileset.png", parentFolder);

	ISprite* tileSprite = new ISprite();
	Texture* spriteSheet = tileSprite->AddSpriteSheet(tilesetFile);
	if (!spriteSheet) {
		delete tileSprite;
		return false;
	}

	int cols = spriteSheet->Width / Scene::TileWidth;
	int rows = spriteSheet->Height / Scene::TileHeight;

	tileSprite->ReserveAnimationCount(1);
	tileSprite->AddAnimation("TileSprite", 0, 0, cols * rows);

	// Add tiles
	TileSpriteInfo info;
	for (int i = 0; i < cols * rows; i++) {
		info.Sprite = tileSprite;
		info.AnimationIndex = 0;
		info.FrameIndex = (int)tileSprite->Animations[0].Frames.size();
		info.TilesetID = Scene::Tilesets.size();
		Scene::TileSpriteInfos.push_back(info);

		tileSprite->AddFrame(0,
			(i % cols) * Scene::TileWidth,
			(i / cols) * Scene::TileHeight,
			Scene::TileWidth,
			Scene::TileHeight,
			-Scene::TileWidth / 2,
			-Scene::TileHeight / 2);
	}

	Scene::EmptyTile = Scene::TileSpriteInfos.size();

	// Add empty tile
	info.Sprite = tileSprite;
	info.AnimationIndex = 0;
	info.FrameIndex = (int)tileSprite->Animations[0].Frames.size();
	info.TilesetID = Scene::Tilesets.size();
	Scene::TileSpriteInfos.push_back(info);

	tileSprite->AddFrame(0, 0, 0, 1, 1, 0, 0);

	tileSprite->RefreshGraphicsID();

	Tileset sceneTileset(tileSprite,
		Scene::TileWidth,
		Scene::TileHeight,
		0,
		curTileCount,
		Scene::TileSpriteInfos.size(),
		tilesetFile);
	Scene::Tilesets.push_back(sceneTileset);

	return true;
}

void HatchSceneReader::ReadEntities() {
	Stream* r = StreamPtr;

	for (Uint16 i = 0; i < NumEntities; i++) {
		SceneHash classHash;
		classHash.A = r->ReadUInt32();
		classHash.B = r->ReadUInt32();
		classHash.C = r->ReadUInt32();
		classHash.D = r->ReadUInt32();

		float posX = r->ReadInt32() / 65536.0;
		float posY = r->ReadInt32() / 65536.0;
		Uint8 filter = r->ReadByte();
		Uint8 numProps = r->ReadByte();

		if (classHash.A == 0x19191919 && classHash.B == 0x29292929 &&
			classHash.C == 0x39393939 && classHash.D == 0x49494949) {
#ifdef HSCN_READER_DEBUG
			Log::Print(Log::LOG_WARN, "Ignoring entity with an unknown class");
#endif
			SkipEntityProperties(numProps);
			continue;
		}

		// Find the class from its hash
		SceneClass* scnClass = HatchSceneReader::FindClass(classHash);
		if (!scnClass) {
#ifdef HSCN_READER_DEBUG
			Log::Print(Log::LOG_WARN,
				"Could not find any class with hash %08x%08x%08x%08x",
				classHash.A,
				classHash.B,
				classHash.C,
				classHash.D);
#endif
			SkipEntityProperties(numProps);
			continue;
		}

		// Get the correct object list from the class name
		Uint32 objectHash = CRC32::EncryptData(&classHash.A, 16);
		char* objectName = scnClass->Name;

		if (!filter) {
			filter = 0xFF;
		}

		if (!(filter & Scene::Filter)) {
			SkipEntityProperties(numProps);
			continue;
		}

		// Spawn the object, if the class exists
		ObjectList* objectList = Scene::GetStaticObjectList(objectName);
		if (!objectList) {
			Log::Print(Log::LOG_WARN,
				"Class \"%s\" does not exist! (ID: %d, X: %f, Y: %f)",
				objectName,
				(int)i,
				posX,
				posY);
			continue;
		}

		Entity* obj = Scene::TrySpawnObject(objectList, posX, posY);
		if (!obj) {
			SkipEntityProperties(numProps);
			continue;
		}

		Scene::AddStatic(objectList, obj);

		obj->SlotID = (int)i + Scene::ReservedSlotIDs;
		obj->InitProperties();

		// Add "filter" property
		obj->Filter = filter;
		obj->Properties->Put("filter", Property::MakeInteger(filter));

		// Add all properties
		for (Uint8 j = 0; j < numProps; j++) {
			SceneHash propHash;
			propHash.A = r->ReadUInt32();
			propHash.B = r->ReadUInt32();
			propHash.C = r->ReadUInt32();
			propHash.D = r->ReadUInt32();

			Uint8 varType = r->ReadByte();

			// Find the class property from the hash
			SceneClassProperty* classProp =
				HatchSceneReader::FindProperty(scnClass, propHash);
			if (!classProp) {
#ifdef HSCN_READER_DEBUG
				Log::Print(Log::LOG_WARN,
					"Could not find any property with hash %08x%08x%08x%08x",
					propHash.A,
					propHash.B,
					propHash.C,
					propHash.D);
#endif
				HatchSceneReader::SkipProperty(varType);
				continue;
			}

			// Add the property
			Uint8 u8Var;
			Uint16 u16Var;
			Uint32 u32Var;
			Uint32 vecVar[2];
			Uint16 strLength;
			char* strVar;

			Property val = Property::MakeNull();

			switch (varType) {
			case HSCN_VAR_INT8:
			case HSCN_VAR_UINT8:
				r->ReadBytes(&u8Var, 1);
				val = Property::MakeInteger(u8Var);
				break;
			case HSCN_VAR_INT16:
			case HSCN_VAR_UINT16:
				r->ReadBytes(&u16Var, 2);
				val = Property::MakeInteger(u16Var);
				break;
			case HSCN_VAR_ENUM:
			case HSCN_VAR_COLOR:
			case HSCN_VAR_INT32:
			case HSCN_VAR_UINT32:
			case HSCN_VAR_UNKNOWN:
				r->ReadBytes(&u32Var, 4);
				val = Property::MakeInteger((int)u32Var);
				break;
			case HSCN_VAR_BOOL:
				r->ReadBytes(&u32Var, 4);
				val = Property::MakeBool((bool)u32Var);
				break;
			case HSCN_VAR_VECTOR2:
				r->ReadBytes(vecVar, 8);
				{
					float fx = vecVar[0] / 65536.0;
					float fy = vecVar[1] / 65536.0;

					PropertyArray array;
					PropertyArray::Init(&array);
					AddPropertyToArray(&array, Property::MakeDecimal(fx));
					AddPropertyToArray(&array, Property::MakeDecimal(fy));

					val = Property::MakeArray(array);
				}
				break;
			case HSCN_VAR_STRING:
				strLength = r->ReadUInt16();
				strVar = (char*)malloc(strLength);
				if (!strVar) {
					Error::Fatal(
						"Out of memory in HatchSceneReader::ReadEntities!");
				}

				for (size_t i = 0; i < strLength; i++) {
					strVar[i] = r->ReadInt16();
				}

				val = Property::MakeString(strVar, strLength);
				free(strVar);
				break;
			}

			obj->Properties->Put(classProp->Name, val);
		}
	}
}

void HatchSceneReader::SkipEntityProperties(Uint8 numProps) {
	Stream* r = StreamPtr;

	for (Uint8 j = 0; j < numProps; j++) {
		r->Skip(16);
		HatchSceneReader::SkipProperty(r->ReadByte());
	}
}

void HatchSceneReader::SkipProperty(Uint8 varType) {
	Stream* r = StreamPtr;

	switch (varType) {
	case HSCN_VAR_INT8:
	case HSCN_VAR_UINT8:
		r->Skip(1);
		break;
	case HSCN_VAR_INT16:
	case HSCN_VAR_UINT16:
		r->Skip(2);
		break;
	case HSCN_VAR_ENUM:
	case HSCN_VAR_BOOL:
	case HSCN_VAR_COLOR:
	case HSCN_VAR_INT32:
	case HSCN_VAR_UINT32:
	case HSCN_VAR_UNKNOWN:
		r->Skip(4);
		break;
	case HSCN_VAR_VECTOR2:
		r->Skip(8);
		break;
	case HSCN_VAR_STRING:
		r->Skip(r->ReadUInt16() * 2);
		break;
	}
}

void HatchSceneReader::Dispose() {
	FreeClasses();
}
