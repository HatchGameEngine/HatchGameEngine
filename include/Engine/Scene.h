#ifndef ENGINE_SCENE_H
#define ENGINE_SCENE_H
class Entity;

#include <Engine/Includes/Standard.h>
#include <Engine/Application.h>
#include <Engine/Graphics.h>
#include <Engine/ResourceTypes/ResourceType.h>
#include <Engine/Includes/HashMap.h>
#include <Engine/Math/Math.h>
#include <Engine/Types/Tileset.h>
#include <Engine/Types/EntityTypes.h>
#include <Engine/Types/ObjectList.h>
#include <Engine/Types/ObjectRegistry.h>
#include <Engine/Types/DrawGroupList.h>
#include <Engine/Rendering/GameTexture.h>
#include <Engine/Scene/SceneConfig.h>
#include <Engine/Scene/SceneLayer.h>
#include <Engine/Scene/SceneEnums.h>
#include <Engine/Scene/TileConfig.h>
#include <Engine/Scene/TileSpriteInfo.h>
#include <Engine/Scene/TileAnimation.h>
#include <Engine/Scene/View.h>
#include <Engine/Diagnostics/PerformanceTypes.h>

class Scene {
private:
    static void RemoveObject(Entity* obj);
    static void RunTileAnimations();
    static void ResetViews();
    static void Iterate(Entity* first, std::function<void(Entity* e)> func);
    static void IterateAll(Entity* first, std::function<void(Entity* e)> func);
    static void ResetPriorityListIndex(Entity* first);
    static Entity* SortEntityList(Entity* head);
    static bool SplitEntityList(Entity* head, Entity** left, Entity** right);
    static Entity* MergeEntityList(Entity* left, Entity* right);
    static int GetPersistenceScopeForObjectDeletion();
    static void ClearPriorityLists();
    static void DeleteObjects(Entity** first, Entity** last, int* count);
    static void RemoveNonPersistentObjects(Entity** first, Entity** last, int* count);
    static void DeleteAllObjects();
    static void AddStaticClass();
    static void CallGameStart();
    static void SpawnStaticObject(const char* objectName);
    static void ReadRSDKTile(TileConfig* tile, Uint8* line);
    static void LoadRSDKTileConfig(int tilesetID, Stream* tileColReader);
    static void LoadHCOLTileConfig(size_t tilesetID, Stream* tileColReader);
    static void InitTileCollisions();
    static void ClearTileCollisions(TileConfig* cfg, size_t numTiles);
    static void SetTileCount(size_t tileCount);
    static bool GetTextureListSpace(size_t* out);

public:
    static int                       ShowTileCollisionFlag;
    static int                       ShowObjectRegions;
    static HashMap<VMValue>*         Properties;
    static HashMap<ObjectList*>*     ObjectLists;
    static HashMap<ObjectRegistry*>* ObjectRegistries;
    static HashMap<ObjectList*>*     StaticObjectLists;
    static int                       StaticObjectCount;
    static Entity*                   StaticObjectFirst;
    static Entity*                   StaticObjectLast;
    static int                       DynamicObjectCount;
    static Entity*                   DynamicObjectFirst;
    static Entity*                   DynamicObjectLast;
    static int                       ObjectCount;
    static Entity*                   ObjectFirst;
    static Entity*                   ObjectLast;
    static int                       BasePriorityPerLayer;
    static int                       PriorityPerLayer;
    static DrawGroupList*            PriorityLists;
    static vector<Tileset>           Tilesets;
    static vector<TileSpriteInfo>    TileSpriteInfos;
    static Uint16                    EmptyTile;
    static vector<SceneLayer>        Layers;
    static bool                      AnyLayerTileChange;
    static int                       TileCount;
    static int                       TileWidth;
    static int                       TileHeight;
    static int                       BaseTileCount;
    static int                       BaseTilesetCount;
    static bool                      TileCfgLoaded;
    static vector<TileConfig*>       TileCfg;
    static vector<ResourceType*>     SpriteList;
    static vector<ResourceType*>     ImageList;
    static vector<ResourceType*>     SoundList;
    static vector<ResourceType*>     MusicList;
    static vector<ResourceType*>     ModelList;
    static vector<ResourceType*>     MediaList;
    static vector<GameTexture*>      TextureList;
    static vector<Animator*>         AnimatorList;
    static int                       Frame;
    static bool                      Paused;
    static bool                      Loaded;
    static bool                      Initializing;
    static bool                      NeedEntitySort;
    static int                       TileAnimationEnabled;
    static View                      Views[MAX_SCENE_VIEWS];
    static int                       ViewCurrent;
    static int                       ViewsActive;
    static int                       CurrentDrawGroup;
    static int                       ObjectViewRenderFlag;
    static int                       TileViewRenderFlag;
    static Perf_ViewRender           PERF_ViewRender[MAX_SCENE_VIEWS];
    static char                      NextScene[256];
    static char                      CurrentScene[256];
    static bool                      DoRestart;
    static bool                      NoPersistency;
    static int                       TimeEnabled;
    static int                       TimeCounter;
    static int                       Minutes;
    static int                       Seconds;
    static int                       Milliseconds;
    static int                       Filter;
    static int                       CurrentSceneInList;
    static char                      CurrentFolder[256];
    static char                      CurrentID[256];
    static char                      CurrentResourceFolder[256];
    static char                      CurrentCategory[256];
    static int                       ActiveCategory;
    static int                       DebugMode;
    static float                     CollisionTolerance;
    static bool                      UseCollisionOffset;
    static float                     CollisionMaskAir;
    static CollisionBox              CollisionOuter;
    static CollisionBox              CollisionInner;
    static Entity*                   CollisionEntity;
    static CollisionSensor           Sensors[6];
    static float                     CollisionMinimumDistance;
    static float                     LowCollisionTolerance;
    static float                     HighCollisionTolerance;
    static int                       FloorAngleTolerance;
    static int                       WallAngleTolerance;
    static int                       RoofAngleTolerance;
    static bool                      ShowHitboxes;
    static int                       DebugHitboxCount;
    static DebugHitboxInfo           DebugHitboxList[DEBUG_HITBOX_COUNT];

    static void Add(Entity** first, Entity** last, int* count, Entity* obj);
    static void Remove(Entity** first, Entity** last, int* count, Entity* obj);
    static void AddToScene(Entity* obj);
    static void RemoveFromScene(Entity* obj);
    static void Clear(Entity** first, Entity** last, int* count);
    static bool AddStatic(ObjectList* objectList, Entity* obj);
    static void AddDynamic(ObjectList* objectList, Entity* obj);
    static void DeleteRemoved(Entity* obj);
    static void OnEvent(Uint32 event);
    static void SetCurrent(const char* categoryName, const char* sceneName);
    static void SetInfoFromCurrentID();
    static void Init();
    static void InitObjectListsAndRegistries();
    static void ResetPerf();
    static void Update();
    static void SortEntities();
    static Tileset* GetTileset(int tileID);
    static TileAnimator* GetTileAnimator(int tileID);
    static void SetViewActive(int viewIndex, bool active);
    static void SetViewPriority(int viewIndex, int priority);
    static void SortViews();
    static void SetView(int viewIndex);
    static bool CheckPosOnScreen(float posX, float posY, float rangeX, float rangeY);
    static void RenderView(int viewIndex, bool doPerf);
    static void Render();
    static void AfterScene();
    static void Restart();
    static void LoadScene(const char* filename);
    static void ProcessSceneTimer();
    static ObjectList* NewObjectList(const char* objectName);
    static ObjectList* GetObjectList(const char* objectName, bool callListLoadFunction);
    static ObjectList* GetObjectList(const char* objectName);
    static ObjectList* GetStaticObjectList(const char* objectName);
    static void AddManagers();
    static void FreePriorityLists();
    static void InitPriorityLists();
    static void SetPriorityPerLayer(int count);
    static bool AddTileset(char* path);
    static void LoadTileCollisions(const char* filename, size_t tilesetID);
    static void UnloadTileCollisions();
    static bool GetResourceListSpace(vector<ResourceType*>* list, ResourceType* resource, size_t& index, bool& foundEmpty);
    static bool GetResource(vector<ResourceType*>* list, ResourceType* resource, size_t& index);
    static int LoadSpriteResource(const char* filename, int unloadPolicy);
    static int LoadImageResource(const char* filename, int unloadPolicy);
    static int LoadFontResource(const char* filename, int pixel_sz, int unloadPolicy);
    static int LoadModelResource(const char* filename, int unloadPolicy);
    static int LoadMusicResource(const char* filename, int unloadPolicy);
    static int LoadSoundResource(const char* filename, int unloadPolicy);
    static int LoadVideoResource(const char* filename, int unloadPolicy);
    static ResourceType* GetSpriteResource(int index);
    static ResourceType* GetImageResource(int index);
    static void DisposeInScope(Uint32 scope);
    static void Dispose();
    static void UnloadTilesets();
    static size_t AddGameTexture(GameTexture* texture);
    static bool FindGameTextureByID(int id, size_t& out);
    static void SetTile(int layer, int x, int y, int tileID, int flip_x, int flip_y, int collA, int collB);
    static int CollisionAt(int x, int y, int collisionField, int collideSide, int* angle);
    static int CollisionInLine(int x, int y, int angleMode, int checkLen, int collisionField, bool compareAngle, Sensor* sensor);
    static void SetupCollisionConfig(float minDistance, float lowTolerance, float highTolerance, int floorAngleTolerance, int wallAngleTolerance, int roofAngleTolerance);
    static int AddDebugHitbox(int type, int dir, Entity* entity, CollisionBox* hitbox);
    static bool CheckObjectCollisionTouch(Entity* thisEntity, CollisionBox* thisHitbox, Entity* otherEntity, CollisionBox* otherHitbox);
    static bool CheckObjectCollisionCircle(Entity* thisEntity, float thisRadius, Entity* otherEntity, float otherRadius);
    static bool CheckObjectCollisionBox(Entity* thisEntity, CollisionBox* thisHitbox, Entity* otherEntity, CollisionBox* otherHitbox, bool setValues);
    static bool CheckObjectCollisionPlatform(Entity* thisEntity, CollisionBox* thisHitbox, Entity* otherEntity, CollisionBox* otherHitbox, bool setValues);
    static bool ObjectTileCollision(Entity* entity, int cLayers, int cMode, int cPlane, int xOffset, int yOffset, bool setPos);
    static bool ObjectTileGrip(Entity* entity, int cLayers, int cMode, int cPlane, float xOffset, float yOffset, float tolerance);
    static void ProcessObjectMovement(Entity* entity, CollisionBox* outerBox, CollisionBox* innerBox);
    static void ProcessPathGrip();
    static void ProcessAirCollision_Down();
    static void ProcessAirCollision_Up();
    static void SetPathGripSensors(CollisionSensor* sensors);
    static void FindFloorPosition(CollisionSensor* sensor);
    static void FindLWallPosition(CollisionSensor* sensor);
    static void FindRoofPosition(CollisionSensor* sensor);
    static void FindRWallPosition(CollisionSensor* sensor);
    static void FloorCollision(CollisionSensor* sensor);
    static void LWallCollision(CollisionSensor* sensor);
    static void RoofCollision(CollisionSensor* sensor);
    static void RWallCollision(CollisionSensor* sensor);
};

#endif /* ENGINE_SCENE_H */
