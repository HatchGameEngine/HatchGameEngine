#ifndef ENGINE_SCENE_H
#define ENGINE_SCENE_H
class Entity;
class ObjectRegistry;
class DrawGroupList;

#include <Engine/Application.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Diagnostics/PerformanceTypes.h>
#include <Engine/Graphics.h>
#include <Engine/Includes/HashMap.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Math/Math.h>
#include <Engine/ResourceTypes/Image.h>
#include <Engine/ResourceTypes/ResourceType.h>
#include <Engine/Scene/SceneConfig.h>
#include <Engine/Scene/SceneEnums.h>
#include <Engine/Scene/SceneLayer.h>
#include <Engine/Scene/TileAnimation.h>
#include <Engine/Scene/TileConfig.h>
#include <Engine/Scene/TileSpriteInfo.h>
#include <Engine/Scene/View.h>
#include <Engine/Types/DrawGroupList.h>
#include <Engine/Types/EntityTypes.h>
#include <Engine/Types/ObjectList.h>
#include <Engine/Types/ObjectRegistry.h>
#include <Engine/Types/Tileset.h>

#include <set>

class Scene {
private:
	static void ResetViews();
	static void SetupView2D(View* currentView, float viewX, float viewY, float viewZ);
	static void SetupView3D(View* currentView, float viewX, float viewY, float viewZ);

	bool CanUpdateEntity(Entity* ent);
	void DetermineEntityIsOnScreen(Entity* ent);
	void UpdateObjectEarly(Entity* ent);
	void UpdateObject(Entity* ent);
	void UpdateObjectLate(Entity* ent);
	void FixedUpdateObjectEarly(Entity* ent);
	void FixedUpdateObject(Entity* ent);
	void FixedUpdateObjectLate(Entity* ent);
	void DeleteEntity(Entity* obj);
	void RunTileAnimations();
	void SortEntities();
	void Iterate(Entity* first, std::function<void(Entity* e)> func);
	void IterateAll(Entity* first, std::function<void(Entity* e)> func);
	void ResetPriorityListIndex(Entity* first);
	Entity* SortEntityList(Entity* head);
	bool SplitEntityList(Entity* head, Entity** left, Entity** right);
	Entity* MergeEntityList(Entity* left, Entity* right);
	int GetPersistenceScopeForObjectDeletion();
	void ClearPriorityLists();
	void DeleteEntities(Entity** first, Entity** last, int* count);
	void RemoveNonPersistentEntities(Entity** first, Entity** last, int* count);
	void SpawnStaticObject(const char* objectName);
	void ReadRSDKTile(TileConfig* tile, Uint8* line);
	void LoadRSDKTileConfig(int tilesetID, Stream* tileColReader);
	void LoadHCOLTileConfig(size_t tilesetID, Stream* tileColReader);
	void InitTileCollisions();
	void ClearTileCollisions(TileConfig* cfg, size_t numTiles);
	void SetTileCount(size_t tileCount);

public:
	static std::vector<Scene*> List;
	static Scene Main;
	static Scene* Current;
	static size_t CurrentIndex;
	static int StartingActiveCategory;
	static int StartingSceneInList;
	static int ShowTileCollisionFlag;
	static int ShowObjectRegions;
	static bool UseRenderRegions;
	static int ReservedSlotIDs;
	static vector<ResourceType*> SpriteList;
	static vector<ResourceType*> ImageList;
	static vector<ResourceType*> SoundList;
	static vector<ResourceType*> MusicList;
	static vector<ResourceType*> ModelList;
	static vector<ResourceType*> MediaList;
	static vector<Animator*> AnimatorList;
	static View Views[MAX_SCENE_VIEWS];
	static int ViewCurrent;
	static int ViewsActive;
	static int CurrentDrawGroup;
	static int CollisionTolerance;
	static bool UseCollisionOffset;
	static float CollisionOffset;
	static CollisionBox CollisionOuter;
	static CollisionBox CollisionInner;
	static Entity* CollisionEntity;
	static CollisionSensor Sensors[6];
	static float CollisionMinimumDistance;
	static int LowCollisionTolerance;
	static int HighCollisionTolerance;
	static int FloorAngleTolerance;
	static int WallAngleTolerance;
	static int RoofAngleTolerance;
	static bool ShowHitboxes;
	static int ViewableHitboxCount;
	static std::vector<ViewableHitbox> ViewableHitboxList;
	static int DebugMode;

	// General
	size_t Index = 0;
	int Frame = 0;
	bool Active = true;
	bool Paused = false;
	bool Loaded = false;
	bool Initializing = false;
	bool NeedEntitySort = false;
	int TileAnimationEnabled = 1;
	bool RefreshTileAnimations = false;

	// Property variables
	HashMap<Property>* Properties = nullptr;

	// Object variables
	OrderedHashMap<ObjectList*>* ObjectLists = nullptr;
	HashMap<ObjectRegistry*>* ObjectRegistries = nullptr;
	HashMap<ObjectList*>* StaticObjectLists = nullptr;
	int StaticObjectCount = 0;
	Entity* StaticObjectFirst = nullptr;
	Entity* StaticObjectLast = nullptr;
	int DynamicObjectCount = 0;
	Entity* DynamicObjectFirst = nullptr;
	Entity* DynamicObjectLast = nullptr;
	int ObjectCount = 0;
	Entity* ObjectFirst = nullptr;
	Entity* ObjectLast = nullptr;

	// Layering variables
	std::vector<SceneLayer*> Layers;
	bool AnyLayerTileChange = false;
	int PriorityPerLayer = 0;
	DrawGroupList** PriorityLists = nullptr;

	// Tile variables
	std::vector<Tileset> Tilesets;
	std::vector<TileSpriteInfo> TileSpriteInfos;
	int TileCount = 0;
	int TileWidth = 16;
	int TileHeight = 16;
	int BaseTileCount = 0;
	int BaseTilesetCount = 0;
	Uint16 EmptyTile = 0;
	bool TileCfgLoaded = false;
	std::vector<TileConfig*> TileCfg;

	// View variables
	int ObjectViewRenderFlag = 0xFFFFFFFF;
	int TileViewRenderFlag = 0xFFFFFFFF;
	Perf_ViewRender PERF_ViewRender[MAX_SCENE_VIEWS];

	char NextScene[MAX_RESOURCE_PATH_LENGTH];
	char CurrentScene[MAX_RESOURCE_PATH_LENGTH];
	int SceneType = SCENETYPE_NONE;
	bool DoRestart = false;
	bool NoPersistency = false;
	bool DoDelete = false;

	// Time variables
	int TimeEnabled = 0;
	int TimeCounter = 0;
	int Minutes = 0;
	int Seconds = 0;
	int Milliseconds = 0;

	int Filter = 0xFF;

	// Scene list variables
	int CurrentSceneInList = 0;
	char CurrentFolder[256];
	char CurrentID[256];
	char CurrentResourceFolder[256];
	char PreviousResourceFolder[256];
	char CurrentCategory[256];
	int ActiveCategory = 0;

	// Resource managing variables
	std::set<ResourceType*> UsedResources;

	static Scene* New();
	static Scene* GetFirstActive();
	static void OnEvent(Uint32 event);
	static void Init();
	static void StaticAfterScene();
	static void SetViewActive(int viewIndex, bool active);
	static void SetViewPriority(int viewIndex, int priority);
	static void SortViews();
	static bool SetView(int viewIndex);
	static void Render();
	static bool CheckPosOnScreen(float posX, float posY, float rangeX, float rangeY);
	static void SetupViewMatrices(View* currentView, float viewX, float viewY, float viewZ);
	static ObjectList* NewObjectList(const char* objectName);
	static void AddStaticClass();
	static void CallGameStart();
	static bool GetResourceListSpace(vector<ResourceType*>* list,
		ResourceType* resource,
		size_t& index,
		bool& foundEmpty);
	static bool GetResource(vector<ResourceType*>* list, ResourceType* resource, size_t& index);
	static int LoadSpriteResource(const char* filename, int unloadPolicy);
	static int LoadImageResource(const char* filename, int unloadPolicy);
	static int AddImageResource(Image* image, const char* filename, int unloadPolicy);
	static int LoadModelResource(const char* filename, int unloadPolicy);
	static int LoadMusicResource(const char* filename, int unloadPolicy);
	static int LoadSoundResource(const char* filename, int unloadPolicy);
	static int LoadVideoResource(const char* filename, int unloadPolicy);
	static ResourceType* GetSpriteResource(int index);
	static ResourceType* GetImageResource(int index);
	static void OrientHitbox(CollisionBox* source, int direction, CollisionBox* destination) {
		*destination = *source;
		if (direction & FLIP_X) {
			int store = -source->Left;
			destination->Left = -source->Right;
			destination->Right = store;
		}
		if (direction & FLIP_Y) {
			int top = -source->Top;
			destination->Top = -source->Bottom;
			destination->Bottom = top;
		}
	};
	static int RegisterHitbox(int type, int dir, Entity* entity, CollisionBox* hitbox);
	static bool CheckEntityTouch(Entity* thisEntity,
		CollisionBox* thisHitbox,
		Entity* otherEntity,
		CollisionBox* otherHitbox);
	static bool CheckEntityCircle(Entity* thisEntity,
		float thisRadius,
		Entity* otherEntity,
		float otherRadius);
	static int CheckEntityBox(Entity* thisEntity,
		CollisionBox* thisHitbox,
		Entity* otherEntity,
		CollisionBox* otherHitbox,
		bool setValues);
	static bool CheckEntityPlatform(Entity* thisEntity,
		CollisionBox* thisHitbox,
		Entity* otherEntity,
		CollisionBox* otherHitbox,
		bool setValues);
	static void SetCollisionVariables(float minDistance,
		float lowTolerance,
		float highTolerance,
		int floorAngleTolerance,
		int wallAngleTolerance,
		int roofAngleTolerance);
	static void SetPathGripSensors(CollisionSensor* sensors);
	static void StaticDispose();

	Scene();
	void Create();
	void Delete();
	void AddToLinkedList(Entity** first, Entity** last, int* count, Entity* obj);
	void AddToLinkedList(Entity* entity);
	void AddEntity(Entity* entity);
	void RemoveFromLinkedList(Entity** first, Entity** last, int* count, Entity* obj);
	void RemoveFromLinkedList(Entity* entity);
	void RemoveAndDeleteEntity(Entity* entity);
	void RemoveEntity(Entity* obj);
	void Clear(Entity** first, Entity** last, int* count);
	bool AddStatic(ObjectList* objectList, Entity* obj);
	void AddDynamic(Entity* obj);
	void SetCurrent(const char* categoryName, const char* sceneName);
	void SetInfoFromCurrentID();
	void InitObjectListsAndRegistries();
	void ResetPerf();
	void FrameUpdate();
	void Update();
	void FixedUpdate();
	Tileset* GetTileset(int tileID);
	TileAnimator* GetTileAnimator(int tileID);
	void RenderView(int viewIndex, bool doPerf);
	void AfterScene();
	void ResetFields();
	void Restart();
	void FinishLoad();
	void Unload();
	void Prepare();
	void LoadScene(const char* filename);
	bool ChangeFromPath(const char* path, int filter);
	void ReadSceneFile(const char* filename);
	void ProcessSceneTimer();
	Entity* SpawnObject(ObjectList* list, float x, float y);
	Entity* SpawnObject(const char* objectName, float x, float y);
	Entity* TrySpawnObject(ObjectList* list, float x, float y);
	Entity* TrySpawnObject(const char* objectName, float x, float y);
	ObjectList* GetObjectList(const char* objectName, bool callListLoadFunction);
	ObjectList* GetObjectList(const char* objectName);
	ObjectList* GetStaticObjectList(const char* objectName);
	void AddManagers();
	std::vector<ObjectList*> GetObjectListPerformance();
	void AddLayer(SceneLayer* layer);
	void InitPriorityLists();
	void FreePriorityLists();
	void SetPriorityPerLayer(int count);
	DrawGroupList* GetDrawGroup(int index);
	DrawGroupList* GetDrawGroupNoCheck(int index);
	bool AddTileset(char* path);
	void LoadTileCollisions(const char* filename, size_t tilesetID);
	void UnloadTileCollisions();
	void MarkResourceAsUsed(ResourceType* resource);
	bool UnmarkResourceAsUsed(ResourceType* resource);
	void DisposeInScope(Uint32 scope);
	void UnloadGPUData(Uint32 scope);
	void Dispose();
	void UnloadTilesets();
	void
	SetTile(int layerIndex, int x, int y, int tileID, int flip_x, int flip_y, int collA, int collB);
	int CollisionAt(int x, int y, int collisionField, int collideSide, int* angle);
	int CollisionInLine(int x,
		int y,
		int angleMode,
		int checkLen,
		int collisionField,
		bool compareAngle,
		Sensor* sensor);
	bool CheckTileCollision(Entity* entity,
		int cLayers,
		int cMode,
		int cPlane,
		int xOffset,
		int yOffset,
		bool setPos);
	bool CheckTileGrip(Entity* entity,
		int cLayers,
		int cMode,
		int cPlane,
		int xOffset,
		int yOffset,
		float tolerance);
	void
	ProcessEntityMovement(Entity* entity, CollisionBox* outerBox, CollisionBox* innerBox);
	void ProcessPathGrip();
	void ProcessAirCollision(bool isUp);
	void CheckVerticalPosition(CollisionSensor* sensor, bool isFloor);
	void CheckHorizontalPosition(CollisionSensor* sensor, bool isLeft);
	void CheckVerticalCollision(CollisionSensor* sensor, bool isFloor);
	void CheckHorizontalCollision(CollisionSensor* sensor, bool isLeft);
};
#endif /* ENGINE_SCENE_H */
