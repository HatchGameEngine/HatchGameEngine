#ifndef ENGINE_SCENE_H
#define ENGINE_SCENE_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED

class Entity;

#include <Engine/Includes/Standard.h>
#include <Engine/Application.h>
#include <Engine/Graphics.h>
#include <Engine/ResourceTypes/ResourceType.h>
#include <Engine/Types/EntityTypes.h>
#include <Engine/Includes/HashMap.h>
#include <Engine/Types/ObjectList.h>
#include <Engine/Types/DrawGroupList.h>
#include <Engine/Scene/SceneLayer.h>
#include <Engine/Scene/TileConfig.h>
#include <Engine/Scene/TileSpriteInfo.h>
#include <Engine/Scene/View.h>
#include <Engine/Diagnostics/PerformanceTypes.h>

class Scene {
public:
    static int                   ShowTileCollisionFlag;
    static int                   ShowObjectRegions;
    static HashMap<ObjectList*>* ObjectLists;
    static HashMap<ObjectList*>* ObjectRegistries;
    static int                   StaticObjectCount;
    static Entity*               StaticObjectFirst;
    static Entity*               StaticObjectLast;
    static int                   DynamicObjectCount;
    static Entity*               DynamicObjectFirst;
    static Entity*               DynamicObjectLast;
    static int                   PriorityPerLayer;
    static DrawGroupList*        PriorityLists;
    static vector<ISprite*>      TileSprites;
    static vector<TileSpriteInfo>TileSpriteInfos;
    static Uint16                EmptyTile;
    static float                 CameraX;
    static float                 CameraY;
    static vector<SceneLayer>    Layers;
    static bool                  AnyLayerTileChange;
    static int                   TileCount;
    static int                   TileSize;
    static TileConfig*           TileCfgA;
    static TileConfig*           TileCfgB;
    static Uint32*               ExtraPalettes;
    static HashMap<ISprite*>*    SpriteMap;
    static vector<ResourceType*> SpriteList;
    static vector<ResourceType*> ImageList;
    static vector<ResourceType*> SoundList;
    static vector<ResourceType*> MusicList;
    static vector<ResourceType*> ShaderList;
    static vector<ResourceType*> ModelList;
    static vector<ResourceType*> MediaList;
    static int                   Frame;
    static bool                  Paused;
    static int                   MainLayer;
    static View                  Views[MAX_SCENE_VIEWS];
    static int                   ViewCurrent;
    static int                   ViewsActive;
    static int                   ObjectViewRenderFlag;
    static int                   TileViewRenderFlag;
    static Perf_ViewRender       PERF_ViewRender[MAX_SCENE_VIEWS];
    static char                  NextScene[256];
    static char                  CurrentScene[256];
    static bool                  DoRestart;

    static void Add(Entity** first, Entity** last, int* count, Entity* obj);
    static void Remove(Entity** first, Entity** last, int* count, Entity* obj);
    static void Clear(Entity** first, Entity** last, int* count);
    static void AddStatic(ObjectList* objectList, Entity* obj);
    static void AddDynamic(ObjectList* objectList, Entity* obj);
    static void OnEvent(Uint32 event);
    static void Init();
    static void ResetPerf();
    static void Update();
    static int ViewSortFunction(const void *a, const void *b);
    static void SortViews();
    static void Render();
    static void AfterScene();
    static void Restart();
    static void LoadScene(const char* filename);
    static void AddStaticClass();
    static void AddManagers();
    static void LoadTileCollisions(const char* filename);
    static void SaveTileCollisions();
    static void DisposeInScope(Uint32 scope);
    static void Dispose();
    static void Exit();
    static void UpdateTileBatchAll();
    static void SetTile(int layer, int x, int y, int tileID, int flip_x, int flip_y, int collA, int collB);
    static int  CollisionAt(int x, int y, int collisionField, int collideSide, int* angle);
    static int  CollisionInLine(int x, int y, int angleMode, int checkLen, int collisionField, bool compareAngle, Sensor* sensor);
};

#endif /* ENGINE_SCENE_H */
