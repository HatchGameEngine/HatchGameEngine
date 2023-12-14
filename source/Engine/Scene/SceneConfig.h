#ifndef ENGINE_SCENE_SCENECONFIG_H
#define ENGINE_SCENE_SCENECONFIG_H

struct SceneListCategory {
    char   Name[256];
    size_t OffsetStart;
    size_t OffsetEnd;
    size_t Count;
};

struct SceneListEntry {
    char   Name[256];
    char   Folder[256];
    char   ID[256];
    char   SpriteFolder[256];
    char   Filetype[16];
    size_t ParentCategoryID;
    size_t CategoryPos;
};

#endif /* ENGINE_SCENE_SCENECONFIG_H */
