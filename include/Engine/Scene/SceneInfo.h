#ifndef ENGINE_SCENE_SCENEINFO_H
#define ENGINE_SCENE_SCENEINFO_H

#include <Engine/Includes/Standard.h>
#include <Engine/Scene/SceneConfig.h>
#include <Engine/TextFormats/XML/XMLNode.h>
#include <Engine/TextFormats/XML/XMLParser.h>

#define SCENEINFO_GLOBAL_CATEGORY_NAME "global"

namespace SceneInfo {
//private:
	SceneListCategory* NewCategory(std::string name);
	SceneListEntry ParseEntry(XMLNode* node, size_t id);
	void FillAttributesHashMap(XMLAttributes* attr, HashMap<char*>* map);

//public:
	extern vector<SceneListCategory> Categories;
	extern int NumTotalScenes;

	int InitGlobalCategory();
	void Dispose();
	bool IsCategoryValid(int categoryID);
	bool IsEntryValid(int categoryID, int entryID);
	bool CategoryHasEntries(int categoryID);
	int GetCategoryID(const char* categoryName);
	int GetEntryID(const char* categoryName, const char* entryName);
	int GetEntryID(int categoryID, const char* entryName);
	std::string GetParentPath(int categoryID, int entryID);
	std::string GetFilename(int categoryID, int entryID);
	std::string GetName(int categoryID, int entryID);
	std::string GetFolder(int categoryID, int entryID);
	std::string GetID(int categoryID, int entryID);
	std::string GetTileConfigFilename(int categoryID, int entryID);
	char* GetEntryProperty(int categoryID, int entryID, char* property);
	char* GetCategoryProperty(int categoryID, char* property);
	bool HasEntryProperty(int categoryID, int entryID, char* property);
	bool HasCategoryProperty(int categoryID, char* property);
	bool Load(XMLNode* node);
};

#endif /* ENGINE_SCENE_SCENEINFO_H */
