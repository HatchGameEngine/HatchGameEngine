#ifndef ENGINE_SCENE_SCENEINFO_H
#define ENGINE_SCENE_SCENEINFO_H

#include <Engine/Includes/Standard.h>
#include <Engine/Scene/SceneConfig.h>
#include <Engine/TextFormats/XML/XMLNode.h>
#include <Engine/TextFormats/XML/XMLParser.h>

#define SCENEINFO_GLOBAL_CATEGORY_NAME "global"

class SceneInfo {
private:
	static SceneListCategory* NewCategory(std::string name);
	static SceneListEntry ParseEntry(XMLNode* node, size_t id);
	static void FillAttributesHashMap(XMLAttributes* attr, HashMap<char*>* map);

public:
	static vector<SceneListCategory> Categories;
	static int NumTotalScenes;

	static int InitGlobalCategory();
	static void Dispose();
	static bool IsCategoryValid(int categoryID);
	static bool IsEntryValid(int categoryID, int entryID);
	static bool CategoryHasEntries(int categoryID);
	static int GetCategoryID(const char* categoryName);
	static int GetEntryID(const char* categoryName, const char* entryName);
	static int GetEntryID(int categoryID, const char* entryName);
	static std::string GetParentPath(int categoryID, int entryID);
	static std::string GetFilename(int categoryID, int entryID);
	static std::string GetName(int categoryID, int entryID);
	static std::string GetFolder(int categoryID, int entryID);
	static std::string GetID(int categoryID, int entryID);
	static std::string GetTileConfigFilename(int categoryID, int entryID);
	static char* GetEntryProperty(int categoryID, int entryID, char* property);
	static char* GetCategoryProperty(int categoryID, char* property);
	static bool HasEntryProperty(int categoryID, int entryID, char* property);
	static bool HasCategoryProperty(int categoryID, char* property);
	static bool Load(XMLNode* node);
};

#endif /* ENGINE_SCENE_SCENEINFO_H */
