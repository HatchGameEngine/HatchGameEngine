#ifndef ENGINE_SCENE_H
#define ENGINE_SCENE_H
class Entity;

#include <Engine/Application.h>
#include <Engine/Diagnostics/PerformanceTypes.h>
#include <Engine/Graphics.h>
#include <Engine/Includes/HashMap.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Math/Math.h>
#include <Engine/Rendering/GameTexture.h>
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

namespace Scene {
//private:
	void RemoveObject(Entity* obj);
	void RunTileAnimations();
	void ResetViews();
	void Iterate(Entity* first, std::function<void(Entity* e)> func);
	void IterateAll(Entity* first, std::function<void(Entity* e)> func);
	void ResetPriorityListIndex(Entity* first);
	int GetPersistenceScopeForObjectDeletion();
	void ClearPriorityLists();
	void DeleteObjects(Entity** first, Entity** last, int* count);
	void RemoveNonPersistentObjects(Entity** first, Entity** last, int* count);
	void DeleteAllObjects();
	void AddStaticClass();
	void CallGameStart();
	void SpawnStaticObject(const char* objectName);
	void ReadRSDKTile(TileConfig* tile, Uint8* line);
	void LoadRSDKTileConfig(int tilesetID, Stream* tileColReader);
	void LoadHCOLTileConfig(size_t tilesetID, Stream* tileColReader);
	void InitTileCollisions();
	void ClearTileCollisions(TileConfig* cfg, size_t numTiles);
	void SetTileCount(size_t tileCount);
	bool GetTextureListSpace(size_t* out);

//public:
	extern int ShowTileCollisionFlag;
	extern int ShowObjectRegions;
	extern HashMap<VMValue>* Properties;
	extern HashMap<ObjectList*>* ObjectLists;
	extern HashMap<ObjectRegistry*>* ObjectRegistries;
	extern HashMap<ObjectList*>* StaticObjectLists;
	extern int StaticObjectCount;
	extern Entity* StaticObjectFirst;
	extern Entity* StaticObjectLast;
	extern int DynamicObjectCount;
	extern Entity* DynamicObjectFirst;
	extern Entity* DynamicObjectLast;
	extern int ObjectCount;
	extern Entity* ObjectFirst;
	extern Entity* ObjectLast;
	extern int BasePriorityPerLayer;
	extern int PriorityPerLayer;
	extern DrawGroupList* PriorityLists;
	extern vector<Tileset> Tilesets;
	extern vector<TileSpriteInfo> TileSpriteInfos;
	extern Uint16 EmptyTile;
	extern vector<SceneLayer> Layers;
	extern bool AnyLayerTileChange;
	extern int TileCount;
	extern int TileWidth;
	extern int TileHeight;
	extern int BaseTileCount;
	extern int BaseTilesetCount;
	extern bool TileCfgLoaded;
	extern vector<TileConfig*> TileCfg;
	extern vector<ResourceType*> SpriteList;
	extern vector<ResourceType*> ImageList;
	extern vector<ResourceType*> SoundList;
	extern vector<ResourceType*> MusicList;
	extern vector<ResourceType*> ModelList;
	extern vector<ResourceType*> MediaList;
	extern vector<GameTexture*> TextureList;
	extern vector<Animator*> AnimatorList;
	extern int Frame;
	extern bool Paused;
	extern bool Loaded;
	extern int TileAnimationEnabled;
	extern View Views[MAX_SCENE_VIEWS];
	extern int ViewCurrent;
	extern int ViewsActive;
	extern int CurrentDrawGroup;
	extern int ObjectViewRenderFlag;
	extern int TileViewRenderFlag;
	extern Perf_ViewRender PERF_ViewRender[MAX_SCENE_VIEWS];
	extern char NextScene[256];
	extern char CurrentScene[256];
	extern bool DoRestart;
	extern bool NoPersistency;
	extern int TimeEnabled;
	extern int TimeCounter;
	extern int Minutes;
	extern int Seconds;
	extern int Milliseconds;
	extern int Filter;
	extern int CurrentSceneInList;
	extern char CurrentFolder[256];
	extern char CurrentID[256];
	extern char CurrentResourceFolder[256];
	extern char CurrentCategory[256];
	extern int ActiveCategory;
	extern int DebugMode;
	extern float CollisionTolerance;
	extern bool UseCollisionOffset;
	extern float CollisionMaskAir;
	extern CollisionBox CollisionOuter;
	extern CollisionBox CollisionInner;
	extern Entity* CollisionEntity;
	extern CollisionSensor Sensors[6];
	extern float CollisionMinimumDistance;
	extern float LowCollisionTolerance;
	extern float HighCollisionTolerance;
	extern int FloorAngleTolerance;
	extern int WallAngleTolerance;
	extern int RoofAngleTolerance;
	extern bool ShowHitboxes;
	extern int DebugHitboxCount;
	extern DebugHitboxInfo DebugHitboxList[DEBUG_HITBOX_COUNT];

	void Add(Entity** first, Entity** last, int* count, Entity* obj);
	void Remove(Entity** first, Entity** last, int* count, Entity* obj);
	void AddToScene(Entity* obj);
	void RemoveFromScene(Entity* obj);
	void Clear(Entity** first, Entity** last, int* count);
	void AddStatic(ObjectList* objectList, Entity* obj);
	void AddDynamic(ObjectList* objectList, Entity* obj);
	void DeleteRemoved(Entity* obj);
	void OnEvent(Uint32 event);
	void SetCurrent(const char* categoryName, const char* sceneName);
	void SetInfoFromCurrentID();
	void Init();
	void InitObjectListsAndRegistries();
	void ResetPerf();
	void Update();
	Tileset* GetTileset(int tileID);
	TileAnimator* GetTileAnimator(int tileID);
	void SetViewActive(int viewIndex, bool active);
	void SetViewPriority(int viewIndex, int priority);
	void SortViews();
	void SetView(int viewIndex);
	bool CheckPosOnScreen(float posX, float posY, float rangeX, float rangeY);
	void RenderView(int viewIndex, bool doPerf);
	void Render();
	void AfterScene();
	void Restart();
	void LoadScene(const char* filename);
	void ProcessSceneTimer();
	ObjectList* NewObjectList(const char* objectName);
	ObjectList* GetObjectList(const char* objectName, bool callListLoadFunction);
	ObjectList* GetObjectList(const char* objectName);
	ObjectList* GetStaticObjectList(const char* objectName);
	void AddManagers();
	void FreePriorityLists();
	void InitPriorityLists();
	void SetPriorityPerLayer(int count);
	bool AddTileset(char* path);
	void LoadTileCollisions(const char* filename, size_t tilesetID);
	void UnloadTileCollisions();
	bool GetResourceListSpace(vector<ResourceType*>* list,
		ResourceType* resource,
		size_t& index,
		bool& foundEmpty);
	bool GetResource(vector<ResourceType*>* list, ResourceType* resource, size_t& index);
	int LoadSpriteResource(const char* filename, int unloadPolicy);
	int LoadImageResource(const char* filename, int unloadPolicy);
	int LoadFontResource(const char* filename, int pixel_sz, int unloadPolicy);
	int LoadModelResource(const char* filename, int unloadPolicy);
	int LoadMusicResource(const char* filename, int unloadPolicy);
	int LoadSoundResource(const char* filename, int unloadPolicy);
	int LoadVideoResource(const char* filename, int unloadPolicy);
	ResourceType* GetSpriteResource(int index);
	ResourceType* GetImageResource(int index);
	void DisposeInScope(Uint32 scope);
	void Dispose();
	void UnloadTilesets();
	size_t AddGameTexture(GameTexture* texture);
	bool FindGameTextureByID(int id, size_t& out);
	void
	SetTile(int layer, int x, int y, int tileID, int flip_x, int flip_y, int collA, int collB);
	int CollisionAt(int x, int y, int collisionField, int collideSide, int* angle);
	int CollisionInLine(int x,
		int y,
		int angleMode,
		int checkLen,
		int collisionField,
		bool compareAngle,
		Sensor* sensor);
	void SetupCollisionConfig(float minDistance,
		float lowTolerance,
		float highTolerance,
		int floorAngleTolerance,
		int wallAngleTolerance,
		int roofAngleTolerance);
	int AddDebugHitbox(int type, int dir, Entity* entity, CollisionBox* hitbox);
	bool CheckObjectCollisionTouch(Entity* thisEntity,
		CollisionBox* thisHitbox,
		Entity* otherEntity,
		CollisionBox* otherHitbox);
	bool CheckObjectCollisionCircle(Entity* thisEntity,
		float thisRadius,
		Entity* otherEntity,
		float otherRadius);
	bool CheckObjectCollisionBox(Entity* thisEntity,
		CollisionBox* thisHitbox,
		Entity* otherEntity,
		CollisionBox* otherHitbox,
		bool setValues);
	bool CheckObjectCollisionPlatform(Entity* thisEntity,
		CollisionBox* thisHitbox,
		Entity* otherEntity,
		CollisionBox* otherHitbox,
		bool setValues);
	bool ObjectTileCollision(Entity* entity,
		int cLayers,
		int cMode,
		int cPlane,
		int xOffset,
		int yOffset,
		bool setPos);
	bool ObjectTileGrip(Entity* entity,
		int cLayers,
		int cMode,
		int cPlane,
		float xOffset,
		float yOffset,
		float tolerance);
	void
	ProcessObjectMovement(Entity* entity, CollisionBox* outerBox, CollisionBox* innerBox);
	void ProcessPathGrip();
	void ProcessAirCollision_Down();
	void ProcessAirCollision_Up();
	void SetPathGripSensors(CollisionSensor* sensors);
	void FindFloorPosition(CollisionSensor* sensor);
	void FindLWallPosition(CollisionSensor* sensor);
	void FindRoofPosition(CollisionSensor* sensor);
	void FindRWallPosition(CollisionSensor* sensor);
	void FloorCollision(CollisionSensor* sensor);
	void LWallCollision(CollisionSensor* sensor);
	void RoofCollision(CollisionSensor* sensor);
	void RWallCollision(CollisionSensor* sensor);
};

#endif /* ENGINE_SCENE_H */
