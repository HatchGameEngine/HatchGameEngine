#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Application.h>
#include <Engine/Graphics.h>

#include <Engine/ResourceTypes/ResourceType.h>

#include <Engine/Includes/HashMap.h>
#include <Engine/Math/Math.h>
#include <Engine/Types/EntityTypes.h>
#include <Engine/Types/ObjectList.h>
#include <Engine/Types/ObjectRegistry.h>
#include <Engine/Types/DrawGroupList.h>
#include <Engine/Scene/SceneConfig.h>
#include <Engine/Scene/SceneLayer.h>
#include <Engine/Scene/TileConfig.h>
#include <Engine/Scene/TileSpriteInfo.h>

#include <Engine/Scene/View.h>
#include <Engine/Diagnostics/PerformanceTypes.h>

need_t Entity;

class Scene {
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

    static int                       PriorityPerLayer;
    static DrawGroupList*            PriorityLists;

    static vector<ISprite*>          TileSprites;
    static vector<TileSpriteInfo>    TileSpriteInfos;
    static Uint16                    EmptyTile;

    static vector<SceneLayer>        Layers;
    static bool                      AnyLayerTileChange;

    static int                       TileCount;
    static int                       TileSize;
    static TileConfig*               TileCfgA;
    static TileConfig*               TileCfgB;

    static vector<ResourceType*>     SpriteList;
    static vector<ResourceType*>     ImageList;
    static vector<ResourceType*>     SoundList;
    static vector<ResourceType*>     MusicList;
    static vector<ResourceType*>     ModelList;
    static vector<ResourceType*>     MediaList;
    static vector<Animator*>         AnimatorList;

    static int                       Frame;
    static bool                      Paused;

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

    static vector<SceneListEntry>    ListData;
    static vector<SceneListInfo>     ListCategory;
    static int                       ListPos;
    static char                      CurrentFolder[16];
    static char                      CurrentID[16];
    static char                      CurrentSpriteFolder[16];
    static char                      CurrentCategory[16];
    static int                       ActiveCategory;
    static int                       CategoryCount;
    static int                       ListCount;
    static int                       StageCount;

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
};
#endif

#include <Engine/Scene.h>

#include <Engine/Audio/AudioManager.h>
#include <Engine/Bytecode/BytecodeObject.h>
#include <Engine/Bytecode/BytecodeObjectManager.h>
#include <Engine/Bytecode/GarbageCollector.h>
#include <Engine/Bytecode/SourceFileMap.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Diagnostics/MemoryPools.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Hashing/CombinedHash.h>
#include <Engine/Hashing/CRC32.h>
#include <Engine/Hashing/FNV1A.h>
#include <Engine/Hashing/MD5.h>
#include <Engine/Includes/HashMap.h>
#include <Engine/IO/FileStream.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/IO/Compression/ZLibStream.h>
#include <Engine/Math/Math.h>
#include <Engine/ResourceTypes/SceneFormats/HatchSceneReader.h>
#include <Engine/ResourceTypes/SceneFormats/RSDKSceneReader.h>
#include <Engine/ResourceTypes/ISound.h>
#include <Engine/ResourceTypes/ResourceManager.h>
#include <Engine/ResourceTypes/SceneFormats/TiledMapReader.h>
#include <Engine/Rendering/SDL2/SDL2Renderer.h>
#include <Engine/Scene/SceneConfig.h>
#include <Engine/TextFormats/XML/XMLParser.h>
#include <Engine/TextFormats/XML/XMLNode.h>
#include <Engine/Types/EntityTypes.h>
#include <Engine/Types/ObjectList.h>
#include <Engine/Types/ObjectRegistry.h>
#include <Engine/Utilities/StringUtils.h>

// Layering variables
vector<SceneLayer>        Scene::Layers;
bool                      Scene::AnyLayerTileChange = false;
int                       Scene::PriorityPerLayer = 16;
DrawGroupList*            Scene::PriorityLists = NULL;

// Rendering variables
int                       Scene::ShowTileCollisionFlag = 0;
int                       Scene::ShowObjectRegions = 0;

// Property variables
HashMap<VMValue>*         Scene::Properties = NULL;

// Object variables
HashMap<ObjectList*>*     Scene::ObjectLists = NULL;
HashMap<ObjectRegistry*>* Scene::ObjectRegistries = NULL;

HashMap<ObjectList*>*     Scene::StaticObjectLists = NULL;

int                       Scene::StaticObjectCount = 0;
Entity*                   Scene::StaticObjectFirst = NULL;
Entity*                   Scene::StaticObjectLast = NULL;
int                       Scene::DynamicObjectCount = 0;
Entity*                   Scene::DynamicObjectFirst = NULL;
Entity*                   Scene::DynamicObjectLast = NULL;

int                       Scene::ObjectCount = 0;
Entity*                   Scene::ObjectFirst = NULL;
Entity*                   Scene::ObjectLast = NULL;

// Tile variables
vector<ISprite*>          Scene::TileSprites;
vector<TileSpriteInfo>    Scene::TileSpriteInfos;
int                       Scene::TileCount = 0;
int                       Scene::TileSize = 16;
TileConfig*               Scene::TileCfgA = NULL;
TileConfig*               Scene::TileCfgB = NULL;
Uint16                    Scene::EmptyTile = 0x000;

// View variables
int                       Scene::Frame = 0;
bool                      Scene::Paused = false;
View                      Scene::Views[MAX_SCENE_VIEWS];
int                       Scene::ViewCurrent = 0;
int                       Scene::ViewsActive = 1;
int                       Scene::CurrentDrawGroup = -1;
int                       Scene::ObjectViewRenderFlag;
int                       Scene::TileViewRenderFlag;
Perf_ViewRender           Scene::PERF_ViewRender[MAX_SCENE_VIEWS];

char                      Scene::NextScene[256];
char                      Scene::CurrentScene[256];
bool                      Scene::DoRestart = false;
bool                      Scene::NoPersistency = false;

// Time variables
int                       Scene::TimeEnabled = 0;
int                       Scene::TimeCounter = 0;
int                       Scene::Milliseconds = 0;
int                       Scene::Seconds = 0;
int                       Scene::Minutes = 0;

// Scene list variables
vector<SceneListEntry>    Scene::ListData;
vector<SceneListInfo>     Scene::ListCategory;
int                       Scene::ListPos;
char                      Scene::CurrentFolder[16];
char                      Scene::CurrentID[16];
char                      Scene::CurrentSpriteFolder[16];
char                      Scene::CurrentCategory[16];
int                       Scene::ActiveCategory;
int                       Scene::CategoryCount;
int                       Scene::ListCount;
int                       Scene::StageCount;

// Debug mode variables
int                       Scene::DebugMode;

// Resource managing variables
vector<ResourceType*>     Scene::SpriteList;
vector<ResourceType*>     Scene::ImageList;
vector<ResourceType*>     Scene::SoundList;
vector<ResourceType*>     Scene::MusicList;
vector<ResourceType*>     Scene::ModelList;
vector<ResourceType*>     Scene::MediaList;
vector<Animator*>         Scene::AnimatorList;

Entity*                   StaticObject = NULL;
ObjectList*               StaticObjectList = NULL;
bool                      DEV_NoTiles = false;
bool                      DEV_NoObjectRender = false;

int ViewRenderList[MAX_SCENE_VIEWS];

#define COLLISION_OFFSET 4.0

// Collision variables
float                       Scene::CollisionTolerance = 0.0;
bool                        Scene::UseCollisionOffset = false;
float                       Scene::CollisionMaskAir = 0.0;
CollisionBox                Scene::CollisionOuter = { 0, 0, 0, 0 };
CollisionBox                Scene::CollisionInner = { 0, 0, 0, 0 };
Entity*                     Scene::CollisionEntity = NULL;
CollisionSensor             Scene::Sensors[6];
float                       Scene::CollisionMinimumDistance = 14.0;
float                       Scene::LowCollisionTolerance = 8.0;
float                       Scene::HighCollisionTolerance = 14.0;
int                         Scene::FloorAngleTolerance = 0x20;
int                         Scene::WallAngleTolerance = 0x20;
int                         Scene::RoofAngleTolerance = 0x20;
bool                        Scene::ShowHitboxes = false;
int                         Scene::DebugHitboxCount = 0;
DebugHitboxInfo             Scene::DebugHitboxList[DEBUG_HITBOX_COUNT];

void ObjectList_CallLoads(Uint32 key, ObjectList* list) {
    // This is called before object lists are cleared, so we need to check
    // if there are any entities in the list.
    if (!list->Count() && !Scene::StaticObjectLists->Exists(key))
        return;

    BytecodeObjectManager::CallFunction(list->LoadFunctionName);
}
void ObjectList_CallGlobalUpdates(Uint32, ObjectList* list) {
    BytecodeObjectManager::CallFunction(list->GlobalUpdateFunctionName);
}
void UpdateObjectEarly(Entity* ent) {
    if (Scene::Paused && ent->Pauseable && ent->ActiveStatus != Active_PAUSED && ent->ActiveStatus != Active_ALWAYS)
        return;
    if (!ent->Active)
        return;
    if (!ent->OnScreen)
        return;

    double elapsed = Clock::GetTicks();

    ent->UpdateEarly();

    elapsed = Clock::GetTicks() - elapsed;

    if (ent->List) {
        ObjectList* list = ent->List;
        double count = list->AverageUpdateEarlyItemCount;
        if (count < 60.0 * 60.0) {
            count += 1.0;
            if (count == 1.0)
                list->AverageUpdateEarlyTime = elapsed;
            else
                list->AverageUpdateEarlyTime =
                list->AverageUpdateEarlyTime + (elapsed - list->AverageUpdateEarlyTime) / count;
            list->AverageUpdateEarlyItemCount = count;
        }
    }
}
void UpdateObjectLate(Entity* ent) {
    if (Scene::Paused && ent->Pauseable && ent->ActiveStatus != Active_PAUSED && ent->ActiveStatus != Active_ALWAYS)
        return;
    if (!ent->Active)
        return;
    if (!ent->OnScreen)
        return;

    double elapsed = Clock::GetTicks();

    ent->UpdateLate();

    elapsed = Clock::GetTicks() - elapsed;

    if (ent->List) {
        ObjectList* list = ent->List;
        double count = list->AverageUpdateLateItemCount;
        if (count < 60.0 * 60.0) {
            count += 1.0;
            if (count == 1.0)
                list->AverageUpdateLateTime = elapsed;
            else
                list->AverageUpdateLateTime =
                list->AverageUpdateLateTime + (elapsed - list->AverageUpdateLateTime) / count;
            list->AverageUpdateLateItemCount = count;
        }
    }
}
void UpdateObject(Entity* ent) {
    if (Scene::Paused && ent->Pauseable && ent->ActiveStatus != Active_PAUSED && ent->ActiveStatus != Active_ALWAYS)
        return;

    if (!ent->Active)
        return;

    bool onScreenX = (ent->OnScreenHitboxW == 0.0f);
    bool onScreenY = (ent->OnScreenHitboxH == 0.0f);

    switch (ent->ActiveStatus) {
    default:
        break;

    case Active_NEVER:
    case Active_PAUSED:
        ent->InRange = false;
        break;

    case Active_ALWAYS:
    case Active_NORMAL:
        ent->InRange = true;
        break;

    case Active_BOUNDS:
        ent->InRange = false;

        for (int i = 0; i < Scene::ViewsActive; i++) {
            if (!onScreenX) {
                onScreenX = (ent->X + ent->OnScreenHitboxW * 0.5f >= Scene::Views[i].X &&
                    ent->X - ent->OnScreenHitboxW * 0.5f < Scene::Views[i].X + Scene::Views[i].Width);
            }
            if (!onScreenY) {
                onScreenY = (ent->Y + ent->OnScreenHitboxH * 0.5f >= Scene::Views[i].Y &&
                    ent->Y - ent->OnScreenHitboxH * 0.5f < Scene::Views[i].Y + Scene::Views[i].Height);
            }
        }

        if (onScreenX && onScreenY)
            ent->InRange = true;

        break;

    case Active_XBOUNDS:
        ent->InRange = false;

        for (int i = 0; i < Scene::ViewsActive; i++) {
            if (!onScreenX) {
                onScreenX = (ent->X + ent->OnScreenHitboxW * 0.5f >= Scene::Views[i].X &&
                    ent->X - ent->OnScreenHitboxW * 0.5f < Scene::Views[i].X + Scene::Views[i].Width);
            }
        }

        if (onScreenX)
            ent->InRange = true;

        break;

    case Active_YBOUNDS:
        ent->InRange = false;

        for (int i = 0; i < Scene::ViewsActive; i++) {
            if (!onScreenY) {
                onScreenY = (ent->Y + ent->OnScreenHitboxH * 0.5f >= Scene::Views[i].Y &&
                    ent->Y - ent->OnScreenHitboxH * 0.5f < Scene::Views[i].Y + Scene::Views[i].Height);
            }
        }

        if (onScreenY)
            ent->InRange = true;

        break;

    case Active_RBOUNDS:
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
            ObjectList* list = ent->List;
            double count = list->AverageUpdateItemCount;
            if (count < 60.0 * 60.0) {
                count += 1.0;
                if (count == 1.0)
                    list->AverageUpdateTime = elapsed;
                else
                    list->AverageUpdateTime =
                        list->AverageUpdateTime + (elapsed - list->AverageUpdateTime) / count;
                list->AverageUpdateItemCount = count;
            }
        }
    }
    else {
        ent->OnScreen = false;
    }

    if (!Scene::PriorityLists)
        return;

    int oldPriority = ent->PriorityOld;
    int maxPriority = Scene::PriorityPerLayer - 1;
    // HACK: This needs to error
    // if (ent->Priority < 0 || (ent->Priority >= (Scene::Layers.size() << Scene::PriorityPerLayer)))
    //     return;
    if (ent->Priority < 0)
        ent->Priority = 0;
    if (ent->Priority > maxPriority)
        ent->Priority = maxPriority;

    // If hasn't been put in a list yet.
    if (ent->PriorityListIndex == -1) {
        ent->PriorityListIndex = Scene::PriorityLists[ent->Priority].Add(ent);
    }
    // If Priority is changed.
    else if (ent->Priority != oldPriority) {
        // Remove entry in old list.
        Scene::PriorityLists[oldPriority].Remove(ent);
        ent->PriorityListIndex = Scene::PriorityLists[ent->Priority].Add(ent);
    }
    // Sort list.
    if (ent->Depth != ent->OldDepth) {
        Scene::PriorityLists[ent->Priority].NeedsSorting = true;
    }
    ent->PriorityOld = ent->Priority;
    ent->OldDepth = ent->Depth;
}

// Double linked-list functions
PUBLIC STATIC void Scene::Add(Entity** first, Entity** last, int* count, Entity* obj) {
    // Set "prev" of obj to last
    obj->PrevEntity = (*last);
    obj->NextEntity = NULL;

    // If the last exists, set that ones "next" to obj
    if ((*last))
        (*last)->NextEntity = obj;

    // Set obj as the first if there is not one
    if (!(*first))
        (*first) = obj;

    (*last) = obj;

    (*count)++;

    // Add to proper list
    if (!obj->List) {
        Log::Print(Log::LOG_ERROR, "obj->List == NULL");
        exit(-1);
    }
    obj->List->Add(obj);

    Scene::AddToScene(obj);
}
PUBLIC STATIC void Scene::Remove(Entity** first, Entity** last, int* count, Entity* obj) {
    if (obj == NULL) return;
    if (obj->Removed) return;

    if ((*first) == obj)
        (*first) = obj->NextEntity;

    if ((*last) == obj)
        (*last) = obj->PrevEntity;

    if (obj->PrevEntity)
        obj->PrevEntity->NextEntity = obj->NextEntity;

    if (obj->NextEntity)
        obj->NextEntity->PrevEntity = obj->PrevEntity;

    (*count)--;

    Scene::RemoveFromScene(obj);
    Scene::RemoveObject(obj);
}
PUBLIC STATIC void Scene::AddToScene(Entity* obj) {
    obj->PrevSceneEntity = Scene::ObjectLast;
    obj->NextSceneEntity = NULL;

    if (Scene::ObjectLast)
        Scene::ObjectLast->NextSceneEntity = obj;
    if (!Scene::ObjectFirst)
        Scene::ObjectFirst = obj;

    Scene::ObjectLast = obj;
    Scene::ObjectCount++;
}
PUBLIC STATIC void Scene::RemoveFromScene(Entity* obj) {
    if (Scene::ObjectFirst == obj)
        Scene::ObjectFirst = obj->NextSceneEntity;
    if (Scene::ObjectLast == obj)
        Scene::ObjectLast = obj->PrevSceneEntity;

    if (obj->PrevSceneEntity)
        obj->PrevSceneEntity->NextSceneEntity = obj->NextSceneEntity;
    if (obj->NextSceneEntity)
        obj->NextSceneEntity->PrevSceneEntity = obj->PrevSceneEntity;

    Scene::ObjectCount--;
}
PRIVATE STATIC void Scene::RemoveObject(Entity* obj) {
    // Remove from proper list
    if (obj->List)
        obj->List->Remove(obj);

    // Remove from draw groups
    for (int l = 0; l < Scene::PriorityPerLayer; l++)
        PriorityLists[l].Remove(obj);

    // If this object is unreachable script-side, that means it can
    // be deleted during garbage collection.
    // It doesn't really matter if it's still active or not, since it
    // won't be in any object list or draw groups at this point.
    obj->Remove();
}
PUBLIC STATIC void Scene::Clear(Entity** first, Entity** last, int* count) {
    (*first) = NULL;
    (*last) = NULL;
    (*count) = 0;
}

// Object management
PUBLIC STATIC void Scene::AddStatic(ObjectList* objectList, Entity* obj) {
    Scene::Add(&Scene::StaticObjectFirst, &Scene::StaticObjectLast, &Scene::StaticObjectCount, obj);
}
PUBLIC STATIC void Scene::AddDynamic(ObjectList* objectList, Entity* obj) {
    Scene::Add(&Scene::DynamicObjectFirst, &Scene::DynamicObjectLast, &Scene::DynamicObjectCount, obj);
}
PUBLIC STATIC void Scene::DeleteRemoved(Entity* obj) {
    if (!obj->Removed)
        return;

    obj->Dispose();
    delete obj;
}

PUBLIC STATIC void Scene::OnEvent(Uint32 event) {
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
PUBLIC STATIC void Scene::SetScene(const char* categoryName, const char* sceneName) {
    for (int i = 0; i < Scene::CategoryCount; ++i) {
        if (!strcmp(Scene::ListCategory[i].name, categoryName)) {
            Scene::ActiveCategory = i;
            Scene::ListPos = Scene::ListCategory[i].sceneOffsetStart;

            for (int s = 0; s < Scene::ListCategory[i].sceneCount; ++s) {
                if (!strcmp(Scene::ListData[Scene::ListCategory[i].sceneOffsetStart + s].name, sceneName)) {
                    Scene::ListPos = Scene::ListCategory[i].sceneOffsetStart + s;
                    break;
                }
            }

            break;
        }
    }
}

// Scene Lifecycle
PUBLIC STATIC void Scene::Init() {
    Scene::NextScene[0] = '\0';
    Scene::CurrentScene[0] = '\0';

    GarbageCollector::Init();

    SourceFileMap::CheckForUpdate();

    Application::GameStart = true;

    BytecodeObjectManager::Init();
    BytecodeObjectManager::ResetStack();
    BytecodeObjectManager::LinkStandardLibrary();
    BytecodeObjectManager::LinkExtensions();

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
        Scene::Views[i].DrawTarget = Graphics::CreateTexture(SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, Scene::Views[i].Width, Scene::Views[i].Height);
        Scene::Views[i].UseDrawTarget = true;
        Scene::Views[i].ProjectionMatrix = Matrix4x4::Create();
        Scene::Views[i].BaseProjectionMatrix = Matrix4x4::Create();
    }
    Scene::Views[0].Active = true;
    Scene::ViewsActive = 1;

    Scene::ObjectViewRenderFlag = 0xFFFFFFFF;
    Scene::TileViewRenderFlag = 0xFFFFFFFF;

    Application::Settings->GetBool("dev", "loadAllClasses", &BytecodeObjectManager::LoadAllClasses);

    if (BytecodeObjectManager::LoadAllClasses)
        BytecodeObjectManager::LoadClasses();
}
PUBLIC STATIC void Scene::InitObjectListsAndRegistries() {
    if (Scene::ObjectLists == NULL)
        Scene::ObjectLists = new HashMap<ObjectList*>(CombinedHash::EncryptData, 4);
    if (Scene::ObjectRegistries == NULL)
        Scene::ObjectRegistries = new HashMap<ObjectRegistry*>(CombinedHash::EncryptData, 16);
    if (Scene::StaticObjectLists == NULL)
        Scene::StaticObjectLists = new HashMap<ObjectList*>(CombinedHash::EncryptData, 4);
}

PUBLIC STATIC void Scene::ResetPerf() {
    if (Scene::ObjectLists) {
        Scene::ObjectLists->ForAll([](Uint32, ObjectList* list) -> void {
            list->ResetPerf();
        });
    }
}
PUBLIC STATIC void Scene::Update() {
    // Call global updates
    if (Scene::ObjectLists)
        Scene::ObjectLists->ForAllOrdered(ObjectList_CallGlobalUpdates);

    // Early Update
    for (Entity* ent = Scene::StaticObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;
        UpdateObjectEarly(ent);
    }
    for (Entity* ent = Scene::DynamicObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;
        UpdateObjectEarly(ent);
    }

    // Update objects
    for (Entity* ent = Scene::StaticObjectFirst, *next; ent; ent = next) {
        // Store the "next" so that when/if the current is removed,
        // it can still be used to point at the end of the loop.
        next = ent->NextEntity;

        // Execute whatever on object
        UpdateObject(ent);
    }
    for (Entity* ent = Scene::DynamicObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;
        UpdateObject(ent);
    }

    // Late Update
    for (Entity* ent = Scene::StaticObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;
        UpdateObjectLate(ent);
    }
    for (Entity* ent = Scene::DynamicObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;
        UpdateObjectLate(ent);

        // Removes the object from the scene, but doesn't delete it yet.
        if (!ent->Active)
            Scene::Remove(&Scene::DynamicObjectFirst, &Scene::DynamicObjectLast, &Scene::DynamicObjectCount, ent);
    }

    #ifdef USING_FFMPEG
        AudioManager::Lock();
        Uint8 audio_buffer[0x8000]; // <-- Should be larger than AudioManager::AudioQueueMaxSize
        int needed = 0x8000; // AudioManager::AudioQueueMaxSize;
        for (size_t i = 0, i_sz = Scene::MediaList.size(); i < i_sz; i++) {
            if (!Scene::MediaList[i])
                continue;

            MediaBag* media = Scene::MediaList[i]->AsMedia;
            int queued = (int)AudioManager::AudioQueueSize;
            if (queued < needed) {
                int ready_bytes = media->Player->GetAudioData(audio_buffer, needed - queued);
                if (ready_bytes > 0) {
                    memcpy(AudioManager::AudioQueue + AudioManager::AudioQueueSize, audio_buffer, ready_bytes);
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

PUBLIC STATIC void Scene::SetViewActive(int viewIndex, bool active) {
    if (Scene::Views[viewIndex].Active == active)
        return;

    Scene::Views[viewIndex].Active = active;
    if (active)
        Scene::ViewsActive++;
    else
        Scene::ViewsActive--;

    Scene::SortViews();
}
PUBLIC STATIC void Scene::SetViewPriority(int viewIndex, int priority) {
    Scene::Views[viewIndex].Priority = priority;
    Scene::SortViews();
}

PRIVATE STATIC void Scene::ResetViews() {
    Scene::ViewsActive = 0;

    // Deactivate extra views
    for (int i = 0; i < MAX_SCENE_VIEWS; i++) {
        Scene::Views[i].Active = false;
        Scene::Views[i].Priority = 0;
    }

    Scene::SetViewActive(0, true);
}

PUBLIC STATIC void Scene::SortViews() {
    int count = 0;

    for (int i = 0; i < MAX_SCENE_VIEWS; i++) {
        if (Scene::Views[i].Active)
            ViewRenderList[count++] = i;
    }

    if (count > 1) {
        std::stable_sort(ViewRenderList, ViewRenderList + count, [](int a, int b) {
            View* viewA = &Scene::Views[a];
            View* viewB = &Scene::Views[b];
            return viewA->Priority < viewB->Priority;
        });
    }
}

PUBLIC STATIC void Scene::SetView(int viewIndex) {
    View* currentView = &Scene::Views[viewIndex];

    if (currentView->UseDrawTarget && currentView->DrawTarget) {
        float view_w = currentView->Width;
        float view_h = currentView->Height;
        Texture* tar = currentView->DrawTarget;

        size_t stride = currentView->Software ? (size_t)currentView->Stride : view_w;
        if (tar->Width != stride || tar->Height != view_h) {
            Graphics::GfxFunctions = &Graphics::Internal;
            Graphics::DisposeTexture(tar);
            Graphics::SetTextureInterpolation(false);
            currentView->DrawTarget = Graphics::CreateTexture(SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, stride, view_h);
        }

        Graphics::SetRenderTarget(currentView->DrawTarget);

        if (currentView->Software)
            Graphics::SoftwareStart();
        else
            Graphics::Clear();
    }

    Scene::ViewCurrent = viewIndex;
}

PUBLIC STATIC bool Scene::CheckPosOnScreen(float posX, float posY, float rangeX, float rangeY) {
    if (!posX || !posY || !rangeX || !rangeY)
        return false;

    for (int s = 0; s < MAX_SCENE_VIEWS; ++s) {
        if (Scene::Views[s].Active) {
            int sx = abs(posX - Scene::Views[s].X);
            int sy = abs(posY - Scene::Views[s].Y);

            if (sx <= rangeX /* + Scene::Views[s].OffsetX*/ && sy <= rangeY /* + Scene::Views[s].OffsetY*/)
                return true;
        }
    }
    
    return false;
}

#define PERF_START(n) if (viewPerf) viewPerf->n = Clock::GetTicks()
#define PERF_END(n) if (viewPerf) viewPerf->n = Clock::GetTicks() - viewPerf->n

PUBLIC STATIC void Scene::RenderView(int viewIndex, bool doPerf) {
    View* currentView = &Scene::Views[viewIndex];
    Perf_ViewRender* viewPerf = doPerf ? &Scene::PERF_ViewRender[viewIndex] : NULL;

    if (viewPerf)
        viewPerf->RecreatedDrawTarget = false;

    bool useDrawTarget = false;
    Texture* drawTarget = currentView->DrawTarget;

    PERF_START(RenderSetupTime);
    if (currentView->UseDrawTarget && drawTarget)
        useDrawTarget = true;
    else if (!currentView->Visible) {
        PERF_END(RenderSetupTime);
        return;
    }
    Scene::SetView(viewIndex);
    PERF_END(RenderSetupTime);

    if (viewPerf && drawTarget != currentView->DrawTarget)
        viewPerf->RecreatedDrawTarget = true;

    float cx = std::floor(currentView->X);
    float cy = std::floor(currentView->Y);
    float cz = std::floor(currentView->Z);

    int viewRenderFlag = 1 << viewIndex;

    // Adjust projection
    PERF_START(ProjectionSetupTime);
    if (currentView->UsePerspective) {
        Graphics::UpdatePerspective(currentView->FOV, currentView->Width / currentView->Height, currentView->NearPlane, currentView->FarPlane);
        Matrix4x4::Rotate(currentView->ProjectionMatrix, currentView->ProjectionMatrix, currentView->RotateX, 1.0, 0.0, 0.0);
        Matrix4x4::Rotate(currentView->ProjectionMatrix, currentView->ProjectionMatrix, currentView->RotateY, 0.0, 1.0, 0.0);
        Matrix4x4::Rotate(currentView->ProjectionMatrix, currentView->ProjectionMatrix, currentView->RotateZ, 0.0, 0.0, 1.0);
        Matrix4x4::Translate(currentView->ProjectionMatrix, currentView->ProjectionMatrix, -currentView->X, -currentView->Y, -currentView->Z);
        Matrix4x4::Copy(currentView->BaseProjectionMatrix, currentView->ProjectionMatrix);
    }
    else {
        Graphics::UpdateOrtho(currentView->Width, currentView->Height);
        if (!currentView->UseDrawTarget)
            Graphics::UpdateOrthoFlipped(currentView->Width, currentView->Height);

        Matrix4x4::Rotate(currentView->ProjectionMatrix, currentView->ProjectionMatrix, currentView->RotateX, 1.0, 0.0, 0.0);
        Matrix4x4::Rotate(currentView->ProjectionMatrix, currentView->ProjectionMatrix, currentView->RotateY, 0.0, 1.0, 0.0);
        Matrix4x4::Rotate(currentView->ProjectionMatrix, currentView->ProjectionMatrix, currentView->RotateZ, 0.0, 0.0, 1.0);
        Matrix4x4::Translate(currentView->ProjectionMatrix, currentView->BaseProjectionMatrix, -cx, -cy, -cz);
    }
    Graphics::UpdateProjectionMatrix();
    PERF_END(ProjectionSetupTime);

    // RenderEarly
    PERF_START(ObjectRenderEarlyTime);
    for (int l = 0; l < Scene::PriorityPerLayer; l++) {
        if (DEV_NoObjectRender)
            break;

        DrawGroupList* drawGroupList = &PriorityLists[l];

        if (drawGroupList->NeedsSorting)
            drawGroupList->Sort();

        Scene::CurrentDrawGroup = l;

        for (Entity* ent : *drawGroupList->Entities) {
            if (ent->Active)
                ent->RenderEarly();
        }
    }
    PERF_END(ObjectRenderEarlyTime);

    // Render Objects and Layer Tiles
    float _vx = currentView->X;
    float _vy = currentView->Y;
    float _vw = currentView->Width;
    float _vh = currentView->Height;
    double objectTimeTotal = 0.0;
    DrawGroupList* drawGroupList;
    for (int l = 0; l < Scene::PriorityPerLayer; l++) {
        if (DEV_NoObjectRender)
            goto DEV_NoTilesCheck;

        double elapsed;
        double objectTime;
        float hbW;
        float hbH;
        float _ox;
        float _oy;
        objectTime = Clock::GetTicks();

        Scene::CurrentDrawGroup = l;

        drawGroupList = &PriorityLists[l];
        for (Entity* ent : *drawGroupList->Entities) {
            if (ent->Active) {
                if (ent->RenderRegionW == 0.0f || ent->RenderRegionH == 0.0f)
                    goto DoCheckRender;

                hbW = ent->RenderRegionW * 0.5f;
                hbH = ent->RenderRegionH * 0.5f;
                _ox = ent->X - _vx;
                _oy = ent->Y - _vy;
                if ((_ox + hbW) < 0.0f || (_ox - hbW) >= _vw ||
                    (_oy + hbH) < 0.0f || (_oy - hbH) >= _vh)
                    continue;

                if (Scene::ShowObjectRegions) {
                    _ox = ent->X - ent->RenderRegionW * 0.5f;
                    _oy = ent->Y - ent->RenderRegionH * 0.5f;
                    Graphics::SetBlendColor(0.0f, 0.0f, 1.0f, 0.5f);
                    Graphics::FillRectangle(_ox, _oy, ent->RenderRegionW, ent->RenderRegionH);

                    _ox = ent->X - ent->OnScreenHitboxW * 0.5f;
                    _oy = ent->Y - ent->OnScreenHitboxH * 0.5f;
                    Graphics::SetBlendColor(1.0f, 0.0f, 0.0f, 0.5f);
                    Graphics::FillRectangle(_ox, _oy, ent->OnScreenHitboxW, ent->OnScreenHitboxH);
                }

                DoCheckRender:
                if ((ent->ViewRenderFlag & viewRenderFlag) == 0)
                    continue;
                if ((ent->ViewOverrideFlag & viewRenderFlag) == 0 && (Scene::ObjectViewRenderFlag & viewRenderFlag) == 0)
                    continue;

                elapsed = Clock::GetTicks();

                ent->Render(_vx, _vy);

                elapsed = Clock::GetTicks() - elapsed;

                if (ent->List) {
                    ObjectList* list = ent->List;
                    double count = list->AverageRenderItemCount;
                    if (count < 60.0 * 60.0) {
                        count += 1.0;
                        if (count == 1.0)
                            list->AverageRenderTime = elapsed;
                        else
                            list->AverageRenderTime =
                                list->AverageRenderTime + (elapsed - list->AverageRenderTime) / count;
                        list->AverageRenderItemCount = count;
                    }
                }
            }
        }
        objectTime = Clock::GetTicks() - objectTime;
        objectTimeTotal += objectTime;

        DEV_NoTilesCheck:
        if (DEV_NoTiles)
            continue;

        if (Scene::TileSprites.size() == 0)
            continue;

        if ((Scene::TileViewRenderFlag & viewRenderFlag) == 0)
            continue;

        bool texBlend = Graphics::TextureBlend;
        for (size_t li = 0; li < Layers.size(); li++) {
            SceneLayer* layer = &Layers[li];
            // Skip layer tile render if already rendered
            if (layer->DrawGroup != l)
                continue;

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
                else
                    Graphics::SetBlendColor(1.0, 1.0, 1.0, 1.0);

                Graphics::DrawSceneLayer(layer, currentView);
                Graphics::ClearClip();

                Graphics::Restore();

                PERF_END(LayerTileRenderTime[li]);
            }
        }
        Graphics::TextureBlend = texBlend;
    }
    if (viewPerf)
        viewPerf->ObjectRenderTime = objectTimeTotal;

    // RenderLate
    PERF_START(ObjectRenderLateTime);
    for (int l = 0; l < Scene::PriorityPerLayer; l++) {
        if (DEV_NoObjectRender)
            break;

        Scene::CurrentDrawGroup = l;

        DrawGroupList* drawGroupList = &PriorityLists[l];
        for (Entity* ent : *drawGroupList->Entities) {
            if (ent->Active)
                ent->RenderLate();
        }
    }
    Scene::CurrentDrawGroup = -1;
    PERF_END(ObjectRenderLateTime);

    PERF_START(RenderFinishTime);
    if (useDrawTarget && currentView->Software)
        Graphics::SoftwareEnd();
    PERF_END(RenderFinishTime);
}

PUBLIC STATIC void Scene::Render() {
    if (!Scene::PriorityLists)
        return;

    Graphics::ResetViewport();

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
                        if (win_w / currentView->Width < win_h / currentView->Height) {
                            out_w = win_w;
                            out_h = win_w * currentView->Height / currentView->Width;
                        }
                        else {
                            out_w = win_h * currentView->Width / currentView->Height;
                            out_h = win_h;
                        }
                        out_x = (win_w - out_w) * 0.5f / scale;
                        out_y = (win_h - out_h) * 0.5f / scale;
                        // needClip = true;
                        break;
                    // Fill to aspect ratio
                    case 2:
                        if (win_w / currentView->Width > win_h / currentView->Height) {
                            out_w = win_w;
                            out_h = win_w * currentView->Height / currentView->Width;
                        }
                        else {
                            out_w = win_h * currentView->Width / currentView->Height;
                            out_h = win_h;
                        }
                        out_x = (win_w - out_w) * 0.5f;
                        out_y = (win_h - out_h) * 0.5f;
                        break;
                    // Splitscreen
                    case 3:
                        out_x = (currentView->OutputX / (float)Application::WindowWidth) * win_w;
                        out_y = (currentView->OutputY / (float)Application::WindowHeight) * win_h;
                        out_w = (currentView->OutputWidth / (float)Application::WindowWidth) * win_w;
                        out_h = (currentView->OutputHeight / (float)Application::WindowHeight) * win_h;
                        break;
                }

                Graphics::TextureBlend = false;
                Graphics::SetBlendMode(BlendMode_NORMAL);
                Graphics::DrawTexture(currentView->DrawTarget,
                    0.0, 0.0, currentView->Width, currentView->Height,
                    out_x, out_y + Graphics::PixelOffset, out_w, out_h + Graphics::PixelOffset);
                Graphics::SetDepthTesting(Graphics::UseDepthTesting);
            }
        }
        renderFinishTime = Clock::GetTicks() - renderFinishTime;
        if (viewPerf)
            viewPerf->RenderFinishTime += renderFinishTime;

        PERF_END(RenderTime);
    }
}

PUBLIC STATIC void Scene::AfterScene() {
    BytecodeObjectManager::ResetStack();
    BytecodeObjectManager::RequestGarbageCollection();

    bool& doRestart = Scene::DoRestart;

    if (Scene::NextScene[0]) {
        if (Scene::NoPersistency)
            Scene::DeleteAllObjects();

        BytecodeObjectManager::ForceGarbageCollection();

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

PRIVATE STATIC void Scene::Iterate(Entity* first, std::function<void(Entity* e)> func) {
    for (Entity* ent = first, *next; ent; ent = next) {
        next = ent->NextEntity;
        func(ent);
    }
}
PRIVATE STATIC void Scene::ResetPriorityListIndex(Entity* first) {
    Scene::Iterate(first, [](Entity* ent) -> void {
        ent->PriorityListIndex = -1;
    });
}

PUBLIC STATIC void Scene::Restart() {
    Scene::ViewCurrent = 0;

    View* currentView = &Scene::Views[Scene::ViewCurrent];
    currentView->X = 0.0f;
    currentView->Y = 0.0f;
    currentView->Z = 0.0f;
    Scene::Frame = 0;
    Scene::Paused = false;

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
            // printf("layer: w %d h %d data %d == %d\n", Layers[l].WidthData, Layers[l].HeightData, Layers[l].DataSize, (Layers[l].WidthMask + 1) * (Layers[l].HeightMask + 1) * sizeof(Uint32));
            memcpy(Layers[l].Tiles, Layers[l].TilesBackup, Layers[l].DataSize);
        }
        Scene::AnyLayerTileChange = false;
    }

    Scene::ClearPriorityLists();

    // Remove all non-persistent objects from lists
    if (Scene::ObjectLists) {
        Scene::ObjectLists->ForAll([](Uint32, ObjectList* list) -> void {
            list->RemoveNonPersistentFromLinkedList(Scene::DynamicObjectFirst);
        });
    }

    // Remove all non-persistent objects from registries
    if (Scene::ObjectRegistries) {
        Scene::ObjectRegistries->ForAll([](Uint32, ObjectRegistry* registry) -> void {
            registry->RemoveNonPersistentFromLinkedList(Scene::DynamicObjectFirst);
        });
    }

    // Dispose of all dynamic objects
    Scene::RemoveNonPersistentObjects(&Scene::DynamicObjectFirst, &Scene::DynamicObjectLast, &Scene::DynamicObjectCount);

    // Run "Load" on all object classes
    // This is done (on purpose) before object lists are cleared.
    // See the comments in ObjectList_CallLoads
    if (Scene::ObjectLists)
        Scene::ObjectLists->ForAllOrdered(ObjectList_CallLoads);

    // Run "Initialize" on all objects
    Scene::Iterate(Scene::StaticObjectFirst, [](Entity* ent) -> void {
        // On a scene restart, static entities are generally subject to having
        // their constructors called again, along with having their positions set
        // to their initial positions. On top of that, their Create event is called.
        // Of course, none of this should be done if the entity is persistent.
        if (!ent->Persistent) {
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
            // ent->Created gets set when Create() is called.
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
            // ent->PostCreated gets set when PostCreate() is called.
            ent->PostCreate();
        }
    });

    BytecodeObjectManager::ResetStack();
    BytecodeObjectManager::RequestGarbageCollection();
}
PRIVATE STATIC void Scene::ClearPriorityLists() {
    if (!Scene::PriorityLists)
        return;

    int layerSize = Scene::PriorityPerLayer;
    for (int l = 0; l < layerSize; l++)
        Scene::PriorityLists[l].Clear();

    // Reset the priority list indexes of all persistent objects
    ResetPriorityListIndex(Scene::StaticObjectFirst);
    ResetPriorityListIndex(Scene::DynamicObjectFirst);
}
PRIVATE STATIC void Scene::DeleteObjects(Entity** first, Entity** last, int* count) {
    Scene::Iterate(*first, [](Entity* ent) -> void {
        // Garbage collection will take care of it later.
        Scene::RemoveObject(ent);
    });
    Scene::Clear(first, last, count);
}
PRIVATE STATIC void Scene::RemoveNonPersistentObjects(Entity** first, Entity** last, int* count) {
    for (Entity* ent = *first, *next; ent; ent = next) {
        next = ent->NextEntity;
        if (!ent->Persistent)
            Scene::Remove(first, last, count, ent);
    }
}
PRIVATE STATIC void Scene::DeleteAllObjects() {
    // Dispose and clear Static objects
    Scene::DeleteObjects(&Scene::StaticObjectFirst, &Scene::StaticObjectLast, &Scene::StaticObjectCount);

    // Dispose and clear Dynamic objects
    Scene::DeleteObjects(&Scene::DynamicObjectFirst, &Scene::DynamicObjectLast, &Scene::DynamicObjectCount);

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
PUBLIC STATIC void Scene::LoadScene(const char* filename) {
    // Remove non-persistent objects from lists
    if (Scene::ObjectLists) {
        Scene::ObjectLists->ForAll([](Uint32, ObjectList* list) -> void {
            list->RemoveNonPersistentFromLinkedList(Scene::StaticObjectFirst);
            list->RemoveNonPersistentFromLinkedList(Scene::DynamicObjectFirst);
        });
    }

    // Remove non-persistent objects from registries
    if (Scene::ObjectRegistries) {
        Scene::ObjectRegistries->ForAll([](Uint32, ObjectRegistry* list) -> void {
            list->RemoveNonPersistentFromLinkedList(Scene::StaticObjectFirst);
            list->RemoveNonPersistentFromLinkedList(Scene::DynamicObjectFirst);
        });
    }

    // Dispose of resources in SCOPE_SCENE
    Scene::DisposeInScope(SCOPE_SCENE);

    // TODO: Make a way for this to delete ONLY of SCOPE_SCENE, and not any sprites needed by SCOPE_GAME
    /*Graphics::SpriteSheetTextureMap->WithAll([](Uint32, Texture* tex) -> void {
        Graphics::DisposeTexture(tex);
    });
    Graphics::SpriteSheetTextureMap->Clear();*/

    // Clear and dispose of non-persistent objects
    // We force garbage collection right after, meaning that non-persistent objects may get deleted.
    Scene::RemoveNonPersistentObjects(&Scene::StaticObjectFirst, &Scene::StaticObjectLast, &Scene::StaticObjectCount);
    Scene::RemoveNonPersistentObjects(&Scene::DynamicObjectFirst, &Scene::DynamicObjectLast, &Scene::DynamicObjectCount);

    // Clear static object lists (but don't delete them)
    if (Scene::StaticObjectLists)
        Scene::StaticObjectLists->Clear();

    // Clear priority lists
    Scene::ClearPriorityLists();

    // Dispose of layers
    for (size_t i = 0; i < Scene::Layers.size(); i++) {
        Scene::Layers[i].Dispose();
    }
    Scene::Layers.clear();

    // Dispose of TileConfigs
    if (Scene::TileCfgA) {
        Memory::Free(Scene::TileCfgA);
    }
    Scene::TileCfgA = NULL;
    Scene::TileCfgB = NULL;

    // Dispose of properties
    if (Scene::Properties)
        delete Scene::Properties;
    Scene::Properties = NULL;

    // Force garbage collect
    BytecodeObjectManager::ResetStack();
    BytecodeObjectManager::ForceGarbageCollection();

    MemoryPools::RunGC(MemoryPools::MEMPOOL_HASHMAP);
    MemoryPools::RunGC(MemoryPools::MEMPOOL_STRING);
    MemoryPools::RunGC(MemoryPools::MEMPOOL_SUBOBJECT);

    char pathParent[4096];
    StringUtils::Copy(pathParent, filename, sizeof(pathParent));
    for (char* i = pathParent + strlen(pathParent); i >= pathParent; i--) {
        if (*i == '/') {
            *++i = 0;
            break;
        }
    }

    memmove(Scene::CurrentScene, filename, strlen(filename) + 1);

    for (size_t i = 0; i < Scene::TileSprites.size(); i++) {
        Scene::TileSprites[i]->Dispose();
        delete Scene::TileSprites[i];
    }
    Scene::TileSprites.clear();
    Scene::TileSpriteInfos.clear();

    Scene::TileSize = 16;
    Scene::EmptyTile = 0;

    Scene::InitObjectListsAndRegistries();
    Scene::InitPriorityLists();

    for (size_t i = 0; i < Scene::Layers.size(); i++)
        Scene::Layers[i].Dispose();
    Scene::Layers.clear();

    // Load Static class
    if (Application::GameStart)
        Scene::AddStaticClass();

    // Actually try to read the scene now
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
            if (StringUtils::StrCaseStr(filename, ".bin"))
                RSDKSceneReader::Read(filename, pathParent);
            else if (StringUtils::StrCaseStr(filename, ".hcsn"))
                HatchSceneReader::Read(filename, pathParent);
            else
                TiledMapReader::Read(filename, pathParent);
        }

        // Load scene info and tile collisions
        if (Scene::ListData.size()) {
            SceneListEntry scene = Scene::ListData[Scene::ListPos];

            strcpy(Scene::CurrentFolder, scene.folder);
            strcpy(Scene::CurrentID, scene.id);
            strcpy(Scene::CurrentSpriteFolder, scene.spriteFolder);

            char filePath[4096];
            if (!strcmp(scene.fileType, "bin")) {
                snprintf(filePath, sizeof(filePath), "Stages/%s/TileConfig.bin", scene.folder);
            }
            else {
                if (scene.folder[0] == '\0')
                    snprintf(filePath, sizeof(filePath), "Scenes/TileConfig.bin");
                else
                    snprintf(filePath, sizeof(filePath), "Scenes/%s/TileConfig.bin", scene.folder);
            }
            Scene::LoadTileCollisions(filePath);
        }
    }
    else
        Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", filename);

    // Call Static's GameStart here
    if (Application::GameStart) {
        Scene::CallGameStart();
        Application::GameStart = false;
    }
}

PUBLIC STATIC void Scene::ProcessSceneTimer() {
    if (Scene::TimeEnabled) {
        Scene::TimeCounter += 100;

        if (Scene::TimeCounter >= 6000) {
            Scene::TimeCounter -= 6025;

            Scene::Seconds++;
            if (Scene::Seconds >= 60) {
                Scene::Seconds = 0;

                Scene::Minutes++;
                if (Scene::Minutes >= 60)
                    Scene::Minutes = 0;
            }
        }

        Scene::Milliseconds = Scene::TimeCounter / 60; // Refresh rate
    }
}

PUBLIC STATIC ObjectList* Scene::NewObjectList(const char* objectName) {
    ObjectList* objectList = new (nothrow) ObjectList(objectName);
    if (objectList && BytecodeObjectManager::LoadClass(objectName))
        objectList->SpawnFunction = BytecodeObjectManager::SpawnFunction;
    return objectList;
}
PRIVATE STATIC void Scene::AddStaticClass() {
    StaticObjectList = Scene::NewObjectList("Static");
    if (!StaticObjectList->SpawnFunction)
        return;

    Entity* obj = StaticObjectList->Spawn();
    if (obj) {
        obj->X = 0.0f;
        obj->Y = 0.0f;
        obj->InitialX = obj->X;
        obj->InitialY = obj->Y;
        obj->List = StaticObjectList;
        obj->Persistent = true;

        BytecodeObjectManager::Globals->Put("global", OBJECT_VAL(((BytecodeObject*)obj)->Instance));
    }

    StaticObject = obj;
}
PRIVATE STATIC void Scene::CallGameStart() {
    if (StaticObject)
        StaticObject->GameStart();
}
PUBLIC STATIC ObjectList* Scene::GetObjectList(const char* objectName, bool callListLoadFunction) {
    Uint32 objectNameHash = Scene::ObjectLists->HashFunction(objectName, strlen(objectName));

    ObjectList* objectList;
    if (Scene::ObjectLists->Exists(objectNameHash))
        objectList = Scene::ObjectLists->Get(objectNameHash);
    else {
        objectList = Scene::NewObjectList(objectName);
        Scene::ObjectLists->Put(objectNameHash, objectList);

        if (callListLoadFunction)
            BytecodeObjectManager::CallFunction(objectList->LoadFunctionName);
    }

    return objectList;
}
PUBLIC STATIC ObjectList* Scene::GetObjectList(const char* objectName) {
    return GetObjectList(objectName, true);
}
PUBLIC STATIC ObjectList* Scene::GetStaticObjectList(const char* objectName) {
    ObjectList* objectList;

    if (Scene::StaticObjectLists->Exists(objectName)) {
        objectList = Scene::StaticObjectLists->Get(objectName);
    }
    else if (Scene::ObjectLists->Exists(objectName)) {
        // There isn't a static object list with this name, but we can check
        // if there's a regular one. If so, we just use it, and then put it
        // in the static object list hash map. This is all so that object
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
PRIVATE STATIC void Scene::SpawnStaticObject(const char* objectName) {
    ObjectList* objectList = Scene::GetObjectList(objectName, false);
    if (objectList->SpawnFunction) {
        Entity* obj = objectList->Spawn();
        obj->X = 0.0f;
        obj->Y = 0.0f;
        obj->InitialX = obj->X;
        obj->InitialY = obj->Y;
        obj->List = objectList;
        Scene::AddStatic(objectList, obj);
    }
}
PUBLIC STATIC void Scene::AddManagers() {
    Scene::SpawnStaticObject("WindowManager");
    Scene::SpawnStaticObject("InputManager");
    Scene::SpawnStaticObject("FadeManager");
}
PUBLIC STATIC void Scene::InitPriorityLists() {
    if (Scene::PriorityLists) {
        for (int i = Scene::PriorityPerLayer - 1; i >= 0; i--)
            Scene::PriorityLists[i].Dispose();
    }
    else {
        Scene::PriorityLists = (DrawGroupList*)Memory::TrackedCalloc("Scene::PriorityLists", Scene::PriorityPerLayer, sizeof(DrawGroupList));

        if (!Scene::PriorityLists) {
            Log::Print(Log::LOG_ERROR, "Out of memory for priority lists!");
            exit(-1);
        }
    }

    for (int i = Scene::PriorityPerLayer - 1; i >= 0; i--)
        Scene::PriorityLists[i].Init();
}
PRIVATE STATIC void Scene::LoadRSDKTileConfig(Stream* tileColReader) {
    Uint32 tileCount = 0x400;

    Uint8* tileInfo = (Uint8*)Memory::Calloc(1, tileCount * 2 * 0x26);
    tileColReader->ReadCompressed(tileInfo);

    Scene::TileSize = 16;
    Scene::TileCount = tileCount;
    if (Scene::TileCfgA == NULL) {
        int totalTileVariantCount = Scene::TileCount;
        // multiplied by 4: For all combinations of tile flipping
        totalTileVariantCount <<= 2;
        // multiplied by 2: For both collision planes
        totalTileVariantCount <<= 1;

        Scene::TileCfgA = (TileConfig*)Memory::TrackedCalloc("Scene::TileCfg", totalTileVariantCount, sizeof(TileConfig));
        Scene::TileCfgB = Scene::TileCfgA + (Scene::TileCount << 2);
    }

    Uint8* line;
    bool isCeiling;
    Uint8 collisionBuffer[16];
    Uint8 hasCollisionBuffer[16];
    TileConfig *tile, *tileDest, *tileLast, *tileBase = &Scene::TileCfgA[0];

    tile = tileBase;
    READ_TILES:
    for (Uint32 i = 0; i < tileCount; i++) {
        line = &tileInfo[i * 0x26];

        tile->IsCeiling = isCeiling = line[0x20];

        // Copy collision
        memcpy(hasCollisionBuffer, &line[0x10], 16);
        memcpy(collisionBuffer, &line[0x00], 16);

        Uint8* col;
        // Interpret up/down collision
        if (isCeiling) {
            col = &collisionBuffer[0];
            for (int c = 0; c < 16; c++) {
                if (hasCollisionBuffer[c]) {
                    tile->CollisionTop[c] = 0;
                    tile->CollisionBottom[c] = *col;
                }
                else {
                    tile->CollisionTop[c] =
                    tile->CollisionBottom[c] = 0xFF;
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
                        goto COLLISION_LINE_LEFT_BOTTOMUP_FOUND;
                    }
                }
                tile->CollisionLeft[y] = 0xFF;

                COLLISION_LINE_LEFT_BOTTOMUP_FOUND:

                // Right-to-left check
                for (int x = 15; x >= 0; x--) {
                    Uint8 data = tile->CollisionBottom[x];
                    if (data != 0xFF && data >= y) {
                        tile->CollisionRight[y] = x;
                        goto COLLISION_LINE_RIGHT_BOTTOMUP_FOUND;
                    }
                }
                tile->CollisionRight[y] = 0xFF;

                COLLISION_LINE_RIGHT_BOTTOMUP_FOUND:
                ;
            }
        }
        else {
            col = &collisionBuffer[0];
            for (int c = 0; c < 16; c++) {
                if (hasCollisionBuffer[c]) {
                    tile->CollisionTop[c] = *col;
                    tile->CollisionBottom[c] = 15;
                }
                else {
                    tile->CollisionTop[c] =
                    tile->CollisionBottom[c] = 0xFF;
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
                        goto COLLISION_LINE_LEFT_TOPDOWN_FOUND;
                    }
                }
                tile->CollisionLeft[y] = 0xFF;

                COLLISION_LINE_LEFT_TOPDOWN_FOUND:

                // Right-to-left check
                for (int x = 15; x >= 0; x--) {
                    Uint8 data = tile->CollisionTop[x];
                    if (data != 0xFF && data <= y) {
                        tile->CollisionRight[y] = x;
                        goto COLLISION_LINE_RIGHT_TOPDOWN_FOUND;
                    }
                }
                tile->CollisionRight[y] = 0xFF;

                COLLISION_LINE_RIGHT_TOPDOWN_FOUND:
                ;
            }
        }

        tile->Behavior = line[0x25];
        memcpy(&tile->AngleTop, &line[0x21], 4);

        // Flip X
        tileDest = tile + tileCount;
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
        tileDest = tile + (tileCount << 1);
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
        tileDest = tile + (tileCount << 1) + tileCount;
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

        tile++;
    }

    if (tileBase == Scene::TileCfgA) {
        tileBase = Scene::TileCfgB;
        tileInfo += tileCount * 0x26;
        tile = tileBase;
        goto READ_TILES;
    }

    // Restore pointer
    tileInfo -= tileCount * 0x26;

    Memory::Free(tileInfo);
}
PRIVATE STATIC void Scene::LoadHCOLTileConfig(Stream* tileColReader) {
    Uint32 tileCount = tileColReader->ReadUInt32();
    Uint8  tileSize  = tileColReader->ReadByte();
    tileColReader->ReadByte();
    tileColReader->ReadByte();
    tileColReader->ReadByte();
    tileColReader->ReadUInt32();

    tileSize = 16;

    Scene::TileCount = tileCount;

    if (Scene::TileSprites.size()) {
        int numTiles = (int)Scene::TileSprites[0]->Animations[0].Frames.size();
        if (Scene::TileCount < numTiles) {
            Log::Print(Log::LOG_WARN, "Less Tile Collisions (%d) than actual Tiles! (%d)", Scene::TileCount, (int)Scene::TileSprites[0]->Animations[0].Frames.size());
            Scene::TileCount = numTiles;
        }
    }

    if (Scene::TileCfgA == NULL) {
        int totalTileVariantCount = Scene::TileCount;
        // multiplied by 4: For all combinations of tile flipping
        totalTileVariantCount <<= 2;
        // multiplied by 2: For both collision planes
        totalTileVariantCount <<= 1;

        Scene::TileSize = tileSize;
        Scene::TileCfgA = (TileConfig*)Memory::TrackedCalloc("Scene::TileCfgA", totalTileVariantCount, sizeof(TileConfig));
        Scene::TileCfgB = Scene::TileCfgA + (tileCount << 2);
    }
    else if (Scene::TileSize != tileSize) {
        Scene::TileSize = tileSize;
    }

    Uint8 collisionBuffer[16];
    TileConfig *tile, *tileDest, *tileLast, *tileBase = &Scene::TileCfgA[0];

    tile = tileBase;
    for (Uint32 i = 0; i < tileCount; i++) {
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
                    tile->CollisionTop[c] =
                    tile->CollisionBottom[c] = 0xFF;
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

                HCOL_COLLISION_LINE_RIGHT_BOTTOMUP_FOUND:
                ;
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
                    tile->CollisionTop[c] =
                    tile->CollisionBottom[c] = 0xFF;
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

                HCOL_COLLISION_LINE_RIGHT_TOPDOWN_FOUND:
                ;
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
        tileDest = tile + tileCount;
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
        tileDest = tile + (tileCount << 1);
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
        tileDest = tile + (tileCount << 1) + tileCount;
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

        tile++;
    }

    // Copy over to B
    memcpy(Scene::TileCfgB, Scene::TileCfgA, (tileCount << 2) * sizeof(TileConfig));
}
PUBLIC STATIC void Scene::LoadTileCollisions(const char* filename) {
    Stream* tileColReader;
    if (!ResourceManager::ResourceExists(filename)) {
        Log::Print(Log::LOG_WARN, "Could not find tile collision file \"%s\"!", filename);
        return;
    }

    tileColReader = ResourceStream::New(filename);
    if (!tileColReader)
        return;

    Uint32 magic = tileColReader->ReadUInt32();
    // RSDK TileConfig
    if (magic == 0x004C4954U) {
        Scene::LoadRSDKTileConfig(tileColReader);
    }
    // HCOL TileConfig
    else if (magic == 0x4C4F4354U) {
        Scene::LoadHCOLTileConfig(tileColReader);
    }
    else {
        Log::Print(Log::LOG_ERROR, "Invalid magic for TileCollisions! %X", magic);
    }
    tileColReader->Close();
}

PUBLIC STATIC void Scene::DisposeInScope(Uint32 scope) {
    // Images
    for (size_t i = 0, i_sz = Scene::ImageList.size(); i < i_sz; i++) {
        if (!Scene::ImageList[i]) continue;
        if (Scene::ImageList[i]->UnloadPolicy > scope) continue;

        Scene::ImageList[i]->AsImage->Dispose();
        delete Scene::ImageList[i]->AsImage;
        delete Scene::ImageList[i];
        Scene::ImageList[i] = NULL;
    }
    // Sprites
    for (size_t i = 0, i_sz = Scene::SpriteList.size(); i < i_sz; i++) {
        if (!Scene::SpriteList[i]) continue;
        if (Scene::SpriteList[i]->UnloadPolicy > scope) continue;

        Scene::SpriteList[i]->AsSprite->Dispose();
        delete Scene::SpriteList[i]->AsSprite;
        delete Scene::SpriteList[i];
        Scene::SpriteList[i] = NULL;
    }
    // Models
    for (size_t i = 0, i_sz = Scene::ModelList.size(); i < i_sz; i++) {
        if (!Scene::ModelList[i]) continue;
        if (Scene::ModelList[i]->UnloadPolicy > scope) continue;

        Scene::ModelList[i]->AsModel->Dispose();
        delete Scene::ModelList[i]->AsModel;
        delete Scene::ModelList[i];
        Scene::ModelList[i] = NULL;
    }
    // Sounds
    AudioManager::ClearMusic();
    AudioManager::ClearSounds();

    AudioManager::Lock();
    for (size_t i = 0, i_sz = Scene::SoundList.size(); i < i_sz; i++) {
        if (!Scene::SoundList[i]) continue;
        if (Scene::SoundList[i]->UnloadPolicy > scope) continue;

        Scene::SoundList[i]->AsSound->Dispose();
        delete Scene::SoundList[i]->AsSound;
        delete Scene::SoundList[i];
        Scene::SoundList[i] = NULL;
    }
    // Music
    for (size_t i = 0, i_sz = Scene::MusicList.size(); i < i_sz; i++) {
        if (!Scene::MusicList[i]) continue;
        if (Scene::MusicList[i]->UnloadPolicy > scope) continue;

        // AudioManager::RemoveMusic(Scene::MusicList[i]->AsMusic);

        Scene::MusicList[i]->AsMusic->Dispose();
        delete Scene::MusicList[i]->AsMusic;
        delete Scene::MusicList[i];
        Scene::MusicList[i] = NULL;
    }
    // Media
    for (size_t i = 0, i_sz = Scene::MediaList.size(); i < i_sz; i++) {
        if (!Scene::MediaList[i]) continue;
        if (Scene::MediaList[i]->UnloadPolicy > scope) continue;

        #ifdef USING_FFMPEG
        Scene::MediaList[i]->AsMedia->Player->Close();
        Scene::MediaList[i]->AsMedia->Source->Close();
        #endif
        delete Scene::MediaList[i]->AsMedia;

        delete Scene::MediaList[i];
        Scene::MediaList[i] = NULL;
    }
    AudioManager::Unlock();
    // Animators
    for (size_t i = 0, i_sz = Scene::AnimatorList.size(); i < i_sz; i++) {
        if (!Scene::AnimatorList[i]) continue;
        if (Scene::AnimatorList[i]->UnloadPolicy > scope) continue;

        delete Scene::AnimatorList[i];
        Scene::AnimatorList[i] = NULL;
    }
}
PUBLIC STATIC void Scene::Dispose() {
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
    }

    Scene::DisposeInScope(SCOPE_GAME);
    // Dispose of all resources
    Scene::ImageList.clear();
    Scene::SpriteList.clear();
    Scene::SoundList.clear();
    Scene::MusicList.clear();
    Scene::ModelList.clear();
    Scene::MediaList.clear();
    Scene::AnimatorList.clear();

    // Dispose of StaticObject
    if (StaticObject) {
        StaticObject->Active = false;
        StaticObject->Removed = true;
        StaticObject = NULL;
    }

    // Dispose and clear Static objects
    Scene::DeleteObjects(&Scene::StaticObjectFirst, &Scene::StaticObjectLast, &Scene::StaticObjectCount);

    // Dispose and clear Dynamic objects
    Scene::DeleteObjects(&Scene::DynamicObjectFirst, &Scene::DynamicObjectLast, &Scene::DynamicObjectCount);

    // Free Priority Lists
    if (Scene::PriorityLists) {
        for (int i = Scene::PriorityPerLayer - 1; i >= 0; i--) {
            Scene::PriorityLists[i].Dispose();
        }
        Memory::Free(Scene::PriorityLists);
    }
    Scene::PriorityLists = NULL;

    for (size_t i = 0; i < Scene::Layers.size(); i++) {
        Scene::Layers[i].Dispose();
    }
    Scene::Layers.clear();

    for (size_t i = 0; i < Scene::TileSprites.size(); i++) {
        Scene::TileSprites[i]->Dispose();
        delete Scene::TileSprites[i];
    }
    Scene::TileSprites.clear();
    Scene::TileSpriteInfos.clear();

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

    if (Scene::TileCfgA)
        Memory::Free(Scene::TileCfgA);
    Scene::TileCfgA = NULL;
    Scene::TileCfgB = NULL;

    if (Scene::Properties)
        delete Scene::Properties;
    Scene::Properties = NULL;

    BytecodeObjectManager::Dispose();
    SourceFileMap::Dispose();
}

// Tile Batching
PUBLIC STATIC void Scene::SetTile(int layer, int x, int y, int tileID, int flip_x, int flip_y, int collA, int collB) {
    Uint32* tile = &Scene::Layers[layer].Tiles[x + (y << Scene::Layers[layer].WidthInBits)];

    *tile = tileID & TILE_IDENT_MASK;
    if (flip_x)
        *tile |= TILE_FLIPX_MASK;
    if (flip_y)
        *tile |= TILE_FLIPY_MASK;
    *tile |= collA << 28;
    *tile |= collB << 26;
}

// Tile Collision
PUBLIC STATIC int  Scene::CollisionAt(int x, int y, int collisionField, int collideSide, int* angle) {
    int temp;
    int checkX;
    int probeXOG = x;
    int probeYOG = y;
    int tileX, tileY, tileID, tileAngle;
    int collisionA, collisionB, collision;

    bool check;
    TileConfig* tileCfg;

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

    if (collisionField < 0)
        return -1;
    if (collisionField > 1)
        return -1;
    if (!Scene::TileCfgA && collisionField == 0)
        return -1;
    if (!Scene::TileCfgB && collisionField == 1)
        return -1;

    for (size_t l = 0, lSz = Layers.size(); l < lSz; l++) {
        SceneLayer layer = Layers[l];
        if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE))
            continue;

        x = probeXOG;
        y = probeYOG;

        x -= layer.OffsetX;
        y -= layer.OffsetY;

        // Check Layer Width
        temp = layer.Width << 4;
        if (x < 0 || x >= temp)
            continue;
        x &= layer.WidthMask << 4 | 0xF; // x = ((x % temp) + temp) % temp;

        // Check Layer Height
        temp = layer.Height << 4;
        if ((y < 0 || y >= temp))
            continue;
        y &= layer.HeightMask << 4 | 0xF; // y = ((y % temp) + temp) % temp;

        tileX = x >> 4;
        tileY = y >> 4;

        tileID = layer.Tiles[tileX + (tileY << layer.WidthInBits)];
        if ((tileID & TILE_IDENT_MASK) != EmptyTile) {
            int tileFlipOffset = (
                ((!!(tileID & TILE_FLIPY_MASK)) << 1) | (!!(tileID & TILE_FLIPX_MASK))
                ) * Scene::TileCount;

            collisionA = (tileID & TILE_COLLA_MASK) >> 28;
            collisionB = (tileID & TILE_COLLB_MASK) >> 26;
            // collisionC = (tileID & TILE_COLLC_MASK) >> 24;
            collision  = collisionField ? collisionB : collisionA;
            tileID = tileID & TILE_IDENT_MASK;

            // Check tile config
            tileCfg = collisionField ? &Scene::TileCfgB[tileID + tileFlipOffset] : &Scene::TileCfgA[tileID + tileFlipOffset];

            Uint8* colT = tileCfg->CollisionTop;
            Uint8* colB = tileCfg->CollisionBottom;

            checkX = x & 0xF;
            if (colT[checkX] >= 0xF0 || colB[checkX] >= 0xF0)
                continue;

            // Check if we can collide with the tile side
            check = ((collision & 1) && (collideSide & CollideSide::TOP)) ||
                (wallAsFloorFlag && ((collision & 1) && (collideSide & (CollideSide::LEFT | CollideSide::RIGHT)))) ||
                ((collision & 2) && (collideSide & CollideSide::BOTTOM_SIDES));
            if (!check)
                continue;

            // Check Y
            tileY = tileY << 4;
            check = (y >= tileY + colT[checkX] && y <= tileY + colB[checkX]);
            if (!check)
                continue;

            // Return angle
            tileAngle = (&tileCfg->AngleTop)[configIndex];
            return tileAngle & 0xFF;
        }
    }

    return -1;
}

PUBLIC STATIC int  Scene::CollisionInLine(int x, int y, int angleMode, int checkLen, int collisionField, bool compareAngle, Sensor* sensor) {
    if (checkLen < 0)
        return -1;

    int probeXOG = x;
    int probeYOG = y;
    int probeDeltaX = 0;
    int probeDeltaY = 1;
    int tileX, tileY, tileID;
    int tileFlipOffset, collisionA, collisionB, collision, collisionMask;
    TileConfig* tileCfg;
    TileConfig* tileCfgBase = collisionField ? Scene::TileCfgB : Scene::TileCfgA;

    int maxTileCheck = ((checkLen + 15) >> 4) + 1;
    int minLength = 0x7FFFFFFF, sensedLength;

    if ((Uint32)collisionField > 1)
        return -1;
    if (!Scene::TileCfgA && collisionField == 0)
        return -1;
    if (!Scene::TileCfgB && collisionField == 1)
        return -1;

    collisionMask = 3;
    switch (angleMode) {
        case 0:
            probeDeltaX =  0;
            probeDeltaY =  1;
            collisionMask = 1;
            break;
        case 1:
            probeDeltaX =  1;
            probeDeltaY =  0;
            collisionMask = 2;
            break;
        case 2:
            probeDeltaX =  0;
            probeDeltaY = -1;
            collisionMask = 2;
            break;
        case 3:
            probeDeltaX = -1;
            probeDeltaY =  0;
            collisionMask = 2;
            break;
    }

    switch (collisionField) {
        case 0: collisionMask <<= 28; break;
        case 1: collisionMask <<= 26; break;
        case 2: collisionMask <<= 24; break;
    }

    // probeDeltaX *= 16;
    // probeDeltaY *= 16;

    sensor->Collided = false;
    for (size_t l = 0, lSz = Layers.size(); l < lSz; l++) {
        SceneLayer layer = Layers[l];
        if (!(layer.Flags & SceneLayer::FLAGS_COLLIDEABLE))
            continue;

        x = probeXOG;
        y = probeYOG;
        x += layer.OffsetX;
        y += layer.OffsetY;

        // x = ((x % temp) + temp) % temp;
        // y = ((y % temp) + temp) % temp;

        tileX = x >> 4;
        tileY = y >> 4;
        for (int sl = 0; sl < maxTileCheck; sl++) {
            if (tileX < 0 || tileX >= layer.Width)
                goto NEXT_TILE;
            if (tileY < 0 || tileY >= layer.Height)
                goto NEXT_TILE;

            tileID = layer.Tiles[tileX + (tileY << layer.WidthInBits)];
            if ((tileID & TILE_IDENT_MASK) != EmptyTile) {
                tileFlipOffset = (
                    ( (!!(tileID & TILE_FLIPY_MASK)) << 1 ) | (!!(tileID & TILE_FLIPX_MASK))
                ) * Scene::TileCount;

                collisionA = ((tileID & TILE_COLLA_MASK & collisionMask) >> 28);
                collisionB = ((tileID & TILE_COLLB_MASK & collisionMask) >> 26);
                tileID = tileID & TILE_IDENT_MASK;

                tileCfg = &tileCfgBase[tileID] + tileFlipOffset;
                if (!(collisionB | collisionA))
                    goto NEXT_TILE;

                switch (angleMode) {
                    case 0:
                        collision = tileCfg->CollisionTop[x & 15];
                        if (collision >= 0xF0)
                            break;

                        collision += tileY << 4;
                        sensedLength = collision - y;
                        if ((Uint32)sensedLength <= (Uint32)checkLen) {
                            if (!compareAngle || abs((int)tileCfg->AngleTop - sensor->Angle) <= 0x20) {
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
                        if (collision >= 0xF0)
                            break;

                        collision += tileX << 4;
                        sensedLength = collision - x;
                        if ((Uint32)sensedLength <= (Uint32)checkLen) {
                            if (!compareAngle || abs((int)tileCfg->AngleLeft - sensor->Angle) <= 0x20) {
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
                        if (collision >= 0xF0)
                            break;

                        collision += tileY << 4;
                        sensedLength = y - collision;
                        if ((Uint32)sensedLength <= (Uint32)checkLen) {
                            if (!compareAngle || abs((int)tileCfg->AngleBottom - sensor->Angle) <= 0x20) {
                                if (minLength > sensedLength) {
                                    minLength = sensedLength;
                                    sensor->Angle = tileCfg->AngleBottom;
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
                        if (collision >= 0xF0)
                            break;

                        collision += tileX << 4;
                        sensedLength = x - collision;
                        if ((Uint32)sensedLength <= (Uint32)checkLen) {
                            if (!compareAngle || abs((int)tileCfg->AngleRight - sensor->Angle) <= 0x20) {
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

    if (sensor->Collided)
        return sensor->Angle;

    return -1;
}

// TODO: Check pointers
PUBLIC STATIC CollisionBox Scene::ArrayToHitbox(float hitbox[4]) {
    CollisionBox box;
    box.Top     = (int)hitbox[0];
    box.Left    = (int)hitbox[1];
    box.Right   = (int)hitbox[2];
    box.Bottom  = (int)hitbox[3];
    return box;
}

PUBLIC STATIC void Scene::SetupCollisionConfig(float minDistance, float lowTolerance, float highTolerance, int floorAngleTolerance, int wallAngleTolerance, int roofAngleTolerance) {
    Scene::CollisionMinimumDistance = minDistance;
    Scene::LowCollisionTolerance    = lowTolerance;
    Scene::HighCollisionTolerance   = highTolerance;
    Scene::FloorAngleTolerance      = floorAngleTolerance;
    Scene::WallAngleTolerance       = wallAngleTolerance;
    Scene::RoofAngleTolerance       = roofAngleTolerance;
}

PUBLIC STATIC int Scene::AddDebugHitbox(int type, int dir, Entity* entity, CollisionBox* hitbox) {
    int i = 0;
    for (; i < Scene::DebugHitboxCount; ++i) {
        if (Scene::DebugHitboxList[i].hitbox.Left == hitbox->Left && Scene::DebugHitboxList[i].hitbox.Top == hitbox->Top
            && Scene::DebugHitboxList[i].hitbox.Right == hitbox->Right && Scene::DebugHitboxList[i].hitbox.Bottom == hitbox->Bottom
            && Scene::DebugHitboxList[i].x == (int)entity->X && Scene::DebugHitboxList[i].y == (int)entity->Y
            && Scene::DebugHitboxList[i].entity == entity) {
            return i;
        }
    }

    if (i < DEBUG_HITBOX_COUNT) {
        Scene::DebugHitboxList[i].type          = type;
        Scene::DebugHitboxList[i].entity        = entity;
        Scene::DebugHitboxList[i].collision     = 0;
        Scene::DebugHitboxList[i].hitbox.Left   = hitbox->Left;
        Scene::DebugHitboxList[i].hitbox.Top    = hitbox->Top;
        Scene::DebugHitboxList[i].hitbox.Right  = hitbox->Right;
        Scene::DebugHitboxList[i].hitbox.Bottom = hitbox->Bottom;
        Scene::DebugHitboxList[i].x             = (int)entity->X;
        Scene::DebugHitboxList[i].y             = (int)entity->Y;

        if ((dir & Flip_X) == Flip_X) {
            int store                               = -Scene::DebugHitboxList[i].hitbox.Left;
            Scene::DebugHitboxList[i].hitbox.Left   = -Scene::DebugHitboxList[i].hitbox.Right;
            Scene::DebugHitboxList[i].hitbox.Right  = store;
        }
        if ((dir & Flip_Y) == Flip_Y) {
            int store                               = -Scene::DebugHitboxList[i].hitbox.Top;
            Scene::DebugHitboxList[i].hitbox.Top    = -Scene::DebugHitboxList[i].hitbox.Bottom;
            Scene::DebugHitboxList[i].hitbox.Bottom = store;
        }

        int id = Scene::DebugHitboxCount;
        Scene::DebugHitboxCount++;
        return id;
    }

    return -1;
}

// TODO: Check boxes are arrays and whether they need to be pointers
PUBLIC STATIC bool Scene::CheckObjectCollisionTouch(Entity* thisEntity, CollisionBox* thisHitbox, Entity* otherEntity, CollisionBox* otherHitbox) {
    int store = 0;
    if (!thisEntity || !otherEntity || !thisHitbox || !otherHitbox)
        return false;

    if ((thisEntity->Direction & Flip_X) == Flip_X) {
        store           = -thisHitbox->Left;
        thisHitbox->Left    = -thisHitbox->Right;
        thisHitbox->Right   = store;

        store               = -otherHitbox->Left;
        otherHitbox->Left   = -otherHitbox->Right;
        otherHitbox->Right  = store;
    }
    if ((thisEntity->Direction & Flip_Y) == Flip_Y) {
        store           = -thisHitbox->Top;
        thisHitbox->Top     = -thisHitbox->Bottom;
        thisHitbox->Bottom  = store;
        
        store               = -otherHitbox->Top;
        otherHitbox->Top    = -otherHitbox->Bottom;
        otherHitbox->Bottom = store;
    }

    bool collided = thisEntity->X + thisHitbox->Left < otherEntity->X + otherHitbox->Right && thisEntity->X + thisHitbox->Right > otherEntity->X + otherHitbox->Left
        && thisEntity->Y + thisHitbox->Top < otherEntity->Y + otherHitbox->Bottom && thisEntity->Y + thisHitbox->Bottom > otherEntity->Y + otherHitbox->Top;

    if ((thisEntity->Direction & Flip_X) == Flip_X) {
        store = -thisHitbox->Left;
        thisHitbox->Left = -thisHitbox->Right;
        thisHitbox->Right = store;

        store = -otherHitbox->Left;
        otherHitbox->Left = -otherHitbox->Right;
        otherHitbox->Right = store;
    }
    if ((thisEntity->Direction & Flip_Y) == Flip_Y) {
        store = -thisHitbox->Top;
        thisHitbox->Top = -thisHitbox->Bottom;
        thisHitbox->Bottom = store;

        store = -otherHitbox->Top;
        otherHitbox->Top = -otherHitbox->Bottom;
        otherHitbox->Bottom = store;
    }

    if (Scene::ShowHitboxes) {
        int thisHitboxID    = Scene::AddDebugHitbox(H_TYPE_TOUCH, thisEntity->Direction, thisEntity, thisHitbox);
        int otherHitboxID   = Scene::AddDebugHitbox(H_TYPE_TOUCH, thisEntity->Direction, otherEntity, otherHitbox);

        if (thisHitboxID >= 0 && collided)
            Scene::DebugHitboxList[thisHitboxID].collision |= 1 << (collided - 1);
        if (otherHitboxID >= 0 && collided)
            Scene::DebugHitboxList[otherHitboxID].collision |= 1 << (collided - 1);
    }

    return collided;
}

PUBLIC STATIC bool Scene::CheckObjectCollisionCircle(Entity* thisEntity, float thisRadius, Entity* otherEntity, float otherRadius) {
    float x = thisEntity->X - otherEntity->X;
    float y = thisEntity->Y - otherEntity->Y;
    float r = thisRadius + otherRadius;

    if (Scene::ShowHitboxes) {
        bool collided = x * x + y * y < r * r;
        CollisionBox thisHitbox;
        CollisionBox otherHitbox;
        thisHitbox.Left     = thisRadius;
        otherHitbox.Left    = otherRadius;

        int thisHitboxID    = Scene::AddDebugHitbox(H_TYPE_CIRCLE, Flip_NONE, thisEntity, &thisHitbox);
        int otherHitboxID   = Scene::AddDebugHitbox(H_TYPE_CIRCLE, Flip_NONE, otherEntity, &otherHitbox);

        if (thisHitboxID >= 0 && collided)
            Scene::DebugHitboxList[thisHitboxID].collision |= 1 << (collided - 1);
        if (otherHitboxID >= 0 && collided)
            Scene::DebugHitboxList[otherHitboxID].collision |= 1 << (collided - 1);
    }

    return x * x + y * y < r * r;
}

PUBLIC STATIC bool Scene::CheckObjectCollisionBox(Entity* thisEntity, CollisionBox* thisHitbox, Entity* otherEntity, CollisionBox* otherHitbox, bool setValues) {
    if (!thisEntity || !otherEntity || !thisHitbox || !otherHitbox)
        return C_NONE;

    int collisionSideH = C_NONE;
    int collisionSideV = C_NONE;

    int collideX = otherEntity->X;
    int collideY = otherEntity->Y;

    if ((thisEntity->Direction & Flip_X) == Flip_X) {
        int store = -thisHitbox->Left;
        thisHitbox->Left = -thisHitbox->Right;
        thisHitbox->Right = store;

        store = -otherHitbox->Left;
        otherHitbox->Left = -otherHitbox->Right;
        otherHitbox->Right = store;
    }
    if ((thisEntity->Direction & Flip_Y) == Flip_Y) {
        int store = -thisHitbox->Top;
        thisHitbox->Top = -thisHitbox->Bottom;
        thisHitbox->Bottom = store;

        store = -otherHitbox->Top;
        otherHitbox->Top = -otherHitbox->Bottom;
        otherHitbox->Bottom = store;
    }

    otherHitbox->Top++;
    otherHitbox->Bottom++;

    if (otherEntity->X <= (thisHitbox->Right + thisHitbox->Left + 2.0 * thisEntity->X) / 2.0) {
        if (otherEntity->X + otherHitbox->Right >= thisEntity->X + thisHitbox->Left && thisEntity->Y + thisHitbox->Top < otherEntity->Y + otherHitbox->Bottom
            && thisEntity->Y + thisHitbox->Bottom > otherEntity->Y + otherHitbox->Top) {
            collisionSideH = C_LEFT;
            collideX = thisEntity->X + (thisHitbox->Left - otherHitbox->Right);
        }
    }
    else {
        if (otherEntity->X + otherHitbox->Left < thisEntity->X + thisHitbox->Right && thisEntity->Y + thisHitbox->Top < otherEntity->Y + otherHitbox->Bottom
            && thisEntity->Y + thisHitbox->Bottom > otherEntity->Y + otherHitbox->Top) {
            collisionSideH = C_RIGHT;
            collideX = thisEntity->X + (thisHitbox->Right - otherHitbox->Left);
        }
    }

    otherHitbox->Left++;
    otherHitbox->Top--;
    otherHitbox->Right--;
    otherHitbox->Bottom++;

    if (otherEntity->Y <= thisEntity->Y + ((thisHitbox->Top + thisHitbox->Bottom) / 2.0)) {
        if (otherEntity->Y + otherHitbox->Bottom >= thisEntity->Y + thisHitbox->Top && thisEntity->X + thisHitbox->Left < otherEntity->X + otherHitbox->Right
            && thisEntity->X + thisHitbox->Right > otherEntity->X + otherHitbox->Left) {
            collisionSideV = C_TOP;
            collideY = thisEntity->Y + (thisHitbox->Top - otherHitbox->Bottom);
        }
    }
    else {
        if (otherEntity->Y + otherHitbox->Top < thisEntity->Y + thisHitbox->Bottom && thisEntity->X + thisHitbox->Left < otherEntity->X + otherHitbox->Right) {
            if (otherEntity->X + otherHitbox->Left < thisEntity->X + thisHitbox->Right) {
                collisionSideV = C_BOTTOM;
                collideY = thisEntity->Y + (thisHitbox->Bottom - otherHitbox->Top);
            }
        }
    }

    otherHitbox->Left--;
    otherHitbox->Right++;

    if ((thisEntity->Direction & Flip_X) == Flip_X) {
        int store           = -thisHitbox->Left;
        thisHitbox->Left    = -thisHitbox->Right;
        thisHitbox->Right   = store;

        store               = -otherHitbox->Left;
        otherHitbox->Left   = -otherHitbox->Right;
        otherHitbox->Right  = store;
    }
    if ((thisEntity->Direction & Flip_Y) == Flip_Y) {
        int store           = -thisHitbox->Top;
        thisHitbox->Top     = -thisHitbox->Bottom;
        thisHitbox->Bottom  = store;

        store               = -otherHitbox->Top;
        otherHitbox->Top    = -otherHitbox->Bottom;
        otherHitbox->Bottom = store;
    }

    int side = C_NONE;

    float cx = collideX - otherEntity->X;
    float cy = collideY - otherEntity->Y;
    if ((cx * cx >= cy * cy && (collisionSideV || !collisionSideH)) || (!collisionSideH && collisionSideV)) {
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

                if (otherEntity->VelocityY > 0.0)
                    otherEntity->VelocityY = 0.0;

                if (otherEntity->TileCollisions != TileCollision_UP) {
                    if (!otherEntity->OnGround && otherEntity->VelocityY >= 0.0) {
                        otherEntity->GroundVel  = otherEntity->VelocityX;
                        otherEntity->Angle      = 0x00;
                        otherEntity->OnGround   = true;
                    }
                }
                break;

            case C_LEFT:
                otherEntity->X = collideX;

                velX = otherEntity->VelocityX;
                if (otherEntity->OnGround) {
                    if (otherEntity->CollisionMode == CMode_ROOF)
                        velX = -otherEntity->GroundVel;
                    else
                        velX = otherEntity->GroundVel;
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
                    if (otherEntity->CollisionMode == CMode_ROOF)
                        velX = -otherEntity->GroundVel;
                    else
                        velX = otherEntity->GroundVel;
                }

                if (velX < 0.0) {
                    otherEntity->VelocityX = 0.0;
                    otherEntity->GroundVel = 0.0;
                }
                break;

            case C_BOTTOM:
                otherEntity->Y = collideY;

                if (otherEntity->VelocityY < 0.0)
                    otherEntity->VelocityY = 0.0;

                if (otherEntity->TileCollisions == TileCollision_UP) {
                    if (!otherEntity->OnGround && otherEntity->VelocityY <= 0.0) {
                        otherEntity->GroundVel  = -otherEntity->VelocityX;
                        otherEntity->Angle      = 0x80;
                        otherEntity->OnGround   = true;
                    }
                }
                break;
        }
    }

    if (Scene::ShowHitboxes) {
        int thisHitboxID    = Scene::AddDebugHitbox(H_TYPE_BOX, thisEntity->Direction, thisEntity, thisHitbox);
        int otherHitboxID   = Scene::AddDebugHitbox(H_TYPE_BOX, thisEntity->Direction, otherEntity, otherHitbox);

        if (thisHitboxID >= 0 && side)
            Scene::DebugHitboxList[thisHitboxID].collision |= 1 << (side - 1);
        if (otherHitboxID >= 0 && side)
            Scene::DebugHitboxList[otherHitboxID].collision |= 1 << (4 - side);
    }

    return side;
}

PUBLIC STATIC bool Scene::CheckObjectCollisionPlatform(Entity* thisEntity, CollisionBox* thisHitbox, Entity* otherEntity, CollisionBox* otherHitbox, bool setValues) {
    int store       = 0;
    bool collided   = false;

    if (!thisEntity || !otherEntity || !thisHitbox || !otherHitbox)
        return false;

    if ((thisEntity->Direction & Flip_X) == Flip_X) {
        store           = -thisHitbox->Left;
        thisHitbox->Left    = -thisHitbox->Right;
        thisHitbox->Right   = store;

        store               = -otherHitbox->Left;
        otherHitbox->Left   = -otherHitbox->Right;
        otherHitbox->Right  = store;
    }
    if ((thisEntity->Direction & Flip_Y) == Flip_Y) {
        store           = -thisHitbox->Top;
        thisHitbox->Top     = -thisHitbox->Bottom;
        thisHitbox->Bottom  = store;

        store               = -otherHitbox->Top;
        otherHitbox->Top    = -otherHitbox->Bottom;
        otherHitbox->Bottom = store;
    }

    float otherMoveY = otherEntity->Y - otherEntity->VelocityY;

    if (otherEntity->TileCollisions == TileCollision_UP) {
        if (otherEntity->Y - otherHitbox->Bottom >= thisEntity->Y + thisHitbox->Top && otherMoveY - otherHitbox->Bottom <= thisEntity->Y + thisHitbox->Bottom
            && thisEntity->X + thisHitbox->Left < otherEntity->X + otherHitbox->Right && thisEntity->X + thisHitbox->Right > otherEntity->X + otherHitbox->Left
            && otherEntity->VelocityY <= 0.0) {

            otherEntity->Y = thisEntity->Y + thisHitbox->Bottom + otherHitbox->Bottom;

            if (setValues) {
                otherEntity->VelocityY = 0.0;

                if (!otherEntity->OnGround) {
                    otherEntity->GroundVel  = -otherEntity->VelocityX;
                    otherEntity->Angle      = 0x80;
                    otherEntity->OnGround   = true;
                }
            }

            collided = true;
        }
    }
    else {
        if (otherEntity->Y + otherHitbox->Bottom >= thisEntity->Y + thisHitbox->Top && otherMoveY + otherHitbox->Bottom <= thisEntity->Y + thisHitbox->Bottom
            && thisEntity->X + thisHitbox->Left < otherEntity->X + otherHitbox->Right && thisEntity->X + thisHitbox->Right > otherEntity->X + otherHitbox->Left
            && otherEntity->VelocityY >= 0.0) {

            otherEntity->Y = thisEntity->Y + (thisHitbox->Top - otherHitbox->Bottom);

            if (setValues) {
                otherEntity->VelocityY = 0.0;

                if (!otherEntity->OnGround) {
                    otherEntity->GroundVel  = otherEntity->VelocityX;
                    otherEntity->Angle      = 0x00;
                    otherEntity->OnGround   = true;
                }
            }

            collided = true;
        }
    }

    if ((thisEntity->Direction & Flip_X) == Flip_X) {
        store               = -thisHitbox->Left;
        thisHitbox->Left    = -thisHitbox->Right;
        thisHitbox->Right   = store;

        store               = -otherHitbox->Left;
        otherHitbox->Left   = -otherHitbox->Right;
        otherHitbox->Right  = store;
    }
    if ((thisEntity->Direction & Flip_Y) == Flip_Y) {
        store               = -thisHitbox->Top;
        thisHitbox->Top     = -thisHitbox->Bottom;
        thisHitbox->Bottom  = store;

        store               = -otherHitbox->Top;
        otherHitbox->Top    = -otherHitbox->Bottom;
        otherHitbox->Bottom = store;
    }

    if (Scene::ShowHitboxes) {
        int thisHitboxID    = Scene::AddDebugHitbox(H_TYPE_PLAT, thisEntity->Direction, thisEntity, thisHitbox);
        int otherHitboxID   = Scene::AddDebugHitbox(H_TYPE_PLAT, thisEntity->Direction, otherEntity, otherHitbox);

        if (otherEntity->TileCollisions == TileCollision_UP) {
            if (thisHitboxID >= 0 && collided)
                Scene::DebugHitboxList[thisHitboxID].collision |= 1 << 3;
            if (otherHitboxID >= 0 && collided)
                Scene::DebugHitboxList[otherHitboxID].collision |= 1 << 0;
        }
        else {
            if (thisHitboxID >= 0 && collided)
                Scene::DebugHitboxList[thisHitboxID].collision |= 1 << 0;
            if (otherHitboxID >= 0 && collided)
                Scene::DebugHitboxList[otherHitboxID].collision |= 1 << 3;
        }
    }

    return collided;
}

PUBLIC STATIC bool Scene::ObjectTileCollision(Entity* entity, int cLayers, int cMode, int cPlane, float xOffset, float yOffset, bool setPos) {
    int layerID     = 1;
    bool collided   = false;
    int posX        = xOffset + entity->X;
    int posY        = xOffset + entity->Y;

    int solid = 0;
    switch (cMode) {
        default: return false;

        case CMode_FLOOR:
            solid = cPlane ? (1 << 14) : (1 << 12);

            for (int l = 0; l < Layers.size(); ++l, layerID << 1) {
                if (cLayers & layerID) {
                    SceneLayer layer = Layers[l];
                    float colX  = posX - layer.OffsetX;
                    float colY  = posY - layer.OffsetY;
                    int cy      = ((int)colY & -Scene::TileSize) - Scene::TileSize;
                    if (colX >= 0.0 && colX < Scene::TileSize * layer.Width) {
                        for (int i = 0; i < 3; ++i) {
                            if (cy >= 0 && cy < Scene::TileSize * layer.Height) {
                                Uint16 tile = layer.Tiles[((int)colX / Scene::TileSize) + ((cy / Scene::TileSize) << layer.WidthInBits)];
                                if (tile < 0xFFFF && tile & solid) {
                                    // int32 ty = cy + collisionMasks[cPlane][tile & 0xFFF].floorMasks[colX & 0xF];
                                    // tileCfgBase = &tileCfgBase[tileID] + ((flipY << 1) | flipX) * Scene::TileCount;
                                    int ty;
                                    if (colY >= ty && abs(colY - ty) <= 14) {
                                        collided    = true;
                                        colY        = ty;
                                        i           = 3;
                                    }
                                }
                            }
                            cy += Scene::TileSize;
                        }
                    }
                    posX = layer.OffsetX + colX;
                    posY = layer.OffsetY + colY;
                }
            }

            if (setPos && collided)
                entity->Y = posY - yOffset;
            return collided;

        case CMode_LWALL:
            return collided;

        case CMode_ROOF:
            return collided;

        case CMode_RWALL:
            return collided;
    }

    return collided;
}

PUBLIC STATIC bool Scene::ObjectTileGrip(Entity* thisEntity, int cLayers, int cMode, int cPlane, float xOffset, float yOffset, float tolerance) {
    return false;
}

PUBLIC STATIC void Scene::ProcessObjectMovement(Entity* entity, CollisionBox* outerBox, CollisionBox* innerBox) {
    if (entity && outerBox && innerBox) {
        if (entity->TileCollisions) {
            entity->Angle &= 0xFF;

            Scene::CollisionTolerance = Scene::HighCollisionTolerance;
            if (abs(entity->GroundVel) < 6.0 && entity->Angle == 0)
                Scene::CollisionTolerance = Scene::LowCollisionTolerance;

            Scene::CollisionOuter.Left      = outerBox->Left;
            Scene::CollisionOuter.Top       = outerBox->Top;
            Scene::CollisionOuter.Right     = outerBox->Right;
            Scene::CollisionOuter.Bottom    = outerBox->Bottom;

            Scene::CollisionInner.Left      = innerBox->Left;
            Scene::CollisionInner.Top       = innerBox->Top;
            Scene::CollisionInner.Right     = innerBox->Right;
            Scene::CollisionInner.Bottom    = innerBox->Bottom;

            Scene::CollisionEntity = entity;

            Scene::CollisionMaskAir = Scene::CollisionOuter.Bottom >= 14 ? 8.0 : 2.0;

            if (entity->OnGround) {
                if (entity->TileCollisions == TileCollision_DOWN)
                    Scene::UseCollisionOffset = entity->Angle == 0x00;
                else
                    Scene::UseCollisionOffset = entity->Angle == 0x80;

                if (Scene::CollisionOuter.Bottom < 14)
                    Scene::UseCollisionOffset = false;

                Scene::ProcessPathGrip();
            }
            else {
                Scene::UseCollisionOffset = false;
                if (entity->TileCollisions == TileCollision_DOWN)
                    Scene::ProcessAirCollision_Down();
                else
                    Scene::ProcessAirCollision_Up();
            }

            if (entity->OnGround) {
                entity->VelocityX = entity->GroundVel * Math::Cos256(entity->Angle) / 256.0;
                entity->VelocityY = entity->GroundVel * Math::Cos256(entity->Angle) / 256.0;
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

PUBLIC STATIC void Scene::ProcessPathGrip() {
}

PUBLIC STATIC void Scene::ProcessAirCollision_Down() {
    int movingDown  = 0;
    int movingUp    = 0;
    int movingLeft  = 0;
    int movingRight = 0;

    int offset = Scene::UseCollisionOffset ? COLLISION_OFFSET : 0.0;

    if (Scene::CollisionEntity->VelocityX >= 0.0) {
        movingRight         = 1;
        Scene::Sensors[0].X = Scene::CollisionEntity->X + Scene::CollisionOuter.Right;
        Scene::Sensors[0].Y = Scene::CollisionEntity->Y + offset;
    }

    if (Scene::CollisionEntity->VelocityX <= 0.0) {
        movingLeft          = 1;
        Scene::Sensors[1].X = Scene::CollisionEntity->X + Scene::CollisionOuter.Left - 1.0;
        Scene::Sensors[1].Y = Scene::CollisionEntity->Y + offset;
    }

    Scene::Sensors[2].X = Scene::CollisionEntity->X + Scene::CollisionInner.Left;
    Scene::Sensors[3].X = Scene::CollisionEntity->X + Scene::CollisionInner.Right;
    Scene::Sensors[4].X = Scene::Sensors[2].X;
    Scene::Sensors[5].X = Scene::Sensors[3].X;

    Scene::Sensors[0].Collided = false;
    Scene::Sensors[1].Collided = false;
    Scene::Sensors[2].Collided = false;
    Scene::Sensors[3].Collided = false;
    Scene::Sensors[4].Collided = false;
    Scene::Sensors[5].Collided = false;
    if (Scene::CollisionEntity->VelocityY >= 0.0) {
        movingDown          = 1;
        Scene::Sensors[2].Y = Scene::CollisionEntity->Y + Scene::CollisionOuter.Bottom;
        Scene::Sensors[3].Y = Scene::CollisionEntity->Y + Scene::CollisionOuter.Bottom;
    }

    if (abs(Scene::CollisionEntity->VelocityX) > 1.0 || Scene::CollisionEntity->VelocityY < 0.0) {
        movingUp = 1;
        Scene::Sensors[4].Y = Scene::CollisionEntity->Y + Scene::CollisionOuter.Top - 1.0;
        Scene::Sensors[5].Y = Scene::CollisionEntity->Y + Scene::CollisionOuter.Top - 1.0;
    }

    int cnt = (abs(Scene::CollisionEntity->VelocityX) <= abs(Scene::CollisionEntity->VelocityY) ? ((abs(Scene::CollisionEntity->VelocityY) / Scene::CollisionMaskAir) + 1.0)
                                                                                                : (abs(Scene::CollisionEntity->VelocityX) / Scene::CollisionMaskAir) + 1.0);
    float velX  = Scene::CollisionEntity->VelocityX / cnt;
    float velY  = Scene::CollisionEntity->VelocityY / cnt;
    float velX2 = Scene::CollisionEntity->VelocityX - velX * (cnt - 1.0);
    float velY2 = Scene::CollisionEntity->VelocityY - velY * (cnt - 1.0);
    while (cnt > 0.0) {
        if (cnt < 2.0) {
            velX = velX2;
            velY = velY2;
        }
        cnt--;

        if (movingRight == 1) {
            Scene::Sensors[0].X += velX;
            Scene::Sensors[0].Y += velY;
            Scene::LWallCollision(&Scene::Sensors[0]);

            if (Scene::Sensors[0].Collided) {
                movingRight = 2;
            }
        }

        if (movingLeft == 1) {
            Scene::Sensors[1].X += velX;
            Scene::Sensors[1].Y += velY;
            Scene::RWallCollision(&Scene::Sensors[1]);

            if (Scene::Sensors[1].Collided) {
                movingLeft = 2;
            }
        }

        if (movingRight == 2) {
            Scene::CollisionEntity->VelocityX   = 0.0;
            Scene::CollisionEntity->GroundVel   = 0.0;
            Scene::CollisionEntity->X           = Scene::Sensors[0].X - Scene::CollisionOuter.Right;

            Scene::Sensors[2].X = Scene::CollisionEntity->X + Scene::CollisionOuter.Left + 1.0;
            Scene::Sensors[3].X = Scene::CollisionEntity->X + Scene::CollisionOuter.Right - 2.0;
            Scene::Sensors[4].X = Scene::Sensors[2].X;
            Scene::Sensors[5].X = Scene::Sensors[3].X;

            velX        = 0.0;
            velX2       = 0.0;
            movingRight = 3;
        }

        if (movingLeft == 2) {
            Scene::CollisionEntity->VelocityX   = 0.0;
            Scene::CollisionEntity->GroundVel   = 0.0;
            Scene::CollisionEntity->X           = Scene::Sensors[1].X - Scene::CollisionOuter.Left + 1.0;

            Scene::Sensors[2].X = Scene::CollisionEntity->X + Scene::CollisionOuter.Left + 1.0;
            Scene::Sensors[3].X = Scene::CollisionEntity->X + Scene::CollisionOuter.Right - 2.0;
            Scene::Sensors[4].X = Scene::Sensors[2].X;
            Scene::Sensors[5].X = Scene::Sensors[3].X;

            velX        = 0.0;
            velX2       = 0.0;
            movingLeft  = 3;
        }

        if (movingDown == 1) {
            for (int i = 2; i < 4; i++) {
                if (!Scene::Sensors[i].Collided) {
                    Scene::Sensors[i].X += velX;
                    Scene::Sensors[i].Y += velY;
                    Scene::FloorCollision(&Scene::Sensors[i]);
                }
            }

            if (Scene::Sensors[2].Collided || Scene::Sensors[3].Collided) {
                movingDown  = 2;
                cnt         = 0.0;
            }
        }

        if (movingUp == 1) {
            for (int i = 4; i < 6; i++) {
                if (!Scene::Sensors[i].Collided) {
                    Scene::Sensors[i].X += velX;
                    Scene::Sensors[i].Y += velY;
                    Scene::RoofCollision(&Scene::Sensors[i]);
                }
            }

            if (Scene::Sensors[4].Collided || Scene::Sensors[5].Collided) {
                movingUp    = 2;
                cnt         = 0.0;
            }
        }
    }

    if (movingRight < 2 && movingLeft < 2)
        Scene::CollisionEntity->X += Scene::CollisionEntity->VelocityX;

    if (movingUp < 2 && movingDown < 2) {
        Scene::CollisionEntity->Y += Scene::CollisionEntity->VelocityY;
        return;
    }

    if (movingDown == 2) {
        Scene::CollisionEntity->OnGround = true;

        if (Scene::Sensors[2].Collided && Scene::Sensors[3].Collided) {
            if (Scene::Sensors[2].Y >= Scene::Sensors[3].Y) {
                Scene::CollisionEntity->Y       = Scene::Sensors[3].Y - Scene::CollisionOuter.Bottom;
                Scene::CollisionEntity->Angle   = Scene::Sensors[3].Angle;
            }
            else {
                Scene::CollisionEntity->Y       = Scene::Sensors[2].Y - Scene::CollisionOuter.Bottom;
                Scene::CollisionEntity->Angle   = Scene::Sensors[2].Angle;
            }
        }
        else if (Scene::Sensors[2].Collided) {
            Scene::CollisionEntity->Y       = Scene::Sensors[2].Y - Scene::CollisionOuter.Bottom;
            Scene::CollisionEntity->Angle   = Scene::Sensors[2].Angle;
        }
        else if (Scene::Sensors[3].Collided) {
            Scene::CollisionEntity->Y       = Scene::Sensors[3].Y - Scene::CollisionOuter.Bottom;
            Scene::CollisionEntity->Angle   = Scene::Sensors[3].Angle;
        }

        if (Scene::CollisionEntity->Angle > 0xA0 && Scene::CollisionEntity->Angle < 0xDE && Scene::CollisionEntity->CollisionMode != CMode_LWALL) {
            Scene::CollisionEntity->CollisionMode   = CMode_LWALL;
            Scene::CollisionEntity->X -= 4.0;
        }

        if (Scene::CollisionEntity->Angle > 0x22 && Scene::CollisionEntity->Angle < 0x60 && Scene::CollisionEntity->CollisionMode != CMode_RWALL) {
            Scene::CollisionEntity->CollisionMode   = CMode_RWALL;
            Scene::CollisionEntity->X += 4.0;
        }

        float speed = 0.0;
        if (Scene::CollisionEntity->Angle < 0x80) {
            if (Scene::CollisionEntity->Angle < 0x10) {
                speed = Scene::CollisionEntity->VelocityX;
            }
            else if (Scene::CollisionEntity->Angle >= 0x20) {
                speed = (abs(Scene::CollisionEntity->VelocityX) <= abs(Scene::CollisionEntity->VelocityY) ? Scene::CollisionEntity->VelocityY
                                                                                                          : Scene::CollisionEntity->VelocityX);
            }
            else {
                speed = (abs(Scene::CollisionEntity->VelocityX) <= abs(Scene::CollisionEntity->VelocityY / 2.0) ? (Scene::CollisionEntity->VelocityY / 2.0)
                                                                                                                : Scene::CollisionEntity->VelocityX);
            }
        }
        else if (Scene::CollisionEntity->Angle > 0xF0) {
            speed = Scene::CollisionEntity->VelocityX;
        }
        else if (Scene::CollisionEntity->Angle <= 0xE0) {
            speed = (abs(Scene::CollisionEntity->VelocityX) <= abs(Scene::CollisionEntity->VelocityY) ? -Scene::CollisionEntity->VelocityY : Scene::CollisionEntity->VelocityX);
        }
        else {
            speed = (abs(Scene::CollisionEntity->VelocityX) <= abs(Scene::CollisionEntity->VelocityY / 2.0) ? -(Scene::CollisionEntity->VelocityY / 2.0)
                                                                                                            : Scene::CollisionEntity->VelocityX);
        }

        if (speed < -24.0)
            speed = -24.0;

        if (speed > 24.0)
            speed = 24.0;

        Scene::CollisionEntity->GroundVel = speed;
        Scene::CollisionEntity->VelocityX = speed;
        Scene::CollisionEntity->VelocityY = 0.0;
    }

    if (movingUp == 2) {
        int sensorAngle = 0;

        if (Scene::Sensors[4].Collided && Scene::Sensors[5].Collided) {
            if (Scene::Sensors[4].Y <= Scene::Sensors[5].Y) {
                Scene::CollisionEntity->Y   = Scene::Sensors[5].Y - Scene::CollisionOuter.Top + 1.0;
                sensorAngle                 = Scene::Sensors[5].Angle;
            }
            else {
                Scene::CollisionEntity->Y   = Scene::Sensors[4].Y - Scene::CollisionOuter.Top + 1.0;
                sensorAngle                 = Scene::Sensors[4].Angle;
            }
        }
        else if (Scene::Sensors[4].Collided) {
            Scene::CollisionEntity->Y   = Scene::Sensors[4].Y - Scene::CollisionOuter.Top + 1.0;
            sensorAngle                 = Scene::Sensors[4].Angle;
        }
        else if (Scene::Sensors[5].Collided) {
            Scene::CollisionEntity->Y   = Scene::Sensors[5].Y - Scene::CollisionOuter.Top + 1.0;
            sensorAngle                 = Scene::Sensors[5].Angle;
        }
        sensorAngle &= 0xFF;

        if (sensorAngle < 0x62) {
            if (Scene::CollisionEntity->VelocityY < -abs(Scene::CollisionEntity->VelocityX)) {
                Scene::CollisionEntity->OnGround        = true;
                Scene::CollisionEntity->Angle           = sensorAngle;
                Scene::CollisionEntity->CollisionMode   = CMode_RWALL;
                Scene::CollisionEntity->X += 4.0;
                Scene::CollisionEntity->Y -= 2.0;

                Scene::CollisionEntity->GroundVel = Scene::CollisionEntity->Angle <= 0x60 ? Scene::CollisionEntity->VelocityY : (Scene::CollisionEntity->VelocityY / 2.0);
            }
        }

        if (sensorAngle > 0x9E && sensorAngle < 0xC1) {
            if (Scene::CollisionEntity->VelocityY < -abs(Scene::CollisionEntity->VelocityX)) {
                Scene::CollisionEntity->OnGround        = true;
                Scene::CollisionEntity->Angle           = sensorAngle;
                Scene::CollisionEntity->CollisionMode   = CMode_LWALL;
                Scene::CollisionEntity->X -= 4.0;
                Scene::CollisionEntity->Y -= 2.0;

                Scene::CollisionEntity->GroundVel = Scene::CollisionEntity->Angle >= 0xA0 ? -Scene::CollisionEntity->VelocityY : -(Scene::CollisionEntity->VelocityY / 2.0);
            }
        }

        if (Scene::CollisionEntity->VelocityY < 0.0)
            Scene::CollisionEntity->VelocityY = 0.0;
    }
}

PUBLIC STATIC void Scene::ProcessAirCollision_Up() {
    int movingDown  = 0;
    int movingUp    = 0;
    int movingLeft  = 0;
    int movingRight = 0;

    int offset = Scene::UseCollisionOffset ? COLLISION_OFFSET : 0.0;

    if (Scene::CollisionEntity->VelocityX >= 0.0) {
        movingRight         = 1;
        Scene::Sensors[0].X = Scene::CollisionEntity->X + Scene::CollisionOuter.Right;
        Scene::Sensors[0].Y = Scene::CollisionEntity->Y + offset;
    }

    if (Scene::CollisionEntity->VelocityX <= 0.0) {
        movingLeft          = 1;
        Scene::Sensors[1].X = Scene::CollisionEntity->X + Scene::CollisionOuter.Left - 1.0;
        Scene::Sensors[1].Y = Scene::CollisionEntity->Y + offset;
    }

    Scene::Sensors[2].X = Scene::CollisionEntity->X + Scene::CollisionInner.Left;
    Scene::Sensors[3].X = Scene::CollisionEntity->X + Scene::CollisionInner.Right;
    Scene::Sensors[4].X = Scene::Sensors[2].X;
    Scene::Sensors[5].X = Scene::Sensors[3].X;

    Scene::Sensors[0].Collided = false;
    Scene::Sensors[1].Collided = false;
    Scene::Sensors[2].Collided = false;
    Scene::Sensors[3].Collided = false;
    Scene::Sensors[4].Collided = false;
    Scene::Sensors[5].Collided = false;
    if (Scene::CollisionEntity->VelocityY <= 0.0) {
        movingDown          = 1;
        Scene::Sensors[4].Y = Scene::CollisionEntity->Y + Scene::CollisionOuter.Top - 1.0;
        Scene::Sensors[5].Y = Scene::CollisionEntity->Y + Scene::CollisionOuter.Top - 1.0;
    }

    if (abs(Scene::CollisionEntity->VelocityX) > 1.0 || Scene::CollisionEntity->VelocityY > 0.0) {
        movingUp            = 1;
        Scene::Sensors[2].Y = Scene::CollisionEntity->Y + Scene::CollisionOuter.Bottom;
        Scene::Sensors[3].Y = Scene::CollisionEntity->Y + Scene::CollisionOuter.Bottom;
    }

    int cnt     = (abs(Scene::CollisionEntity->VelocityX) <= abs(Scene::CollisionEntity->VelocityY) ? ((abs(Scene::CollisionEntity->VelocityY) / Scene::CollisionMaskAir) + 1.0)
                                                                                                : (abs(Scene::CollisionEntity->VelocityX) / Scene::CollisionMaskAir) + 1.0);
    float velX  = Scene::CollisionEntity->VelocityX / cnt;
    float velY  = Scene::CollisionEntity->VelocityY / cnt;
    float velX2 = Scene::CollisionEntity->VelocityX - velX * (cnt - 1.0);
    float velY2 = Scene::CollisionEntity->VelocityY - velY * (cnt - 1.0);
    while (cnt > 0.0) {
        if (cnt < 2.0) {
            velX = velX2;
            velY = velY2;
        }
        cnt--;

        if (movingRight == 1) {
            Scene::Sensors[0].X += velX;
            Scene::Sensors[0].Y += velY;
            Scene::LWallCollision(&Scene::Sensors[0]);

            if (Scene::Sensors[0].Collided) {
                movingRight = 2;
            }
        }

        if (movingLeft == 1) {
            Scene::Sensors[1].X += velX;
            Scene::Sensors[1].Y += velY;
            Scene::RWallCollision(&Scene::Sensors[1]);

            if (Scene::Sensors[1].Collided) {
                movingLeft = 2;
            }
        }

        if (movingRight == 2) {
            Scene::CollisionEntity->VelocityX   = 0.0;
            Scene::CollisionEntity->GroundVel   = 0.0;
            Scene::CollisionEntity->X           = Scene::Sensors[0].X - Scene::CollisionOuter.Right;

            Scene::Sensors[2].X = Scene::CollisionEntity->X + Scene::CollisionOuter.Left + 1.0;
            Scene::Sensors[3].X = Scene::CollisionEntity->X + Scene::CollisionOuter.Right - 2.0;
            Scene::Sensors[4].X = Scene::Sensors[2].X;
            Scene::Sensors[5].X = Scene::Sensors[3].X;

            velX        = 0.0;
            velX2       = 0.0;
            movingRight = 3;
        }

        if (movingLeft == 2) {
            Scene::CollisionEntity->VelocityX   = 0.0;
            Scene::CollisionEntity->GroundVel   = 0.0;
            Scene::CollisionEntity->X           = Scene::Sensors[1].X - Scene::CollisionOuter.Left + 1.0;

            Scene::Sensors[2].X = Scene::CollisionEntity->X + Scene::CollisionOuter.Left + 1.0;
            Scene::Sensors[3].X = Scene::CollisionEntity->X + Scene::CollisionOuter.Right - 2.0;
            Scene::Sensors[4].X = Scene::Sensors[2].X;
            Scene::Sensors[5].X = Scene::Sensors[3].X;

            velX        = 0.0;
            velX2       = 0.0;
            movingLeft  = 3;
        }

        if (movingUp == 1) {
            for (int i = 2; i < 4; i++) {
                if (!Scene::Sensors[i].Collided) {
                    Scene::Sensors[i].X += velX;
                    Scene::Sensors[i].Y += velY;
                    Scene::FloorCollision(&Scene::Sensors[i]);
                }
            }

            if (Scene::Sensors[2].Collided || Scene::Sensors[3].Collided) {
                movingUp    = 2;
                cnt         = 0.0;
            }
        }

        if (movingDown == 1) {
            for (int i = 4; i < 6; i++) {
                if (!Scene::Sensors[i].Collided) {
                    Scene::Sensors[i].X += velX;
                    Scene::Sensors[i].Y += velY;
                    Scene::RoofCollision(&Scene::Sensors[i]);
                }
            }

            if (Scene::Sensors[4].Collided || Scene::Sensors[5].Collided) {
                movingDown  = 2;
                cnt         = 0.0;
            }
        }
    }

    if (movingRight < 2 && movingLeft < 2)
        Scene::CollisionEntity->X += Scene::CollisionEntity->VelocityX;

    if (movingUp < 2 && movingDown < 2) {
        Scene::CollisionEntity->Y += Scene::CollisionEntity->VelocityY;
        return;
    }

    if (movingDown == 2) {
        Scene::CollisionEntity->OnGround = true;

        if (Scene::Sensors[4].Collided && Scene::Sensors[5].Collided) {
            if (Scene::Sensors[4].Y <= Scene::Sensors[5].Y) {
                Scene::CollisionEntity->Y       = Scene::Sensors[5].Y - Scene::CollisionOuter.Top + 1.0;
                Scene::CollisionEntity->Angle   = Scene::Sensors[5].Angle;
            }
            else {
                Scene::CollisionEntity->Y       = Scene::Sensors[4].Y - Scene::CollisionOuter.Top + 1.0;
                Scene::CollisionEntity->Angle   = Scene::Sensors[4].Angle;
            }
        }
        else if (Scene::Sensors[4].Collided) {
            Scene::CollisionEntity->Y       = Scene::Sensors[4].Y - Scene::CollisionOuter.Top + 1.0;
            Scene::CollisionEntity->Angle   = Scene::Sensors[4].Angle;
        }
        else if (Scene::Sensors[5].Collided) {
            Scene::CollisionEntity->Y       = Scene::Sensors[5].Y - Scene::CollisionOuter.Top + 1.0;
            Scene::CollisionEntity->Angle   = Scene::Sensors[5].Angle;
        }

        if (Scene::CollisionEntity->Angle > 0xA2 && Scene::CollisionEntity->Angle < 0xE0 && Scene::CollisionEntity->CollisionMode != CMode_LWALL) {
            Scene::CollisionEntity->CollisionMode = CMode_LWALL;
            Scene::CollisionEntity->X -= 4.0;
        }

        if (Scene::CollisionEntity->Angle > 0x20 && Scene::CollisionEntity->Angle < 0x5E && Scene::CollisionEntity->CollisionMode != CMode_RWALL) {
            Scene::CollisionEntity->CollisionMode = CMode_RWALL;
            Scene::CollisionEntity->X += 4.0;
        }

        float speed = 0.0;
        if (Scene::CollisionEntity->Angle >= 0x80) {
            if (Scene::CollisionEntity->Angle < 0x90) {
                speed = -Scene::CollisionEntity->VelocityX;
            }
            else if (Scene::CollisionEntity->Angle >= 0xA0) {
                speed = (abs(Scene::CollisionEntity->VelocityX) <= abs(Scene::CollisionEntity->VelocityY) ? Scene::CollisionEntity->VelocityY
                                                                                                          : Scene::CollisionEntity->VelocityX);
            }
            else {
                speed = (abs(Scene::CollisionEntity->VelocityX) <= abs(Scene::CollisionEntity->VelocityY / 2.0) ? (Scene::CollisionEntity->VelocityY / 2.0)
                                                                                                                : Scene::CollisionEntity->VelocityX);
            }
        }
        else if (Scene::CollisionEntity->Angle > 0x70) {
            speed = Scene::CollisionEntity->VelocityX;
        }
        else if (Scene::CollisionEntity->Angle <= 0x60) {
            speed = (abs(Scene::CollisionEntity->VelocityX) <= abs(Scene::CollisionEntity->VelocityY) ? -Scene::CollisionEntity->VelocityY : Scene::CollisionEntity->VelocityX);
        }
        else {
            speed = (abs(Scene::CollisionEntity->VelocityX) <= abs(Scene::CollisionEntity->VelocityY / 2.0) ? -(Scene::CollisionEntity->VelocityY / 2.0)
                                                                                                            : Scene::CollisionEntity->VelocityX);
        }

        if (speed < -24.0)
            speed = -24.0;

        if (speed > 24.0)
            speed = 24.0;

        Scene::CollisionEntity->GroundVel = speed;
        Scene::CollisionEntity->VelocityX = speed;
        Scene::CollisionEntity->VelocityY = 0.0;
    }

    if (movingUp == 2) {
        int sensorAngle = 0;

        if (Scene::Sensors[2].Collided && Scene::Sensors[3].Collided) {
            if (Scene::Sensors[2].Y >= Scene::Sensors[3].Y) {
                Scene::CollisionEntity->Y   = Scene::Sensors[3].Y - Scene::CollisionOuter.Bottom;
                sensorAngle                 = Scene::Sensors[3].Angle;
            }
            else {
                Scene::CollisionEntity->Y   = Scene::Sensors[2].Y - Scene::CollisionOuter.Bottom;
                sensorAngle                 = Scene::Sensors[2].Angle;
            }
        }
        else if (Scene::Sensors[2].Collided) {
            Scene::CollisionEntity->Y   = Scene::Sensors[2].Y - Scene::CollisionOuter.Bottom;
            sensorAngle                 = Scene::Sensors[2].Angle;
        }
        else if (Scene::Sensors[3].Collided) {
            Scene::CollisionEntity->Y   = Scene::Sensors[3].Y - Scene::CollisionOuter.Bottom;
            sensorAngle                 = Scene::Sensors[3].Angle;
        }
        sensorAngle &= 0xFF;

        if (sensorAngle >= 0x21 && sensorAngle <= 0x40) {
            if (Scene::CollisionEntity->VelocityY > -abs(Scene::CollisionEntity->VelocityX)) {
                Scene::CollisionEntity->OnGround        = true;
                Scene::CollisionEntity->Angle           = sensorAngle;
                Scene::CollisionEntity->CollisionMode   = CMode_RWALL;
                Scene::CollisionEntity->X += 4.0;
                Scene::CollisionEntity->Y -= 2.0;

                Scene::CollisionEntity->GroundVel = Scene::CollisionEntity->Angle <= 0x20 ? Scene::CollisionEntity->VelocityY : (Scene::CollisionEntity->VelocityY / 2.0);
            }
        }

        if (sensorAngle > 0xC0 && sensorAngle < 0xE2) {
            if (Scene::CollisionEntity->VelocityY > -abs(Scene::CollisionEntity->VelocityX)) {
                Scene::CollisionEntity->OnGround        = true;
                Scene::CollisionEntity->Angle           = sensorAngle;
                Scene::CollisionEntity->CollisionMode   = CMode_LWALL;
                Scene::CollisionEntity->X -= 4.0;
                Scene::CollisionEntity->Y -= 2.0;

                Scene::CollisionEntity->GroundVel = Scene::CollisionEntity->Angle <= 0xE0 ? -Scene::CollisionEntity->VelocityY : -(Scene::CollisionEntity->VelocityY / 2.0);
            }
        }

        if (Scene::CollisionEntity->VelocityY > 0.0)
            Scene::CollisionEntity->VelocityY = 0.0;
    }
}

PUBLIC STATIC void Scene::SetPathGripSensors(Sensor* sensors) {
    int offset = Scene::UseCollisionOffset ? COLLISION_OFFSET : 0.0;

    switch (Scene::CollisionEntity->CollisionMode) {
        case CMode_FLOOR:
            sensors[0].Y = sensors[4].Y + Scene::CollisionOuter.Bottom;
            sensors[1].Y = sensors[4].Y + Scene::CollisionOuter.Bottom;
            sensors[2].Y = sensors[4].Y + Scene::CollisionOuter.Bottom;
            sensors[3].Y = sensors[4].Y + offset;

            sensors[0].X = sensors[4].X + Scene::CollisionInner.Left - 1;
            sensors[1].X = sensors[4].X;
            sensors[2].X = sensors[4].X + Scene::CollisionInner.Right;
            if (Scene::CollisionEntity->GroundVel <= 0.0)
                sensors[3].X = sensors[4].X + Scene::CollisionOuter.Left - 1;
            else
                sensors[3].X = sensors[4].X + Scene::CollisionOuter.Right;
            break;

        case CMode_LWALL:
            sensors[0].X = sensors[4].X + Scene::CollisionOuter.Bottom;
            sensors[1].X = sensors[4].X + Scene::CollisionOuter.Bottom;
            sensors[2].X = sensors[4].X + Scene::CollisionOuter.Bottom;
            sensors[3].X = sensors[4].X;

            sensors[0].Y = sensors[4].Y + Scene::CollisionInner.Left - 1;
            sensors[1].Y = sensors[4].Y;
            sensors[2].Y = sensors[4].Y + Scene::CollisionInner.Right;
            if (Scene::CollisionEntity->GroundVel <= 0.0)
                sensors[3].Y = sensors[4].Y - Scene::CollisionOuter.Left;
            else
                sensors[3].Y = sensors[4].Y - Scene::CollisionOuter.Right - 1;
            break;

        case CMode_ROOF:
            sensors[0].Y = sensors[4].Y - Scene::CollisionOuter.Bottom - 1;
            sensors[1].Y = sensors[4].Y - Scene::CollisionOuter.Bottom - 1;
            sensors[2].Y = sensors[4].Y - Scene::CollisionOuter.Bottom - 1;
            sensors[3].Y = sensors[4].Y - offset;

            sensors[0].X = sensors[4].X + Scene::CollisionInner.Left - 1;
            sensors[1].X = sensors[4].X;
            sensors[2].X = sensors[4].X + Scene::CollisionInner.Right;
            if (Scene::CollisionEntity->GroundVel <= 0.0)
                sensors[3].X = sensors[4].X - Scene::CollisionOuter.Left;
            else
                sensors[3].X = sensors[4].X - Scene::CollisionOuter.Right - 1;
            break;

        case CMode_RWALL:
            sensors[0].X = sensors[4].X - Scene::CollisionOuter.Bottom + 1;
            sensors[1].X = sensors[4].X - Scene::CollisionOuter.Bottom + 1;
            sensors[2].X = sensors[4].X - Scene::CollisionOuter.Bottom + 1;
            sensors[3].X = sensors[4].X;

            sensors[0].Y = sensors[4].Y + Scene::CollisionInner.Left - 1;
            sensors[1].Y = sensors[4].Y;
            sensors[2].Y = sensors[4].Y + Scene::CollisionInner.Right;
            if (Scene::CollisionEntity->GroundVel <= 0.0)
                sensors[3].Y = sensors[4].Y + Scene::CollisionOuter.Left - 1;
            else
                sensors[3].Y = sensors[4].Y + Scene::CollisionOuter.Right;
            break;

        default: break;
    }
}

PUBLIC STATIC void Scene::FindFloorPosition(CollisionSensor* sensor) {
}

PUBLIC STATIC void Scene::FindLWallPosition(CollisionSensor* sensor) {
}

PUBLIC STATIC void Scene::FindRoofPosition(CollisionSensor* sensor) {
}

PUBLIC STATIC void Scene::FindRWallPosition(CollisionSensor* sensor) {
}

PUBLIC STATIC void Scene::FloorCollision(CollisionSensor* sensor) {
}

PUBLIC STATIC void Scene::LWallCollision(CollisionSensor* sensor) {
}

PUBLIC STATIC void Scene::RoofCollision(CollisionSensor* sensor) {
}

PUBLIC STATIC void Scene::RWallCollision(CollisionSensor* sensor) {
}