#if INTERFACE
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

need_t Entity;

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
};
#endif

#include <Engine/Scene.h>

#include <Engine/Audio/AudioManager.h>
#include <Engine/Bytecode/BytecodeObject.h>
#include <Engine/Bytecode/BytecodeObjectManager.h>
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
#include <Engine/ResourceTypes/SceneFormats/HatchSceneReader.h>
#include <Engine/ResourceTypes/SceneFormats/RSDKSceneReader.h>
#include <Engine/ResourceTypes/ISound.h>
#include <Engine/ResourceTypes/ResourceManager.h>
#include <Engine/ResourceTypes/SceneFormats/TiledMapReader.h>
#include <Engine/Rendering/SDL2/SDL2Renderer.h>
#include <Engine/TextFormats/XML/XMLParser.h>
#include <Engine/Types/EntityTypes.h>
#include <Engine/Types/ObjectList.h>
#include <Engine/Utilities/StringUtils.h>

// Layering variables
vector<SceneLayer>    Scene::Layers;
bool                  Scene::AnyLayerTileChange = false;
int                   Scene::MainLayer = 0;
int                   Scene::PriorityPerLayer = 16;
DrawGroupList*        Scene::PriorityLists = NULL;

// Rendering variables
int                   Scene::ShowTileCollisionFlag = 0;
int                   Scene::ShowObjectRegions = 0;

// Object variables
HashMap<ObjectList*>* Scene::ObjectLists = NULL;
HashMap<ObjectList*>* Scene::ObjectRegistries = NULL;

int                   Scene::StaticObjectCount = 0;
Entity*               Scene::StaticObjectFirst = NULL;
Entity*               Scene::StaticObjectLast = NULL;
int                   Scene::DynamicObjectCount = 0;
Entity*               Scene::DynamicObjectFirst = NULL;
Entity*               Scene::DynamicObjectLast = NULL;

// Tile variables
vector<ISprite*>      Scene::TileSprites;
vector<TileSpriteInfo>Scene::TileSpriteInfos;
int                   Scene::TileCount = 0;
int                   Scene::TileSize = 16;
TileConfig*           Scene::TileCfgA = NULL;
TileConfig*           Scene::TileCfgB = NULL;
Uint16                Scene::EmptyTile = 0x000;
Uint32*               Scene::ExtraPalettes = NULL;

// View variables
int                   Scene::Frame = 0;
bool                  Scene::Paused = false;
float                 Scene::CameraX = 0.0f;
float                 Scene::CameraY = 0.0f;
View                  Scene::Views[MAX_SCENE_VIEWS];
int                   Scene::ViewCurrent = 0;
int                   Scene::ViewsActive = 1;
int                   Scene::ObjectViewRenderFlag;
int                   Scene::TileViewRenderFlag;
Perf_ViewRender       Scene::PERF_ViewRender[MAX_SCENE_VIEWS];

char                  Scene::NextScene[256];
char                  Scene::CurrentScene[256];
bool                  Scene::DoRestart = false;

// Resource managing variables
HashMap<ISprite*>*    Scene::SpriteMap = NULL;
vector<ResourceType*> Scene::SpriteList;
vector<ResourceType*> Scene::ImageList;
vector<ResourceType*> Scene::SoundList;
vector<ResourceType*> Scene::MusicList;
vector<ResourceType*> Scene::ShaderList;
vector<ResourceType*> Scene::ModelList;
vector<ResourceType*> Scene::MediaList;

Entity*               StaticObject = NULL;
ObjectList*           StaticObjectList = NULL;
bool                  DEV_NoTiles = false;
bool                  DEV_NoObjectRender = false;
const char*           DEBUG_lastTileColFilename = NULL;

int ViewRenderList[MAX_SCENE_VIEWS];
int TileBatchMaxSize = 8;

void _ObjectList_RemoveNonPersistentDynamicFromLists(Uint32, ObjectList* list) {
    // NOTE: We don't use any list clearing functions so that
    //   we can support persistent objects later.
    for (Entity* ent = Scene::DynamicObjectFirst, *next; ent; ent = next) {
        // Store the "next" so that when/if the current is removed,
        // it can still be used to point at the end of the loop.
        next = ent->NextEntity;

        // Remove the current
        if (ent->List == list)
            list->Remove(ent);
    }
}
void _ObjectList_Clear(Uint32, ObjectList* list) {
    list->Clear();
}
void _ObjectList_ResetPerf(Uint32, ObjectList* list) {
    // list->AverageUpdateTime = 0.0;
    list->AverageUpdateItemCount = 0.0;
    list->AverageUpdateEarlyItemCount = 0.0;
    list->AverageUpdateLateItemCount = 0.0;
    list->AverageRenderItemCount = 0.0;
}
void _ObjectList_CallLoads(Uint32, ObjectList* list) {
    char   entitySpecialFunctions[sizeof(list->ObjectName) + 5];
    snprintf(entitySpecialFunctions, sizeof entitySpecialFunctions, "%s_Load", list->ObjectName);
    BytecodeObjectManager::CallFunction(entitySpecialFunctions);
}
void _ObjectList_CallGlobalUpdates(Uint32, ObjectList* list) {
    char   entitySpecialFunctions[sizeof(list->ObjectName) + 13];
    snprintf(entitySpecialFunctions, sizeof entitySpecialFunctions, "%s_GlobalUpdate", list->ObjectName);
    BytecodeObjectManager::CallFunction(entitySpecialFunctions);
}
void _UpdateObjectEarly(Entity* ent) {
    if (Scene::Paused && ent->Pauseable)
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
void _UpdateObjectLate(Entity* ent) {
    if (Scene::Paused && ent->Pauseable)
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
void _UpdateObject(Entity* ent) {
    if (Scene::Paused && ent->Pauseable)
        return;

    if (!ent->Active)
        return;

    bool onScreenX = (ent->OnScreenHitboxW == 0.0f);
    bool onScreenY = (ent->OnScreenHitboxH == 0.0f);

    if (Scene::ViewsActive < 2) {
        if (!onScreenX) {
            onScreenX = (ent->X + ent->OnScreenHitboxW * 0.5f >= Scene::Views[0].X &&
                         ent->X - ent->OnScreenHitboxW * 0.5f <  Scene::Views[0].X + Scene::Views[0].Width);
        }
        if (!onScreenY) {
            onScreenY = (ent->Y + ent->OnScreenHitboxH * 0.5f >= Scene::Views[0].Y &&
                         ent->Y - ent->OnScreenHitboxH * 0.5f <  Scene::Views[0].Y + Scene::Views[0].Height);
        }
    } else {
        for (int i = 0; i < Scene::ViewsActive; i++) {
            if (!onScreenX) {
                onScreenX = (ent->X + ent->OnScreenHitboxW * 0.5f >= Scene::Views[i].X &&
                             ent->X - ent->OnScreenHitboxW * 0.5f <  Scene::Views[i].X + Scene::Views[i].Width);
            }
            if (!onScreenY) {
                onScreenY = (ent->Y + ent->OnScreenHitboxH * 0.5f >= Scene::Views[i].Y &&
                             ent->Y - ent->OnScreenHitboxH * 0.5f <  Scene::Views[i].Y + Scene::Views[i].Height);
            }
        }
    }

    if (onScreenX && onScreenY) {
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
    else if (ent->Depth != ent->OldDepth) {
        Scene::PriorityLists[ent->Priority].NeedsSorting = true;
    }
    ent->PriorityOld = ent->Priority;
    ent->OldDepth = ent->Depth;
}
inline int _CEILPOW(int n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 16;
    n++;
    return n;
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
        Log::Print(Log::LOG_IMPORTANT, "obj->List = NULL");
        exit(-1);
    }
    obj->List->Add(obj);
}
PUBLIC STATIC void Scene::Remove(Entity** first, Entity** last, int* count, Entity* obj) {
    if (obj == NULL) return;

    if ((*first) == obj)
        (*first) = obj->NextEntity;

    if ((*last) == obj)
        (*last) = obj->PrevEntity;

    if (obj->PrevEntity)
        obj->PrevEntity->NextEntity = obj->NextEntity;

    if (obj->NextEntity)
        obj->NextEntity->PrevEntity = obj->PrevEntity;

    (*count)--;

    // Remove from proper list
    if (!obj->List) {
        Log::Print(Log::LOG_IMPORTANT, "obj->List = NULL");
        exit(-1);
    }
    obj->List->Remove(obj);

    // Remove from DrawGroups
    for (int l = 0; l < Scene::PriorityPerLayer; l++) {
        DrawGroupList* drawGroupList = &PriorityLists[l];
        for (size_t o = 0; o < drawGroupList->EntityCapacity; o++) {
            if (drawGroupList->Entities[o] == obj)
                drawGroupList->Entities[o] = NULL;
        }
    }

    obj->Dispose();
    delete obj;
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

// Scene Lifecycle
PUBLIC STATIC void Scene::Init() {
    Scene::NextScene[0] = '\0';
    Scene::CurrentScene[0] = '\0';

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
        Scene::Views[i].Software = false;
        Scene::Views[i].Priority = 0;
        Scene::Views[i].Width = Application::WindowWidth;
        Scene::Views[i].Height = Application::WindowHeight;
        Scene::Views[i].OutputWidth = Application::WindowWidth;
        Scene::Views[i].OutputHeight = Application::WindowHeight;
        Scene::Views[i].Stride = _CEILPOW(Scene::Views[i].Width);
        Scene::Views[i].FOV = 45.0f;
        Scene::Views[i].UsePerspective = false;
        Scene::Views[i].DrawTarget = Graphics::CreateTexture(SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, Scene::Views[i].Stride, Scene::Views[i].Height);
        Scene::Views[i].UseDrawTarget = true;
        Scene::Views[i].ProjectionMatrix = Matrix4x4::Create();
        Scene::Views[i].BaseProjectionMatrix = Matrix4x4::Create();
    }
    Scene::Views[0].Active = true;
    Scene::ViewsActive = 1;

    Scene::ObjectViewRenderFlag = 0xFFFFFFFF;
    Scene::TileViewRenderFlag = 0xFFFFFFFF;

    BytecodeObjectManager::LoadAllClasses();
}

PUBLIC STATIC void Scene::ResetPerf() {
    if (Scene::ObjectLists)
        Scene::ObjectLists->ForAll(_ObjectList_ResetPerf);
}
PUBLIC STATIC void Scene::Update() {
    // Call global updates
    if (Scene::ObjectLists)
        Scene::ObjectLists->ForAllOrdered(_ObjectList_CallGlobalUpdates);

    // Early Update
    for (Entity* ent = Scene::StaticObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;
        _UpdateObjectEarly(ent);
    }
    for (Entity* ent = Scene::DynamicObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;
        _UpdateObjectEarly(ent);
    }

    // Update objects
    for (Entity* ent = Scene::StaticObjectFirst, *next; ent; ent = next) {
        // Store the "next" so that when/if the current is removed,
        // it can still be used to point at the end of the loop.
        next = ent->NextEntity;

        // Execute whatever on object
        _UpdateObject(ent);
    }
    for (Entity* ent = Scene::DynamicObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;

        _UpdateObject(ent);
    }

    // Late Update
    for (Entity* ent = Scene::StaticObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;
        _UpdateObjectLate(ent);
    }
    for (Entity* ent = Scene::DynamicObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;
        _UpdateObjectLate(ent);

        if (!ent->Active) {
            Scene::Remove(&Scene::DynamicObjectFirst, &Scene::DynamicObjectLast, &Scene::DynamicObjectCount, ent);
        }
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

    if (!Scene::Paused)
        Scene::Frame++;
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

static int ViewSortFunction(const void *a, const void *b) {
    View* viewA = &Scene::Views[*(const int *)a];
    View* viewB = &Scene::Views[*(const int *)b];
    return viewA->Priority - viewB->Priority;
}
PUBLIC STATIC void Scene::SortViews() {
    int count = 0;

    for (int i = 0; i < MAX_SCENE_VIEWS; i++) {
        if (Scene::Views[i].Active)
            ViewRenderList[count++] = i;
    }

    if (count > 1)
        qsort(ViewRenderList, count, sizeof(int), ViewSortFunction);
}

PUBLIC STATIC void Scene::SetView(int viewIndex) {
    View* currentView = &Scene::Views[viewIndex];

    if (currentView->UseDrawTarget && currentView->DrawTarget) {
        float view_w = currentView->Width;
        float view_h = currentView->Height;
        Texture* tar = currentView->DrawTarget;
        if (tar->Width != (size_t)currentView->Stride || tar->Height != view_h) {
            Graphics::GfxFunctions = &Graphics::Internal;
            Graphics::DisposeTexture(tar);
            Graphics::SetTextureInterpolation(false);
            currentView->DrawTarget = Graphics::CreateTexture(SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, _CEILPOW(view_w), view_h);
        }

        Graphics::SetRenderTarget(currentView->DrawTarget);

        if (currentView->Software)
            Graphics::SoftwareStart();
        else
            Graphics::Clear();
    }

    Scene::ViewCurrent = viewIndex;
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
        if (drawGroupList->NeedsSorting) {
            drawGroupList->Sort();
        }
        for (size_t o = 0; o < drawGroupList->EntityCapacity; o++) {
            if (drawGroupList->Entities[o] && drawGroupList->Entities[o]->Active)
                drawGroupList->Entities[o]->RenderEarly();
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
        size_t oSz = PriorityLists[l].EntityCapacity;

        if (DEV_NoObjectRender)
            goto DEV_NoTilesCheck;

        double elapsed;
        double objectTime;
        float hbW;
        float hbH;
        float _ox;
        float _oy;
        objectTime = Clock::GetTicks();

        drawGroupList = &PriorityLists[l];
        for (size_t o = 0; o < oSz; o++) {
            if (drawGroupList->Entities[o] && drawGroupList->Entities[o]->Active) {
                Entity* ent = drawGroupList->Entities[o];

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
                if (ent->ViewRenderFlag != 0) {
                    if ((ent->ViewRenderFlag & viewRenderFlag) == 0)
                        continue;
                }
                else if ((Scene::ObjectViewRenderFlag & viewRenderFlag) == 0)
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

        DrawGroupList* drawGroupList = &PriorityLists[l];
        size_t oSz = drawGroupList->EntityCapacity;
        for (size_t o = 0; o < oSz; o++) {
            if (drawGroupList->Entities[o] && drawGroupList->Entities[o]->Active)
                drawGroupList->Entities[o]->RenderLate();
        }
    }
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

    // DEV_NoTiles = true;
    // DEV_NoObjectRender = true;

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

    if (Scene::NextScene[0]) {
        BytecodeObjectManager::ForceGarbageCollection();

        Scene::LoadScene(Scene::NextScene);
        Scene::Restart();

        Scene::NextScene[0] = '\0';
        Scene::DoRestart = false;
    }

    if (Scene::DoRestart) {
        // BytecodeObjectManager::ForceGarbageCollection();

        Scene::Restart();
        Scene::DoRestart = false;
    }
}
PUBLIC STATIC void Scene::Restart() {
    Scene::ViewCurrent = 0;

    View* currentView = &Scene::Views[Scene::ViewCurrent];
    currentView->X = 0.0f;
    currentView->Y = 0.0f;
    currentView->Z = 0.0f;
    Scene::Frame = 0;
    Scene::Paused = false;

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
    Scene::UpdateTileBatchAll();

    if (Scene::PriorityLists) {
        for (int l = 0; l < Scene::PriorityPerLayer; l++) {
            Scene::PriorityLists[l].Clear();
        }
    }

    // Remove all non-persistent objects from lists
	if (Scene::ObjectLists)
		Scene::ObjectLists->ForAll(_ObjectList_RemoveNonPersistentDynamicFromLists);

    // Remove all non-persistent objects from lists
	if (Scene::ObjectRegistries)
		Scene::ObjectRegistries->ForAll(_ObjectList_Clear);

    // Dispose of all dynamic objects
    for (Entity* ent = Scene::DynamicObjectFirst, *next; ent; ent = next) {
        // Store the "next" so that when/if the current is removed,
        // it can still be used to point at the end of the loop.
        next = ent->NextEntity;

        // Dispose of object
        ent->Dispose();
    }

    Scene::Clear(&Scene::DynamicObjectFirst, &Scene::DynamicObjectLast, &Scene::DynamicObjectCount);

    // Run "Setup" on all objects
    for (Entity* ent = Scene::StaticObjectFirst, *next; ent; ent = next) {
        // Store the "next" so that when/if the current is removed,
        // it can still be used to point at the end of the loop.
        next = ent->NextEntity;

        ent->X = ent->InitialX;
        ent->Y = ent->InitialY;

        // Execute whatever on object
        ent->Setup();
    }

    // Run "Load" on all object classes
    if (Scene::ObjectLists)
		Scene::ObjectLists->ForAllOrdered(_ObjectList_CallLoads);

    // Run "Create" on all objects
    for (Entity* ent = Scene::StaticObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;
        ent->Create(0);
    }
    // NOTE: We don't need to do dynamic objects here since we cleared out the list.
    // TODO: We should do this for any persistent dynamic objects.

    BytecodeObjectManager::ResetStack();
    BytecodeObjectManager::RequestGarbageCollection();
}
PUBLIC STATIC void Scene::LoadScene(const char* filename) {
    /// Reset everything
    // Clear lists
    if (Scene::ObjectLists) {
        Scene::ObjectLists->ForAll(_ObjectList_Clear);
        Scene::ObjectLists->Clear();
    }
    if (Scene::ObjectRegistries) {
        Scene::ObjectRegistries->ForAll(_ObjectList_Clear);
        Scene::ObjectRegistries->Clear();
    }

    // Dispose of resources in SCOPE_SCENE
    Scene::DisposeInScope(SCOPE_SCENE);

    // TODO: Make a way for this to delete ONLY of SCOPE_SCENE, and not any sprites needed by SCOPE_GAME
    /*Graphics::SpriteSheetTextureMap->WithAll([](Uint32, Texture* tex) -> void {
        Graphics::DisposeTexture(tex);
    });
    Graphics::SpriteSheetTextureMap->Clear();*/

    // Clear and dispose of objects
    // TODO: Alter this for persistency
    for (Entity* ent = Scene::StaticObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;
        if (!ent->Persistent) {
            ent->Dispose();
            delete ent;

            // Don't remove since we're clearing anyways, until persistence works
            // Scene::Remove(&Scene::StaticObjectFirst, &Scene::StaticObjectLast, &Scene::StaticObjectCount, ent);
        }
    }
    Scene::Clear(&Scene::StaticObjectFirst, &Scene::StaticObjectLast, &Scene::StaticObjectCount);

    for (Entity* ent = Scene::DynamicObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;
        if (!ent->Persistent) {
            ent->Dispose();
            delete ent;

            // Don't remove since we're clearing anyways, until persistence works
            // Scene::Remove(&Scene::DynamicObjectFirst, &Scene::DynamicObjectLast, &Scene::DynamicObjectCount, ent);
        }
    }
    Scene::Clear(&Scene::DynamicObjectFirst, &Scene::DynamicObjectLast, &Scene::DynamicObjectCount);

    // Clear PriorityLists
    if (Scene::PriorityLists) {
        int layerSize = Scene::PriorityPerLayer;
        for (int l = 0; l < layerSize; l++) {
            Scene::PriorityLists[l].Clear();
        }
    }

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

    // Force garbage collect
    BytecodeObjectManager::ResetStack();
    BytecodeObjectManager::ForceGarbageCollection();
    ////

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

    strcpy(Scene::CurrentScene, filename);
    Scene::CurrentScene[strlen(filename)] = 0;

    for (size_t i = 0; i < Scene::TileSprites.size(); i++) {
        Scene::TileSprites[i]->Dispose();
        delete Scene::TileSprites[i];
    }
    Scene::TileSprites.clear();
    Scene::TileSpriteInfos.clear();

    Log::Print(Log::LOG_INFO, "Starting scene \"%s\"...", filename);

    // Try to read it
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
    }
    else
        Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", filename);

    Scene::AddStaticClass();
}
PUBLIC STATIC void Scene::AddStaticClass() {
    // Add "Static" class
    if (Application::GameStart) {
        Entity* obj = NULL;
        const char* objectName;
        Uint32 objectNameHash;

        objectName = "Static";
        objectNameHash = CombinedHash::EncryptString(objectName);
        StaticObjectList = new ObjectList();
        strcpy(StaticObjectList->ObjectName, objectName);
        StaticObjectList->SpawnFunction = (Entity*(*)())BytecodeObjectManager::GetSpawnFunction(objectNameHash, (char*)objectName);
        // Scene::ObjectLists->Put(objectNameHash, objectList);

        if (StaticObjectList->SpawnFunction) {
            obj = StaticObjectList->SpawnFunction();
            if (obj) {
                obj->X = 0.0f;
                obj->Y = 0.0f;
                obj->InitialX = obj->X;
                obj->InitialY = obj->Y;
                obj->List = StaticObjectList;
                obj->Persistent = true;
                // Scene::AddStatic(objectList, obj);

                BytecodeObjectManager::Globals->Put("global", OBJECT_VAL(((BytecodeObject*)obj)->Instance));

                if (Application::GameStart) {
                    if (StaticObject) {
                        StaticObject->Dispose();
                        delete StaticObject;
                        StaticObject = NULL;
                    }

                    obj->GameStart();
                    Application::GameStart = false;
                }
            }
        }

        StaticObject = obj;
    }
}
PUBLIC STATIC void Scene::AddManagers() {
    ObjectList* objectList;
    Uint32 objectNameHash;

#define ADD_OBJECT_CLASS(objectName) \
    objectNameHash = CombinedHash::EncryptString(objectName); \
    if (Scene::ObjectLists->Exists(objectNameHash)) \
        objectList = Scene::ObjectLists->Get(objectNameHash); \
    else { \
        objectList = new ObjectList(); \
        strcpy(objectList->ObjectName, objectName); \
        objectList->SpawnFunction = (Entity*(*)())BytecodeObjectManager::GetSpawnFunction(objectNameHash, objectName); \
        Scene::ObjectLists->Put(objectNameHash, objectList); \
    } \
 \
    if (objectList->SpawnFunction) { \
        Entity* obj = objectList->SpawnFunction(); \
        obj->X = 0.0f; \
        obj->Y = 0.0f; \
        obj->InitialX = obj->X; \
        obj->InitialY = obj->Y; \
        obj->List = objectList; \
        Scene::AddStatic(objectList, obj); \
    }

    ADD_OBJECT_CLASS("WindowManager");
    ADD_OBJECT_CLASS("InputManager");
    ADD_OBJECT_CLASS("PauseManager");
    ADD_OBJECT_CLASS("FadeManager");

#undef ADD_OBJECT_CLASS
}
PUBLIC STATIC void Scene::LoadTileCollisions(const char* filename) {
    Stream* tileColReader;
    if (!ResourceManager::ResourceExists(filename)) {
        Log::Print(Log::LOG_WARN, "Could not find tile collision file \"%s\"!", filename);
        return;
    }

    tileColReader = ResourceStream::New(filename);
    if (!tileColReader) {
        DEBUG_lastTileColFilename = NULL;
        return;
    }

    DEBUG_lastTileColFilename = filename;

    Uint32 magic = tileColReader->ReadUInt32();
    // RSDK TileConfig
    if (magic == 0x004C4954U) {
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
    // HCOL TileConfig
    else if (magic == 0x4C4F4354U) {
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
    else {
        Log::Print(Log::LOG_ERROR, "Invalid magic for TileCollisions! %X", magic);
    }
    tileColReader->Close();
}
PUBLIC STATIC void Scene::SaveTileCollisions() {
    /*
    if (DEBUG_lastTileColFilename == NULL) return;

    char yyyyyyyyyaaaaaa[256];
    sprintf(yyyyyyyyyaaaaaa, "Resources/%s", DEBUG_lastTileColFilename);

    Stream* tileColWriter;
    tileColWriter = FileStream::New(yyyyyyyyyaaaaaa, FileStream::WRITE_ACCESS);
    if (!tileColWriter) {
        Log::Print(Log::LOG_ERROR, "Could not open map file \"%s\"!", yyyyyyyyyaaaaaa);
        return;
    }

    tileColWriter->WriteUInt32(0x12345678);

    tileColWriter->WriteUInt32(Scene::TileCount);
    tileColWriter->WriteByte(Scene::TileSize);
    tileColWriter->WriteByte(0x00);
    tileColWriter->WriteByte(0x00);
    tileColWriter->WriteByte(0x00);
    tileColWriter->WriteUInt32(0x00000000);

    for (int i = 0; i < Scene::TileCount; i++) {
        tileColWriter->WriteByte(Scene::TileCfgA[i].IsCeiling);
        tileColWriter->WriteByte(Scene::TileCfgA[i].Angles[0]);

        bool hasCollision = false;
        for (int t = 0; t < Scene::TileSize; t++) {
            hasCollision |= !!Scene::TileCfgA[i].HasCollision[t];
        }
        tileColWriter->WriteByte(hasCollision);

        for (int t = 0; t < Scene::TileSize; t++) {
            tileColWriter->WriteByte(Scene::TileCfgA[i].Collision[t]); // collision
        }
    }
    tileColWriter->Close();
    //*/
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
    // Sounds
    // AudioManager::AudioPauseAll();
    AudioManager::ClearMusic();
    AudioManager::AudioStopAll();

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
    Scene::MediaList.clear();

    if (StaticObject) {
        StaticObject->Dispose();
        delete StaticObject;
        StaticObject = NULL;
    }

    // Dispose and clear Static objects
    for (Entity* ent = Scene::StaticObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;
        ent->Dispose();
        delete ent;
    }
    Scene::Clear(&Scene::StaticObjectFirst, &Scene::StaticObjectLast, &Scene::StaticObjectCount);

    // Dispose and clear Dynamic objects
    for (Entity* ent = Scene::DynamicObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;
        ent->Dispose();
        delete ent;
    }
    Scene::Clear(&Scene::DynamicObjectFirst, &Scene::DynamicObjectLast, &Scene::DynamicObjectCount);

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
        Scene::ObjectRegistries->ForAll([](Uint32, ObjectList* list) -> void {
            delete list;
        });
        delete Scene::ObjectRegistries;
    }
    Scene::ObjectRegistries = NULL;

    if (StaticObjectList) {
        delete StaticObjectList;
        StaticObjectList = NULL;
    }

    if (Scene::ExtraPalettes)
        Memory::Free(Scene::ExtraPalettes);
    Scene::ExtraPalettes = NULL;

    if (Scene::TileCfgA) {
        Memory::Free(Scene::TileCfgA);
    }
    Scene::TileCfgA = NULL;
    Scene::TileCfgB = NULL;

    BytecodeObjectManager::Dispose();
    SourceFileMap::Dispose();
}

PUBLIC STATIC void Scene::Exit() {

}

// Tile Batching
PUBLIC STATIC void Scene::UpdateTileBatchAll() {

}
PUBLIC STATIC void Scene::SetTile(int layer, int x, int y, int tileID, int flip_x, int flip_y, int collA, int collB) {
    Uint32* tile = &Scene::Layers[layer].Tiles[x + (y << Scene::Layers[layer].WidthInBits)];

    *tile = tileID & TILE_IDENT_MASK;
    if (flip_x)
        *tile |= TILE_FLIPX_MASK;
    if (flip_y)
        *tile |= TILE_FLIPY_MASK;
    *tile |= collA << 28;
    *tile |= collB << 26;

    // UpdateTileBatch(layer, x / TileBatchMaxSize, y / TileBatchMaxSize);
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
