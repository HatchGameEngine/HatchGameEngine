#include <Engine/Scene.h>

#include <Engine/Audio/AudioManager.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/GarbageCollector.h>
#include <Engine/Bytecode/ScriptEntity.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/SourceFileMap.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Diagnostics/MemoryPools.h>
#include <Engine/Filesystem/File.h>
#include <Engine/FontFace.h>
#include <Engine/Hashing/CRC32.h>
#include <Engine/Hashing/CombinedHash.h>
#include <Engine/Hashing/FNV1A.h>
#include <Engine/Hashing/MD5.h>
#include <Engine/IO/Compression/ZLibStream.h>
#include <Engine/IO/FileStream.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/Includes/HashMap.h>
#include <Engine/Math/Math.h>
#include <Engine/Rendering/SDL2/SDL2Renderer.h>
#include <Engine/ResourceTypes/ISound.h>
#include <Engine/ResourceTypes/ResourceManager.h>
#include <Engine/ResourceTypes/SceneFormats/HatchSceneReader.h>
#include <Engine/ResourceTypes/SceneFormats/RSDKSceneReader.h>
#include <Engine/ResourceTypes/SceneFormats/TiledMapReader.h>
#include <Engine/Scene/SceneInfo.h>
#include <Engine/TextFormats/XML/XMLNode.h>
#include <Engine/TextFormats/XML/XMLParser.h>
#include <Engine/Types/EntityTypes.h>
#include <Engine/Types/ObjectList.h>
#include <Engine/Types/ObjectRegistry.h>
#include <Engine/Utilities/StringUtils.h>

// General
int Scene::Frame = 0;
bool Scene::Paused = false;
bool Scene::Loaded = false;
int Scene::TileAnimationEnabled = 1;

// Layering variables
vector<SceneLayer> Scene::Layers;
bool Scene::AnyLayerTileChange = false;
int Scene::BasePriorityPerLayer = 32;
int Scene::PriorityPerLayer = 0;
DrawGroupList* Scene::PriorityLists = NULL;

// Rendering variables
int Scene::ShowTileCollisionFlag = 0;
int Scene::ShowObjectRegions = 0;

// Property variables
HashMap<VMValue>* Scene::Properties = NULL;

// Object variables
HashMap<ObjectList*>* Scene::ObjectLists = NULL;
HashMap<ObjectRegistry*>* Scene::ObjectRegistries = NULL;

HashMap<ObjectList*>* Scene::StaticObjectLists = NULL;

int Scene::StaticObjectCount = 0;
Entity* Scene::StaticObjectFirst = NULL;
Entity* Scene::StaticObjectLast = NULL;
int Scene::DynamicObjectCount = 0;
Entity* Scene::DynamicObjectFirst = NULL;
Entity* Scene::DynamicObjectLast = NULL;

int Scene::ObjectCount = 0;
Entity* Scene::ObjectFirst = NULL;
Entity* Scene::ObjectLast = NULL;

// Tile variables
vector<Tileset> Scene::Tilesets;
vector<TileSpriteInfo> Scene::TileSpriteInfos;
int Scene::TileCount = 0;
int Scene::TileWidth = 16;
int Scene::TileHeight = 16;
int Scene::BaseTileCount = 0;
int Scene::BaseTilesetCount = 0;
bool Scene::TileCfgLoaded = false;
vector<TileConfig*> Scene::TileCfg;
Uint16 Scene::EmptyTile = 0x000;

// View variables
View Scene::Views[MAX_SCENE_VIEWS];
int Scene::ViewCurrent = 0;
int Scene::ViewsActive = 1;
int Scene::CurrentDrawGroup = -1;
int Scene::ObjectViewRenderFlag;
int Scene::TileViewRenderFlag;
Perf_ViewRender Scene::PERF_ViewRender[MAX_SCENE_VIEWS];

char Scene::NextScene[256];
char Scene::CurrentScene[256];
bool Scene::DoRestart = false;
bool Scene::NoPersistency = false;

// Time variables
int Scene::TimeEnabled = 0;
int Scene::TimeCounter = 0;
int Scene::Milliseconds = 0;
int Scene::Seconds = 0;
int Scene::Minutes = 0;

int Scene::Filter = 0xFF;

// Scene list variables
int Scene::CurrentSceneInList;
char Scene::CurrentFolder[256];
char Scene::CurrentID[256];
char Scene::CurrentResourceFolder[256];
char Scene::CurrentCategory[256];
int Scene::ActiveCategory;

// Debug mode variables
int Scene::DebugMode;

// Resource managing variables
vector<ResourceType*> Scene::SpriteList;
vector<ResourceType*> Scene::ImageList;
vector<ResourceType*> Scene::SoundList;
vector<ResourceType*> Scene::MusicList;
vector<ResourceType*> Scene::ModelList;
vector<ResourceType*> Scene::MediaList;
vector<GameTexture*> Scene::TextureList;
vector<Animator*> Scene::AnimatorList;

Entity* StaticObject = NULL;
ObjectList* StaticObjectList = NULL;
bool DEV_NoTiles = false;
bool DEV_NoObjectRender = false;

int ViewRenderList[MAX_SCENE_VIEWS];

#define COLLISION_OFFSET 4.0

// Collision variables
float Scene::CollisionTolerance = 0.0;
bool Scene::UseCollisionOffset = false;
float Scene::CollisionMaskAir = 0.0;
CollisionBox Scene::CollisionOuter = {0, 0, 0, 0};
CollisionBox Scene::CollisionInner = {0, 0, 0, 0};
Entity* Scene::CollisionEntity = NULL;
CollisionSensor Scene::Sensors[6];
float Scene::CollisionMinimumDistance = 14.0;
float Scene::LowCollisionTolerance = 8.0;
float Scene::HighCollisionTolerance = 14.0;
int Scene::FloorAngleTolerance = 0x20;
int Scene::WallAngleTolerance = 0x20;
int Scene::RoofAngleTolerance = 0x20;
bool Scene::ShowHitboxes = false;
int Scene::DebugHitboxCount = 0;
DebugHitboxInfo Scene::DebugHitboxList[DEBUG_HITBOX_COUNT];

void ObjectList_CallLoads(Uint32 key, ObjectList* list) {
	// This is called before object lists are cleared, so we need
	// to check if there are any entities in the list.
	if (!list->Count() && !Scene::StaticObjectLists->Exists(key)) {
		return;
	}

	ScriptManager::CallFunction(list->LoadFunctionName);
}
void ObjectList_CallGlobalUpdates(Uint32, ObjectList* list) {
	if (list->Activity == ACTIVE_ALWAYS ||
		(list->Activity == ACTIVE_NORMAL && !Scene::Paused) ||
		(list->Activity == ACTIVE_PAUSED && Scene::Paused)) {
		ScriptManager::CallFunction(list->GlobalUpdateFunctionName);
	}
}
void UpdateObjectEarly(Entity* ent) {
	if (Scene::Paused && ent->Pauseable && ent->Activity != ACTIVE_PAUSED &&
		ent->Activity != ACTIVE_ALWAYS) {
		return;
	}
	if (!ent->Active) {
		return;
	}
	if (!ent->OnScreen) {
		return;
	}

	double elapsed = Clock::GetTicks();

	ent->UpdateEarly();

	elapsed = Clock::GetTicks() - elapsed;

	if (ent->List) {
		ent->List->Performance.EarlyUpdate.DoAverage(elapsed);
	}
}
void UpdateObjectLate(Entity* ent) {
	if (Scene::Paused && ent->Pauseable && ent->Activity != ACTIVE_PAUSED &&
		ent->Activity != ACTIVE_ALWAYS) {
		return;
	}
	if (!ent->Active) {
		return;
	}
	if (!ent->OnScreen) {
		return;
	}

	double elapsed = Clock::GetTicks();

	ent->UpdateLate();

	elapsed = Clock::GetTicks() - elapsed;

	if (ent->List) {
		ent->List->Performance.LateUpdate.DoAverage(elapsed);
	}
}
void UpdateObject(Entity* ent) {
	if (Scene::Paused && ent->Pauseable && ent->Activity != ACTIVE_PAUSED &&
		ent->Activity != ACTIVE_ALWAYS) {
		return;
	}

	if (!ent->Active) {
		return;
	}

	bool onScreenX = false;
	bool onScreenY = false;

	float entX1, entX2;
	float entY1, entY2;

	if (ent->OnScreenRegionLeft || ent->OnScreenRegionRight) {
		entX1 = ent->X - ent->OnScreenRegionLeft;
		entX2 = ent->X + ent->OnScreenRegionRight;
		onScreenX = ent->OnScreenRegionLeft != 0.0 || ent->OnScreenRegionRight != 0.0;
	}
	else {
		onScreenX = ent->OnScreenHitboxW == 0.0f;
		entX1 = ent->X - ent->OnScreenHitboxW * 0.5f;
		entX2 = ent->X + ent->OnScreenHitboxW * 0.5f;
	}

	if (ent->OnScreenRegionTop || ent->OnScreenRegionBottom) {
		entY1 = ent->Y - ent->OnScreenRegionTop;
		entY2 = ent->Y + ent->OnScreenRegionBottom;
		onScreenY = ent->OnScreenRegionTop != 0.0 || ent->OnScreenRegionBottom != 0.0;
	}
	else {
		onScreenY = ent->OnScreenHitboxH == 0.0f;
		entY1 = ent->Y - ent->OnScreenHitboxH * 0.5f;
		entY2 = ent->Y + ent->OnScreenHitboxH * 0.5f;
	}

	switch (ent->Activity) {
	default:
		break;

	case ACTIVE_NEVER:
	case ACTIVE_PAUSED:
		ent->InRange = false;
		break;

	case ACTIVE_ALWAYS:
	case ACTIVE_NORMAL:
		ent->InRange = true;
		break;

	case ACTIVE_BOUNDS:
		ent->InRange = false;

		for (int i = 0; i < Scene::ViewsActive; i++) {
			if (onScreenX && onScreenY) {
				break;
			}
			if (!onScreenX) {
				onScreenX = entX2 >= Scene::Views[i].X &&
					entX1 < Scene::Views[i].X + Scene::Views[i].Width;
			}
			if (!onScreenY) {
				onScreenY = entY2 >= Scene::Views[i].Y &&
					entY1 < Scene::Views[i].Y + Scene::Views[i].Height;
			}
		}

		if (onScreenX && onScreenY) {
			ent->InRange = true;
		}

		break;

	case ACTIVE_XBOUNDS:
		ent->InRange = false;

		for (int i = 0; i < Scene::ViewsActive; i++) {
			if (onScreenX) {
				break;
			}
			onScreenX = entX2 >= Scene::Views[i].X &&
				entX1 < Scene::Views[i].X + Scene::Views[i].Width;
		}

		if (onScreenX) {
			ent->InRange = true;
		}

		break;

	case ACTIVE_YBOUNDS:
		ent->InRange = false;

		for (int i = 0; i < Scene::ViewsActive; i++) {
			if (onScreenY) {
				break;
			}
			onScreenY = entY2 >= Scene::Views[i].Y &&
				entY1 < Scene::Views[i].Y + Scene::Views[i].Height;
		}

		if (onScreenY) {
			ent->InRange = true;
		}

		break;

	case ACTIVE_RBOUNDS:
		ent->InRange = false;

		// TODO: Double check this works properly
		for (int v = 0; v < Scene::ViewsActive; v++) {
			float sx = abs(ent->X - Scene::Views[v].X);
			float sy = abs(ent->Y - Scene::Views[v].Y);

			if (sx * sx + sy * sy <= ent->OnScreenHitboxW || onScreenX || onScreenY) {
				ent->InRange = true;
				break;
			}
		}
		break;
	}

	if (ent->InRange) {
		double elapsed = Clock::GetTicks();

		ent->OnScreen = true;

		ent->Update();

		elapsed = Clock::GetTicks() - elapsed;

		if (ent->List) {
			ent->List->Performance.Update.DoAverage(elapsed);
		}

		ent->WasOffScreen = false;
	}
	else {
		ent->OnScreen = false;
		ent->WasOffScreen = true;
	}

	if (!Scene::PriorityLists) {
		return;
	}

	int oldPriority = ent->PriorityOld;
	int maxPriority = Scene::PriorityPerLayer - 1;
	if (ent->Priority < 0) {
		ent->Priority = 0;
	}
	if (ent->Priority > maxPriority) {
		ent->Priority = maxPriority;
	}

	// If hasn't been put in a list yet:
	if (ent->PriorityListIndex == -1) {
		int index = Scene::PriorityLists[ent->Priority].GetEntityIndex(ent);
		if (index == -1) {
			index = Scene::PriorityLists[ent->Priority].Add(ent);
		}
		ent->PriorityListIndex = index;
	}
	// If Priority has changed:
	else if (ent->Priority != oldPriority) {
		// Remove entry in old list.
		if (oldPriority != -1) {
			Scene::PriorityLists[oldPriority].Remove(ent);
		}
		int index = Scene::PriorityLists[ent->Priority].GetEntityIndex(ent);
		if (index == -1) {
			index = Scene::PriorityLists[ent->Priority].Add(ent);
		}
		ent->PriorityListIndex = index;
	}

	// Sort list if needed
	if (ent->Depth != ent->OldDepth) {
		Scene::PriorityLists[ent->Priority].NeedsSorting = true;
	}

	ent->PriorityOld = ent->Priority;
	ent->OldDepth = ent->Depth;
}

// Double linked-list functions
void Scene::Add(Entity** first, Entity** last, int* count, Entity* obj) {
	// Set "prev" of obj to last
	obj->PrevEntity = (*last);
	obj->NextEntity = NULL;

	// If the last exists, set that ones "next" to obj
	if ((*last)) {
		(*last)->NextEntity = obj;
	}

	// Set obj as the first if there is not one
	if (!(*first)) {
		(*first) = obj;
	}

	(*last) = obj;

	(*count)++;

	// Add to proper list
	if (!obj->List) {
		Log::Print(Log::LOG_ERROR, "Entity %p has no list!", obj);
		abort();
	}
	obj->List->Add(obj);

	Scene::AddToScene(obj);
}
void Scene::Remove(Entity** first, Entity** last, int* count, Entity* obj) {
	if (obj == NULL) {
		return;
	}
	if (obj->Removed) {
		return;
	}

	if ((*first) == obj) {
		(*first) = obj->NextEntity;
	}

	if ((*last) == obj) {
		(*last) = obj->PrevEntity;
	}

	if (obj->PrevEntity) {
		obj->PrevEntity->NextEntity = obj->NextEntity;
	}

	if (obj->NextEntity) {
		obj->NextEntity->PrevEntity = obj->PrevEntity;
	}

	(*count)--;

	Scene::RemoveObject(obj);
}
void Scene::AddToScene(Entity* obj) {
	obj->PrevSceneEntity = Scene::ObjectLast;
	obj->NextSceneEntity = NULL;

	if (obj->PrevSceneEntity) {
		obj->PrevSceneEntity->NextSceneEntity = obj;
	}
	if (!Scene::ObjectFirst) {
		Scene::ObjectFirst = obj;
	}

	Scene::ObjectLast = obj;
	Scene::ObjectCount++;
}
void Scene::RemoveFromScene(Entity* obj) {
	if (Scene::ObjectFirst == obj) {
		Scene::ObjectFirst = obj->NextSceneEntity;
	}
	if (Scene::ObjectLast == obj) {
		Scene::ObjectLast = obj->PrevSceneEntity;
	}

	if (obj->PrevSceneEntity) {
		obj->PrevSceneEntity->NextSceneEntity = obj->NextSceneEntity;
	}
	if (obj->NextSceneEntity) {
		obj->NextSceneEntity->PrevSceneEntity = obj->PrevSceneEntity;
	}

	obj->PrevSceneEntity = obj->NextSceneEntity = NULL;

	Scene::ObjectCount--;
}
void Scene::RemoveObject(Entity* obj) {
	// Remove from proper list
	if (obj->List) {
		obj->List->Remove(obj);
	}

	// Remove from registries
	if (Scene::ObjectRegistries) {
		Scene::ObjectRegistries->WithAll([obj](Uint32, ObjectRegistry* registry) -> void {
			registry->Remove(obj);
		});
	}

	// Remove from draw groups
	for (int l = 0; l < Scene::PriorityPerLayer; l++) {
		PriorityLists[l].Remove(obj);
	}

	// Stop all sounds
	AudioManager::StopAllOriginSounds((void*)obj);

	// Remove it from the scene
	Scene::RemoveFromScene(obj);

	// If this object is unreachable script-side, that means it can
	// be deleted during garbage collection.
	// It doesn't really matter if it's still active or not, since
	// it won't be in any object list or draw groups at this point.
	obj->Remove();
}
void Scene::Clear(Entity** first, Entity** last, int* count) {
	(*first) = NULL;
	(*last) = NULL;
	(*count) = 0;
}

// Object management
void Scene::AddStatic(ObjectList* objectList, Entity* obj) {
	Scene::Add(&Scene::StaticObjectFirst,
		&Scene::StaticObjectLast,
		&Scene::StaticObjectCount,
		obj);
}
void Scene::AddDynamic(ObjectList* objectList, Entity* obj) {
	Scene::Add(&Scene::DynamicObjectFirst,
		&Scene::DynamicObjectLast,
		&Scene::DynamicObjectCount,
		obj);
}
void Scene::DeleteRemoved(Entity* obj) {
	if (!obj->Removed) {
		return;
	}

	obj->Dispose();
	delete obj;
}

void Scene::OnEvent(Uint32 event) {
	switch (event) {
	case SDL_APP_TERMINATING:
	case SDL_APP_LOWMEMORY:
	case SDL_APP_WILLENTERBACKGROUND:
	case SDL_APP_DIDENTERBACKGROUND:
	case SDL_APP_WILLENTERFOREGROUND:
	case SDL_APP_DIDENTERFOREGROUND:
		// Call "WindowFocusLost" event on all objects
		break;
	default:
		break;
	}
}

// Scene List Functions
void Scene::SetCurrent(const char* categoryName, const char* sceneName) {
	int categoryID = SceneInfo::GetCategoryID(categoryName);
	if (!SceneInfo::CategoryHasEntries(categoryID)) {
		return;
	}

	SceneListCategory& category = SceneInfo::Categories[categoryID];
	Scene::ActiveCategory = categoryID;

	int entryID = SceneInfo::GetEntryID(categoryID, sceneName);
	if (entryID != -1) {
		Scene::CurrentSceneInList = entryID;
	}
	else {
		Scene::CurrentSceneInList = 0;
	}
}
void Scene::SetInfoFromCurrentID() {
	if (!SceneInfo::IsEntryValid(Scene::ActiveCategory, Scene::CurrentSceneInList)) {
		return;
	}

	SceneListCategory& category = SceneInfo::Categories[Scene::ActiveCategory];
	SceneListEntry& scene = category.Entries[Scene::CurrentSceneInList];

	strcpy(Scene::CurrentID, scene.ID);
	strcpy(Scene::CurrentCategory, category.Name);

	if (scene.Folder != nullptr) {
		strcpy(Scene::CurrentFolder, scene.Folder);
	}
	else {
		Scene::CurrentFolder[0] = '\0';
	}

	if (scene.ResourceFolder != nullptr) {
		strcpy(Scene::CurrentResourceFolder, scene.ResourceFolder);
	}
	else {
		Scene::CurrentResourceFolder[0] = '\0';
	}
}

// Scene Lifecycle
void Scene::Init() {
	Scene::NextScene[0] = '\0';
	Scene::CurrentScene[0] = '\0';

	GarbageCollector::Init();

	Compiler::Init();

	Application::GameStart = true;

	ScriptManager::Init();
	ScriptManager::ResetStack();
	ScriptManager::LinkStandardLibrary();
	ScriptManager::LinkExtensions();

	Compiler::GetStandardConstants();

	SourceFileMap::CheckForUpdate();

	Application::Settings->GetBool("dev", "notiles", &DEV_NoTiles);
	Application::Settings->GetBool("dev", "noobjectrender", &DEV_NoObjectRender);
	Application::Settings->GetInteger("dev", "viewCollision", &ShowTileCollisionFlag);

	Graphics::SetTextureInterpolation(false);

	Scene::ViewCurrent = 0;
	for (int i = 0; i < MAX_SCENE_VIEWS; i++) {
		Scene::Views[i].Active = false;
		Scene::Views[i].Software = Graphics::UseSoftwareRenderer;
		Scene::Views[i].Priority = 0;
		Scene::Views[i].Width = Application::WindowWidth;
		Scene::Views[i].Height = Application::WindowHeight;
		Scene::Views[i].OutputWidth = Application::WindowWidth;
		Scene::Views[i].OutputHeight = Application::WindowHeight;
		Scene::Views[i].Stride = Math::CeilPOT(Scene::Views[i].Width);
		Scene::Views[i].FOV = 45.0f;
		Scene::Views[i].UsePerspective = false;
		Scene::Views[i].DrawTarget = Graphics::CreateTexture(SDL_PIXELFORMAT_ARGB8888,
			SDL_TEXTUREACCESS_TARGET,
			Scene::Views[i].Width,
			Scene::Views[i].Height);
		Scene::Views[i].UseDrawTarget = true;
		Scene::Views[i].ProjectionMatrix = Matrix4x4::Create();
		Scene::Views[i].BaseProjectionMatrix = Matrix4x4::Create();
	}
	Scene::Views[0].Active = true;
	Scene::ViewsActive = 1;

	Scene::ObjectViewRenderFlag = 0xFFFFFFFF;
	Scene::TileViewRenderFlag = 0xFFFFFFFF;

	Application::Settings->GetBool("dev", "loadAllClasses", &ScriptManager::LoadAllClasses);

	ScriptManager::LoadScript("init.hsl");

	if (ScriptManager::LoadAllClasses) {
		ScriptManager::LoadClasses();
	}
}
void Scene::InitObjectListsAndRegistries() {
	if (Scene::ObjectLists == NULL) {
		Scene::ObjectLists = new HashMap<ObjectList*>(CombinedHash::EncryptData, 4);
	}
	if (Scene::ObjectRegistries == NULL) {
		Scene::ObjectRegistries =
			new HashMap<ObjectRegistry*>(CombinedHash::EncryptData, 16);
	}
	if (Scene::StaticObjectLists == NULL) {
		Scene::StaticObjectLists = new HashMap<ObjectList*>(CombinedHash::EncryptData, 4);
	}
}

void Scene::ResetPerf() {
	if (Scene::ObjectLists) {
		Scene::ObjectLists->ForAll([](Uint32, ObjectList* list) -> void {
			list->ResetPerf();
		});
	}
}
void Scene::Update() {
	// Animate tiles
	Scene::RunTileAnimations();

	// Call global updates
	if (Scene::ObjectLists) {
		Scene::ObjectLists->ForAllOrdered(ObjectList_CallGlobalUpdates);
	}

	// Early Update
	for (Entity *ent = Scene::StaticObjectFirst, *next; ent; ent = next) {
		next = ent->NextEntity;
		UpdateObjectEarly(ent);
	}
	for (Entity *ent = Scene::DynamicObjectFirst, *next; ent; ent = next) {
		next = ent->NextEntity;
		UpdateObjectEarly(ent);
	}

	// Update objects
	for (Entity *ent = Scene::StaticObjectFirst, *next; ent; ent = next) {
		// Store the "next" so that when/if the current is
		// removed, it can still be used to point at the end of
		// the loop.
		next = ent->NextEntity;

		// Execute whatever on object
		UpdateObject(ent);
	}
	for (Entity *ent = Scene::DynamicObjectFirst, *next; ent; ent = next) {
		next = ent->NextEntity;
		UpdateObject(ent);
	}

	// Late Update
	for (Entity *ent = Scene::StaticObjectFirst, *next; ent; ent = next) {
		next = ent->NextEntity;
		UpdateObjectLate(ent);
	}
	for (Entity *ent = Scene::DynamicObjectFirst, *next; ent; ent = next) {
		next = ent->NextEntity;
		UpdateObjectLate(ent);

		// Removes the object from the scene, but doesn't
		// delete it yet.
		if (!ent->Active) {
			Scene::Remove(&Scene::DynamicObjectFirst,
				&Scene::DynamicObjectLast,
				&Scene::DynamicObjectCount,
				ent);
		}
	}

#ifdef USING_FFMPEG
	AudioManager::Lock();
	Uint8 audio_buffer[0x8000]; // <-- Should be larger than
		// AudioManager::AudioQueueMaxSize
	int needed = 0x8000; // AudioManager::AudioQueueMaxSize;
	for (size_t i = 0, i_sz = Scene::MediaList.size(); i < i_sz; i++) {
		if (!Scene::MediaList[i]) {
			continue;
		}

		MediaBag* media = Scene::MediaList[i]->AsMedia;
		int queued = (int)AudioManager::AudioQueueSize;
		if (queued < needed) {
			int ready_bytes =
				media->Player->GetAudioData(audio_buffer, needed - queued);
			if (ready_bytes > 0) {
				memcpy(AudioManager::AudioQueue + AudioManager::AudioQueueSize,
					audio_buffer,
					ready_bytes);
				AudioManager::AudioQueueSize += ready_bytes;
			}
		}
	}
	AudioManager::Unlock();
#endif

	if (!Scene::Paused) {
		Scene::Frame++;
		Scene::ProcessSceneTimer();
	}
}
void Scene::RunTileAnimations() {
	if ((Scene::TileAnimationEnabled == 1 && !Scene::Paused) ||
		Scene::TileAnimationEnabled == 2) {
		for (Tileset& tileset : Scene::Tilesets) {
			tileset.RunAnimations();
		}
	}
}
Tileset* Scene::GetTileset(int tileID) {
	// TODO: Optimize this.
	for (size_t i = Scene::Tilesets.size(); i > 0; i--) {
		Tileset* tileset = &Scene::Tilesets[i - 1];
		if (tileID >= tileset->StartTile) {
			return tileset;
		}
	}

	return nullptr;
}
TileAnimator* Scene::GetTileAnimator(int tileID) {
	for (Tileset& tileset : Scene::Tilesets) {
		TileAnimator* animator = tileset.GetTileAnimSequence(tileID);
		if (animator) {
			return animator;
		}
	}
	return nullptr;
}
void Scene::SetViewActive(int viewIndex, bool active) {
	if (Scene::Views[viewIndex].Active == active) {
		return;
	}

	Scene::Views[viewIndex].Active = active;
	if (active) {
		Scene::ViewsActive++;
	}
	else {
		Scene::ViewsActive--;
	}

	Scene::SortViews();
}
void Scene::SetViewPriority(int viewIndex, int priority) {
	Scene::Views[viewIndex].Priority = priority;
	Scene::SortViews();
}

void Scene::ResetViews() {
	Scene::ViewsActive = 0;

	// Deactivate extra views
	for (int i = 0; i < MAX_SCENE_VIEWS; i++) {
		Scene::Views[i].Active = false;
		Scene::Views[i].Priority = 0;
	}

	Scene::SetViewActive(0, true);
}

void Scene::SortViews() {
	int count = 0;

	for (int i = 0; i < MAX_SCENE_VIEWS; i++) {
		if (Scene::Views[i].Active) {
			ViewRenderList[count++] = i;
		}
	}

	if (count > 1) {
		std::stable_sort(ViewRenderList, ViewRenderList + count, [](int a, int b) {
			View* viewA = &Scene::Views[a];
			View* viewB = &Scene::Views[b];
			return viewA->Priority < viewB->Priority;
		});
	}
}

void Scene::SetView(int viewIndex) {
	View* currentView = &Scene::Views[viewIndex];

	Graphics::CurrentView = currentView;

	if (currentView->UseDrawTarget && currentView->DrawTarget) {
		float view_w = currentView->Width;
		float view_h = currentView->Height;
		Texture* tar = currentView->DrawTarget;

		size_t stride = currentView->Software ? (size_t)currentView->Stride : view_w;
		if (tar->Width != stride || tar->Height != view_h) {
			Graphics::GfxFunctions = &Graphics::Internal;
			Graphics::DisposeTexture(tar);
			Graphics::SetTextureInterpolation(false);
			currentView->DrawTarget = Graphics::CreateTexture(
				SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, stride, view_h);
		}

		Graphics::SetRenderTarget(currentView->DrawTarget);

		if (currentView->Software) {
			Graphics::SoftwareStart();
		}
		else {
			Graphics::Clear();
		}
	}

	Scene::ViewCurrent = viewIndex;
}

bool Scene::CheckPosOnScreen(float posX, float posY, float rangeX, float rangeY) {
	for (int s = 0; s < MAX_SCENE_VIEWS; ++s) {
		if (Scene::Views[s].Active && posX + rangeX >= Scene::Views[s].X &&
			posY + rangeY >= Scene::Views[s].Y &&
			posX < Scene::Views[s].X + Scene::Views[s].Width &&
			posY < Scene::Views[s].Y + Scene::Views[s].Height) {
			return true;
		}
	}

	return false;
}

#define PERF_START(n) \
	if (viewPerf) \
	viewPerf->n = Clock::GetTicks()
#define PERF_END(n) \
	if (viewPerf) \
	viewPerf->n = Clock::GetTicks() - viewPerf->n

void Scene::RenderView(int viewIndex, bool doPerf) {
	View* currentView = &Scene::Views[viewIndex];
	Perf_ViewRender* viewPerf = doPerf ? &Scene::PERF_ViewRender[viewIndex] : NULL;

	if (viewPerf) {
		viewPerf->RecreatedDrawTarget = false;
	}

	bool useDrawTarget = false;
	Texture* drawTarget = currentView->DrawTarget;

	PERF_START(RenderSetupTime);
	if (currentView->UseDrawTarget && drawTarget) {
		useDrawTarget = true;
	}
	else if (!currentView->Visible) {
		PERF_END(RenderSetupTime);
		return;
	}
	Scene::SetView(viewIndex);
	PERF_END(RenderSetupTime);

	if (viewPerf && drawTarget != currentView->DrawTarget) {
		viewPerf->RecreatedDrawTarget = true;
	}

	float cx = std::floor(currentView->X);
	float cy = std::floor(currentView->Y);
	float cz = std::floor(currentView->Z);

	int viewRenderFlag = 1 << viewIndex;

	// Adjust projection
	PERF_START(ProjectionSetupTime);
	if (currentView->UsePerspective) {
		Graphics::UpdatePerspective(currentView->FOV,
			currentView->Width / currentView->Height,
			currentView->NearPlane,
			currentView->FarPlane);
		Matrix4x4::Rotate(currentView->ProjectionMatrix,
			currentView->ProjectionMatrix,
			currentView->RotateX,
			1.0,
			0.0,
			0.0);
		Matrix4x4::Rotate(currentView->ProjectionMatrix,
			currentView->ProjectionMatrix,
			currentView->RotateY,
			0.0,
			1.0,
			0.0);
		Matrix4x4::Rotate(currentView->ProjectionMatrix,
			currentView->ProjectionMatrix,
			currentView->RotateZ,
			0.0,
			0.0,
			1.0);
		Matrix4x4::Translate(currentView->ProjectionMatrix,
			currentView->ProjectionMatrix,
			-currentView->X,
			-currentView->Y,
			-currentView->Z);
		Matrix4x4::Copy(currentView->BaseProjectionMatrix, currentView->ProjectionMatrix);
	}
	else {
		Graphics::UpdateOrtho(currentView->Width, currentView->Height);
		if (!currentView->UseDrawTarget) {
			Graphics::UpdateOrthoFlipped(currentView->Width, currentView->Height);
		}

		Matrix4x4::Rotate(currentView->ProjectionMatrix,
			currentView->ProjectionMatrix,
			currentView->RotateX,
			1.0,
			0.0,
			0.0);
		Matrix4x4::Rotate(currentView->ProjectionMatrix,
			currentView->ProjectionMatrix,
			currentView->RotateY,
			0.0,
			1.0,
			0.0);
		Matrix4x4::Rotate(currentView->ProjectionMatrix,
			currentView->ProjectionMatrix,
			currentView->RotateZ,
			0.0,
			0.0,
			1.0);
		Matrix4x4::Translate(currentView->ProjectionMatrix,
			currentView->BaseProjectionMatrix,
			-cx,
			-cy,
			-cz);
	}
	Graphics::UpdateProjectionMatrix();
	PERF_END(ProjectionSetupTime);

	// RenderEarly
	PERF_START(ObjectRenderEarlyTime);
	for (int l = 0; l < Scene::PriorityPerLayer; l++) {
		if (DEV_NoObjectRender) {
			break;
		}

		DrawGroupList* drawGroupList = &PriorityLists[l];
		if (drawGroupList->NeedsSorting) {
			drawGroupList->Sort();
		}

		Scene::CurrentDrawGroup = l;

		for (Entity* ent : *drawGroupList->Entities) {
			if (ent->Active) {
				ent->RenderEarly();
			}
		}
	}
	PERF_END(ObjectRenderEarlyTime);

	bool showObjectRegions = Scene::ShowObjectRegions;

	// Render Objects and Layer Tiles
	float _vx = currentView->X;
	float _vy = currentView->Y;
	float _vw = currentView->Width;
	float _vh = currentView->Height;
	double objectTimeTotal = 0.0;
	DrawGroupList* drawGroupList;
	for (int l = 0; l < Scene::PriorityPerLayer; l++) {
		if (DEV_NoObjectRender) {
			goto DEV_NoTilesCheck;
		}

		double elapsed;
		double objectTime;
		float _ox;
		float _oy;
		float entX1, entX2;
		float entY1, entY2;
		objectTime = Clock::GetTicks();

		Scene::CurrentDrawGroup = l;

		drawGroupList = &PriorityLists[l];
		for (Entity* ent : *drawGroupList->Entities) {
			if (ent->Active) {
				_ox = ent->X - _vx;
				_oy = ent->Y - _vy;

				if (ent->RenderRegionLeft || ent->RenderRegionRight) {
					if (ent->RenderRegionLeft == 0.0f &&
						ent->RenderRegionRight == 0.0f) {
						goto DoCheckRender;
					}

					entX1 = _ox - ent->RenderRegionLeft;
					entX2 = _ox + ent->RenderRegionRight;
				}
				else {
					if (ent->RenderRegionW == 0.0f) {
						goto DoCheckRender;
					}

					entX1 = _ox - ent->RenderRegionW * 0.5f;
					entX2 = _ox + ent->RenderRegionW * 0.5f;
				}

				if (ent->RenderRegionTop || ent->RenderRegionBottom) {
					if (ent->RenderRegionTop == 0.0f &&
						ent->RenderRegionBottom == 0.0f) {
						goto DoCheckRender;
					}

					entY1 = _oy - ent->RenderRegionTop;
					entY2 = _oy + ent->RenderRegionBottom;
				}
				else {
					if (ent->RenderRegionH == 0.0f) {
						goto DoCheckRender;
					}

					entY1 = _oy - ent->RenderRegionH * 0.5f;
					entY2 = _oy + ent->RenderRegionH * 0.5f;
				}

				if (entX2 < 0.0f || entX1 >= _vw || entY2 < 0.0f || entY1 >= _vh) {
					continue;
				}

				// Show render region
				if (showObjectRegions) {
					Graphics::SetBlendColor(0.0f, 0.0f, 1.0f, 0.5f);
					Graphics::FillRectangle(entX1 + _vx,
						entY1 + _vy,
						entX2 - entX1,
						entY2 - entY1);
				}

			DoCheckRender:
				// Show update region
				if (showObjectRegions) {
					if (ent->OnScreenRegionLeft || ent->OnScreenRegionRight) {
						entX1 = ent->X - ent->OnScreenRegionLeft;
						entX2 = ent->X + ent->OnScreenRegionRight;
					}
					else {
						entX1 = ent->X - ent->OnScreenHitboxW * 0.5f;
						entX2 = ent->X + ent->OnScreenHitboxW * 0.5f;
					}

					if (ent->OnScreenRegionTop || ent->OnScreenRegionBottom) {
						entY1 = ent->Y - ent->OnScreenRegionTop;
						entY2 = ent->Y + ent->OnScreenRegionBottom;
					}
					else {
						entY1 = ent->Y - ent->OnScreenHitboxH * 0.5f;
						entY2 = ent->Y + ent->OnScreenHitboxH * 0.5f;
					}

					Graphics::SetBlendColor(1.0f, 0.0f, 0.0f, 0.5f);
					Graphics::FillRectangle(
						entX1, entY1, entX2 - entX1, entY2 - entY1);
				}

				if (!ent->Visible || (ent->ViewRenderFlag & viewRenderFlag) == 0) {
					continue;
				}
				if ((ent->ViewOverrideFlag & viewRenderFlag) == 0 &&
					(Scene::ObjectViewRenderFlag & viewRenderFlag) == 0) {
					continue;
				}

				elapsed = Clock::GetTicks();

				ent->Render(_vx, _vy);

				elapsed = Clock::GetTicks() - elapsed;

				if (ent->List) {
					ent->List->Performance.Render.DoAverage(elapsed);
				}
			}
		}
		objectTime = Clock::GetTicks() - objectTime;
		objectTimeTotal += objectTime;

	DEV_NoTilesCheck:
		if (DEV_NoTiles) {
			continue;
		}

		if (Scene::Tilesets.size() == 0) {
			continue;
		}

		if ((Scene::TileViewRenderFlag & viewRenderFlag) == 0) {
			continue;
		}

		bool texBlend = Graphics::TextureBlend;
		for (size_t li = 0; li < Layers.size(); li++) {
			SceneLayer* layer = &Layers[li];
			// Skip layer tile render if already rendered
			if (layer->DrawGroup != l) {
				continue;
			}

			// Draw Tiles
			if (layer->Visible) {
				PERF_START(LayerTileRenderTime[li]);

				Graphics::Save();
				Graphics::Translate(cx, cy, cz);

				Graphics::TextureBlend = layer->Blending;
				if (Graphics::TextureBlend) {
					Graphics::SetBlendColor(1.0, 1.0, 1.0, layer->Opacity);
					Graphics::SetBlendMode(layer->BlendMode);
				}
				else {
					Graphics::SetBlendColor(1.0, 1.0, 1.0, 1.0);
				}

				Graphics::DrawSceneLayer(layer, currentView, (int)li, true);
				Graphics::ClearClip();

				Graphics::Restore();

				PERF_END(LayerTileRenderTime[li]);
			}
		}
		Graphics::TextureBlend = texBlend;
	}
	if (viewPerf) {
		viewPerf->ObjectRenderTime = objectTimeTotal;
	}

	// RenderLate
	PERF_START(ObjectRenderLateTime);
	for (int l = 0; l < Scene::PriorityPerLayer; l++) {
		if (DEV_NoObjectRender) {
			break;
		}

		Scene::CurrentDrawGroup = l;

		DrawGroupList* drawGroupList = &PriorityLists[l];
		for (Entity* ent : *drawGroupList->Entities) {
			if (ent->Active) {
				ent->RenderLate();
			}
		}
	}
	Scene::CurrentDrawGroup = -1;
	PERF_END(ObjectRenderLateTime);

	PERF_START(RenderFinishTime);
	if (useDrawTarget && currentView->Software) {
		Graphics::SoftwareEnd();
	}
	PERF_END(RenderFinishTime);
}

void Scene::Render() {
	if (!Scene::PriorityLists) {
		return;
	}

	Graphics::ResetViewport();

	if (Graphics::PaletteUpdated) {
		Graphics::UpdateGlobalPalette();
		Graphics::PaletteUpdated = false;
	}

	int win_w, win_h, ren_w, ren_h;
	SDL_GetWindowSize(Application::Window, &win_w, &win_h);
	SDL_GL_GetDrawableSize(Application::Window, &ren_w, &ren_h);
#ifdef IOS
	SDL2Renderer::GetMetalSize(&ren_w, &ren_h);
#endif

	int viewCount = Scene::ViewsActive;
	for (int i = 0; i < viewCount; i++) {
		int viewIndex = ViewRenderList[i];
		View* currentView = &Scene::Views[viewIndex];
		Perf_ViewRender* viewPerf = &Scene::PERF_ViewRender[viewIndex];

		PERF_START(RenderTime);

		Scene::RenderView(viewIndex, true);

		double renderFinishTime = Clock::GetTicks();
		if (currentView->UseDrawTarget && currentView->DrawTarget) {
			Graphics::SetRenderTarget(NULL);
			if (currentView->Visible) {
				Graphics::UpdateOrthoFlipped(win_w, win_h);
				Graphics::UpdateProjectionMatrix();
				Graphics::SetDepthTesting(false);

				float out_x = 0.0f;
				float out_y = 0.0f;
				float out_w = 0.0f, out_h = 0.0f;
				float scale = 1.f;
				// bool needClip = false;
				int aspectMode = (viewCount > 1) ? 3 : 1;
				switch (aspectMode) {
				// Stretch
				case 0:
					out_w = win_w;
					out_h = win_h;
					break;
				// Fit to aspect ratio
				case 1:
					if (win_w / currentView->Width <
						win_h / currentView->Height) {
						out_w = win_w;
						out_h = win_w * currentView->Height /
							currentView->Width;
					}
					else {
						out_w = win_h * currentView->Width /
							currentView->Height;
						out_h = win_h;
					}
					out_x = (win_w - out_w) * 0.5f / scale;
					out_y = (win_h - out_h) * 0.5f / scale;
					// needClip = true;
					break;
				// Fill to aspect ratio
				case 2:
					if (win_w / currentView->Width >
						win_h / currentView->Height) {
						out_w = win_w;
						out_h = win_w * currentView->Height /
							currentView->Width;
					}
					else {
						out_w = win_h * currentView->Width /
							currentView->Height;
						out_h = win_h;
					}
					out_x = (win_w - out_w) * 0.5f;
					out_y = (win_h - out_h) * 0.5f;
					break;
				// Splitscreen
				case 3:
					out_x = (currentView->OutputX /
							(float)Application::WindowWidth) *
						win_w;
					out_y = (currentView->OutputY /
							(float)Application::WindowHeight) *
						win_h;
					out_w = (currentView->OutputWidth /
							(float)Application::WindowWidth) *
						win_w;
					out_h = (currentView->OutputHeight /
							(float)Application::WindowHeight) *
						win_h;
					break;
				}

				Graphics::TextureBlend = false;
				Graphics::SetBlendMode(BlendMode_NORMAL);
				Graphics::DrawTexture(currentView->DrawTarget,
					0.0,
					0.0,
					currentView->Width,
					currentView->Height,
					out_x,
					out_y + Graphics::PixelOffset,
					out_w,
					out_h + Graphics::PixelOffset);
				Graphics::SetDepthTesting(Graphics::UseDepthTesting);
			}
		}
		renderFinishTime = Clock::GetTicks() - renderFinishTime;
		viewPerf->RenderFinishTime += renderFinishTime;
		PERF_END(RenderTime);
	}

	Graphics::CurrentView = NULL;

	Scene::ViewCurrent = 0;
}

void Scene::AfterScene() {
	ScriptManager::ResetStack();
	ScriptManager::RequestGarbageCollection();

	bool& doRestart = Scene::DoRestart;

	if (Scene::NextScene[0]) {
		ScriptManager::ForceGarbageCollection();

		Scene::LoadScene(Scene::NextScene);
		Scene::NextScene[0] = '\0';

		doRestart = true;
	}

	if (doRestart) {
		Scene::Restart();
		Scene::NoPersistency = false;

		doRestart = false;
	}
}

void Scene::Iterate(Entity* first, std::function<void(Entity* e)> func) {
	for (Entity *ent = first, *next; ent; ent = next) {
		next = ent->NextEntity;
		func(ent);
	}
}
void Scene::IterateAll(Entity* first, std::function<void(Entity* e)> func) {
	for (Entity *ent = first, *next; ent; ent = next) {
		next = ent->NextSceneEntity;
		func(ent);
	}
}
void Scene::ResetPriorityListIndex(Entity* first) {
	Scene::Iterate(first, [](Entity* ent) -> void {
		ent->PriorityListIndex = -1;
	});
}

int Scene::GetPersistenceScopeForObjectDeletion() {
	return Scene::NoPersistency ? Persistence_SCENE : Persistence_NONE;
}

void Scene::Restart() {
	Scene::ViewCurrent = 0;
	Graphics::CurrentView = NULL;

	View* currentView = &Scene::Views[Scene::ViewCurrent];
	currentView->X = 0.0f;
	currentView->Y = 0.0f;
	currentView->Z = 0.0f;
	Scene::Frame = 0;
	Scene::Paused = false;
	Scene::TileAnimationEnabled = 1;

	Scene::TimeCounter = 0;
	Scene::Minutes = 0;
	Scene::Seconds = 0;
	Scene::Milliseconds = 0;

	Scene::DebugMode = 0;

	Scene::ResetViews();

	Scene::ObjectViewRenderFlag = 0xFFFFFFFF;
	Scene::TileViewRenderFlag = 0xFFFFFFFF;

	Graphics::UnloadSceneData();

	if (Scene::AnyLayerTileChange) {
		// Copy backup tiles into main tiles
		for (int l = 0; l < (int)Layers.size(); l++) {
			memcpy(Layers[l].Tiles, Layers[l].TilesBackup, Layers[l].DataSize);
		}
		Scene::AnyLayerTileChange = false;
	}

	Scene::ClearPriorityLists();

	// Remove all non-persistent objects from lists
	if (Scene::ObjectLists) {
		Scene::ObjectLists->ForAll([](Uint32, ObjectList* list) -> void {
			list->RemoveNonPersistentFromLinkedList(Scene::DynamicObjectFirst,
				Scene::GetPersistenceScopeForObjectDeletion());
		});
	}

	// Remove all non-persistent objects from registries
	if (Scene::ObjectRegistries) {
		Scene::ObjectRegistries->ForAll([](Uint32, ObjectRegistry* registry) -> void {
			registry->RemoveNonPersistentFromLinkedList(Scene::DynamicObjectFirst,
				Scene::GetPersistenceScopeForObjectDeletion());
		});
	}

	// Dispose of all dynamic objects
	Scene::RemoveNonPersistentObjects(
		&Scene::DynamicObjectFirst, &Scene::DynamicObjectLast, &Scene::DynamicObjectCount);

	if (Scene::BaseTilesetCount != Scene::Tilesets.size()) {
		while (Scene::Tilesets.size() > Scene::BaseTilesetCount) {
			size_t i = Scene::Tilesets.size() - 1;

			if (Scene::Tilesets[i].Sprite) {
				delete Scene::Tilesets[i].Sprite;
			}
			if (Scene::Tilesets[i].Filename) {
				Memory::Free(Scene::Tilesets[i].Filename);
			}

			Scene::Tilesets.erase(Scene::Tilesets.begin() + i);
		}
	}

	if (Scene::BaseTileCount != Scene::TileCount) {
		Scene::TileSpriteInfos.resize(Scene::BaseTileCount);
		Scene::SetTileCount(Scene::BaseTileCount);
	}

	// Restart tile animations
	for (Tileset& tileset : Scene::Tilesets) {
		tileset.RestartAnimations();
	}

	// Run "Load" on all object classes
	// This is done (on purpose) before object lists are cleared.
	// See the comments in ObjectList_CallLoads
	if (Scene::ObjectLists) {
		Scene::ObjectLists->ForAllOrdered(ObjectList_CallLoads);
	}

	// Run "Initialize" on all objects
	Scene::Iterate(Scene::StaticObjectFirst, [](Entity* ent) -> void {
		// On a scene restart, static entities are
		// generally subject to having their
		// constructors called again, along with having
		// their positions set to their initial
		// positions. On top of that, their Create
		// event is called. Of course, none of this
		// should be done if the entity is persistent.
		if (ent->Persistence == Persistence_NONE) {
			ent->Created = false;
			ent->PostCreated = false;
		}

		if (!ent->Created) {
			ent->X = ent->InitialX;
			ent->Y = ent->InitialY;
			ent->Initialize();
		}
	});

	// Run "Create" on all objects
	Scene::Iterate(Scene::StaticObjectFirst, [](Entity* ent) -> void {
		if (!ent->Created) {
			// ent->Created gets set when Create()
			// is called.
			ent->Create();
		}
	});

	// Clean up object lists
	// Done after all objects are created.
	if (Scene::ObjectLists && Scene::StaticObjectLists) {
		Scene::ObjectLists->ForAll([](Uint32 key, ObjectList* list) -> void {
			if (!list->Count() && !Scene::StaticObjectLists->Exists(key)) {
				Scene::ObjectLists->Remove(key);
				delete list;
			}
		});
	}

	// Run "PostCreate" on all objects
	Scene::Iterate(Scene::StaticObjectFirst, [](Entity* ent) -> void {
		if (!ent->PostCreated) {
			// ent->PostCreated gets set when
			// PostCreate() is called.
			ent->PostCreate();
		}
	});

	// Run "OnSceneLoad" or "OnSceneRestart" on all objects
	Scene::IterateAll(Scene::ObjectFirst, [](Entity* ent) -> void {
		if (Scene::Loaded) {
			ent->OnSceneRestart();
		}
		else {
			ent->OnSceneLoad();
		}
	});

	Scene::Loaded = true;

	ScriptManager::ResetStack();
	ScriptManager::RequestGarbageCollection();

	Application::FirstFrame = true;
}
void Scene::ClearPriorityLists() {
	if (!Scene::PriorityLists) {
		return;
	}

	int layerSize = Scene::PriorityPerLayer;
	for (int l = 0; l < layerSize; l++) {
		Scene::PriorityLists[l].Clear();
	}

	// Reset the priority list indexes of all persistent objects
	ResetPriorityListIndex(Scene::StaticObjectFirst);
	ResetPriorityListIndex(Scene::DynamicObjectFirst);
}
void Scene::DeleteObjects(Entity** first, Entity** last, int* count) {
	Scene::Iterate(*first, [](Entity* ent) -> void {
		// Garbage collection will take care of it later.
		Scene::RemoveObject(ent);
	});
	Scene::Clear(first, last, count);
}
void Scene::RemoveNonPersistentObjects(Entity** first, Entity** last, int* count) {
	int persistencyScope = Scene::GetPersistenceScopeForObjectDeletion();
	for (Entity *ent = *first, *next; ent; ent = next) {
		next = ent->NextEntity;
		if (ent->Persistence <= persistencyScope) {
			Scene::Remove(first, last, count, ent);
		}
	}
}
void Scene::DeleteAllObjects() {
	// Dispose and clear Static objects
	Scene::DeleteObjects(
		&Scene::StaticObjectFirst, &Scene::StaticObjectLast, &Scene::StaticObjectCount);

	// Dispose and clear Dynamic objects
	Scene::DeleteObjects(
		&Scene::DynamicObjectFirst, &Scene::DynamicObjectLast, &Scene::DynamicObjectCount);

	// Clear lists
	if (Scene::ObjectLists) {
		Scene::ObjectLists->ForAll([](Uint32, ObjectList* list) -> void {
			list->Clear();
		});
	}

	// Clear registries
	if (Scene::ObjectRegistries) {
		Scene::ObjectRegistries->ForAll([](Uint32, ObjectRegistry* registry) -> void {
			registry->Clear();
		});
	}
}
void Scene::LoadScene(const char* sceneFilename) {
	// Remove non-persistent objects from lists
	if (Scene::ObjectLists) {
		Scene::ObjectLists->ForAll([](Uint32, ObjectList* list) -> void {
			int persistencyScope = Scene::GetPersistenceScopeForObjectDeletion();
			list->RemoveNonPersistentFromLinkedList(
				Scene::StaticObjectFirst, persistencyScope);
			list->RemoveNonPersistentFromLinkedList(
				Scene::DynamicObjectFirst, persistencyScope);
		});
	}

	// Remove non-persistent objects from registries
	if (Scene::ObjectRegistries) {
		Scene::ObjectRegistries->ForAll([](Uint32, ObjectRegistry* list) -> void {
			int persistencyScope = Scene::GetPersistenceScopeForObjectDeletion();
			list->RemoveNonPersistentFromLinkedList(
				Scene::StaticObjectFirst, persistencyScope);
			list->RemoveNonPersistentFromLinkedList(
				Scene::DynamicObjectFirst, persistencyScope);
		});
	}

	// Dispose of resources in SCOPE_SCENE
	Scene::DisposeInScope(SCOPE_SCENE);

	// Clear and dispose of non-persistent objects
	// We force garbage collection right after, meaning that
	// non-persistent objects may get deleted.
	Scene::RemoveNonPersistentObjects(
		&Scene::StaticObjectFirst, &Scene::StaticObjectLast, &Scene::StaticObjectCount);
	Scene::RemoveNonPersistentObjects(
		&Scene::DynamicObjectFirst, &Scene::DynamicObjectLast, &Scene::DynamicObjectCount);

	// Clear static object lists (but don't delete them)
	if (Scene::StaticObjectLists) {
		Scene::StaticObjectLists->Clear();
	}

	// Clear priority lists
	Scene::ClearPriorityLists();

	// Dispose of layers
	for (size_t i = 0; i < Scene::Layers.size(); i++) {
		Scene::Layers[i].Dispose();
	}
	Scene::Layers.clear();

	// Dispose of TileConfigs
	Scene::UnloadTileCollisions();

	// Dispose of properties
	if (Scene::Properties) {
		delete Scene::Properties;
	}
	Scene::Properties = NULL;

	// Force garbage collect
	ScriptManager::ResetStack();
	ScriptManager::ForceGarbageCollection();

#if 0
    MemoryPools::RunGC(MemoryPools::MEMPOOL_HASHMAP);
    MemoryPools::RunGC(MemoryPools::MEMPOOL_STRING);
    MemoryPools::RunGC(MemoryPools::MEMPOOL_SUBOBJECT);
#endif

	char* filename = StringUtils::NormalizePath(sceneFilename);

	char pathParent[MAX_RESOURCE_PATH_LENGTH];
	StringUtils::Copy(pathParent, filename, sizeof(pathParent));
	for (char* i = pathParent + strlen(pathParent); i >= pathParent; i--) {
		if (*i == '/') {
			*++i = 0;
			break;
		}
	}

	memmove(Scene::CurrentScene, filename, strlen(filename) + 1);

	Scene::UnloadTilesets();

	Scene::TileWidth = Scene::TileHeight = 16;
	Scene::EmptyTile = 0;

	Scene::InitObjectListsAndRegistries();
	Scene::FreePriorityLists();

	for (size_t i = 0; i < Scene::Layers.size(); i++) {
		Scene::Layers[i].Dispose();
	}
	Scene::Layers.clear();

	// Load Static class
	if (Application::GameStart) {
		Scene::AddStaticClass();
	}

	// Read the scene file
	Log::Print(Log::LOG_INFO, "Starting scene \"%s\"...", filename);

	Stream* r = ResourceStream::New(filename);
	if (r) {
		// Guess from the header first
		Uint32 magic = r->ReadUInt32();

		if (magic == HatchSceneReader::Magic) {
			r->Seek(0);
			HatchSceneReader::Read(r, pathParent);
		}
		else if (magic == RSDKSceneReader::Magic) {
			r->Seek(0);
			RSDKSceneReader::Read(r, pathParent);
		}
		else {
			r->Close();

			// Guess from the filename
			if (StringUtils::StrCaseStr(filename, ".bin")) {
				RSDKSceneReader::Read(filename, pathParent);
			}
			else if (StringUtils::StrCaseStr(filename, ".hcsn")) {
				HatchSceneReader::Read(filename, pathParent);
			}
			else {
				TiledMapReader::Read(filename, pathParent);
			}
		}

		InitTileCollisions();
		SetInfoFromCurrentID();

		if (SceneInfo::IsEntryValid(ActiveCategory, CurrentSceneInList)) {
			std::string cfgFilename = SceneInfo::GetTileConfigFilename(
				ActiveCategory, CurrentSceneInList);
			Scene::LoadTileCollisions(cfgFilename.c_str(), 0);
		}
	}
	else {
		Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", filename);
	}

	// Call Static's GameStart here
	if (Application::GameStart) {
		Scene::CallGameStart();
		Application::GameStart = false;
	}

	Scene::Loaded = false;

	Memory::Free(filename);
}

void Scene::ProcessSceneTimer() {
	if (Scene::TimeEnabled) {
		Scene::TimeCounter += 100;

		if (Scene::TimeCounter >= 6000) {
			Scene::TimeCounter -= 6025;

			Scene::Seconds++;
			if (Scene::Seconds >= 60) {
				Scene::Seconds = 0;

				Scene::Minutes++;
				if (Scene::Minutes >= 60) {
					Scene::Minutes = 0;
				}
			}
		}

		Scene::Milliseconds = Scene::TimeCounter / 60; // Refresh rate
	}
}

ObjectList* Scene::NewObjectList(const char* objectName) {
	ObjectList* objectList = new (std::nothrow) ObjectList(objectName);
	if (objectList && ScriptManager::LoadObjectClass(objectName, true)) {
		objectList->SpawnFunction = ScriptManager::ObjectSpawnFunction;
	}
	return objectList;
}
void Scene::AddStaticClass() {
	StaticObjectList = Scene::NewObjectList("Static");
	if (!StaticObjectList->SpawnFunction) {
		return;
	}

	Entity* obj = StaticObjectList->Spawn();
	if (obj) {
		obj->X = 0.0f;
		obj->Y = 0.0f;
		obj->InitialX = obj->X;
		obj->InitialY = obj->Y;
		obj->List = StaticObjectList;
		obj->Persistence = Persistence_GAME;

		ScriptManager::Globals->Put("global", OBJECT_VAL(((ScriptEntity*)obj)->Instance));
	}

	StaticObject = obj;
}
void Scene::CallGameStart() {
	if (StaticObject) {
		StaticObject->GameStart();
	}
}
ObjectList* Scene::GetObjectList(const char* objectName, bool callListLoadFunction) {
	Uint32 objectNameHash = Scene::ObjectLists->HashFunction(objectName, strlen(objectName));

	ObjectList* objectList;
	if (Scene::ObjectLists->Exists(objectNameHash)) {
		objectList = Scene::ObjectLists->Get(objectNameHash);
	}
	else {
		objectList = Scene::NewObjectList(objectName);
		Scene::ObjectLists->Put(objectNameHash, objectList);

		if (callListLoadFunction) {
			ScriptManager::CallFunction(objectList->LoadFunctionName);
		}
	}

	return objectList;
}
ObjectList* Scene::GetObjectList(const char* objectName) {
	return GetObjectList(objectName, true);
}
ObjectList* Scene::GetStaticObjectList(const char* objectName) {
	ObjectList* objectList;

	if (Scene::StaticObjectLists->Exists(objectName)) {
		objectList = Scene::StaticObjectLists->Get(objectName);
	}
	else if (Scene::ObjectLists->Exists(objectName)) {
		// There isn't a static object list with this name, but
		// we can check if there's a regular one. If so, we
		// just use it, and then put it in the static object
		// list hash map. This is all so that object
		// persistency can work.
		objectList = Scene::ObjectLists->Get(objectName);
		Scene::StaticObjectLists->Put(objectName, objectList);
	}
	else {
		// Create a new object list
		objectList = Scene::NewObjectList(objectName);
		Scene::StaticObjectLists->Put(objectName, objectList);
		Scene::ObjectLists->Put(objectName, objectList);
	}

	return objectList;
}
void Scene::SpawnStaticObject(const char* objectName) {
	ObjectList* objectList = Scene::GetObjectList(objectName, false);
	if (objectList->SpawnFunction) {
		Entity* obj = objectList->Spawn();
		if (!obj) {
			return;
		}
		obj->X = 0.0f;
		obj->Y = 0.0f;
		obj->InitialX = obj->X;
		obj->InitialY = obj->Y;
		obj->List = objectList;
		Scene::AddStatic(objectList, obj);
	}
}
void Scene::AddManagers() {
	Scene::SpawnStaticObject("WindowManager");
	Scene::SpawnStaticObject("InputManager");
	Scene::SpawnStaticObject("FadeManager");
}

void Scene::FreePriorityLists() {
	if (Scene::PriorityLists) {
		for (int i = Scene::PriorityPerLayer - 1; i >= 0; i--) {
			Scene::PriorityLists[i].Dispose();
		}
		Memory::Free(Scene::PriorityLists);
	}
	Scene::PriorityLists = NULL;
	Scene::PriorityPerLayer = 0;
}
void Scene::InitPriorityLists() {
	if (Scene::PriorityPerLayer == 0) {
		Scene::PriorityPerLayer = Scene::BasePriorityPerLayer;
	}

	if (Scene::PriorityLists) {
		for (int i = Scene::PriorityPerLayer - 1; i >= 0; i--) {
			Scene::PriorityLists[i].Dispose();
		}
	}
	else {
		Scene::PriorityLists = (DrawGroupList*)Memory::TrackedCalloc(
			"Scene::PriorityLists", Scene::PriorityPerLayer, sizeof(DrawGroupList));
		if (!Scene::PriorityLists) {
			Log::Print(Log::LOG_ERROR, "Out of memory for priority lists!");
			exit(-1);
		}
	}

	for (int i = Scene::PriorityPerLayer - 1; i >= 0; i--) {
		Scene::PriorityLists[i].Init();
	}
}
void Scene::SetPriorityPerLayer(int count) {
	if (count < 1) {
		count = 1;
	}
	else if (count > 256) {
		count = 256;
	}

	int currentCount = Scene::PriorityPerLayer;
	if (count < currentCount) {
		for (int i = count; i < currentCount; i++) {
			Scene::PriorityLists[i].Dispose();
		}
	}
	else if (count > currentCount) {
		Scene::PriorityLists = (DrawGroupList*)Memory::Realloc(
			Scene::PriorityLists, Scene::PriorityPerLayer * sizeof(DrawGroupList));
		for (int i = currentCount; i < count; i++) {
			Scene::PriorityLists[i].Init();
		}
	}

	Scene::PriorityPerLayer = count;
}

void Scene::ReadRSDKTile(TileConfig* tile, Uint8* line) {
	int bufferPos = 0;

	Uint8 collisionHeights[16];
	Uint8 collisionActive[16];

	TileConfig *tileDest, *tileLast;

	// Copy collision
	memcpy(collisionHeights, &line[bufferPos], TileWidth);
	bufferPos += TileWidth;
	memcpy(collisionActive, &line[bufferPos], TileWidth);
	bufferPos += TileWidth;

	bool isCeiling;
	tile->IsCeiling = isCeiling = line[bufferPos++];
	tile->AngleTop = line[bufferPos++];
	tile->AngleLeft = line[bufferPos++];
	tile->AngleRight = line[bufferPos++];
	tile->AngleBottom = line[bufferPos++];
	tile->Behavior = line[bufferPos++];

	Uint8* col;
	// Interpret up/down collision
	if (isCeiling) {
		col = &collisionHeights[0];
		for (int c = 0; c < TileWidth; c++) {
			if (collisionActive[c]) {
				tile->CollisionTop[c] = 0x00;
				tile->CollisionBottom[c] = *col;
			}
			else {
				tile->CollisionTop[c] = 0xFF;
				tile->CollisionBottom[c] = 0xFF;
			}
			col++;
		}

		// Left Wall rotations
		// TODO: Should be Width or Height?
		for (int c = 0; c < TileWidth; ++c) {
			int h = 0;
			while (true) {
				if (h == TileWidth) {
					tile->CollisionLeft[c] = 0xFF;
					break;
				}
				Uint8 m = tile->CollisionBottom[h];
				if (m != 0xFF && c <= m) {
					tile->CollisionLeft[c] = h;
					break;
				}
				else {
					++h;
					if (h <= -1) {
						break;
					}
				}
			}
		}

		// Right Wall rotations
		// TODO: Should be Width or Height?
		for (int c = 0; c < TileWidth; ++c) {
			int h = TileWidth - 1;
			while (true) {
				if (h == -1) {
					tile->CollisionRight[c] = 0xFF;
					break;
				}

				Uint8 m = tile->CollisionBottom[h];
				if (m != 0xFF && c <= m) {
					tile->CollisionRight[c] = h;
					break;
				}
				else {
					--h;
					if (h >= TileWidth) {
						break;
					}
				}
			}
		}
	}
	else {
		col = &collisionHeights[0];
		for (int c = 0; c < TileWidth; c++) {
			if (collisionActive[c]) {
				tile->CollisionTop[c] = *col;
				tile->CollisionBottom[c] = 0x0F;
			}
			else {
				tile->CollisionTop[c] = 0xFF;
				tile->CollisionBottom[c] = 0xFF;
			}
			col++;
		}

		// Left Wall rotations
		// TODO: Should be Width or Height?
		for (int c = 0; c < TileWidth; ++c) {
			int h = 0;
			while (true) {
				if (h == TileWidth) {
					tile->CollisionLeft[c] = 0xFF;
					break;
				}
				Uint8 m = tile->CollisionTop[h];
				if (m != 0xFF && c >= m) {
					tile->CollisionLeft[c] = h;
					break;
				}
				else {
					++h;
					if (h <= -1) {
						break;
					}
				}
			}
		}

		// Right Wall rotations
		// TODO: Should be Width or Height?
		for (int c = 0; c < TileWidth; ++c) {
			int h = TileWidth - 1;
			while (true) {
				if (h == -1) {
					tile->CollisionRight[c] = 0xFF;
					break;
				}

				Uint8 m = tile->CollisionTop[h];
				if (m != 0xFF && c >= m) {
					tile->CollisionRight[c] = h;
					break;
				}
				else {
					--h;
					if (h >= TileWidth) {
						break;
					}
				}
			}
		}
	}

	// Flip X
	tileDest = tile + Scene::TileCount;
	tileDest->AngleTop = -tile->AngleTop;
	tileDest->AngleLeft = -tile->AngleRight;
	tileDest->AngleRight = -tile->AngleLeft;
	tileDest->AngleBottom = -tile->AngleBottom;
	for (int xD = 0, xS = 15; xD <= 15; xD++, xS--) {
		tileDest->CollisionTop[xD] = tile->CollisionTop[xS];
		tileDest->CollisionBottom[xD] = tile->CollisionBottom[xS];
		// Swaps
		tileDest->CollisionLeft[xD] = tile->CollisionRight[xD] ^ 15;
		tileDest->CollisionRight[xD] = tile->CollisionLeft[xD] ^ 15;
	}

	// Flip Y
	tileDest = tile + (Scene::TileCount << 1);
	tileDest->AngleTop = 0x80 - tile->AngleBottom;
	tileDest->AngleLeft = 0x80 - tile->AngleLeft;
	tileDest->AngleRight = 0x80 - tile->AngleRight;
	tileDest->AngleBottom = 0x80 - tile->AngleTop;
	for (int xD = 0, xS = 15; xD <= 15; xD++, xS--) {
		tileDest->CollisionLeft[xD] = tile->CollisionLeft[xS];
		tileDest->CollisionRight[xD] = tile->CollisionRight[xS];
		// Swaps
		tileDest->CollisionTop[xD] = tile->CollisionBottom[xD] ^ 15;
		tileDest->CollisionBottom[xD] = tile->CollisionTop[xD] ^ 15;
	}

	// Flip XY
	tileLast = tileDest;
	tileDest = tile + (Scene::TileCount << 1) + Scene::TileCount;
	tileDest->AngleTop = -tileLast->AngleTop;
	tileDest->AngleLeft = -tileLast->AngleRight;
	tileDest->AngleRight = -tileLast->AngleLeft;
	tileDest->AngleBottom = -tileLast->AngleBottom;
	for (int xD = 0, xS = 15; xD <= 15; xD++, xS--) {
		tileDest->CollisionTop[xD] = tile->CollisionBottom[xS] ^ 15;
		tileDest->CollisionLeft[xD] = tile->CollisionRight[xS] ^ 15;
		tileDest->CollisionRight[xD] = tile->CollisionLeft[xS] ^ 15;
		tileDest->CollisionBottom[xD] = tile->CollisionTop[xS] ^ 15;
	}
}
void Scene::LoadRSDKTileConfig(int tilesetID, Stream* tileColReader) {
	Uint32 tileCount = 0x400;

	Uint8* tileData = (Uint8*)Memory::Calloc(1, tileCount * 2 * 0x26);
	Uint8* tileInfo = tileData;
	tileColReader->ReadCompressed(tileInfo);

	Scene::TileCfgLoaded = true;

	// Read plane A
	TileConfig* tile = &Scene::TileCfg[0][0];

	for (Uint32 i = 0; i < tileCount; i++) {
		Uint8* line = &tileInfo[i * 38];

		ReadRSDKTile(tile, line);

		tile++;
	}

	// Read plane B
	tileInfo += tileCount * 38;
	tile = &Scene::TileCfg[1][0];

	for (Uint32 i = 0; i < tileCount; i++) {
		Uint8* line = &tileInfo[i * 38];

		ReadRSDKTile(tile, line);

		tile++;
	}

	Memory::Free(tileData);
}
void Scene::LoadHCOLTileConfig(size_t tilesetID, Stream* tileColReader) {
	if (!Scene::Tilesets.size() || tilesetID >= Scene::Tilesets.size()) {
		return;
	}

	Uint32 tileCount = tileColReader->ReadUInt32();
	Uint8 tileSize = tileColReader->ReadByte();
	tileColReader->ReadByte();
	tileColReader->ReadByte();
	tileColReader->ReadByte();
	tileColReader->ReadUInt32();

	size_t tileStart = Scene::Tilesets[tilesetID].StartTile;
	size_t numTiles = Scene::Tilesets[tilesetID].TileCount;
	size_t tilesToRead = tileCount;

	if (tileCount < numTiles - 1) {
		if (tileCount != numTiles) {
			Log::Print(Log::LOG_WARN,
				"There are less tile collisions (%d) than actual tiles! (%d)",
				tileCount,
				numTiles);
		}
		tileCount = numTiles - 1;
	}
	else if (tileCount >= numTiles) {
		if (tileCount != numTiles) {
			Log::Print(Log::LOG_WARN,
				"There are more tile collisions (%d) than actual tiles! (%d)",
				tileCount,
				numTiles);
		}
		tileCount = numTiles - 1;
	}

	if (tilesToRead >= numTiles) {
		tilesToRead = numTiles - 1;
	}

	Scene::TileCfgLoaded = true;

	// Read it now
	Uint8 collisionBuffer[16];
	TileConfig *tile, *tileDest, *tileLast;
	TileConfig* tileBase = &Scene::TileCfg[0][tileStart];
	TileConfig* maxTile = &Scene::TileCfg[0][tileStart + tilesToRead];

	for (tile = tileBase; tile < maxTile; tile++) {
		tile->IsCeiling = tileColReader->ReadByte();
		tile->AngleTop = tileColReader->ReadByte();
		bool hasCollision = tileColReader->ReadByte();

		// Read collision
		tileColReader->ReadBytes(collisionBuffer, tileSize);

		Uint8* col;
		// Interpret up/down collision
		if (tile->IsCeiling) {
			col = &collisionBuffer[0];
			for (int c = 0; c < 16; c++) {
				if (hasCollision && *col < tileSize) {
					tile->CollisionTop[c] = 0;
					tile->CollisionBottom[c] = *col ^ 15;
				}
				else {
					tile->CollisionTop[c] = tile->CollisionBottom[c] = 0xFF;
				}
				col++;
			}

			// Interpret left/right collision
			for (int y = 15; y >= 0; y--) {
				// Left-to-right check
				for (int x = 0; x <= 15; x++) {
					Uint8 data = tile->CollisionBottom[x];
					if (data != 0xFF && data >= y) {
						tile->CollisionLeft[y] = x;
						goto HCOL_COLLISION_LINE_LEFT_BOTTOMUP_FOUND;
					}
				}
				tile->CollisionLeft[y] = 0xFF;

			HCOL_COLLISION_LINE_LEFT_BOTTOMUP_FOUND:

				// Right-to-left check
				for (int x = 15; x >= 0; x--) {
					Uint8 data = tile->CollisionBottom[x];
					if (data != 0xFF && data >= y) {
						tile->CollisionRight[y] = x;
						goto HCOL_COLLISION_LINE_RIGHT_BOTTOMUP_FOUND;
					}
				}
				tile->CollisionRight[y] = 0xFF;

			HCOL_COLLISION_LINE_RIGHT_BOTTOMUP_FOUND:;
			}
		}
		else {
			col = &collisionBuffer[0];
			for (int c = 0; c < 16; c++) {
				if (hasCollision && *col < tileSize) {
					tile->CollisionTop[c] = *col;
					tile->CollisionBottom[c] = 15;
				}
				else {
					tile->CollisionTop[c] = tile->CollisionBottom[c] = 0xFF;
				}
				col++;
			}

			// Interpret left/right collision
			for (int y = 0; y <= 15; y++) {
				// Left-to-right check
				for (int x = 0; x <= 15; x++) {
					Uint8 data = tile->CollisionTop[x];
					if (data != 0xFF && data <= y) {
						tile->CollisionLeft[y] = x;
						goto HCOL_COLLISION_LINE_LEFT_TOPDOWN_FOUND;
					}
				}
				tile->CollisionLeft[y] = 0xFF;

			HCOL_COLLISION_LINE_LEFT_TOPDOWN_FOUND:

				// Right-to-left check
				for (int x = 15; x >= 0; x--) {
					Uint8 data = tile->CollisionTop[x];
					if (data != 0xFF && data <= y) {
						tile->CollisionRight[y] = x;
						goto HCOL_COLLISION_LINE_RIGHT_TOPDOWN_FOUND;
					}
				}
				tile->CollisionRight[y] = 0xFF;

			HCOL_COLLISION_LINE_RIGHT_TOPDOWN_FOUND:;
			}
		}

		// Interpret angles
		Uint8 angle = tile->AngleTop;
		if (angle == 0xFF) {
			tile->AngleTop = 0x00; // Top
			tile->AngleLeft = 0xC0; // Left
			tile->AngleRight = 0x40; // Right
			tile->AngleBottom = 0x80; // Bottom
		}
		else {
			if (tile->IsCeiling) {
				tile->AngleTop = 0x00;
				tile->AngleLeft = angle >= 0x81 && angle <= 0xB6 ? angle : 0xC0;
				tile->AngleRight = angle >= 0x4A && angle <= 0x7F ? angle : 0x40;
				tile->AngleBottom = angle;
			}
			else {
				tile->AngleTop = angle;
				tile->AngleLeft = angle >= 0xCA && angle <= 0xF6 ? angle : 0xC0;
				tile->AngleRight = angle >= 0x0A && angle <= 0x36 ? angle : 0x40;
				tile->AngleBottom = 0x80;
			}
		}

		// Flip X
		tileDest = tile + Scene::TileCount;
		tileDest->AngleTop = -tile->AngleTop;
		tileDest->AngleLeft = -tile->AngleRight;
		tileDest->AngleRight = -tile->AngleLeft;
		tileDest->AngleBottom = -tile->AngleBottom;
		for (int xD = 0, xS = 15; xD <= 15; xD++, xS--) {
			tileDest->CollisionTop[xD] = tile->CollisionTop[xS];
			tileDest->CollisionBottom[xD] = tile->CollisionBottom[xS];
			// Swaps
			tileDest->CollisionLeft[xD] = tile->CollisionRight[xD] ^ 15;
			tileDest->CollisionRight[xD] = tile->CollisionLeft[xD] ^ 15;
		}
		// Flip Y
		tileDest = tile + (Scene::TileCount << 1);
		tileDest->AngleTop = 0x80 - tile->AngleBottom;
		tileDest->AngleLeft = 0x80 - tile->AngleLeft;
		tileDest->AngleRight = 0x80 - tile->AngleRight;
		tileDest->AngleBottom = 0x80 - tile->AngleTop;
		for (int xD = 0, xS = 15; xD <= 15; xD++, xS--) {
			tileDest->CollisionLeft[xD] = tile->CollisionLeft[xS];
			tileDest->CollisionRight[xD] = tile->CollisionRight[xS];
			// Swaps
			tileDest->CollisionTop[xD] = tile->CollisionBottom[xD] ^ 15;
			tileDest->CollisionBottom[xD] = tile->CollisionTop[xD] ^ 15;
		}
		// Flip XY
		tileLast = tileDest;
		tileDest = tile + (Scene::TileCount << 1) + Scene::TileCount;
		tileDest->AngleTop = -tileLast->AngleTop;
		tileDest->AngleLeft = -tileLast->AngleRight;
		tileDest->AngleRight = -tileLast->AngleLeft;
		tileDest->AngleBottom = -tileLast->AngleBottom;
		for (int xD = 0, xS = 15; xD <= 15; xD++, xS--) {
			tileDest->CollisionTop[xD] = tile->CollisionBottom[xS] ^ 15;
			tileDest->CollisionLeft[xD] = tile->CollisionRight[xS] ^ 15;
			tileDest->CollisionRight[xD] = tile->CollisionLeft[xS] ^ 15;
			tileDest->CollisionBottom[xD] = tile->CollisionTop[xS] ^ 15;
		}
	}

	// Copy over to the other planes
	for (size_t i = 1; i < Scene::TileCfg.size(); i++) {
		TileConfig* cfgA = &Scene::TileCfg[0][tileStart];
		TileConfig* cfgB = &Scene::TileCfg[i][tileStart];
		for (size_t j = 0; j < 3; j++) {
			memcpy(cfgB, cfgA, tileCount * sizeof(TileConfig));
			cfgA += Scene::TileCount;
			cfgB += Scene::TileCount;
		}
	}
}
void Scene::InitTileCollisions() {
	if (Scene::TileCount == 0) {
		size_t tileCount = 0x400;

		if (Scene::TileSpriteInfos.size()) {
			tileCount = Scene::TileSpriteInfos.size();
		}

		Scene::TileCount = tileCount;
	}

	Scene::BaseTileCount = Scene::TileCount;
	Scene::BaseTilesetCount = Scene::Tilesets.size();

	size_t totalTileVariantCount = Scene::TileCount
		<< 2; // multiplied by 4: For all combinations of tile
	// flipping

	TileConfig* tileCfgA = (TileConfig*)Memory::TrackedCalloc(
		"Scene::TileCfgA", totalTileVariantCount, sizeof(TileConfig));
	TileConfig* tileCfgB = (TileConfig*)Memory::TrackedCalloc(
		"Scene::TileCfgB", totalTileVariantCount, sizeof(TileConfig));

	ClearTileCollisions(tileCfgA, totalTileVariantCount);

	memcpy(tileCfgB, tileCfgA, totalTileVariantCount * sizeof(TileConfig));

	Scene::TileCfg.push_back(tileCfgA);
	Scene::TileCfg.push_back(tileCfgB);
}
void Scene::ClearTileCollisions(TileConfig* cfg, size_t numTiles) {
	for (size_t i = 0; i < numTiles; i++) {
		TileConfig* tile = &cfg[i];
		memset(tile->CollisionTop, 16, 16);
		memset(tile->CollisionBottom, 16, 16);
		memset(tile->CollisionLeft, 16, 16);
		memset(tile->CollisionRight, 16, 16);
	}
}
bool Scene::AddTileset(char* path) {
	ISprite* tileSprite = new ISprite();
	Texture* spriteSheet = tileSprite->AddSpriteSheet(path);
	if (!spriteSheet) {
		delete tileSprite;
		return false;
	}

	int cols = spriteSheet->Width / Scene::TileWidth;
	int rows = spriteSheet->Height / Scene::TileHeight;

	tileSprite->ReserveAnimationCount(1);
	tileSprite->AddAnimation("TileSprite", 0, 0, cols * rows);

	Tileset sceneTileset(tileSprite,
		Scene::TileWidth,
		Scene::TileHeight,
		0,
		Scene::TileSpriteInfos.size(),
		cols * rows,
		path);
	Scene::Tilesets.push_back(sceneTileset);

	// Add tiles
	TileSpriteInfo info;
	for (int i = 0; i < cols * rows; i++) {
		info.Sprite = tileSprite;
		info.AnimationIndex = 0;
		info.FrameIndex = (int)tileSprite->Animations[0].Frames.size();
		info.TilesetID = Scene::Tilesets.size() - 1;
		Scene::TileSpriteInfos.push_back(info);

		tileSprite->AddFrame(0,
			(i % cols) * Scene::TileWidth,
			(i / cols) * Scene::TileHeight,
			Scene::TileWidth,
			Scene::TileHeight,
			-Scene::TileWidth / 2,
			-Scene::TileHeight / 2);
	}

	tileSprite->RefreshGraphicsID();

	Scene::SetTileCount(Scene::TileCount + (cols * rows));

	return true;
}
void Scene::SetTileCount(size_t tileCount) {
	vector<TileConfig*> configFlipX;
	vector<TileConfig*> configFlipY;
	vector<TileConfig*> configFlipXY;

	size_t copySize = ((tileCount > Scene::TileCount) ? Scene::TileCount : tileCount) *
		sizeof(TileConfig);

	for (size_t i = 0; i < Scene::TileCfg.size(); i++) {
		TileConfig* srcCfg = Scene::TileCfg[i];

		TileConfig* flipX = (TileConfig*)Memory::Malloc(copySize);
		TileConfig* flipY = (TileConfig*)Memory::Malloc(copySize);
		TileConfig* flipXY = (TileConfig*)Memory::Malloc(copySize);

		memcpy(flipX, srcCfg, copySize);
		memcpy(flipY, srcCfg + Scene::TileCount, copySize);
		memcpy(flipXY, srcCfg + (Scene::TileCount * 2), copySize);

		configFlipX.push_back(flipX);
		configFlipY.push_back(flipY);
		configFlipXY.push_back(flipXY);
	}

	size_t totalTileVariantCount = tileCount
		<< 2; // multiplied by 4: For all combinations of tile
	// flipping

	for (size_t i = 0; i < Scene::TileCfg.size(); i++) {
		Scene::TileCfg[i] = (TileConfig*)Memory::Realloc(
			Scene::TileCfg[i], totalTileVariantCount * sizeof(TileConfig));

		TileConfig* destCfg = Scene::TileCfg[i];

		ClearTileCollisions(destCfg, totalTileVariantCount);

		TileConfig* flipX = configFlipX[i];
		TileConfig* flipY = configFlipY[i];
		TileConfig* flipXY = configFlipXY[i];

		memcpy(destCfg, flipX, copySize);
		memcpy(destCfg + tileCount, flipY, copySize);
		memcpy(destCfg + (tileCount * 2), flipXY, copySize);

		Memory::Free(flipX);
		Memory::Free(flipY);
		Memory::Free(flipXY);
	}

	Scene::TileCount = tileCount;
}
void Scene::LoadTileCollisions(const char* filename, size_t tilesetID) {
	if (!ResourceManager::ResourceExists(filename)) {
		Log::Print(Log::LOG_WARN, "Could not find tile collision file \"%s\"!", filename);
		return;
	}

	Stream* tileColReader = ResourceStream::New(filename);
	if (!tileColReader) {
		return;
	}

	Uint32 magic = tileColReader->ReadUInt32();
	// RSDK TileConfig
	if (magic == 0x004C4954U) {
		Scene::LoadRSDKTileConfig(tilesetID, tileColReader);
	}
	// HCOL TileConfig
	else if (magic == 0x4C4F4354U) {
		Scene::LoadHCOLTileConfig(tilesetID, tileColReader);
	}
	else {
		Log::Print(Log::LOG_ERROR, "Invalid magic for TileCollisions! %X", magic);
	}

	tileColReader->Close();
}
void Scene::UnloadTileCollisions() {
	for (size_t i = 0; i < Scene::TileCfg.size(); i++) {
		Memory::Free(Scene::TileCfg[i]);
	}

	Scene::TileCfg.clear();
	Scene::TileCfgLoaded = false;
	Scene::TileCount = 0;
}

// Resource Management
// return true if we found it in the list
bool Scene::GetResourceListSpace(vector<ResourceType*>* list,
	ResourceType* resource,
	size_t& index,
	bool& foundEmpty) {
	foundEmpty = false;
	index = list->size();
	for (size_t i = 0, listSz = list->size(); i < listSz; i++) {
		if (!(*list)[i]) {
			if (!foundEmpty) {
				foundEmpty = true;
				index = i;
			}
			continue;
		}
		if ((*list)[i]->FilenameHash == resource->FilenameHash) {
			index = i;
			delete resource;
			return true;
		}
	}
	return false;
}

bool Scene::GetResource(vector<ResourceType*>* list, ResourceType* resource, size_t& index) {
	bool foundEmpty = false;
	if (GetResourceListSpace(list, resource, index, foundEmpty)) {
		return true;
	}
	else if (foundEmpty) {
		(*list)[index] = resource;
	}
	else {
		list->push_back(resource);
	}
	return false;
}

int Scene::LoadSpriteResource(const char* filename, int unloadPolicy) {
	ResourceType* resource = new (std::nothrow) ResourceType();
	resource->FilenameHash = CRC32::EncryptString(filename);
	resource->UnloadPolicy = unloadPolicy;

	size_t index = 0;
	vector<ResourceType*>* list = &Scene::SpriteList;
	if (Scene::GetResource(list, resource, index)) {
		return (int)index;
	}

	resource->AsSprite = new (std::nothrow) ISprite(filename);
	if (resource->AsSprite->LoadFailed) {
		delete resource->AsSprite;
		delete resource;
		(*list)[index] = NULL;
		return -1;
	}

	return (int)index;
}
int Scene::LoadImageResource(const char* filename, int unloadPolicy) {
	ResourceType* resource = new (std::nothrow) ResourceType();
	resource->FilenameHash = CRC32::EncryptString(filename);
	resource->UnloadPolicy = unloadPolicy;

	size_t index = 0;
	vector<ResourceType*>* list = &Scene::ImageList;
	if (Scene::GetResource(list, resource, index)) {
		return (int)index;
	}

	resource->AsImage = new (std::nothrow) Image(filename);
	if (!resource->AsImage->TexturePtr) {
		delete resource->AsImage;
		delete resource;
		(*list)[index] = NULL;
		return -1;
	}

	resource->AsImage->ID = (int)index;

	return (int)index;
}
int Scene::LoadFontResource(const char* filename, int pixel_sz, int unloadPolicy) {
	ResourceType* resource = new (std::nothrow) ResourceType();
	resource->FilenameHash = CRC32::EncryptString(filename);
	resource->FilenameHash = CRC32::EncryptData(&pixel_sz, sizeof(int), resource->FilenameHash);
	resource->UnloadPolicy = unloadPolicy;

	size_t index = 0;
	vector<ResourceType*>* list = &Scene::SpriteList;
	if (Scene::GetResource(list, resource, index)) {
		return (int)index;
	}

	ResourceStream* stream = ResourceStream::New(filename);
	if (!stream) {
		delete resource;
		(*list)[index] = NULL;
		return -1;
	}

	resource->AsSprite = FontFace::SpriteFromFont(stream, pixel_sz, filename);

	stream->Close();

	if (resource->AsSprite->LoadFailed) {
		delete resource->AsSprite;
		delete resource;
		(*list)[index] = NULL;
		return -1;
	}

	return (int)index;
}
int Scene::LoadModelResource(const char* filename, int unloadPolicy) {
	ResourceType* resource = new (std::nothrow) ResourceType();
	resource->FilenameHash = CRC32::EncryptString(filename);
	resource->UnloadPolicy = unloadPolicy;

	size_t index = 0;
	vector<ResourceType*>* list = &Scene::ModelList;
	if (Scene::GetResource(list, resource, index)) {
		return (int)index;
	}

	ResourceStream* stream = ResourceStream::New(filename);
	if (!stream) {
		Log::Print(Log::LOG_ERROR, "Could not read resource \"%s\"!", filename);
		delete resource;
		(*list)[index] = NULL;
		return -1;
	}

	resource->AsModel = new (std::nothrow) IModel();

	if (!resource->AsModel->Load(stream, filename)) {
		delete resource->AsModel;
		delete resource;
		list->pop_back();
		stream->Close();
		(*list)[index] = NULL;
		return -1;
	}

	stream->Close();

	return (int)index;
}
int Scene::LoadMusicResource(const char* filename, int unloadPolicy) {
	ResourceType* resource = new (std::nothrow) ResourceType();
	resource->FilenameHash = CRC32::EncryptString(filename);
	resource->UnloadPolicy = unloadPolicy;

	size_t index = 0;
	vector<ResourceType*>* list = &Scene::MusicList;
	if (Scene::GetResource(list, resource, index)) {
		return (int)index;
	}

	ResourceStream* stream = ResourceStream::New(filename);
	if (!stream) {
		Log::Print(Log::LOG_ERROR, "Could not read resource \"%s\"!", filename);
		delete resource;
		(*list)[index] = NULL;
		return -1;
	}

	resource->AsMusic = new (std::nothrow) ISound(filename);
	if (resource->AsMusic->LoadFailed) {
		delete resource->AsMusic;
		delete resource;
		stream->Close();
		(*list)[index] = NULL;
		return -1;
	}

	stream->Close();

	return (int)index;
}
int Scene::LoadSoundResource(const char* filename, int unloadPolicy) {
	ResourceType* resource = new (std::nothrow) ResourceType();
	resource->FilenameHash = CRC32::EncryptString(filename);
	resource->UnloadPolicy = unloadPolicy;

	size_t index = 0;
	vector<ResourceType*>* list = &Scene::SoundList;
	if (Scene::GetResource(list, resource, index)) {
		return (int)index;
	}

	resource->AsSound = new (std::nothrow) ISound(filename);
	if (resource->AsSound->LoadFailed) {
		delete resource->AsSound;
		delete resource;
		(*list)[index] = NULL;
		return -1;
	}

	return (int)index;
}
int Scene::LoadVideoResource(const char* filename, int unloadPolicy) {
#ifdef USING_FFMPEG
	ResourceType* resource = new (std::nothrow) ResourceType();
	resource->FilenameHash = CRC32::EncryptString(filename);
	resource->UnloadPolicy = unloadPolicy;

	size_t index = 0;
	vector<ResourceType*>* list = &Scene::MediaList;
	if (Scene::GetResource(list, resource, index)) {
		return (int)index;
	}

	Texture* VideoTexture = NULL;
	MediaSource* Source = NULL;
	MediaPlayer* Player = NULL;

	Stream* stream = ResourceStream::New(filename);
	if (!stream) {
		Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", filename);
		delete resource;
		(*list)[index] = NULL;
		return -1;
	}

	Source = MediaSource::CreateSourceFromStream(stream);
	if (!Source) {
		delete resource;
		stream->Close();
		(*list)[index] = NULL;
		return -1;
	}

	Player = MediaPlayer::Create(Source,
		Source->GetBestStream(MediaSource::STREAMTYPE_VIDEO),
		Source->GetBestStream(MediaSource::STREAMTYPE_AUDIO),
		Source->GetBestStream(MediaSource::STREAMTYPE_SUBTITLE),
		Scene::Views[0].Width,
		Scene::Views[0].Height);
	if (!Player) {
		Source->Close();
		delete resource;
		(*list)[index] = NULL;
		return -1;
	}

	PlayerInfo playerInfo;
	Player->GetInfo(&playerInfo);
	VideoTexture = Graphics::CreateTexture(playerInfo.Video.Output.Format,
		SDL_TEXTUREACCESS_STATIC,
		playerInfo.Video.Output.Width,
		playerInfo.Video.Output.Height);
	if (!VideoTexture) {
		Player->Close();
		Source->Close();
		delete resource;
		(*list)[index] = NULL;
		return -1;
	}

	if (Player->GetVideoStream() > -1) {
		Log::Print(Log::LOG_WARN, "VIDEO STREAM:");
		Log::Print(Log::LOG_INFO,
			"    Resolution:  %d x %d",
			playerInfo.Video.Output.Width,
			playerInfo.Video.Output.Height);
	}
	if (Player->GetAudioStream() > -1) {
		Log::Print(Log::LOG_WARN, "AUDIO STREAM:");
		Log::Print(
			Log::LOG_INFO, "    Sample Rate: %d", playerInfo.Audio.Output.SampleRate);
		Log::Print(Log::LOG_INFO,
			"    Bit Depth:   %d-bit",
			playerInfo.Audio.Output.Format & 0xFF);
		Log::Print(Log::LOG_INFO, "    Channels:    %d", playerInfo.Audio.Output.Channels);
	}

	MediaBag* newMediaBag = new (std::nothrow) MediaBag;
	newMediaBag->Source = Source;
	newMediaBag->Player = Player;
	newMediaBag->VideoTexture = VideoTexture;

	resource->AsMedia = newMediaBag;
	return (int)index;
#else
	return -1;
#endif
}

ResourceType* Scene::GetSpriteResource(int index) {
	if (index < 0 || index >= (int)Scene::SpriteList.size()) {
		return NULL;
	}

	if (!Scene::SpriteList[index]) {
		return NULL;
	}

	return Scene::SpriteList[index];
}
ResourceType* Scene::GetImageResource(int index) {
	if (index < 0 || index >= (int)Scene::ImageList.size()) {
		return NULL;
	}

	if (!Scene::ImageList[index]) {
		return NULL;
	}

	return Scene::ImageList[index];
}

void Scene::DisposeInScope(Uint32 scope) {
	// Models
	for (size_t i = 0, i_sz = Scene::ModelList.size(); i < i_sz; i++) {
		if (!Scene::ModelList[i]) {
			continue;
		}
		if (Scene::ModelList[i]->UnloadPolicy > scope) {
			continue;
		}

		delete Scene::ModelList[i]->AsModel;
		delete Scene::ModelList[i];
		Scene::ModelList[i] = NULL;
	}
	// Images
	for (size_t i = 0, i_sz = Scene::ImageList.size(); i < i_sz; i++) {
		if (!Scene::ImageList[i]) {
			continue;
		}
		if (Scene::ImageList[i]->UnloadPolicy > scope) {
			continue;
		}
		if (Scene::ImageList[i]->AsImage->References > 1) {
			continue;
		}

		delete Scene::ImageList[i]->AsImage;
		delete Scene::ImageList[i];
		Scene::ImageList[i] = NULL;
	}
	// Sprites
	for (size_t i = 0, i_sz = Scene::SpriteList.size(); i < i_sz; i++) {
		if (!Scene::SpriteList[i]) {
			continue;
		}
		if (Scene::SpriteList[i]->UnloadPolicy > scope) {
			continue;
		}

		delete Scene::SpriteList[i]->AsSprite;
		delete Scene::SpriteList[i];
		Scene::SpriteList[i] = NULL;
	}
	// Sounds
	for (size_t i = 0, i_sz = Scene::SoundList.size(); i < i_sz; i++) {
		if (!Scene::SoundList[i]) {
			continue;
		}
		if (Scene::SoundList[i]->UnloadPolicy > scope) {
			continue;
		}

		AudioManager::AudioRemove(Scene::SoundList[i]->AsSound);

		Scene::SoundList[i]->AsSound->Dispose();
		delete Scene::SoundList[i]->AsSound;
		delete Scene::SoundList[i];
		Scene::SoundList[i] = NULL;
	}
	// Music
	for (size_t i = 0, i_sz = Scene::MusicList.size(); i < i_sz; i++) {
		if (!Scene::MusicList[i]) {
			continue;
		}
		if (Scene::MusicList[i]->UnloadPolicy > scope) {
			continue;
		}

		AudioManager::RemoveMusic(Scene::MusicList[i]->AsMusic);

		Scene::MusicList[i]->AsMusic->Dispose();
		delete Scene::MusicList[i]->AsMusic;
		delete Scene::MusicList[i];
		Scene::MusicList[i] = NULL;
	}
	// Media
	AudioManager::Lock();
	for (size_t i = 0, i_sz = Scene::MediaList.size(); i < i_sz; i++) {
		if (!Scene::MediaList[i]) {
			continue;
		}
		if (Scene::MediaList[i]->UnloadPolicy > scope) {
			continue;
		}

#ifdef USING_FFMPEG
		Scene::MediaList[i]->AsMedia->Player->Close();
		Scene::MediaList[i]->AsMedia->Source->Close();
#endif
		delete Scene::MediaList[i]->AsMedia;

		delete Scene::MediaList[i];
		Scene::MediaList[i] = NULL;
	}
	AudioManager::Unlock();
	// Textures
	for (size_t i = 0, i_sz = Scene::TextureList.size(); i < i_sz; i++) {
		if (!Scene::TextureList[i]) {
			continue;
		}
		if (Scene::TextureList[i]->UnloadPolicy > scope) {
			continue;
		}
		delete Scene::TextureList[i];
		Scene::TextureList[i] = NULL;
	}
	// Animators
	for (size_t i = 0, i_sz = Scene::AnimatorList.size(); i < i_sz; i++) {
		if (!Scene::AnimatorList[i]) {
			continue;
		}
		if (Scene::AnimatorList[i]->UnloadPolicy > scope) {
			continue;
		}

		delete Scene::AnimatorList[i];
		Scene::AnimatorList[i] = NULL;
	}
}
void Scene::Dispose() {
	for (int i = 0; i < MAX_SCENE_VIEWS; i++) {
		if (Scene::Views[i].DrawTarget) {
			Graphics::DisposeTexture(Scene::Views[i].DrawTarget);
			Scene::Views[i].DrawTarget = NULL;
		}

		if (Scene::Views[i].ProjectionMatrix) {
			delete Scene::Views[i].ProjectionMatrix;
			Scene::Views[i].ProjectionMatrix = NULL;
		}

		if (Scene::Views[i].BaseProjectionMatrix) {
			delete Scene::Views[i].BaseProjectionMatrix;
			Scene::Views[i].BaseProjectionMatrix = NULL;
		}

		if (Scene::Views[i].UseStencil) {
			Scene::Views[i].DeleteStencil();
			Scene::Views[i].UseStencil = false;
		}
	}

	Scene::DisposeInScope(SCOPE_GAME);
	// Dispose of all resources
	Scene::ImageList.clear();
	Scene::SpriteList.clear();
	Scene::SoundList.clear();
	Scene::MusicList.clear();
	Scene::ModelList.clear();
	Scene::MediaList.clear();
	Scene::TextureList.clear();
	Scene::AnimatorList.clear();

	// Dispose of StaticObject
	if (StaticObject) {
		StaticObject->Active = false;
		StaticObject->Removed = true;
		StaticObject = NULL;
	}

	// Dispose and clear Static objects
	Scene::DeleteObjects(
		&Scene::StaticObjectFirst, &Scene::StaticObjectLast, &Scene::StaticObjectCount);

	// Dispose and clear Dynamic objects
	Scene::DeleteObjects(
		&Scene::DynamicObjectFirst, &Scene::DynamicObjectLast, &Scene::DynamicObjectCount);

	// Initialize the list that contains all of the scene's objects
	// (they have already been removed from it before this)
	Scene::ObjectCount = 0;
	Scene::ObjectFirst = NULL;
	Scene::ObjectLast = NULL;

	// Free Priority Lists
	Scene::FreePriorityLists();

	for (size_t i = 0; i < Scene::Layers.size(); i++) {
		Scene::Layers[i].Dispose();
	}
	Scene::Layers.clear();

	Scene::UnloadTilesets();

	if (Scene::ObjectLists) {
		Scene::ObjectLists->ForAll([](Uint32, ObjectList* list) -> void {
			delete list;
		});
		delete Scene::ObjectLists;
	}
	Scene::ObjectLists = NULL;

	if (Scene::ObjectRegistries) {
		Scene::ObjectRegistries->ForAll([](Uint32, ObjectRegistry* registry) -> void {
			delete registry;
		});
		delete Scene::ObjectRegistries;
	}
	Scene::ObjectRegistries = NULL;

	if (StaticObjectList) {
		delete StaticObjectList;
		StaticObjectList = NULL;
	}

	Scene::UnloadTileCollisions();

	if (Scene::Properties) {
		delete Scene::Properties;
	}
	Scene::Properties = NULL;

	ScriptManager::Dispose();
	SourceFileMap::Dispose();
	Compiler::Dispose();
}

void Scene::UnloadTilesets() {
	for (size_t i = 0; i < Scene::Tilesets.size(); i++) {
		if (Scene::Tilesets[i].Sprite) {
			delete Scene::Tilesets[i].Sprite;
		}
		if (Scene::Tilesets[i].Filename) {
			Memory::Free(Scene::Tilesets[i].Filename);
		}
	}
	Scene::Tilesets.clear();
	Scene::TileSpriteInfos.clear();
}

bool Scene::GetTextureListSpace(size_t* out) {
	for (size_t i = 0, listSz = TextureList.size(); i < listSz; i++) {
		if (!TextureList[i]) {
			*out = i;
			return true;
		}
	}
	return false;
}
size_t Scene::AddGameTexture(GameTexture* texture) {
	size_t i = 0;
	bool foundEmpty = GetTextureListSpace(&i);

	if (foundEmpty) {
		TextureList[i] = texture;
	}
	else {
		i = TextureList.size();
		TextureList.push_back(texture);
	}

	return i;
}
bool Scene::FindGameTextureByID(int id, size_t& out) {
	for (size_t i = 0, listSz = TextureList.size(); i < listSz; i++) {
		if (TextureList[i] && TextureList[i]->GetID() == id) {
			out = i;
			return true;
		}
	}
	return false;
}

// Tile Batching
void Scene::SetTile(int layer,
	int x,
	int y,
	int tileID,
	int flip_x,
	int flip_y,
	int collA,
	int collB) {
	Uint32* tile = &Scene::Layers[layer].Tiles[x + (y << Scene::Layers[layer].WidthInBits)];

	*tile = tileID & TILE_IDENT_MASK;
	if (flip_x) {
		*tile |= TILE_FLIPX_MASK;
	}
	if (flip_y) {
		*tile |= TILE_FLIPY_MASK;
	}
	*tile |= collA << 28;
	*tile |= collB << 26;
}

// Tile Collision
int Scene::CollisionAt(int x, int y, int collisionField, int collideSide, int* angle) {
	if (collisionField < 0 || collisionField >= Scene::TileCfg.size()) {
		return -1;
	}

	int temp;
	int checkX;
	int probeXOG = x;
	int probeYOG = y;
	int tileX, tileY, tileID, tileAngle;
	int collisionA, collisionB, collision;

	bool check;
	TileConfig* tileCfgBase = Scene::TileCfg[collisionField];

	bool wallAsFloorFlag = collideSide & 0x10;

	collideSide &= 0xF;

	int configIndex = 0;
	switch (collideSide) {
	case CollideSide::TOP:
		configIndex = 0;
		break;
	case CollideSide::LEFT:
		configIndex = 1;
		break;
	case CollideSide::RIGHT:
		configIndex = 2;
		break;
	case CollideSide::BOTTOM:
		configIndex = 3;
		break;
	}

	for (size_t l = 0, lSz = Layers.size(); l < lSz; l++) {
		SceneLayer& layer = Layers[l];
		if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
			continue;
		}

		x = probeXOG;
		y = probeYOG;

		x -= layer.OffsetX;
		y -= layer.OffsetY;

		// Check Layer Width
		temp = layer.Width << 4;
		if (x < 0 || x >= temp) {
			continue;
		}
		x &= layer.WidthMask << 4 | 0xF; // x = ((x % temp) + temp) % temp;

		// Check Layer Height
		temp = layer.Height << 4;
		if ((y < 0 || y >= temp)) {
			continue;
		}
		y &= layer.HeightMask << 4 | 0xF; // y = ((y % temp) + temp) % temp;

		tileX = x >> 4;
		tileY = y >> 4;

		tileID = layer.Tiles[tileX + (tileY << layer.WidthInBits)];
		if ((tileID & TILE_IDENT_MASK) != EmptyTile) {
			int tileFlipOffset = (((!!(tileID & TILE_FLIPY_MASK)) << 1) |
						     (!!(tileID & TILE_FLIPX_MASK))) *
				Scene::TileCount;

			collisionA = (tileID & TILE_COLLA_MASK) >> 28;
			collisionB = (tileID & TILE_COLLB_MASK) >> 26;
			// collisionC = (tileID & TILE_COLLC_MASK) >>
			// 24;
			collision = collisionField ? collisionB : collisionA;
			tileID = tileID & TILE_IDENT_MASK;

			// Check tile config
			TileConfig* tileCfg = &tileCfgBase[tileID + tileFlipOffset];
			Uint8* colT = tileCfg->CollisionTop;
			Uint8* colB = tileCfg->CollisionBottom;

			checkX = x & 0xF;
			if (colT[checkX] >= 0xF0 || colB[checkX] >= 0xF0) {
				continue;
			}

			// Check if we can collide with the tile side
			check = ((collision & 1) && (collideSide & CollideSide::TOP)) ||
				(wallAsFloorFlag &&
					((collision & 1) &&
						(collideSide &
							(CollideSide::LEFT |
								CollideSide::RIGHT)))) ||
				((collision & 2) && (collideSide & CollideSide::BOTTOM_SIDES));
			if (!check) {
				continue;
			}

			// Check Y
			tileY = tileY << 4;
			check = (y >= tileY + colT[checkX] && y <= tileY + colB[checkX]);
			if (!check) {
				continue;
			}

			// Return angle
			tileAngle = (&tileCfg->AngleTop)[configIndex];
			return tileAngle & 0xFF;
		}
	}

	return -1;
}

int Scene::CollisionInLine(int x,
	int y,
	int angleMode,
	int checkLen,
	int collisionField,
	bool compareAngle,
	Sensor* sensor) {
	if (checkLen < 0 || collisionField < 0 || collisionField >= Scene::TileCfg.size()) {
		return -1;
	}

	int probeXOG = x;
	int probeYOG = y;
	int probeDeltaX = 0;
	int probeDeltaY = 1;
	int tileX, tileY, tileID;
	int tileFlipOffset, collisionA, collisionB, collision, collisionMask;
	TileConfig* tileCfg;
	TileConfig* tileCfgBase = Scene::TileCfg[collisionField];

	int maxTileCheck = ((checkLen + 15) >> 4) + 1;
	int minLength = 0x7FFFFFFF, sensedLength;

	collisionMask = 3;
	switch (angleMode) {
	case 0:
		probeDeltaX = 0;
		probeDeltaY = 1;
		collisionMask = 1;
		break;
	case 1:
		probeDeltaX = 1;
		probeDeltaY = 0;
		collisionMask = 2;
		break;
	case 2:
		probeDeltaX = 0;
		probeDeltaY = -1;
		collisionMask = 2;
		break;
	case 3:
		probeDeltaX = -1;
		probeDeltaY = 0;
		collisionMask = 2;
		break;
	}

	switch (collisionField) {
	case 0:
		collisionMask <<= 28;
		break;
	case 1:
		collisionMask <<= 26;
		break;
	case 2:
		collisionMask <<= 24;
		break;
	}

	// probeDeltaX *= 16;
	// probeDeltaY *= 16;

	sensor->Collided = false;
	for (size_t l = 0, lSz = Layers.size(); l < lSz; l++) {
		SceneLayer& layer = Layers[l];
		if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
			continue;
		}

		x = probeXOG;
		y = probeYOG;
		x += layer.OffsetX;
		y += layer.OffsetY;

		// x = ((x % temp) + temp) % temp;
		// y = ((y % temp) + temp) % temp;

		tileX = x >> 4;
		tileY = y >> 4;
		for (int sl = 0; sl < maxTileCheck; sl++) {
			if (tileX < 0 || tileX >= layer.Width) {
				goto NEXT_TILE;
			}
			if (tileY < 0 || tileY >= layer.Height) {
				goto NEXT_TILE;
			}

			tileID = layer.Tiles[tileX + (tileY << layer.WidthInBits)];
			if ((tileID & TILE_IDENT_MASK) != EmptyTile) {
				tileFlipOffset = (((!!(tileID & TILE_FLIPY_MASK)) << 1) |
							 (!!(tileID & TILE_FLIPX_MASK))) *
					Scene::TileCount;

				collisionA = ((tileID & TILE_COLLA_MASK & collisionMask) >> 28);
				collisionB = ((tileID & TILE_COLLB_MASK & collisionMask) >> 26);
				tileID = tileID & TILE_IDENT_MASK;

				tileCfg = &tileCfgBase[tileID] + tileFlipOffset;
				if (!(collisionB | collisionA)) {
					goto NEXT_TILE;
				}

				switch (angleMode) {
				case 0:
					collision = tileCfg->CollisionTop[x & 15];
					if (collision >= 0xF0) {
						break;
					}

					collision += tileY << 4;
					sensedLength = collision - y;
					if ((Uint32)sensedLength <= (Uint32)checkLen) {
						if (!compareAngle ||
							abs((int)tileCfg->AngleTop -
								sensor->Angle) <= 0x20) {
							if (minLength > sensedLength) {
								minLength = sensedLength;
								sensor->Angle = tileCfg->AngleTop;
								sensor->Collided = true;
								sensor->X = x;
								sensor->Y = collision;
								sensor->X -= layer.OffsetX;
								sensor->Y -= layer.OffsetY;
								sl = maxTileCheck;
							}
						}
					}
					break;
				case 1:
					collision = tileCfg->CollisionLeft[y & 15];
					if (collision >= 0xF0) {
						break;
					}

					collision += tileX << 4;
					sensedLength = collision - x;
					if ((Uint32)sensedLength <= (Uint32)checkLen) {
						if (!compareAngle ||
							abs((int)tileCfg->AngleLeft -
								sensor->Angle) <= 0x20) {
							if (minLength > sensedLength) {
								minLength = sensedLength;
								sensor->Angle = tileCfg->AngleLeft;
								sensor->Collided = true;
								sensor->X = collision;
								sensor->Y = y;
								sensor->X -= layer.OffsetX;
								sensor->Y -= layer.OffsetY;
							}
						}
					}
					break;
				case 2:
					collision = tileCfg->CollisionBottom[x & 15];
					if (collision >= 0xF0) {
						break;
					}

					collision += tileY << 4;
					sensedLength = y - collision;
					if ((Uint32)sensedLength <= (Uint32)checkLen) {
						if (!compareAngle ||
							abs((int)tileCfg->AngleBottom -
								sensor->Angle) <= 0x20) {
							if (minLength > sensedLength) {
								minLength = sensedLength;
								sensor->Angle =
									tileCfg->AngleBottom;
								sensor->Collided = true;
								sensor->X = x;
								sensor->Y = collision;
								sensor->X -= layer.OffsetX;
								sensor->Y -= layer.OffsetY;
							}
						}
					}
					break;
				case 3:
					collision = tileCfg->CollisionRight[y & 15];
					if (collision >= 0xF0) {
						break;
					}

					collision += tileX << 4;
					sensedLength = x - collision;
					if ((Uint32)sensedLength <= (Uint32)checkLen) {
						if (!compareAngle ||
							abs((int)tileCfg->AngleRight -
								sensor->Angle) <= 0x20) {
							if (minLength > sensedLength) {
								minLength = sensedLength;
								sensor->Angle = tileCfg->AngleRight;
								sensor->Collided = true;
								sensor->X = collision;
								sensor->Y = y;
								sensor->X -= layer.OffsetX;
								sensor->Y -= layer.OffsetY;
							}
						}
					}
					break;
				}
			}

		NEXT_TILE:
			tileX += probeDeltaX;
			tileY += probeDeltaY;
		}
	}

	if (sensor->Collided) {
		return sensor->Angle;
	}

	return -1;
}

void Scene::SetupCollisionConfig(float minDistance,
	float lowTolerance,
	float highTolerance,
	int floorAngleTolerance,
	int wallAngleTolerance,
	int roofAngleTolerance) {
	CollisionMinimumDistance = minDistance;
	LowCollisionTolerance = lowTolerance;
	HighCollisionTolerance = highTolerance;
	FloorAngleTolerance = floorAngleTolerance;
	WallAngleTolerance = wallAngleTolerance;
	RoofAngleTolerance = roofAngleTolerance;
}

int Scene::AddDebugHitbox(int type, int dir, Entity* entity, CollisionBox* hitbox) {
	int i = 0;
	for (; i < DebugHitboxCount; ++i) {
		if (DebugHitboxList[i].hitbox.Left == hitbox->Left &&
			DebugHitboxList[i].hitbox.Top == hitbox->Top &&
			DebugHitboxList[i].hitbox.Right == hitbox->Right &&
			DebugHitboxList[i].hitbox.Bottom == hitbox->Bottom &&
			DebugHitboxList[i].x == (int)entity->X &&
			DebugHitboxList[i].y == (int)entity->Y &&
			DebugHitboxList[i].entity == entity) {
			return i;
		}
	}

	if (i < DEBUG_HITBOX_COUNT) {
		DebugHitboxList[i].type = type;
		DebugHitboxList[i].entity = entity;
		DebugHitboxList[i].collision = 0;
		DebugHitboxList[i].hitbox.Left = hitbox->Left;
		DebugHitboxList[i].hitbox.Top = hitbox->Top;
		DebugHitboxList[i].hitbox.Right = hitbox->Right;
		DebugHitboxList[i].hitbox.Bottom = hitbox->Bottom;
		DebugHitboxList[i].x = (int)entity->X;
		DebugHitboxList[i].y = (int)entity->Y;

		if ((dir & FLIP_X) == FLIP_X) {
			int store = -DebugHitboxList[i].hitbox.Left;
			DebugHitboxList[i].hitbox.Left = -DebugHitboxList[i].hitbox.Right;
			DebugHitboxList[i].hitbox.Right = store;
		}
		if ((dir & FLIP_Y) == FLIP_Y) {
			int store = -DebugHitboxList[i].hitbox.Top;
			DebugHitboxList[i].hitbox.Top = -DebugHitboxList[i].hitbox.Bottom;
			DebugHitboxList[i].hitbox.Bottom = store;
		}

		int id = DebugHitboxCount;
		DebugHitboxCount++;
		return id;
	}

	return -1;
}

bool Scene::CheckObjectCollisionTouch(Entity* thisEntity,
	CollisionBox* thisHitbox,
	Entity* otherEntity,
	CollisionBox* otherHitbox) {
	int store = 0;
	if (!thisEntity || !otherEntity || !thisHitbox || !otherHitbox) {
		return false;
	}

	if ((thisEntity->Direction & FLIP_X) == FLIP_X) {
		store = -thisHitbox->Left;
		thisHitbox->Left = -thisHitbox->Right;
		thisHitbox->Right = store;

		store = -otherHitbox->Left;
		otherHitbox->Left = -otherHitbox->Right;
		otherHitbox->Right = store;
	}
	if ((thisEntity->Direction & FLIP_Y) == FLIP_Y) {
		store = -thisHitbox->Top;
		thisHitbox->Top = -thisHitbox->Bottom;
		thisHitbox->Bottom = store;

		store = -otherHitbox->Top;
		otherHitbox->Top = -otherHitbox->Bottom;
		otherHitbox->Bottom = store;
	}

	bool collided = thisEntity->X + thisHitbox->Left < otherEntity->X + otherHitbox->Right &&
		thisEntity->X + thisHitbox->Right > otherEntity->X + otherHitbox->Left &&
		thisEntity->Y + thisHitbox->Top < otherEntity->Y + otherHitbox->Bottom &&
		thisEntity->Y + thisHitbox->Bottom > otherEntity->Y + otherHitbox->Top;

	if ((thisEntity->Direction & FLIP_X) == FLIP_X) {
		store = -thisHitbox->Left;
		thisHitbox->Left = -thisHitbox->Right;
		thisHitbox->Right = store;

		store = -otherHitbox->Left;
		otherHitbox->Left = -otherHitbox->Right;
		otherHitbox->Right = store;
	}
	if ((thisEntity->Direction & FLIP_Y) == FLIP_Y) {
		store = -thisHitbox->Top;
		thisHitbox->Top = -thisHitbox->Bottom;
		thisHitbox->Bottom = store;

		store = -otherHitbox->Top;
		otherHitbox->Top = -otherHitbox->Bottom;
		otherHitbox->Bottom = store;
	}

	if (ShowHitboxes) {
		int thisHitboxID =
			AddDebugHitbox(H_TYPE_TOUCH, thisEntity->Direction, thisEntity, thisHitbox);
		int otherHitboxID = AddDebugHitbox(
			H_TYPE_TOUCH, thisEntity->Direction, otherEntity, otherHitbox);

		if (thisHitboxID >= 0 && collided) {
			DebugHitboxList[thisHitboxID].collision |= 1 << (collided - 1);
		}
		if (otherHitboxID >= 0 && collided) {
			DebugHitboxList[otherHitboxID].collision |= 1 << (collided - 1);
		}
	}

	return collided;
}

bool Scene::CheckObjectCollisionCircle(Entity* thisEntity,
	float thisRadius,
	Entity* otherEntity,
	float otherRadius) {
	float x = thisEntity->X - otherEntity->X;
	float y = thisEntity->Y - otherEntity->Y;
	float r = thisRadius + otherRadius;

	if (ShowHitboxes) {
		bool collided = x * x + y * y < r * r;
		CollisionBox thisHitbox;
		CollisionBox otherHitbox;
		thisHitbox.Left = thisRadius;
		otherHitbox.Left = otherRadius;

		int thisHitboxID =
			AddDebugHitbox(H_TYPE_CIRCLE, FLIP_NONE, thisEntity, &thisHitbox);
		int otherHitboxID =
			AddDebugHitbox(H_TYPE_CIRCLE, FLIP_NONE, otherEntity, &otherHitbox);

		if (thisHitboxID >= 0 && collided) {
			DebugHitboxList[thisHitboxID].collision |= 1 << (collided - 1);
		}
		if (otherHitboxID >= 0 && collided) {
			DebugHitboxList[otherHitboxID].collision |= 1 << (collided - 1);
		}
	}

	return x * x + y * y < r * r;
}

bool Scene::CheckObjectCollisionBox(Entity* thisEntity,
	CollisionBox* thisHitbox,
	Entity* otherEntity,
	CollisionBox* otherHitbox,
	bool setValues) {
	if (!thisEntity || !otherEntity || !thisHitbox || !otherHitbox) {
		return C_NONE;
	}

	int collisionSideH = C_NONE;
	int collisionSideV = C_NONE;

	float collideX = otherEntity->X;
	float collideY = otherEntity->X;

	if ((thisEntity->Direction & FLIP_X) == FLIP_X) {
		int store = -thisHitbox->Left;
		thisHitbox->Left = -thisHitbox->Right;
		thisHitbox->Right = store;

		store = -otherHitbox->Left;
		otherHitbox->Left = -otherHitbox->Right;
		otherHitbox->Right = store;
	}

	if ((thisEntity->Direction & FLIP_Y) == FLIP_Y) {
		int store = -thisHitbox->Top;
		thisHitbox->Top = -thisHitbox->Bottom;
		thisHitbox->Bottom = store;

		store = -otherHitbox->Top;
		otherHitbox->Top = -otherHitbox->Bottom;
		otherHitbox->Bottom = store;
	}

	float thisIX = thisEntity->X;
	float thisIY = thisEntity->Y;
	float otherIX = otherEntity->X;
	float otherIY = otherEntity->Y;

	otherHitbox->Top++;
	otherHitbox->Bottom--;

	if (otherIX <= (thisHitbox->Right + thisHitbox->Left + 2.0 * thisIX) / 2.0) {
		if (otherIX + otherHitbox->Right >= thisIX + thisHitbox->Left &&
			thisIY + thisHitbox->Top < otherIY + otherHitbox->Bottom &&
			thisIY + thisHitbox->Bottom > otherIY + otherHitbox->Top) {
			collisionSideH = C_LEFT;
			collideX = thisEntity->X + thisHitbox->Left - otherHitbox->Right;
		}
	}
	else {
		if (otherIX + otherHitbox->Left < thisIX + thisHitbox->Right &&
			thisIY + thisHitbox->Top < otherIY + otherHitbox->Bottom &&
			thisIY + thisHitbox->Bottom > otherIY + otherHitbox->Top) {
			collisionSideH = C_RIGHT;
			collideX = thisEntity->X + thisHitbox->Right - otherHitbox->Left;
		}
	}

	otherHitbox->Left++;
	otherHitbox->Top--;
	otherHitbox->Right--;
	otherHitbox->Bottom++;

	if (otherIY <= thisIY + ((thisHitbox->Top + thisHitbox->Bottom) / 2.0)) {
		if (otherIY + otherHitbox->Bottom >= thisIY + thisHitbox->Top &&
			thisIX + thisHitbox->Left < otherIX + otherHitbox->Right &&
			thisIX + thisHitbox->Right > otherIX + otherHitbox->Left) {
			collisionSideV = C_TOP;
			collideY = thisEntity->Y + thisHitbox->Top - otherHitbox->Bottom;
		}
	}
	else {
		if (otherIY + otherHitbox->Top < thisIY + thisHitbox->Bottom &&
			thisIX + thisHitbox->Left < otherIX + otherHitbox->Right) {
			if (otherIX + otherHitbox->Left < thisIX + thisHitbox->Right) {
				collisionSideV = C_BOTTOM;
				collideY = thisEntity->Y + thisHitbox->Bottom - otherHitbox->Top;
			}
		}
	}

	otherHitbox->Left--;
	otherHitbox->Right++;

	if ((thisEntity->Direction & FLIP_X) == FLIP_X) {
		int store = -thisHitbox->Left;
		thisHitbox->Left = -thisHitbox->Right;
		thisHitbox->Right = store;

		store = -otherHitbox->Left;
		otherHitbox->Left = -otherHitbox->Right;
		otherHitbox->Right = store;
	}

	if ((thisEntity->Direction & FLIP_Y) == FLIP_Y) {
		int store = -thisHitbox->Top;
		thisHitbox->Top = -thisHitbox->Bottom;
		thisHitbox->Bottom = store;

		store = -otherHitbox->Top;
		otherHitbox->Top = -otherHitbox->Bottom;
		otherHitbox->Bottom = store;
	}

	int side = C_NONE;

	float cx = collideX - otherEntity->X;
	float cy = collideY - otherEntity->Y;
	if ((cx * cx >= cy * cy && (collisionSideV || !collisionSideH)) ||
		(!collisionSideH && collisionSideV)) {
		side = collisionSideV;
	}
	else {
		side = collisionSideH;
	}

	if (setValues) {
		float velX = 0.0;
		switch (side) {
		default:
		case C_NONE:
			break;

		case C_TOP:
			otherEntity->Y = collideY;

			if (otherEntity->VelocityY > 0.0) {
				otherEntity->VelocityY = 0.0;
			}

			if (otherEntity->TileCollisions != TILECOLLISION_UP) {
				if (!otherEntity->OnGround && otherEntity->VelocityY >= 0.0) {
					otherEntity->GroundVel = otherEntity->VelocityX;
					otherEntity->Angle = 0x00;
					otherEntity->OnGround = true;
				}
			}
			break;

		case C_LEFT:
			otherEntity->X = collideX;

			velX = otherEntity->VelocityX;
			if (otherEntity->OnGround) {
				if (otherEntity->CollisionMode == CMODE_ROOF) {
					velX = -otherEntity->GroundVel;
				}
				else {
					velX = otherEntity->GroundVel;
				}
			}

			if (velX > 0.0) {
				otherEntity->VelocityX = 0.0;
				otherEntity->GroundVel = 0.0;
			}
			break;

		case C_RIGHT:
			otherEntity->X = collideX;

			velX = otherEntity->VelocityX;
			if (otherEntity->OnGround) {
				if (otherEntity->CollisionMode == CMODE_ROOF) {
					velX = -otherEntity->GroundVel;
				}
				else {
					velX = otherEntity->GroundVel;
				}
			}

			if (velX < 0.0) {
				otherEntity->VelocityX = 0.0;
				otherEntity->GroundVel = 0.0;
			}
			break;

		case C_BOTTOM:
			otherEntity->Y = collideY;

			if (otherEntity->VelocityY < 0.0) {
				otherEntity->VelocityY = 0.0;
			}

			if (otherEntity->TileCollisions == TILECOLLISION_UP) {
				if (!otherEntity->OnGround && otherEntity->VelocityY <= 0.0) {
					otherEntity->Angle = 0x80;
					otherEntity->GroundVel = -otherEntity->VelocityX;
					otherEntity->OnGround = true;
				}
			}
			break;
		}
	}

	if (ShowHitboxes) {
		int thisHitboxID =
			AddDebugHitbox(H_TYPE_BOX, thisEntity->Direction, thisEntity, thisHitbox);
		int otherHitboxID =
			AddDebugHitbox(H_TYPE_BOX, thisEntity->Direction, otherEntity, otherHitbox);

		if (thisHitboxID >= 0 && side) {
			DebugHitboxList[thisHitboxID].collision |= 1 << (side - 1);
		}
		if (otherHitboxID >= 0 && side) {
			DebugHitboxList[otherHitboxID].collision |= 1 << (4 - side);
		}
	}
	return side;
}

bool Scene::CheckObjectCollisionPlatform(Entity* thisEntity,
	CollisionBox* thisHitbox,
	Entity* otherEntity,
	CollisionBox* otherHitbox,
	bool setValues) {
	int store = 0;
	bool collided = false;

	if (!thisEntity || !otherEntity || !thisHitbox || !otherHitbox) {
		return false;
	}

	if ((thisEntity->Direction & FLIP_X) == FLIP_X) {
		store = -thisHitbox->Left;
		thisHitbox->Left = -thisHitbox->Right;
		thisHitbox->Right = store;

		store = -otherHitbox->Left;
		otherHitbox->Left = -otherHitbox->Right;
		otherHitbox->Right = store;
	}
	if ((thisEntity->Direction & FLIP_Y) == FLIP_Y) {
		store = -thisHitbox->Top;
		thisHitbox->Top = -thisHitbox->Bottom;
		thisHitbox->Bottom = store;

		store = -otherHitbox->Top;
		otherHitbox->Top = -otherHitbox->Bottom;
		otherHitbox->Bottom = store;
	}

	float otherMoveY = otherEntity->Y - otherEntity->VelocityY;

	if (otherEntity->TileCollisions == TILECOLLISION_UP) {
		if (otherEntity->Y - otherHitbox->Bottom >= thisEntity->Y + thisHitbox->Top &&
			otherMoveY - otherHitbox->Bottom <= thisEntity->Y + thisHitbox->Bottom &&
			thisEntity->X + thisHitbox->Left < otherEntity->X + otherHitbox->Right &&
			thisEntity->X + thisHitbox->Right > otherEntity->X + otherHitbox->Left &&
			otherEntity->VelocityY <= 0.0) {

			otherEntity->Y = thisEntity->Y + thisHitbox->Bottom + otherHitbox->Bottom;

			if (setValues) {
				otherEntity->VelocityY = 0.0;

				if (!otherEntity->OnGround) {
					otherEntity->GroundVel = -otherEntity->VelocityX;
					otherEntity->Angle = 0x80;
					otherEntity->OnGround = true;
				}
			}

			collided = true;
		}
	}
	else {
		if (otherEntity->Y + otherHitbox->Bottom >= thisEntity->Y + thisHitbox->Top &&
			otherMoveY + otherHitbox->Bottom <= thisEntity->Y + thisHitbox->Bottom &&
			thisEntity->X + thisHitbox->Left < otherEntity->X + otherHitbox->Right &&
			thisEntity->X + thisHitbox->Right > otherEntity->X + otherHitbox->Left &&
			otherEntity->VelocityY >= 0.0) {

			otherEntity->Y = thisEntity->Y + (thisHitbox->Top - otherHitbox->Bottom);

			if (setValues) {
				otherEntity->VelocityY = 0.0;

				if (!otherEntity->OnGround) {
					otherEntity->GroundVel = otherEntity->VelocityX;
					otherEntity->Angle = 0x00;
					otherEntity->OnGround = true;
				}
			}

			collided = true;
		}
	}

	if ((thisEntity->Direction & FLIP_X) == FLIP_X) {
		store = -thisHitbox->Left;
		thisHitbox->Left = -thisHitbox->Right;
		thisHitbox->Right = store;

		store = -otherHitbox->Left;
		otherHitbox->Left = -otherHitbox->Right;
		otherHitbox->Right = store;
	}
	if ((thisEntity->Direction & FLIP_Y) == FLIP_Y) {
		store = -thisHitbox->Top;
		thisHitbox->Top = -thisHitbox->Bottom;
		thisHitbox->Bottom = store;

		store = -otherHitbox->Top;
		otherHitbox->Top = -otherHitbox->Bottom;
		otherHitbox->Bottom = store;
	}

	if (ShowHitboxes) {
		int thisHitboxID =
			AddDebugHitbox(H_TYPE_PLAT, thisEntity->Direction, thisEntity, thisHitbox);
		int otherHitboxID = AddDebugHitbox(
			H_TYPE_PLAT, thisEntity->Direction, otherEntity, otherHitbox);
		if (otherEntity->TileCollisions == TILECOLLISION_UP) {
			if (thisHitboxID >= 0 && collided) {
				DebugHitboxList[thisHitboxID].collision |= 1 << 3;
			}
			if (otherHitboxID >= 0 && collided) {
				DebugHitboxList[otherHitboxID].collision |= 1 << 0;
			}
		}
		else {
			if (thisHitboxID >= 0 && collided) {
				DebugHitboxList[thisHitboxID].collision |= 1 << 0;
			}
			if (otherHitboxID >= 0 && collided) {
				DebugHitboxList[otherHitboxID].collision |= 1 << 3;
			}
		}
	}

	return collided;
}

bool Scene::ObjectTileCollision(Entity* entity,
	int cLayers,
	int cMode,
	int cPlane,
	int xOffset,
	int yOffset,
	bool setPos) {
	int layerID = 1;
	bool collided = false;
	int posX = xOffset + entity->X;
	int posY = xOffset + entity->Y;

	if (cPlane < 0 || cPlane >= Scene::TileCfg.size()) {
		return false;
	}

	TileConfig* tileCfg;
	TileConfig* tileCfgBase = Scene::TileCfg[cPlane];

	int solid = 0;
	switch (cMode) {
	default:
		return false;

	case CMODE_FLOOR:
		for (size_t l = 0; l < Layers.size(); ++l, layerID <<= 1) {
			SceneLayer& layer = Layers[l];

			if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
				continue;
			}

			if (cLayers & layerID) {
				int colX = posX - layer.OffsetX;
				int colY = posY - layer.OffsetY;
				int cy = (colY & -TileHeight) - TileHeight;
				if (colX >= 0.0 && colX < TileWidth * layer.Width) {
					for (int i = 0; i < 3; ++i) {
						if (cy >= 0 && cy < TileHeight * layer.Height) {
							int tileID = layer.Tiles[(colX / TileWidth) +
								((cy / TileHeight)
									<< layer.WidthInBits)];

							if ((tileID & TILE_IDENT_MASK) !=
								EmptyTile) {
								int tileFlipOffset =
									(((!!(tileID &
										  TILE_FLIPY_MASK))
										 << 1) |
										(!!(tileID &
											TILE_FLIPX_MASK))) *
									TileCount;

								solid = cPlane
									? ((tileID &
										   TILE_COLLA_MASK &
										   1) >>
										  28)
									: ((tileID &
										   TILE_COLLB_MASK &
										   1) >>
										  26);
								tileID &= TILE_IDENT_MASK;

								tileCfg = &tileCfgBase[tileID] +
									tileFlipOffset;

								if (solid) {
									int ty = cy +
										tileCfg[tileID &
											0xFFF]
											.CollisionTop
												[(int)colX &
													0xF];

									if (colY >= ty &&
										abs(colY - ty) <=
											14.0) {
										collided = true;
										colY = ty;
										i = 3;
									}
								}
							}
						}
						cy += TileHeight;
					}
				}
				posX = layer.OffsetX + colX;
				posY = layer.OffsetY + colY;
			}
		}

		if (setPos && collided) {
			entity->Y = posY - yOffset;
		}
		return collided;

	case CMODE_LWALL:
		for (size_t l = 0; l < Layers.size(); ++l, layerID <<= 1) {
			SceneLayer& layer = Layers[l];

			if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
				continue;
			}

			if (cLayers & layerID) {
				int colX = posX - layer.OffsetX;
				int colY = posY - layer.OffsetY;
				int cx = (colX & -TileWidth) - TileWidth;
				if (colY >= 0.0 && colY < TileHeight * layer.Height) {
					for (int i = 0; i < 3; ++i) {
						if (cx >= 0 && cx < TileWidth * layer.Width) {
							int tileID = layer.Tiles[(cx / TileWidth) +
								((colY / TileHeight)
									<< layer.WidthInBits)];

							if ((tileID & TILE_IDENT_MASK) !=
								EmptyTile) {
								int tileFlipOffset =
									(((!!(tileID &
										  TILE_FLIPY_MASK))
										 << 1) |
										(!!(tileID &
											TILE_FLIPX_MASK))) *
									TileCount;

								solid = cPlane
									? ((tileID &
										   TILE_COLLA_MASK &
										   2) >>
										  28)
									: ((tileID &
										   TILE_COLLB_MASK &
										   2) >>
										  26);
								tileID &= TILE_IDENT_MASK;

								tileCfg = &tileCfgBase[tileID] +
									tileFlipOffset;

								if (solid) {
									int tx = cx +
										tileCfg[tileID &
											0xFFF]
											.CollisionLeft
												[(int)colY &
													0xF];

									if (colX >= tx &&
										abs(colX - tx) <=
											14.0) {
										collided = true;
										colX = tx;
										i = 3;
									}
								}
							}
						}
						cx += TileWidth;
					}
				}
				posX = layer.OffsetX + colX;
				posY = layer.OffsetY + colY;
			}
		}

		if (setPos && collided) {
			entity->X = posX - xOffset;
		}
		return collided;

	case CMODE_ROOF:
		for (size_t l = 0; l < Layers.size(); ++l, layerID <<= 1) {
			SceneLayer& layer = Layers[l];

			if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
				continue;
			}

			if (cLayers & layerID) {
				int colX = posX - layer.OffsetX;
				int colY = posY - layer.OffsetY;
				int cy = ((int)colY & -TileHeight) - TileHeight;
				if (colX >= 0.0 && colX < TileWidth * layer.Width) {
					for (int i = 0; i < 3; ++i) {
						if (cy >= 0 && cy < TileHeight * layer.Height) {
							int tileID = layer.Tiles[(colX / TileWidth) +
								((cy / TileHeight)
									<< layer.WidthInBits)];

							if ((tileID & TILE_IDENT_MASK) !=
								EmptyTile) {
								int tileFlipOffset =
									(((!!(tileID &
										  TILE_FLIPY_MASK))
										 << 1) |
										(!!(tileID &
											TILE_FLIPX_MASK))) *
									TileCount;

								solid = cPlane
									? ((tileID &
										   TILE_COLLA_MASK &
										   2) >>
										  28)
									: ((tileID &
										   TILE_COLLB_MASK &
										   2) >>
										  26);
								tileID &= TILE_IDENT_MASK;

								tileCfg = &tileCfgBase[tileID] +
									tileFlipOffset;

								if (solid) {
									int ty = cy +
										tileCfg[tileID &
											0xFFF]
											.CollisionBottom
												[(int)colX &
													0xF];

									if (colY <= ty &&
										abs(colY - ty) <=
											14.0) {
										collided = true;
										colY = ty;
										i = 3;
									}
								}
							}
						}
						cy -= TileHeight;
					}
				}
				posX = layer.OffsetX + colX;
				posY = layer.OffsetY + colY;
			}
		}

		if (setPos && collided) {
			entity->Y = posY - yOffset;
		}
		return collided;

	case CMODE_RWALL:
		for (size_t l = 0; l < Layers.size(); ++l, layerID <<= 1) {
			SceneLayer& layer = Layers[l];

			if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
				continue;
			}

			if (cLayers & layerID) {
				int colX = posX - layer.OffsetX;
				int colY = posY - layer.OffsetY;
				int cx = (colX & -TileWidth) - TileWidth;
				if (colY >= 0.0 && colY < TileHeight * layer.Height) {
					for (int i = 0; i < 3; ++i) {
						if (cx >= 0 && cx < TileWidth * layer.Width) {
							int tileID = layer.Tiles[(cx / TileWidth) +
								((colY / TileHeight)
									<< layer.WidthInBits)];

							if ((tileID & TILE_IDENT_MASK) !=
								EmptyTile) {
								int tileFlipOffset =
									(((!!(tileID &
										  TILE_FLIPY_MASK))
										 << 1) |
										(!!(tileID &
											TILE_FLIPX_MASK))) *
									TileCount;

								solid = cPlane
									? ((tileID &
										   TILE_COLLA_MASK &
										   2) >>
										  28)
									: ((tileID &
										   TILE_COLLB_MASK &
										   2) >>
										  26);
								tileID &= TILE_IDENT_MASK;

								tileCfg = &tileCfgBase[tileID] +
									tileFlipOffset;

								if (solid) {
									int tx = cx +
										tileCfg[tileID &
											0xFFF]
											.CollisionRight
												[(int)colY &
													0xF];

									if (colX >= tx &&
										abs(colX - tx) <=
											14.0) {
										collided = true;
										colX = tx;
										i = 3;
									}
								}
							}
						}
						cx -= TileWidth;
					}
				}
				posX = layer.OffsetX + colX;
				posY = layer.OffsetY + colY;
			}
		}

		if (setPos && collided) {
			entity->X = posX - xOffset;
		}
		return collided;
	}
}

bool Scene::ObjectTileGrip(Entity* entity,
	int cLayers,
	int cMode,
	int cPlane,
	float xOffset,
	float yOffset,
	float tolerance) {
	int layerID = 1;
	bool collided = false;
	int posX = xOffset + entity->X;
	int posY = xOffset + entity->Y;

	if (cPlane < 0 || cPlane >= Scene::TileCfg.size()) {
		return false;
	}

	TileConfig* tileCfg;
	TileConfig* tileCfgBase = Scene::TileCfg[cPlane];

	int solid = 0;
	switch (cMode) {
	default:
		return false;

	case CMODE_FLOOR:
		for (size_t l = 0; l < Layers.size(); ++l, layerID <<= 1) {
			SceneLayer& layer = Layers[l];

			if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
				continue;
			}

			if (cLayers & layerID) {
				float colX = posX - layer.OffsetX;
				float colY = posY - layer.OffsetY;
				int cy = ((int)colY & -TileHeight) - TileHeight;
				if (colX >= 0.0 && colX < TileWidth * layer.Width) {
					for (int i = 0; i < 3; ++i) {
						if (cy >= 0 && cy < TileHeight * layer.Height) {
							int tileID = layer.Tiles[((int)colX /
											 TileWidth) +
								((cy / TileHeight)
									<< layer.WidthInBits)];

							if ((tileID & TILE_IDENT_MASK) !=
								EmptyTile) {
								int tileFlipOffset =
									(((!!(tileID &
										  TILE_FLIPY_MASK))
										 << 1) |
										(!!(tileID &
											TILE_FLIPX_MASK))) *
									TileCount;

								solid = cPlane
									? ((tileID &
										   TILE_COLLA_MASK &
										   1) >>
										  28)
									: ((tileID &
										   TILE_COLLB_MASK &
										   1) >>
										  26);
								tileID &= TILE_IDENT_MASK;

								tileCfg = &tileCfgBase[tileID] +
									tileFlipOffset;

								if (solid) {
									int mask =
										tileCfg[tileID &
											0xFFF]
											.CollisionTop
												[(int)colX &
													0xF];
									int ty = cy + mask;

									if (mask < 0xFF) {
										if (abs(colY -
											    ty) <=
											tolerance) {
											collided =
												true;
											colY = ty;
										}
										i = 3;
									}
								}
							}
						}
						cy += TileHeight;
					}
				}
				posX = layer.OffsetX + colX;
				posY = layer.OffsetY + colY;
			}
		}

		if (collided) {
			entity->Y = posY - yOffset;
		}
		return collided;

	case CMODE_LWALL:
		for (size_t l = 0; l < Layers.size(); ++l, layerID <<= 1) {
			SceneLayer& layer = Layers[l];

			if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
				continue;
			}

			if (cLayers & layerID) {
				float colX = posX - layer.OffsetX;
				float colY = posY - layer.OffsetY;
				int cx = ((int)colX & -TileWidth) - TileWidth;
				if (colY >= 0.0 && colY < TileHeight * layer.Height) {
					for (int i = 0; i < 3; ++i) {
						if (cx >= 0 && cx < TileWidth * layer.Width) {
							int tileID = layer.Tiles[(cx / TileWidth) +
								(((int)colY / TileHeight)
									<< layer.WidthInBits)];

							if ((tileID & TILE_IDENT_MASK) !=
								EmptyTile) {
								int tileFlipOffset =
									(((!!(tileID &
										  TILE_FLIPY_MASK))
										 << 1) |
										(!!(tileID &
											TILE_FLIPX_MASK))) *
									TileCount;

								solid = cPlane
									? ((tileID &
										   TILE_COLLA_MASK &
										   2) >>
										  28)
									: ((tileID &
										   TILE_COLLB_MASK &
										   2) >>
										  26);
								tileID &= TILE_IDENT_MASK;

								tileCfg = &tileCfgBase[tileID] +
									tileFlipOffset;

								if (solid) {
									int mask =
										tileCfg[tileID &
											0xFFF]
											.CollisionLeft
												[(int)colY &
													0xF];
									int tx = cx + mask;

									if (mask < 0xFF) {
										if (abs(colX -
											    tx) <=
											tolerance) {
											collided =
												true;
											colX = tx;
										}
										i = 3;
									}
								}
							}
						}
						cx += TileWidth;
					}
				}
				posX = layer.OffsetX + colX;
				posY = layer.OffsetY + colY;
			}
		}

		if (collided) {
			entity->X = posX - xOffset;
		}
		return collided;

	case CMODE_ROOF:
		for (size_t l = 0; l < Layers.size(); ++l, layerID <<= 1) {
			SceneLayer& layer = Layers[l];

			if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
				continue;
			}

			if (cLayers & layerID) {
				float colX = posX - layer.OffsetX;
				float colY = posY - layer.OffsetY;
				int cy = ((int)colY & -TileHeight) - TileHeight;
				if (colX >= 0.0 && colX < TileWidth * layer.Width) {
					for (int i = 0; i < 3; ++i) {
						if (cy >= 0 && cy < TileHeight * layer.Height) {
							int tileID = layer.Tiles[((int)colX /
											 TileWidth) +
								((cy / TileHeight)
									<< layer.WidthInBits)];

							if ((tileID & TILE_IDENT_MASK) !=
								EmptyTile) {
								int tileFlipOffset =
									(((!!(tileID &
										  TILE_FLIPY_MASK))
										 << 1) |
										(!!(tileID &
											TILE_FLIPX_MASK))) *
									TileCount;

								solid = cPlane
									? ((tileID &
										   TILE_COLLA_MASK &
										   2) >>
										  28)
									: ((tileID &
										   TILE_COLLB_MASK &
										   2) >>
										  26);
								tileID &= TILE_IDENT_MASK;

								tileCfg = &tileCfgBase[tileID] +
									tileFlipOffset;

								if (solid) {
									int mask =
										tileCfg[tileID &
											0xFFF]
											.CollisionBottom
												[(int)colX &
													0xF];
									int ty = cy + mask;

									if (mask < 0xFF) {
										if (abs(colY -
											    ty) <=
											tolerance) {
											collided =
												true;
											colY = ty;
										}
										i = 3;
									}
								}
							}
						}
						cy -= TileHeight;
					}
				}
				posX = layer.OffsetX + colX;
				posY = layer.OffsetY + colY;
			}
		}

		if (collided) {
			entity->Y = posY - yOffset;
		}
		return collided;

	case CMODE_RWALL:
		for (size_t l = 0; l < Layers.size(); ++l, layerID <<= 1) {
			SceneLayer& layer = Layers[l];

			if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
				continue;
			}

			if (cLayers & layerID) {
				float colX = posX - layer.OffsetX;
				float colY = posY - layer.OffsetY;
				int cx = ((int)colX & -TileWidth) - TileWidth;
				if (colY >= 0.0 && colY < TileHeight * layer.Height) {
					for (int i = 0; i < 3; ++i) {
						if (cx >= 0 && cx < TileWidth * layer.Width) {
							int tileID = layer.Tiles[(cx / TileWidth) +
								(((int)colY / TileHeight)
									<< layer.WidthInBits)];

							if ((tileID & TILE_IDENT_MASK) !=
								EmptyTile) {
								int tileFlipOffset =
									(((!!(tileID &
										  TILE_FLIPY_MASK))
										 << 1) |
										(!!(tileID &
											TILE_FLIPX_MASK))) *
									TileCount;

								solid = cPlane
									? ((tileID &
										   TILE_COLLA_MASK &
										   2) >>
										  28)
									: ((tileID &
										   TILE_COLLB_MASK &
										   2) >>
										  26);
								tileID &= TILE_IDENT_MASK;

								tileCfg = &tileCfgBase[tileID] +
									tileFlipOffset;

								if (solid) {
									int mask =
										tileCfg[tileID &
											0xFFF]
											.CollisionRight
												[(int)colY &
													0xF];
									int tx = cx + mask;

									if (mask < 0xFF) {
										if (abs(colX -
											    tx) <=
											tolerance) {
											collided =
												true;
											colX = tx;
										}
										i = 3;
									}
								}
							}
						}
						cx -= TileWidth;
					}
				}
				posX = layer.OffsetX + colX;
				posY = layer.OffsetY + colY;
			}
		}

		if (collided) {
			entity->X = posX - xOffset;
		}
		return collided;
	}
}

void Scene::ProcessObjectMovement(Entity* entity, CollisionBox* outerBox, CollisionBox* innerBox) {
	if (entity && outerBox && innerBox) {
		if (entity->TileCollisions) {
			entity->Angle &= 0xFF;

			CollisionTolerance = HighCollisionTolerance;
			if (abs(entity->GroundVel) < 6.0 && entity->Angle == 0) {
				CollisionTolerance = LowCollisionTolerance;
			}

			CollisionOuter.Left = outerBox->Left;
			CollisionOuter.Top = outerBox->Top;
			CollisionOuter.Right = outerBox->Right;
			CollisionOuter.Bottom = outerBox->Bottom;

			CollisionInner.Left = innerBox->Left;
			CollisionInner.Top = innerBox->Top;
			CollisionInner.Right = innerBox->Right;
			CollisionInner.Bottom = innerBox->Bottom;

			CollisionEntity = entity;

			// TODO: Are these for shifting or dividing?
			CollisionMaskAir = CollisionOuter.Bottom >= 14 ? 8.0 : 2.0;

			if (entity->OnGround) {
				if (entity->TileCollisions == TILECOLLISION_DOWN) {
					UseCollisionOffset = entity->Angle == 0x00;
				}
				else {
					UseCollisionOffset = entity->Angle == 0x80;
				}

				if (CollisionOuter.Bottom < 14) {
					UseCollisionOffset = false;
				}
				ProcessPathGrip();
			}
			else {
				UseCollisionOffset = false;
				if (entity->TileCollisions == TILECOLLISION_DOWN) {
					ProcessAirCollision_Down();
				}
				else {
					ProcessAirCollision_Up();
				}
			}

			if (entity->OnGround) {
				entity->VelocityX = entity->GroundVel *
					Math::Cos256(entity->Angle & 0xFF) / 256.0;
				entity->VelocityY = entity->GroundVel *
					Math::Cos256(entity->Angle & 0xFF) / 256.0;
			}
			else {
				entity->GroundVel = entity->VelocityX;
			}
		}
		else {
			entity->X += entity->VelocityX;
			entity->Y += entity->VelocityY;
		}
	}
}

void Scene::ProcessPathGrip() {
	float xVel = 0.0;
	float yVel = 0.0;

	Sensors[4].X = CollisionEntity->X;
	Sensors[4].Y = CollisionEntity->Y;
	for (int i = 0; i < 6; i++) {
		Sensors[i].Angle = CollisionEntity->Angle;
		Sensors[i].Collided = false;
	}
	SetPathGripSensors(Sensors);

	float absSpeed = abs(CollisionEntity->GroundVel);
	float checkDist = absSpeed / 4.0;
	// absSpeed &= 4.0;
	while (checkDist >= -1.0) {
		if (checkDist >= 1.0) {
			xVel = absSpeed * Math::Cos256(CollisionEntity->Angle) * 1024.0;
			yVel = absSpeed * Math::Sin256(CollisionEntity->Angle) * 1024.0;
			checkDist--;
		}
		else {
			xVel = absSpeed * Math::Cos256(CollisionEntity->Angle) / 256.0;
			yVel = absSpeed * Math::Sin256(CollisionEntity->Angle) / 256.0;
			checkDist = -1.0;
		}

		if (CollisionEntity->GroundVel < 0.0) {
			xVel = -xVel;
			yVel = -yVel;
		}

		Sensors[0].Collided = false;
		Sensors[1].Collided = false;
		Sensors[2].Collided = false;
		Sensors[4].X += xVel;
		Sensors[4].Y += yVel;
		int tileDistance = -1;

		switch (CollisionEntity->CollisionMode) {
		case CMODE_FLOOR: {
			Sensors[3].X += xVel;
			Sensors[3].Y += yVel;

			if (CollisionEntity->GroundVel > 0.0) {
				LWallCollision(&Sensors[3]);
				if (Sensors[3].Collided) {
					Sensors[0].X = Sensors[3].X - 2.0;
				}
			}

			if (CollisionEntity->GroundVel < 0.0) {
				RWallCollision(&Sensors[3]);
				if (Sensors[3].Collided) {
					Sensors[0].X = Sensors[3].X + 2.0;
				}
			}

			if (Sensors[3].Collided) {
				xVel = 0.0;
				checkDist = -1.0;
			}

			for (int i = 0; i < 3; i++) {
				Sensors[i].X += xVel;
				Sensors[i].Y += yVel;
				FindFloorPosition(&Sensors[i]);
			}

			tileDistance = -1;
			for (int i = 0; i < 3; i++) {
				if (tileDistance > -1) {
					if (Sensors[i].Y < Sensors[tileDistance].Y) {
						tileDistance = i;
					}

					// Do these Y values need to be
					// typecasted as ints for this
					// check?
					if ((int)Sensors[i].Y == (int)Sensors[tileDistance].Y &&
						(Sensors[i].Angle < 0x08 ||
							Sensors[i].Angle > 0xF8)) {
						tileDistance = i;
					}
				}
				else if (Sensors[i].Collided) {
					tileDistance = i;
				}
			}

			if (tileDistance <= -1) {
				checkDist = -1.0;
			}
			else {
				Sensors[0].Y = Sensors[tileDistance].Y;
				Sensors[0].Angle = Sensors[tileDistance].Angle;

				Sensors[1].Y = Sensors[0].Y;
				Sensors[1].Angle = Sensors[0].Angle;

				Sensors[2].Y = Sensors[0].Y;
				Sensors[2].Angle = Sensors[0].Angle;

				Sensors[4].X = Sensors[1].X;
				Sensors[4].Y = Sensors[0].Y - CollisionOuter.Bottom;
			}

			if (Sensors[0].Angle < 0xDE && Sensors[0].Angle > 0x80) {
				CollisionEntity->CollisionMode = CMODE_LWALL;
			}
			if (Sensors[0].Angle > 0x22 && Sensors[0].Angle < 0x80) {
				CollisionEntity->CollisionMode = CMODE_RWALL;
			}
			break;
		}

		case CMODE_LWALL: {
			Sensors[3].X += xVel;
			Sensors[3].Y += yVel;

			if (CollisionEntity->GroundVel > 0.0) {
				RoofCollision(&Sensors[3]);
			}

			if (CollisionEntity->GroundVel < 0.0) {
				FloorCollision(&Sensors[3]);
			}

			if (Sensors[3].Collided) {
				yVel = 0.0;
				checkDist = -1.0;
			}

			for (int i = 0; i < 3; i++) {
				Sensors[i].X += xVel;
				Sensors[i].Y += yVel;
				FindLWallPosition(&Sensors[i]);
			}

			tileDistance = -1;
			for (int i = 0; i < 3; i++) {
				if (tileDistance > -1) {
					if (Sensors[i].X < Sensors[tileDistance].X &&
						Sensors[i].Collided) {
						tileDistance = i;
					}
				}
				else if (Sensors[i].Collided) {
					tileDistance = i;
				}
			}

			if (tileDistance <= -1) {
				checkDist = -1;
			}
			else {
				Sensors[0].X = Sensors[tileDistance].X;
				Sensors[0].Angle = Sensors[tileDistance].Angle;

				Sensors[1].X = Sensors[0].X;
				Sensors[1].Angle = Sensors[0].Angle;

				Sensors[2].X = Sensors[0].X;
				Sensors[2].Angle = Sensors[0].Angle;

				Sensors[4].X = Sensors[1].X - CollisionOuter.Bottom;
				Sensors[4].Y = Sensors[1].Y;
			}

			if (Sensors[0].Angle > 0xE2) {
				CollisionEntity->CollisionMode = CMODE_FLOOR;
			}
			if (Sensors[0].Angle < 0x9E) {
				CollisionEntity->CollisionMode = CMODE_ROOF;
			}
			break;
		}

		case CMODE_ROOF: {
			Sensors[3].X += xVel;
			Sensors[3].Y += yVel;

			if (CollisionEntity->GroundVel > 0.0) {
				RWallCollision(&Sensors[3]);
				if (Sensors[3].Collided) {
					Sensors[2].X = Sensors[3].X + 2.0;
				}
			}

			if (CollisionEntity->GroundVel < 0.0) {
				LWallCollision(&Sensors[3]);
				if (Sensors[3].Collided) {
					Sensors[0].X = Sensors[3].X - 2.0;
				}
			}

			if (Sensors[3].Collided) {
				xVel = 0.0;
				checkDist = -1.0;
			}

			for (int i = 0; i < 3; i++) {
				Sensors[i].X += xVel;
				Sensors[i].Y += yVel;
				FindRoofPosition(&Sensors[i]);
			}

			tileDistance = -1;
			for (int i = 0; i < 3; i++) {
				if (tileDistance > -1) {
					if (Sensors[i].Y > Sensors[tileDistance].Y &&
						Sensors[i].Collided) {
						tileDistance = i;
					}
				}
				else if (Sensors[i].Collided) {
					tileDistance = i;
				}
			}

			if (tileDistance <= -1) {
				checkDist = -1;
			}
			else {
				Sensors[0].Y = Sensors[tileDistance].Y;
				Sensors[0].Angle = Sensors[tileDistance].Angle;

				Sensors[1].Y = Sensors[0].Y;
				Sensors[1].Angle = Sensors[0].Angle;

				Sensors[2].Y = Sensors[0].Y;
				Sensors[2].Angle = Sensors[0].Angle;

				Sensors[4].X = Sensors[1].X;
				Sensors[4].Y = Sensors[0].Y - CollisionOuter.Bottom + 1.0;
			}

			if (Sensors[0].Angle > 0xA2) {
				CollisionEntity->CollisionMode = CMODE_LWALL;
			}
			if (Sensors[0].Angle < 0x5E) {
				CollisionEntity->CollisionMode = CMODE_RWALL;
			}
			break;
		}

		case CMODE_RWALL: {
			Sensors[3].X += xVel;
			Sensors[3].Y += yVel;

			if (CollisionEntity->GroundVel > 0.0) {
				FloorCollision(&Sensors[3]);
			}

			if (CollisionEntity->GroundVel < 0.0) {
				RoofCollision(&Sensors[3]);
			}

			if (Sensors[3].Collided) {
				yVel = 0.0;
				checkDist = -1.0;
			}

			for (int i = 0; i < 3; i++) {
				Sensors[i].X += xVel;
				Sensors[i].Y += yVel;
				FindRWallPosition(&Sensors[i]);
			}

			tileDistance = -1;
			for (int i = 0; i < 3; i++) {
				if (tileDistance > -1) {
					if (Sensors[i].X > Sensors[tileDistance].X &&
						Sensors[i].Collided) {
						tileDistance = i;
					}
				}
				else if (Sensors[i].Collided) {
					tileDistance = i;
				}
			}

			if (tileDistance <= -1) {
				checkDist = -1.0;
			}
			else {
				Sensors[0].X = Sensors[tileDistance].X;
				Sensors[0].Angle = Sensors[tileDistance].Angle;

				Sensors[1].X = Sensors[0].X;
				Sensors[1].Angle = Sensors[0].Angle;

				Sensors[2].X = Sensors[0].X;
				Sensors[2].Angle = Sensors[0].Angle;

				Sensors[4].X = Sensors[1].X + CollisionOuter.Bottom + 1.0;
				Sensors[4].Y = Sensors[1].Y;
			}

			if (Sensors[0].Angle < 0x1E) {
				CollisionEntity->CollisionMode = CMODE_FLOOR;
			}
			if (Sensors[0].Angle > 0x62) {
				CollisionEntity->CollisionMode = CMODE_ROOF;
			}
			break;
		}
		}

		if (tileDistance != -1) {
			CollisionEntity->Angle = Sensors[0].Angle;
		}

		if (!Sensors[3].Collided) {
			SetPathGripSensors(Sensors);
		}
		else {
			checkDist = -2.0;
		}
	}

	int newCollisionMode =
		CollisionEntity->TileCollisions == TILECOLLISION_DOWN ? CMODE_FLOOR : CMODE_ROOF;
	int newAngle = newCollisionMode << 6;

	switch (CollisionEntity->CollisionMode) {
	case CMODE_FLOOR: {
		if (Sensors[0].Collided || Sensors[1].Collided || Sensors[2].Collided) {
			CollisionEntity->Angle = Sensors[0].Angle;

			if (!Sensors[3].Collided) {
				CollisionEntity->X = Sensors[4].X;
			}
			else {
				if (CollisionEntity->GroundVel > 0.0) {
					CollisionEntity->X = Sensors[3].X - CollisionOuter.Right;
				}

				if (CollisionEntity->GroundVel < 0.0) {
					CollisionEntity->X =
						Sensors[3].X - CollisionOuter.Left + 1.0;
				}

				CollisionEntity->GroundVel = 0.0;
				CollisionEntity->VelocityX = 0.0;
			}

			CollisionEntity->Y = Sensors[4].Y;
		}
		else {
			CollisionEntity->OnGround = false;
			CollisionEntity->CollisionMode = newCollisionMode;
			CollisionEntity->VelocityX = Math::Cos256(CollisionEntity->Angle) *
				CollisionEntity->GroundVel / 256.0;
			CollisionEntity->VelocityY = Math::Sin256(CollisionEntity->Angle) *
				CollisionEntity->GroundVel / 256.0;

			if (CollisionEntity->VelocityY < -16.0) {
				CollisionEntity->VelocityY = -16.0;
			}

			if (CollisionEntity->VelocityY > 16.0) {
				CollisionEntity->VelocityY = 16.0;
			}

			CollisionEntity->GroundVel = CollisionEntity->VelocityX;
			CollisionEntity->Angle = newAngle;
			if (!Sensors[3].Collided) {
				CollisionEntity->X += CollisionEntity->VelocityX;
			}
			else {
				if (CollisionEntity->GroundVel > 0.0) {
					CollisionEntity->X = Sensors[3].X - CollisionOuter.Right;
				}
				if (CollisionEntity->GroundVel < 0.0) {
					CollisionEntity->X =
						Sensors[3].X - CollisionOuter.Left + 1.0;
				}

				CollisionEntity->GroundVel = 0.0;
				CollisionEntity->VelocityX = 0.0;
			}

			CollisionEntity->Y += CollisionEntity->VelocityY;
		}
		break;
	}

	case CMODE_LWALL: {
		if (Sensors[0].Collided || Sensors[1].Collided || Sensors[2].Collided) {
			CollisionEntity->Angle = Sensors[0].Angle;
		}
		else {
			CollisionEntity->OnGround = false;
			CollisionEntity->CollisionMode = newCollisionMode;
			CollisionEntity->VelocityX = Math::Cos256(CollisionEntity->Angle) *
				CollisionEntity->GroundVel / 256.0;
			CollisionEntity->VelocityY = Math::Sin256(CollisionEntity->Angle) *
				CollisionEntity->GroundVel / 256.0;

			if (CollisionEntity->VelocityY < -16.0) {
				CollisionEntity->VelocityY = -16.0;
			}

			if (CollisionEntity->VelocityY > 16.0) {
				CollisionEntity->VelocityY = 16.0;
			}

			CollisionEntity->GroundVel = CollisionEntity->VelocityX;
			CollisionEntity->Angle = newAngle;
		}

		if (!Sensors[3].Collided) {
			CollisionEntity->X = Sensors[4].X;
			CollisionEntity->Y = Sensors[4].Y;
		}
		else {
			if (CollisionEntity->GroundVel > 0.0) {
				CollisionEntity->Y = Sensors[3].Y + CollisionOuter.Right + 1.0;
			}

			if (CollisionEntity->GroundVel < 0.0) {
				CollisionEntity->Y = Sensors[3].Y - CollisionOuter.Left;
			}

			CollisionEntity->GroundVel = 0.0;
			CollisionEntity->X = Sensors[4].X;
		}
		break;
	}

	case CMODE_ROOF: {
		if (Sensors[0].Collided || Sensors[1].Collided || Sensors[2].Collided) {
			CollisionEntity->Angle = Sensors[0].Angle;

			if (!Sensors[3].Collided) {
				CollisionEntity->X = Sensors[4].X;
			}
			else {
				if (CollisionEntity->GroundVel > 0.0) {
					CollisionEntity->X = Sensors[3].X + CollisionOuter.Right;
				}

				if (CollisionEntity->GroundVel < 0.0) {
					CollisionEntity->X =
						Sensors[3].X + CollisionOuter.Left - 1.0;
				}

				CollisionEntity->GroundVel = 0.0;
			}
		}
		else {
			CollisionEntity->OnGround = false;
			CollisionEntity->CollisionMode = newCollisionMode;
			CollisionEntity->VelocityX = Math::Cos256(CollisionEntity->Angle) *
				CollisionEntity->GroundVel / 256.0;
			CollisionEntity->VelocityY = Math::Sin256(CollisionEntity->Angle) *
				CollisionEntity->GroundVel / 256.0;

			if (CollisionEntity->VelocityY < -16.0) {
				CollisionEntity->VelocityY = -16.0;
			}

			if (CollisionEntity->VelocityY > 16.0) {
				CollisionEntity->VelocityY = 16.0;
			}

			CollisionEntity->Angle = newAngle;
			CollisionEntity->GroundVel = CollisionEntity->VelocityX;

			if (!Sensors[3].Collided) {
				CollisionEntity->X += CollisionEntity->VelocityX;
			}
			else {
				if (CollisionEntity->GroundVel > 0.0) {
					CollisionEntity->X = Sensors[3].X - CollisionOuter.Right;
				}

				if (CollisionEntity->GroundVel < 0.0) {
					CollisionEntity->X =
						Sensors[3].X - CollisionOuter.Left + 1.0;
				}

				CollisionEntity->GroundVel = 0.0;
			}
		}
		CollisionEntity->Y = Sensors[4].Y;
		break;
	}

	case CMODE_RWALL: {
		if (Sensors[0].Collided || Sensors[1].Collided || Sensors[2].Collided) {
			CollisionEntity->Angle = Sensors[0].Angle;
		}
		else {
			CollisionEntity->OnGround = false;
			CollisionEntity->CollisionMode = newCollisionMode;
			CollisionEntity->VelocityX = Math::Cos256(CollisionEntity->Angle) *
				CollisionEntity->GroundVel / 256.0;
			CollisionEntity->VelocityY = Math::Sin256(CollisionEntity->Angle) *
				CollisionEntity->GroundVel / 256.0;

			if (CollisionEntity->VelocityY < -16.0) {
				CollisionEntity->VelocityY = -16.0;
			}

			if (CollisionEntity->VelocityY > 16.0) {
				CollisionEntity->VelocityY = 16.0;
			}

			CollisionEntity->GroundVel = CollisionEntity->VelocityX;
			CollisionEntity->Angle = newAngle;
		}

		if (!Sensors[3].Collided) {
			CollisionEntity->X = Sensors[4].X;
			CollisionEntity->Y = Sensors[4].Y;
		}
		else {
			if (CollisionEntity->GroundVel > 0.0) {
				CollisionEntity->Y = Sensors[3].Y - CollisionOuter.Right;
			}

			if (CollisionEntity->GroundVel < 0.0) {
				CollisionEntity->Y = Sensors[3].Y - CollisionOuter.Left + 1.0;
			}

			CollisionEntity->GroundVel = 0.0;
			CollisionEntity->X = Sensors[4].X;
		}
		break;
	}

	default:
		break;
	}
}

void Scene::ProcessAirCollision_Down() {
	int movingDown = 0;
	int movingUp = 0;
	int movingLeft = 0;
	int movingRight = 0;

	int offset = UseCollisionOffset ? COLLISION_OFFSET : 0.0;

	if (CollisionEntity->VelocityX >= 0.0) {
		movingRight = 1;
		Sensors[0].X = CollisionEntity->X + CollisionOuter.Right;
		Sensors[0].Y = CollisionEntity->Y + offset;
	}

	if (CollisionEntity->VelocityX <= 0.0) {
		movingLeft = 1;
		Sensors[1].X = CollisionEntity->X + CollisionOuter.Left - 1.0;
		Sensors[1].Y = CollisionEntity->Y + offset;
	}

	Sensors[2].X = CollisionEntity->X + CollisionInner.Left;
	Sensors[3].X = CollisionEntity->X + CollisionInner.Right;
	Sensors[4].X = Sensors[2].X;
	Sensors[5].X = Sensors[3].X;

	Sensors[0].Collided = false;
	Sensors[1].Collided = false;
	Sensors[2].Collided = false;
	Sensors[3].Collided = false;
	Sensors[4].Collided = false;
	Sensors[5].Collided = false;
	if (CollisionEntity->VelocityY >= 0.0) {
		movingDown = 1;
		Sensors[2].Y = CollisionEntity->Y + CollisionOuter.Bottom;
		Sensors[3].Y = CollisionEntity->Y + CollisionOuter.Bottom;
	}

	if (abs(CollisionEntity->VelocityX) > 1.0 || CollisionEntity->VelocityY < 0.0) {
		movingUp = 1;
		Sensors[4].Y = CollisionEntity->Y + CollisionOuter.Top - 1.0;
		Sensors[5].Y = CollisionEntity->Y + CollisionOuter.Top - 1.0;
	}

	float cnt = (abs(CollisionEntity->VelocityX) <= abs(CollisionEntity->VelocityY)
			? ((abs(CollisionEntity->VelocityY) / CollisionMaskAir) + 1.0)
			: (abs(CollisionEntity->VelocityX) / CollisionMaskAir) + 1.0);
	float velX = CollisionEntity->VelocityX / cnt;
	float velY = CollisionEntity->VelocityY / cnt;
	float velX2 = CollisionEntity->VelocityX - velX * (cnt - 1.0);
	float velY2 = CollisionEntity->VelocityY - velY * (cnt - 1.0);
	while (cnt > 0.0) {
		if (cnt < 2.0) {
			velX = velX2;
			velY = velY2;
		}
		cnt--;

		if (movingRight == 1) {
			Sensors[0].X += velX;
			Sensors[0].Y += velY;
			LWallCollision(&Sensors[0]);

			if (Sensors[0].Collided) {
				movingRight = 2;
			}
		}

		if (movingLeft == 1) {
			Sensors[1].X += velX;
			Sensors[1].Y += velY;
			RWallCollision(&Sensors[1]);

			if (Sensors[1].Collided) {
				movingLeft = 2;
			}
		}

		if (movingRight == 2) {
			CollisionEntity->VelocityX = 0.0;
			CollisionEntity->GroundVel = 0.0;
			CollisionEntity->X = Sensors[0].X - CollisionOuter.Right;

			Sensors[2].X = CollisionEntity->X + CollisionOuter.Left + 1.0;
			Sensors[3].X = CollisionEntity->X + CollisionOuter.Right - 2.0;
			Sensors[4].X = Sensors[2].X;
			Sensors[5].X = Sensors[3].X;

			velX = 0.0;
			velX2 = 0.0;
			movingRight = 3;
		}

		if (movingLeft == 2) {
			CollisionEntity->VelocityX = 0.0;
			CollisionEntity->GroundVel = 0.0;
			CollisionEntity->X = Sensors[1].X - CollisionOuter.Left + 1.0;

			Sensors[2].X = CollisionEntity->X + CollisionOuter.Left + 1.0;
			Sensors[3].X = CollisionEntity->X + CollisionOuter.Right - 2.0;
			Sensors[4].X = Sensors[2].X;
			Sensors[5].X = Sensors[3].X;

			velX = 0.0;
			velX2 = 0.0;
			movingLeft = 3;
		}

		if (movingDown == 1) {
			for (int i = 2; i < 4; i++) {
				if (!Sensors[i].Collided) {
					Sensors[i].X += velX;
					Sensors[i].Y += velY;
					FloorCollision(&Sensors[i]);
				}
			}

			if (Sensors[2].Collided || Sensors[3].Collided) {
				movingDown = 2;
				cnt = 0.0;
			}
		}

		if (movingUp == 1) {
			for (int i = 4; i < 6; i++) {
				if (Sensors[i].Collided) {
					Sensors[i].X += velX;
					Sensors[i].Y += velY;
					RoofCollision(&Sensors[i]);
				}
			}

			if (Sensors[4].Collided || Sensors[5].Collided) {
				movingUp = 2;
				cnt = 0.0;
			}
		}
	}

	if (movingRight < 2 && movingLeft < 2) {
		CollisionEntity->X += CollisionEntity->VelocityX;
	}

	if (movingUp < 2 && movingDown < 2) {
		CollisionEntity->Y += CollisionEntity->VelocityY;
		return;
	}

	if (movingDown == 2) {
		CollisionEntity->OnGround = true;

		if (Sensors[2].Collided && Sensors[3].Collided) {
			if (Sensors[2].Y >= Sensors[3].Y) {
				CollisionEntity->Y = Sensors[3].Y - CollisionOuter.Bottom;
				CollisionEntity->Angle = Sensors[3].Angle;
			}
			else {
				CollisionEntity->Y = Sensors[2].Y - CollisionOuter.Bottom;
				CollisionEntity->Angle = Sensors[2].Angle;
			}
		}
		else if (Sensors[2].Collided) {
			CollisionEntity->Y = Sensors[2].Y - CollisionOuter.Bottom;
			CollisionEntity->Angle = Sensors[2].Angle;
		}
		else if (Sensors[3].Collided) {
			CollisionEntity->Y = Sensors[3].Y - CollisionOuter.Bottom;
			CollisionEntity->Angle = Sensors[3].Angle;
		}

		if (CollisionEntity->Angle > 0xA0 && CollisionEntity->Angle < 0xDE &&
			CollisionEntity->CollisionMode != CMODE_LWALL) {
			CollisionEntity->CollisionMode = CMODE_LWALL;
			CollisionEntity->X -= 4.0;
		}

		if (CollisionEntity->Angle > 0x22 && CollisionEntity->Angle < 0x60 &&
			CollisionEntity->CollisionMode != CMODE_RWALL) {
			CollisionEntity->CollisionMode = CMODE_RWALL;
			CollisionEntity->X += 4.0;
		}

		float speed = 0.0;
		if (CollisionEntity->Angle < 0x80) {
			if (CollisionEntity->Angle < 0x10) {
				speed = CollisionEntity->VelocityX;
			}
			else if (CollisionEntity->Angle >= 0x20) {
				speed = (abs(CollisionEntity->VelocityX) <=
							abs(CollisionEntity->VelocityY)
						? CollisionEntity->VelocityY
						: CollisionEntity->VelocityX);
			}
			else {
				speed = (abs(CollisionEntity->VelocityX) <=
							abs(CollisionEntity->VelocityY / 2.0)
						? (CollisionEntity->VelocityY / 2.0)
						: CollisionEntity->VelocityX);
			}
		}
		else if (CollisionEntity->Angle > 0xF0) {
			speed = CollisionEntity->VelocityX;
		}
		else if (CollisionEntity->Angle <= 0xE0) {
			speed = (abs(CollisionEntity->VelocityX) <= abs(CollisionEntity->VelocityY)
					? -CollisionEntity->VelocityY
					: CollisionEntity->VelocityX);
		}
		else {
			speed = (abs(CollisionEntity->VelocityX) <=
						abs(CollisionEntity->VelocityY / 2.0)
					? -(CollisionEntity->VelocityY / 2.0)
					: CollisionEntity->VelocityX);
		}

		if (speed < -24.0) {
			speed = -24.0;
		}

		if (speed > 24.0) {
			speed = 24.0;
		}

		CollisionEntity->GroundVel = speed;
		CollisionEntity->VelocityX = speed;
		CollisionEntity->VelocityY = 0.0;
	}

	if (movingUp == 2) {
		int sensorAngle = 0;

		if (Sensors[4].Collided && Sensors[5].Collided) {
			if (Sensors[4].Y <= Sensors[5].Y) {
				CollisionEntity->Y = Sensors[5].Y - CollisionOuter.Top + 1.0;
				sensorAngle = Sensors[5].Angle;
			}
			else {
				CollisionEntity->Y = Sensors[4].Y - CollisionOuter.Top + 1.0;
				sensorAngle = Sensors[4].Angle;
			}
		}
		else if (Sensors[4].Collided) {
			CollisionEntity->Y = Sensors[4].Y - CollisionOuter.Top + 1.0;
			sensorAngle = Sensors[4].Angle;
		}
		else if (Sensors[5].Collided) {
			CollisionEntity->Y = Sensors[5].Y - CollisionOuter.Top + 1.0;
			sensorAngle = Sensors[5].Angle;
		}
		sensorAngle &= 0xFF;

		if (sensorAngle < 0x62) {
			if (CollisionEntity->VelocityY < -abs(CollisionEntity->VelocityX)) {
				CollisionEntity->OnGround = true;
				CollisionEntity->Angle = sensorAngle;
				CollisionEntity->CollisionMode = CMODE_RWALL;
				CollisionEntity->X += 4.0;
				CollisionEntity->Y -= 2.0;

				CollisionEntity->GroundVel = CollisionEntity->Angle <= 0x60
					? CollisionEntity->VelocityY
					: (CollisionEntity->VelocityY / 2.0);
			}
		}

		if (sensorAngle > 0x9E && sensorAngle < 0xC1) {
			if (CollisionEntity->VelocityY < -abs(CollisionEntity->VelocityX)) {
				CollisionEntity->OnGround = true;
				CollisionEntity->Angle = sensorAngle;
				CollisionEntity->CollisionMode = CMODE_LWALL;
				CollisionEntity->X -= 4.0;
				CollisionEntity->Y -= 2.0;

				CollisionEntity->GroundVel = CollisionEntity->Angle >= 0xA0
					? -CollisionEntity->VelocityY
					: -(CollisionEntity->VelocityY / 2.0);
			}
		}

		if (CollisionEntity->VelocityY < 0.0) {
			CollisionEntity->VelocityY = 0.0;
		}
	}
}

void Scene::ProcessAirCollision_Up() {
	int movingDown = 0;
	int movingUp = 0;
	int movingLeft = 0;
	int movingRight = 0;

	int offset = UseCollisionOffset ? COLLISION_OFFSET : 0.0;

	if (CollisionEntity->VelocityX >= 0.0) {
		movingRight = 1;
		Sensors[0].X = CollisionEntity->X + CollisionOuter.Right;
		Sensors[0].Y = CollisionEntity->Y + offset;
	}

	if (CollisionEntity->VelocityX <= 0.0) {
		movingLeft = 1;
		Sensors[1].X = CollisionEntity->X + CollisionOuter.Left - 1.0;
		Sensors[1].Y = CollisionEntity->Y + offset;
	}

	Sensors[2].X = CollisionEntity->X + CollisionInner.Left;
	Sensors[3].X = CollisionEntity->X + CollisionInner.Right;
	Sensors[4].X = Sensors[2].X;
	Sensors[5].X = Sensors[3].X;

	Sensors[0].Collided = false;
	Sensors[1].Collided = false;
	Sensors[2].Collided = false;
	Sensors[3].Collided = false;
	Sensors[4].Collided = false;
	Sensors[5].Collided = false;
	if (CollisionEntity->VelocityY <= 0.0) {
		movingDown = 1;
		Sensors[4].Y = CollisionEntity->Y + CollisionOuter.Top - 1.0;
		Sensors[5].Y = CollisionEntity->Y + CollisionOuter.Top - 1.0;
	}

	if (abs(CollisionEntity->VelocityX) > 1.0 || CollisionEntity->VelocityY > 0.0) {
		movingUp = 1;
		Sensors[2].Y = CollisionEntity->Y + CollisionOuter.Bottom;
		Sensors[3].Y = CollisionEntity->Y + CollisionOuter.Bottom;
	}

	float cnt = (abs(CollisionEntity->VelocityX) <= abs(CollisionEntity->VelocityY)
			? ((abs(CollisionEntity->VelocityY) / CollisionMaskAir) + 1.0)
			: (abs(CollisionEntity->VelocityX) / CollisionMaskAir) + 1.0);
	float velX = CollisionEntity->VelocityX / cnt;
	float velY = CollisionEntity->VelocityY / cnt;
	float velX2 = CollisionEntity->VelocityX - velX * (cnt - 1.0);
	float velY2 = CollisionEntity->VelocityY - velY * (cnt - 1.0);
	while (cnt > 0.0) {
		if (cnt < 2.0) {
			velX = velX2;
			velY = velY2;
		}
		cnt--;

		if (movingRight == 1) {
			Sensors[0].X += velX;
			Sensors[0].Y += velY;
			LWallCollision(&Sensors[0]);

			if (Sensors[0].Collided) {
				movingRight = 2;
			}
		}

		if (movingLeft == 1) {
			Sensors[1].X += velX;
			Sensors[1].Y += velY;
			RWallCollision(&Sensors[1]);

			if (Sensors[1].Collided) {
				movingLeft = 2;
			}
		}

		if (movingRight == 2) {
			CollisionEntity->VelocityX = 0.0;
			CollisionEntity->GroundVel = 0.0;
			CollisionEntity->X = Sensors[0].X - CollisionOuter.Right;

			Sensors[2].X = CollisionEntity->X + CollisionOuter.Left + 1.0;
			Sensors[3].X = CollisionEntity->X + CollisionOuter.Right - 2.0;
			Sensors[4].X = Sensors[2].X;
			Sensors[5].X = Sensors[3].X;

			velX = 0.0;
			velX2 = 0.0;
			movingRight = 3;
		}

		if (movingLeft == 2) {
			CollisionEntity->VelocityX = 0.0;
			CollisionEntity->GroundVel = 0.0;
			CollisionEntity->X = Sensors[1].X - CollisionOuter.Left + 1.0;

			Sensors[2].X = CollisionEntity->X + CollisionOuter.Left + 1.0;
			Sensors[3].X = CollisionEntity->X + CollisionOuter.Right - 2.0;
			Sensors[4].X = Sensors[2].X;
			Sensors[5].X = Sensors[3].X;

			velX = 0.0;
			velX2 = 0.0;
			movingLeft = 3;
		}

		if (movingUp == 1) {
			for (int i = 2; i < 4; i++) {
				if (!Sensors[i].Collided) {
					Sensors[i].X += velX;
					Sensors[i].Y += velY;
					FloorCollision(&Sensors[i]);
				}
			}

			if (Sensors[2].Collided || Sensors[3].Collided) {
				movingUp = 2;
				cnt = 0.0;
			}
		}

		if (movingDown == 1) {
			for (int i = 4; i < 6; i++) {
				if (!Sensors[i].Collided) {
					Sensors[i].X += velX;
					Sensors[i].Y += velY;
					RoofCollision(&Sensors[i]);
				}
			}

			if (Sensors[4].Collided || Sensors[5].Collided) {
				movingDown = 2;
				cnt = 0.0;
			}
		}
	}

	if (movingRight < 2 && movingLeft < 2) {
		CollisionEntity->X += CollisionEntity->VelocityX;
	}

	if (movingUp < 2 && movingDown < 2) {
		CollisionEntity->Y += CollisionEntity->VelocityY;
		return;
	}

	if (movingDown == 2) {
		CollisionEntity->OnGround = true;

		if (Sensors[4].Collided && Sensors[5].Collided) {
			if (Sensors[4].Y <= Sensors[5].Y) {
				CollisionEntity->Y = Sensors[5].Y - CollisionOuter.Top + 1.0;
				CollisionEntity->Angle = Sensors[5].Angle;
			}
			else {
				CollisionEntity->Y = Sensors[4].Y - CollisionOuter.Top + 1.0;
				CollisionEntity->Angle = Sensors[4].Angle;
			}
		}
		else if (Sensors[4].Collided) {
			CollisionEntity->Y = Sensors[4].Y - CollisionOuter.Top + 1.0;
			CollisionEntity->Angle = Sensors[4].Angle;
		}
		else if (Sensors[5].Collided) {
			CollisionEntity->Y = Sensors[5].Y - CollisionOuter.Top + 1.0;
			CollisionEntity->Angle = Sensors[5].Angle;
		}

		if (CollisionEntity->Angle > 0xA2 && CollisionEntity->Angle < 0xE0 &&
			CollisionEntity->CollisionMode != CMODE_LWALL) {
			CollisionEntity->CollisionMode = CMODE_LWALL;
			CollisionEntity->X -= 4.0;
		}

		if (CollisionEntity->Angle > 0x20 && CollisionEntity->Angle < 0x5E &&
			CollisionEntity->CollisionMode != CMODE_RWALL) {
			CollisionEntity->CollisionMode = CMODE_RWALL;
			CollisionEntity->X += 4.0;
		}

		float speed = 0.0;
		if (CollisionEntity->Angle >= 0x80) {
			if (CollisionEntity->Angle < 0x90) {
				speed = -CollisionEntity->VelocityX;
			}
			else if (CollisionEntity->Angle >= 0xA0) {
				speed = (abs(CollisionEntity->VelocityX) <=
							abs(CollisionEntity->VelocityY)
						? CollisionEntity->VelocityY
						: CollisionEntity->VelocityX);
			}
			else {
				speed = (abs(CollisionEntity->VelocityX) <=
							abs(CollisionEntity->VelocityY / 2.0)
						? (CollisionEntity->VelocityY / 2.0)
						: CollisionEntity->VelocityX);
			}
		}
		else if (CollisionEntity->Angle <= 0x70) {
			speed = CollisionEntity->VelocityX;
		}
		else if (CollisionEntity->Angle <= 0x60) {
			speed = (abs(CollisionEntity->VelocityX) <= abs(CollisionEntity->VelocityY)
					? -CollisionEntity->VelocityY
					: CollisionEntity->VelocityX);
		}
		else {
			speed = (abs(CollisionEntity->VelocityX) <=
						abs(CollisionEntity->VelocityY / 2.0)
					? -(CollisionEntity->VelocityY / 2.0)
					: CollisionEntity->VelocityX);
		}

		if (speed < -24.0) {
			speed = -24.0;
		}

		if (speed > 24.0) {
			speed = 24.0;
		}

		CollisionEntity->GroundVel = speed;
		CollisionEntity->VelocityX = speed;
		CollisionEntity->VelocityY = 0.0;
	}

	if (movingUp == 2) {
		int sensorAngle = 0;

		if (Sensors[2].Collided && Sensors[3].Collided) {
			if (Sensors[2].Y >= Sensors[3].Y) {
				CollisionEntity->Y = Sensors[3].Y - CollisionOuter.Bottom;
				sensorAngle = Sensors[3].Angle;
			}
			else {
				CollisionEntity->Y = Sensors[2].Y - CollisionOuter.Bottom;
				sensorAngle = Sensors[2].Angle;
			}
		}
		else if (Sensors[2].Collided) {
			CollisionEntity->Y = Sensors[2].Y - CollisionOuter.Bottom;
			sensorAngle = Sensors[2].Angle;
		}
		else if (Sensors[3].Collided) {
			CollisionEntity->Y = Sensors[3].Y - CollisionOuter.Bottom;
			sensorAngle = Sensors[3].Angle;
		}
		sensorAngle &= 0xFF;

		if (sensorAngle >= 0x21 && sensorAngle <= 0x40) {
			if (CollisionEntity->VelocityY > -abs(CollisionEntity->VelocityX)) {
				CollisionEntity->OnGround = true;
				CollisionEntity->Angle = sensorAngle;
				CollisionEntity->CollisionMode = CMODE_RWALL;
				CollisionEntity->X += 4.0;
				CollisionEntity->Y -= 2.0;

				CollisionEntity->GroundVel = CollisionEntity->Angle <= 0x20
					? CollisionEntity->VelocityY
					: (CollisionEntity->VelocityY / 2.0);
			}
		}

		if (sensorAngle > 0xC0 && sensorAngle < 0xE2) {
			if (CollisionEntity->VelocityY > -abs(CollisionEntity->VelocityX)) {
				CollisionEntity->OnGround = true;
				CollisionEntity->Angle = sensorAngle;
				CollisionEntity->CollisionMode = CMODE_LWALL;
				CollisionEntity->X -= 4.0;
				CollisionEntity->Y -= 2.0;

				CollisionEntity->GroundVel = CollisionEntity->Angle <= 0xE0
					? -CollisionEntity->VelocityY
					: -(CollisionEntity->VelocityY / 2.0);
			}
		}

		if (CollisionEntity->VelocityY > 0.0) {
			CollisionEntity->VelocityY = 0.0;
		}
	}
}

void Scene::SetPathGripSensors(CollisionSensor* sensors) {
	int offset = UseCollisionOffset ? COLLISION_OFFSET : 0.0;

	switch (CollisionEntity->CollisionMode) {
	case CMODE_FLOOR:
		sensors[0].Y = sensors[4].Y + CollisionOuter.Bottom;
		sensors[1].Y = sensors[4].Y + CollisionOuter.Bottom;
		sensors[2].Y = sensors[4].Y + CollisionOuter.Bottom;
		sensors[3].Y = sensors[4].Y + offset;

		sensors[0].X = sensors[4].X + CollisionInner.Left - 1.0;
		sensors[1].X = sensors[4].X;
		sensors[2].X = sensors[4].X + CollisionInner.Right;
		if (CollisionEntity->GroundVel <= 0.0) {
			sensors[3].X = sensors[4].X + CollisionOuter.Left - 1.0;
		}
		else {
			sensors[3].X = sensors[4].X + CollisionOuter.Right;
		}
		break;

	case CMODE_LWALL:
		sensors[0].X = sensors[4].X + CollisionOuter.Bottom;
		sensors[1].X = sensors[4].X + CollisionOuter.Bottom;
		sensors[2].X = sensors[4].X + CollisionOuter.Bottom;
		sensors[3].X = sensors[4].X;

		sensors[0].Y = sensors[4].Y + CollisionInner.Left - 1.0;
		sensors[1].Y = sensors[4].Y;
		sensors[2].Y = sensors[4].Y + CollisionInner.Right;
		if (CollisionEntity->GroundVel <= 0.0) {
			sensors[3].Y = sensors[4].Y - CollisionOuter.Left;
		}
		else {
			sensors[3].Y = sensors[4].Y - CollisionOuter.Right - 1.0;
		}
		break;

	case CMODE_ROOF:
		sensors[0].Y = sensors[4].Y - CollisionOuter.Bottom - 1.0;
		sensors[1].Y = sensors[4].Y - CollisionOuter.Bottom - 1.0;
		sensors[2].Y = sensors[4].Y - CollisionOuter.Bottom - 1.0;
		sensors[3].Y = sensors[4].Y - offset;

		sensors[0].X = sensors[4].X + CollisionInner.Left - 1.0;
		sensors[1].X = sensors[4].X;
		sensors[2].X = sensors[4].X + CollisionInner.Right;
		if (CollisionEntity->GroundVel <= 0.0) {
			sensors[3].X = sensors[4].X - CollisionOuter.Left;
		}
		else {
			sensors[3].X = sensors[4].X - CollisionOuter.Right - 1.0;
		}
		break;

	case CMODE_RWALL:
		sensors[0].X = sensors[4].X - CollisionOuter.Bottom - 1.0;
		sensors[1].X = sensors[4].X - CollisionOuter.Bottom - 1.0;
		sensors[2].X = sensors[4].X - CollisionOuter.Bottom - 1.0;
		sensors[3].X = sensors[4].X;

		sensors[0].Y = sensors[4].Y + CollisionInner.Left - 1.0;
		sensors[1].Y = sensors[4].Y;
		sensors[2].Y = sensors[4].Y + CollisionInner.Right;
		if (CollisionEntity->GroundVel <= 0.0) {
			sensors[3].Y = sensors[4].Y + CollisionOuter.Left - 1.0;
		}
		else {
			sensors[3].Y = sensors[4].Y + CollisionOuter.Right;
		}
		break;

	default:
		break;
	}
}

void Scene::FindFloorPosition(CollisionSensor* sensor) {
	int x = sensor->X;
	int y = sensor->Y;
	int temp;

	int posX = sensor->X;
	int posY = sensor->Y;
	int OGX = sensor->X;
	int OGY = sensor->Y;

	int tileX, tileY, tileID;

	if (CollisionEntity->CollisionPlane < 0 ||
		CollisionEntity->CollisionPlane >= TileCfg.size()) {
		return;
	}

	TileConfig* tileCfg;
	TileConfig* tileCfgBase = TileCfg[CollisionEntity->CollisionPlane];

	int solid = (CollisionEntity->TileCollisions == TILECOLLISION_DOWN) ? 1 : 2;

	int startY = posY;

	int layerID = 1;
	for (size_t l = 0; l < Layers.size(); ++l, layerID <<= 1) {
		SceneLayer& layer = Layers[l];

		if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
			continue;
		}

		x -= layer.OffsetX;
		x -= layer.OffsetY;

		temp = layer.Width << 4;
		if (x < 0 || x >= temp) {
			continue;
		}
		x &= layer.WidthMask << 4 | 0xF;

		temp = layer.Height << 4;
		if ((y < 0 || y >= temp)) {
			continue;
		}
		y &= layer.HeightMask << 4 | 0xF;

		tileX = x / TileWidth;
		tileY = y / TileHeight;

		if (CollisionEntity->CollisionLayers & layerID) {
			float colX = posX - layer.OffsetX;
			float colY = posY - layer.OffsetY;
			int cy = ((int)colY & -TileHeight) - TileHeight;

			if (colX >= 0.0 && colX < TileWidth * layer.Width) {
				for (int i = 0; i < 3; ++i) {
					if (cy >= 0 && cy < TileHeight * layer.Height) {
						tileID = layer.Tiles[((int)colX / TileWidth) +
							(((int)colY / TileHeight)
								<< layer.WidthInBits)];

						if ((tileID & TILE_IDENT_MASK) != EmptyTile) {
							int tileFlipOffset =
								(((!!(tileID & TILE_FLIPY_MASK))
									 << 1) |
									(!!(tileID &
										TILE_FLIPX_MASK))) *
								TileCount;

							int collisionA =
								(tileID & TILE_COLLA_MASK) >> 28;
							int collisionB =
								(tileID & TILE_COLLB_MASK) >> 26;
							int collision =
								CollisionEntity->CollisionPlane
								? collisionB
								: collisionA;

							tileID &= TILE_IDENT_MASK;

							tileCfg = &tileCfgBase[tileID +
								tileFlipOffset];
							Uint8* colT = tileCfg->CollisionTop;

							if (collision & 1) {
								int mask = colT[(int)colX & 0xF];
								int ty = cy + mask;
								int tileAngle = tileCfg->AngleTop;

								if (mask < 0xFF) {
									if (!sensor->Collided ||
										startY >= ty) {
										if (abs(colY -
											    ty) <=
											CollisionTolerance) {
											if (abs(sensor->Angle -
												    tileAngle) <=
													TileHeight *
														2 ||
												abs(sensor->Angle -
													tileAngle +
													0x100) <=
													FloorAngleTolerance ||
												abs(sensor->Angle -
													tileAngle -
													0x100) <=
													FloorAngleTolerance) {
												sensor->Collided =
													true;
												sensor->Angle =
													tileAngle;
												sensor->Y =
													ty +
													layer.OffsetY;
												startY =
													ty;
												i = 3;
											}
										}
									}
								}
							}
						}
					}
					cy += TileHeight;
				}
			}

			posX = OGX + colX + layer.OffsetX;
			posY = OGY + colY + layer.OffsetY;
		}
	}
}

void Scene::FindLWallPosition(CollisionSensor* sensor) {
	int posX = sensor->X;
	int posY = sensor->Y;
	int OGX = sensor->X;
	int OGY = sensor->Y;

	// Should this also be setting the sensor as blank? Or not
	// since it is set as blank elsewhere?
	if (CollisionEntity->CollisionPlane < 0 ||
		CollisionEntity->CollisionPlane >= Scene::TileCfg.size()) {
		return;
	}

	TileConfig* tileCfg;
	TileConfig* tileCfgBase = Scene::TileCfg[CollisionEntity->CollisionPlane];

	int solid = 0;

	int startX = posX;

	int layerID = 1;
	for (size_t l = 0; l < Layers.size(); ++l, layerID <<= 1) {
		SceneLayer& layer = Layers[l];

		if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
			continue;
		}

		if (CollisionEntity->CollisionLayers & layerID) {
			float colX = posX - layer.OffsetX - OGX;
			float colY = posY - layer.OffsetY - OGY;
			int cx = ((int)colX & -TileWidth) - TileWidth;
			if (colY >= 0.0 && colY < TileHeight * layer.Height) {
				for (int i = 0; i < 3; ++i) {
					if (cx >= 0 && cx < TileWidth * layer.Width) {
						int tileID = layer.Tiles[(cx / TileWidth) +
							(((int)colY / TileHeight)
								<< layer.WidthInBits)];

						if ((tileID & TILE_IDENT_MASK) != EmptyTile) {
							int tileFlipOffset =
								(((!!(tileID & TILE_FLIPY_MASK))
									 << 1) |
									(!!(tileID &
										TILE_FLIPX_MASK))) *
								TileCount;

							// int32 solid
							// =
							// collisionEntity->collisionPlane
							// ? ((1 << 14)
							// | (1 << 15))
							// : ((1 << 12)
							// | (1 <<
							// 13));
							int isSolid =
								CollisionEntity->CollisionPlane
								? ((tileID & TILE_COLLA_MASK &
									   solid) >>
									  28)
								: ((tileID & TILE_COLLB_MASK &
									   solid) >>
									  26);
							tileID &= TILE_IDENT_MASK;

							tileCfg = &tileCfgBase[tileID] +
								tileFlipOffset;

							if (isSolid) {
								int mask =
									tileCfg[tileID & 0xFFF]
										.CollisionLeft
											[(int)colY &
												0xF];
								int tx = cx + mask;
								int tileAngle = tileCfg->AngleLeft;

								if (mask < 0xFF) {
									if (!sensor->Collided ||
										startX >= tx) {
										if (abs(colX -
											    tx) <=
												CollisionTolerance &&
											abs(sensor->Angle -
												tileAngle) <=
												WallAngleTolerance) {
											sensor->Collided =
												true;
											sensor->Angle =
												tileAngle;
											sensor->X =
												tx +
												layer.OffsetX +
												OGX;
											startX = tx;
											i = 3;
										}
									}
								}
							}
						}
					}
					cx += TileWidth;
				}
			}
			posX = OGX + layer.OffsetX + colX;
			posY = OGY + layer.OffsetY + colY;
		}
	}
}

void Scene::FindRoofPosition(CollisionSensor* sensor) {
	int posX = sensor->X;
	int posY = sensor->Y;
	int OGX = sensor->X;
	int OGY = sensor->Y;

	// Should this also be setting the sensor as blank? Or not
	// since it is set as blank elsewhere?
	if (CollisionEntity->CollisionPlane < 0 ||
		CollisionEntity->CollisionPlane >= Scene::TileCfg.size()) {
		return;
	}

	TileConfig* tileCfg;
	TileConfig* tileCfgBase = Scene::TileCfg[CollisionEntity->CollisionPlane];

	int solid = (CollisionEntity->TileCollisions == TILECOLLISION_DOWN) ? 2 : 1;

	int startY = posY;

	int layerID = 1;
	for (size_t l = 0; l < Layers.size(); ++l, layerID <<= 1) {
		SceneLayer& layer = Layers[l];

		if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
			continue;
		}

		if (CollisionEntity->CollisionLayers & layerID) {
			float colX = posX - layer.OffsetX - OGX;
			float colY = posY - layer.OffsetY - OGY;
			int cy = ((int)colY & -TileHeight) - TileHeight;
			if (colX >= 0.0 && colX < TileWidth * layer.Width) {
				for (int i = 0; i < 3; ++i) {
					if (cy >= 0 && cy < TileHeight * layer.Height) {
						int tileID = layer.Tiles[((int)colX / TileWidth) +
							((cy / TileHeight) << layer.WidthInBits)];

						if ((tileID & TILE_IDENT_MASK) != EmptyTile) {
							int tileFlipOffset =
								(((!!(tileID & TILE_FLIPY_MASK))
									 << 1) |
									(!!(tileID &
										TILE_FLIPX_MASK))) *
								TileCount;

							int isSolid =
								CollisionEntity->CollisionPlane
								? ((tileID & TILE_COLLA_MASK &
									   solid) >>
									  28)
								: ((tileID & TILE_COLLB_MASK &
									   solid) >>
									  26);
							tileID &= TILE_IDENT_MASK;

							tileCfg = &tileCfgBase[tileID] +
								tileFlipOffset;

							if (isSolid) {
								int mask =
									tileCfg[tileID & 0xFFF]
										.CollisionBottom
											[(int)colX &
												0xF];
								int ty = cy + mask;
								int tileAngle =
									tileCfg->AngleBottom;

								if (mask < 0xFF) {
									if (!sensor->Collided ||
										startY <= ty) {
										if (abs(colY -
											    ty) <=
												CollisionTolerance &&
											abs(sensor->Angle -
												tileAngle) <=
												RoofAngleTolerance) {
											sensor->Collided =
												true;
											sensor->Angle =
												tileAngle;
											sensor->Y =
												ty +
												OGY +
												layer.OffsetY;
											startY = ty;
											i = 3;
										}
									}
								}
							}
						}
					}
					cy -= TileHeight;
				}
			}
			posX = OGX + layer.OffsetX + colX;
			posY = OGY + layer.OffsetY + colY;
		}
	}
}

void Scene::FindRWallPosition(CollisionSensor* sensor) {
	int posX = sensor->X;
	int posY = sensor->Y;
	int OGX = sensor->X;
	int OGY = sensor->Y;

	// Should this also be setting the sensor as blank? Or not
	// since it is set as blank elsewhere?
	if (CollisionEntity->CollisionPlane < 0 ||
		CollisionEntity->CollisionPlane >= Scene::TileCfg.size()) {
		return;
	}

	TileConfig* tileCfg;
	TileConfig* tileCfgBase = Scene::TileCfg[CollisionEntity->CollisionPlane];

	int solid = 0;

	int startX = posX;

	int layerID = 1;
	for (size_t l = 0; l < Layers.size(); ++l, layerID <<= 1) {
		SceneLayer& layer = Layers[l];

		if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
			continue;
		}

		if (CollisionEntity->CollisionLayers & layerID) {
			float colX = posX - layer.OffsetX - OGX;
			float colY = posY - layer.OffsetY - OGY;
			int cx = ((int)colX & -TileWidth) - TileWidth;
			if (colY >= 0.0 && colY < TileHeight * layer.Height) {
				for (int i = 0; i < 3; ++i) {
					if (cx >= 0 && cx < TileWidth * layer.Width) {
						int tileID = layer.Tiles[(cx / TileWidth) +
							(((int)colY / TileHeight)
								<< layer.WidthInBits)];

						if ((tileID & TILE_IDENT_MASK) != EmptyTile) {
							int tileFlipOffset =
								(((!!(tileID & TILE_FLIPY_MASK))
									 << 1) |
									(!!(tileID &
										TILE_FLIPX_MASK))) *
								TileCount;

							// int32 solid
							// =
							// collisionEntity->collisionPlane
							// ? ((1 << 14)
							// | (1 << 15))
							// : ((1 << 12)
							// | (1 <<
							// 13));
							int isSolid =
								CollisionEntity->CollisionPlane
								? ((tileID & TILE_COLLA_MASK &
									   solid) >>
									  28)
								: ((tileID & TILE_COLLB_MASK &
									   solid) >>
									  26);
							tileID &= TILE_IDENT_MASK;

							tileCfg = &tileCfgBase[tileID] +
								tileFlipOffset;

							if (isSolid) {
								int mask =
									tileCfg[tileID & 0xFFF]
										.CollisionRight
											[(int)colY &
												0xF];
								int tx = cx + mask;
								int tileAngle = tileCfg->AngleRight;

								if (mask < 0xFF) {
									if (!sensor->Collided ||
										startX <= tx) {
										if (abs(colX -
											    tx) <=
												CollisionTolerance &&
											abs(sensor->Angle -
												tileAngle) <=
												WallAngleTolerance) {
											sensor->Collided =
												true;
											sensor->Angle =
												tileAngle;
											sensor->X =
												tx +
												+OGX +
												layer.OffsetX;
											startX = tx;
											i = 3;
										}
									}
								}
							}
						}
					}
					cx -= TileWidth;
				}
			}
			posX = OGX + layer.OffsetX + colX;
			posY = OGY + layer.OffsetY + colY;
		}
	}
}

void Scene::FloorCollision(CollisionSensor* sensor) {
	int posX = sensor->X;
	int posY = sensor->Y;
	int OGX = sensor->X;
	int OGY = sensor->Y;

	// Should this also be setting the sensor as blank? Or not
	// since it is set as blank elsewhere?
	if (CollisionEntity->CollisionPlane < 0 ||
		CollisionEntity->CollisionPlane >= Scene::TileCfg.size()) {
		return;
	}

	TileConfig* tileCfg;
	TileConfig* tileCfgBase = Scene::TileCfg[CollisionEntity->CollisionPlane];

	int solid = (CollisionEntity->TileCollisions == TILECOLLISION_DOWN) ? 1 : 2;

	int collideAngle = 0;
	float collidePos = 65536.0;

	int layerID = 1;
	for (size_t l = 0; l < Layers.size(); ++l, layerID <<= 1) {
		SceneLayer& layer = Layers[l];

		if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
			continue;
		}

		if (CollisionEntity->CollisionLayers & layerID) {
			float colX = posX - layer.OffsetX - OGX;
			float colY = posY - layer.OffsetY - OGY;
			int cy = ((int)colY & -TileHeight) - TileHeight;
			if (colX >= 0.0 && colX < TileWidth * layer.Width) {
				int stepCount = 2;
				for (int i = 0; i < stepCount; ++i) {
					int step = TileHeight;

					if (cy >= 0 && cy < TileHeight * layer.Height) {
						int tileID = layer.Tiles[((int)colX / TileWidth) +
							((cy / TileHeight) << layer.WidthInBits)];

						if ((tileID & TILE_IDENT_MASK) != EmptyTile) {
							int tileFlipOffset =
								(((!!(tileID & TILE_FLIPY_MASK))
									 << 1) |
									(!!(tileID &
										TILE_FLIPX_MASK))) *
								TileCount;

							int isSolid =
								CollisionEntity->CollisionPlane
								? ((tileID & TILE_COLLA_MASK &
									   solid) >>
									  28)
								: ((tileID & TILE_COLLB_MASK &
									   solid) >>
									  26);
							tileID &= TILE_IDENT_MASK;

							tileCfg = &tileCfgBase[tileID] +
								tileFlipOffset;

							if (isSolid) {
								int mask =
									tileCfg[tileID & 0xFFF]
										.CollisionTop
											[(int)colX &
												0xF];
								int ty = OGY + layer.OffsetY + cy +
									mask;

								if (mask < 0xFF) {
									step = -TileHeight;
									if (colY < collidePos) {
										collideAngle =
											tileCfg->AngleTop;
										collidePos = ty;
										i = stepCount;
									}
								}
							}
						}
					}
					cy += step;
				}
			}
			posX = OGX + layer.OffsetX + colX;
			posY = OGY + layer.OffsetY + colY;
		}
	}

	if (collidePos != 65536.0) {
		float collideDist = sensor->Y - collidePos;
		if (sensor->Y >= collidePos && collideDist <= CollisionMinimumDistance) {
			sensor->Angle = collideAngle;
			sensor->Y = collidePos;
			sensor->Collided = true;
		}
	}
}

void Scene::LWallCollision(CollisionSensor* sensor) {
	int posX = sensor->X;
	int posY = sensor->Y;
	int OGX = sensor->X;
	int OGY = sensor->Y;

	// Should this also be setting the sensor as blank? Or not
	// since it is set as blank elsewhere?
	if (CollisionEntity->CollisionPlane < 0 ||
		CollisionEntity->CollisionPlane >= Scene::TileCfg.size()) {
		return;
	}

	TileConfig* tileCfg;
	TileConfig* tileCfgBase = Scene::TileCfg[CollisionEntity->CollisionPlane];

	int solid = 2;

	int layerID = 1;
	for (size_t l = 0; l < Layers.size(); ++l, layerID <<= 1) {
		SceneLayer& layer = Layers[l];

		if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
			continue;
		}

		if (CollisionEntity->CollisionLayers & layerID) {
			float colX = posX - layer.OffsetX - OGX;
			float colY = posY - layer.OffsetY - OGY;
			int cx = ((int)colX & -TileWidth) - TileWidth;
			if (colY >= 0.0 && colY < TileHeight * layer.Height) {
				for (int i = 0; i < 3; ++i) {
					if (cx >= 0 && cx < TileWidth * layer.Width) {
						int tileID = layer.Tiles[(cx / TileWidth) +
							(((int)colY / TileHeight)
								<< layer.WidthInBits)];

						if ((tileID & TILE_IDENT_MASK) != EmptyTile) {
							int tileFlipOffset =
								(((!!(tileID & TILE_FLIPY_MASK))
									 << 1) |
									(!!(tileID &
										TILE_FLIPX_MASK))) *
								TileCount;

							int isSolid =
								CollisionEntity->CollisionPlane
								? ((tileID & TILE_COLLA_MASK &
									   solid) >>
									  28)
								: ((tileID & TILE_COLLB_MASK &
									   solid) >>
									  26);
							tileID &= TILE_IDENT_MASK;

							tileCfg = &tileCfgBase[tileID] +
								tileFlipOffset;

							if (isSolid) {
								int mask =
									tileCfg[tileID & 0xFFF]
										.CollisionTop
											[(int)colY &
												0xF];
								int tx = cx + mask;

								if (mask < 0xFF && colX >= tx &&
									abs(colX - tx) <= 14.0) {
									sensor->Collided = true;
									sensor->Angle =
										tileCfg->AngleLeft;
									sensor->X = tx + OGX +
										layer.OffsetX;
									i = 3;
								}
							}
						}
					}
					cx += TileWidth;
				}
			}
			posX = OGX + layer.OffsetX + colX;
			posY = OGY + layer.OffsetY + colY;
		}
	}
}

void Scene::RoofCollision(CollisionSensor* sensor) {
	int posX = sensor->X;
	int posY = sensor->Y;
	int OGX = sensor->X;
	int OGY = sensor->Y;

	// Should this also be setting the sensor as blank? Or not
	// since it is set as blank elsewhere?
	if (CollisionEntity->CollisionPlane < 0 ||
		CollisionEntity->CollisionPlane >= Scene::TileCfg.size()) {
		return;
	}

	TileConfig* tileCfg;
	TileConfig* tileCfgBase = Scene::TileCfg[CollisionEntity->CollisionPlane];

	int solid = (CollisionEntity->TileCollisions == TILECOLLISION_DOWN) ? 2 : 1;

	int collideAngle = 0;
	float collidePos = -1.0;

	int layerID = 1;
	for (size_t l = 0; l < Layers.size(); ++l, layerID <<= 1) {
		SceneLayer& layer = Layers[l];

		if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
			continue;
		}

		if (CollisionEntity->CollisionLayers & layerID) {
			float colX = posX - layer.OffsetX - OGX;
			float colY = posY - layer.OffsetY - OGY;
			int cy = ((int)colY & -TileHeight) - TileHeight;
			if (colX >= 0.0 && colX < TileWidth * layer.Width) {
				int stepCount = 2;
				for (int i = 0; i < stepCount; ++i) {
					int step = -TileHeight;

					if (cy >= 0 && cy < TileHeight * layer.Height) {
						int tileID = layer.Tiles[((int)colX / TileWidth) +
							((cy / TileHeight) << layer.WidthInBits)];

						if ((tileID & TILE_IDENT_MASK) != EmptyTile) {
							int tileFlipOffset =
								(((!!(tileID & TILE_FLIPY_MASK))
									 << 1) |
									(!!(tileID &
										TILE_FLIPX_MASK))) *
								TileCount;

							int isSolid =
								CollisionEntity->CollisionPlane
								? ((tileID & TILE_COLLA_MASK &
									   solid) >>
									  28)
								: ((tileID & TILE_COLLB_MASK &
									   solid) >>
									  26);
							tileID &= TILE_IDENT_MASK;

							tileCfg = &tileCfgBase[tileID] +
								tileFlipOffset;

							if (isSolid) {
								int mask =
									tileCfg[tileID & 0xFFF]
										.CollisionBottom
											[(int)colX &
												0xF];
								int ty = OGY + layer.OffsetY + cy +
									mask;

								if (mask < 0xFF) {
									step = TileHeight;
									if (colY > collidePos) {
										collideAngle =
											tileCfg->AngleBottom;
										collidePos = ty;
										i = stepCount;
									}
								}
							}
						}
					}
					cy += step;
				}
			}
			posX = OGX + layer.OffsetX + colX;
			posY = OGY + layer.OffsetY + colY;
		}
	}

	if (collidePos >= 0.0 && sensor->Y <= collidePos &&
		sensor->Y - collidePos >= -CollisionMinimumDistance) {
		sensor->Angle = collideAngle;
		sensor->Y = collidePos;
		sensor->Collided = true;
	}
}

void Scene::RWallCollision(CollisionSensor* sensor) {
	int posX = sensor->X;
	int posY = sensor->Y;
	int OGX = sensor->X;
	int OGY = sensor->Y;

	// Should this also be setting the sensor as blank? Or not
	// since it is set as blank elsewhere?
	if (CollisionEntity->CollisionPlane < 0 ||
		CollisionEntity->CollisionPlane >= Scene::TileCfg.size()) {
		return;
	}

	TileConfig* tileCfg;
	TileConfig* tileCfgBase = Scene::TileCfg[CollisionEntity->CollisionPlane];

	int solid = 2;

	int layerID = 1;
	for (size_t l = 0; l < Layers.size(); ++l, layerID <<= 1) {
		SceneLayer& layer = Layers[l];

		if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE)) {
			continue;
		}

		if (CollisionEntity->CollisionLayers & layerID) {
			float colX = posX - layer.OffsetX - OGX;
			float colY = posY - layer.OffsetY - OGY;
			int cx = ((int)colX & -TileWidth) - TileWidth;
			if (colY >= 0.0 && colY < TileHeight * layer.Height) {
				for (int i = 0; i < 3; ++i) {
					if (cx >= 0 && cx < TileWidth * layer.Width) {
						int tileID = layer.Tiles[(cx / TileWidth) +
							(((int)colY / TileHeight)
								<< layer.WidthInBits)];

						if ((tileID & TILE_IDENT_MASK) != EmptyTile) {
							int tileFlipOffset =
								(((!!(tileID & TILE_FLIPY_MASK))
									 << 1) |
									(!!(tileID &
										TILE_FLIPX_MASK))) *
								TileCount;

							int isSolid =
								CollisionEntity->CollisionPlane
								? ((tileID & TILE_COLLA_MASK &
									   solid) >>
									  28)
								: ((tileID & TILE_COLLB_MASK &
									   solid) >>
									  26);
							tileID &= TILE_IDENT_MASK;

							tileCfg = &tileCfgBase[tileID] +
								tileFlipOffset;

							if (isSolid) {
								int mask =
									tileCfg[tileID & 0xFFF]
										.CollisionTop
											[(int)colY &
												0xF];
								int tx = cx + mask;

								if (mask < 0xFF && colX <= tx &&
									abs(colX - tx) <= 14.0) {
									sensor->Collided = true;
									sensor->Angle =
										tileCfg->AngleRight;
									sensor->X = tx + OGX +
										layer.OffsetX;
									i = 3;
								}
							}
						}
					}
					cx -= TileWidth;
				}
			}
			posX = OGX + layer.OffsetX + colX;
			posY = OGY + layer.OffsetY + colY;
		}
	}
}