#ifndef ENGINE_SCENE_SCENEINFO_H
#define ENGINE_SCENE_SCENEINFO_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/Scene/SceneConfig.h>
#include <Engine/TextFormats/XML/XMLParser.h>
#include <Engine/TextFormats/XML/XMLNode.h>

class SceneInfo {
private:
    static void FillAttributesHashMap(XMLAttributes* attr, HashMap<char*>* map);

public:
    static vector<SceneListEntry>    Entries;
    static vector<SceneListCategory> Categories;

    static void Dispose();
    static bool IsCategoryValid(int categoryID);
    static bool IsEntryValid(int entryID);
    static bool IsEntryValidInCategory(size_t categoryID, size_t entryID);
    static int GetCategoryID(const char* categoryName);
    static int GetEntryID(const char* categoryName, const char* entryName);
    static int GetEntryID(const char* categoryName, size_t entryID);
    static int GetEntryID(size_t categoryID, size_t entryID);
    static int GetEntryPosInCategory(const char *categoryName, const char* entryName);
    static int GetEntryPosInCategory(size_t categoryID, const char* entryName);
    static int GetEntryIDWithinRange(size_t start, size_t end, const char* entryName);
    static string GetParentPath(int entryID);
    static string GetFilename(int entryID);
    static string GetTileConfigFilename(int entryID);
    static char* GetEntryProperty(int entryID, char* property);
    static char* GetCategoryProperty(int categoryID, char* property);
    static bool HasEntryProperty(int entryID, char* property);
    static bool HasCategoryProperty(int categoryID, char* property);
    static bool Load(XMLNode* node);
};

#endif /* ENGINE_SCENE_SCENEINFO_H */
