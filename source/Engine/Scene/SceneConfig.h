#ifndef ENGINE_SCENE_SCENECONFIG_H
#define ENGINE_SCENE_SCENECONFIG_H

#include <Engine/Includes/HashMap.h>

struct SceneListCategory {
    char*           Name = nullptr;

    size_t          OffsetStart;
    size_t          OffsetEnd;
    size_t          Count;

    HashMap<char*>* Properties = nullptr;
};

struct SceneListEntry {
    char*           Name = nullptr;
    char*           Folder = nullptr;
    char*           ID = nullptr;
    char*           SpriteFolder = nullptr;
    char*           Filetype = nullptr;

    size_t          ParentCategoryID;
    size_t          CategoryPos;

    HashMap<char*>* Properties = nullptr;
};

#endif /* ENGINE_SCENE_SCENECONFIG_H */
