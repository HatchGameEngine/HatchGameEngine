#ifndef ENGINE_SCENE_SCENECONFIG_H
#define ENGINE_SCENE_SCENECONFIG_H

struct SceneListInfo {
    char    name[32];
    int     sceneOffsetStart;
    int     sceneOffsetEnd;
    int     sceneCount;
};

struct SceneListEntry {
    char    name[32];
    char    folder[16];
    char    id[8];
    char    spriteFolder[16];
    char    fileType[8];
};

#endif /* ENGINE_SCENE_SCENECONFIG_H */