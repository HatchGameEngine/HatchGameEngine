#include <Engine/Scene/SceneInfo.h>
#include <Engine/Utilities/StringUtils.h>

vector<SceneListCategory> SceneInfo::Categories;
int SceneInfo::NumTotalScenes;

void SceneInfo::Dispose() {
	for (size_t i = 0; i < Categories.size(); i++) {
		Categories[i].Dispose();
	}
	Categories.clear();

	NumTotalScenes = 0;
}

int SceneInfo::InitGlobalCategory() {
	size_t gid;

	NewCategory(std::string(SCENEINFO_GLOBAL_CATEGORY_NAME));

	gid = Categories.size() - 1;

	return (int)gid;
}

SceneListCategory* SceneInfo::NewCategory(std::string name) {
	SceneListCategory newCategory;
	Categories.push_back(std::move(newCategory));

	SceneListCategory* category = &Categories[Categories.size() - 1];
	category->Name = StringUtils::Create(name);

	category->Properties = new HashMap<char*>(NULL, 8);
	category->Properties->Put("name", category->Name);

	return category;
}

bool SceneInfo::IsCategoryValid(int categoryID) {
	return categoryID >= 0 && categoryID < (int)Categories.size();
}

bool SceneInfo::CategoryHasEntries(int categoryID) {
	if (!IsCategoryValid(categoryID)) {
		return false;
	}

	return Categories[categoryID].Entries.size() > 0;
}

bool SceneInfo::IsEntryValid(int categoryID, int entryID) {
	if (!CategoryHasEntries(categoryID)) {
		return false;
	}

	return entryID >= 0 && (size_t)entryID < Categories[categoryID].Entries.size();
}

int SceneInfo::GetCategoryID(const char* categoryName) {
	if (categoryName != nullptr) {
		for (size_t i = 0; i < Categories.size(); i++) {
			if (!strcmp(Categories[i].Name, categoryName)) {
				return i;
			}
		}
	}

	return -1;
}

int SceneInfo::GetEntryID(const char* categoryName, const char* entryName) {
	int categoryID = GetCategoryID(categoryName);
	if (categoryID < 0) {
		return -1;
	}

	return GetEntryID(categoryID, entryName);
}

int SceneInfo::GetEntryID(int categoryID, const char* entryName) {
	if (!entryName || !IsCategoryValid(categoryID)) {
		return -1;
	}

	SceneListCategory& category = Categories[categoryID];
	for (size_t i = 0; i < category.Entries.size(); i++) {
		if (!strcmp(category.Entries[i].Name, entryName)) {
			return (int)i;
		}
	}

	return -1;
}

std::string SceneInfo::GetParentPath(int categoryID, int entryID) {
	if (!SceneInfo::IsEntryValid(categoryID, entryID)) {
		return "";
	}

	SceneListEntry& entry = Categories[categoryID].Entries[entryID];

	std::string filePath;

	if (entry.Filetype != nullptr && strcmp(entry.Filetype, "bin") == 0) {
		filePath += "Stages/";
	}
	else {
		filePath += "Scenes/";
	}

	if (entry.Folder != nullptr) {
		filePath += std::string(entry.Folder) + "/";
	}

	return filePath;
}

std::string SceneInfo::GetFilename(int categoryID, int entryID) {
	if (!SceneInfo::IsEntryValid(categoryID, entryID)) {
		return "";
	}

	SceneListEntry& entry = Categories[categoryID].Entries[entryID];

	if (entry.Path != nullptr) {
		return std::string(entry.Path);
	}

	std::string parentPath = GetParentPath(categoryID, entryID);

	std::string id = entry.ID != nullptr ? std::string(entry.ID) : std::string(entry.Name);

	std::string filePath = "";

	// RSDK compatibility.
	if (entry.Filetype != nullptr && strcmp(entry.Filetype, "bin") == 0) {
		filePath = "Scene";
	}

	filePath += id;

	if (entry.Filetype != nullptr) {
		filePath += "." + std::string(entry.Filetype);
	}

	parentPath += std::string(filePath);

	return parentPath;
}

std::string SceneInfo::GetName(int categoryID, int entryID) {
	if (!SceneInfo::IsEntryValid(categoryID, entryID)) {
		return "";
	}

	SceneListEntry& entry = Categories[categoryID].Entries[entryID];

	if (entry.Name == nullptr) {
		return "";
	}

	return std::string(entry.Name);
}

std::string SceneInfo::GetFolder(int categoryID, int entryID) {
	if (!SceneInfo::IsEntryValid(categoryID, entryID)) {
		return "";
	}

	SceneListEntry& entry = Categories[categoryID].Entries[entryID];

	if (entry.Folder == nullptr) {
		return "";
	}

	return std::string(entry.Folder);
}

std::string SceneInfo::GetID(int categoryID, int entryID) {
	if (!SceneInfo::IsEntryValid(categoryID, entryID)) {
		return "";
	}

	SceneListEntry& entry = Categories[categoryID].Entries[entryID];

	if (entry.ID == nullptr) {
		return "";
	}

	return std::string(entry.ID);
}

std::string SceneInfo::GetTileConfigFilename(int categoryID, int entryID) {
	return GetParentPath(categoryID, entryID) + "TileConfig.bin";
}

char* SceneInfo::GetEntryProperty(int categoryID, int entryID, char* property) {
	if (!SceneInfo::IsEntryValid(categoryID, entryID)) {
		return nullptr;
	}

	SceneListEntry& entry = Categories[categoryID].Entries[entryID];
	if (entry.Properties->Exists(property)) {
		return entry.Properties->Get(property);
	}

	return nullptr;
}
char* SceneInfo::GetCategoryProperty(int categoryID, char* property) {
	if (IsCategoryValid(categoryID)) {
		SceneListCategory& category = Categories[categoryID];
		if (category.Properties->Exists(property)) {
			return category.Properties->Get(property);
		}
	}
	return nullptr;
}

bool SceneInfo::HasEntryProperty(int categoryID, int entryID, char* property) {
	if (!SceneInfo::IsEntryValid(categoryID, entryID)) {
		return false;
	}

	SceneListEntry& entry = Categories[categoryID].Entries[entryID];
	return entry.Properties->Exists(property);
}
bool SceneInfo::HasCategoryProperty(int categoryID, char* property) {
	if (IsCategoryValid(categoryID)) {
		return Categories[categoryID].Properties->Exists(property);
	}
	return false;
}

void SceneInfo::FillAttributesHashMap(XMLAttributes* attr, HashMap<char*>* map) {
	for (size_t i = 0; i < attr->KeyVector.size(); i++) {
		char* key = attr->KeyVector[i];
		char* value = XMLParser::TokenToString(attr->ValueMap.Get(key));
		if (!map->Exists(key)) {
			map->Put(key, value);
		}
	}
}

SceneListEntry SceneInfo::ParseEntry(XMLNode* node, size_t id) {
	SceneListEntry entry;

	// Name
	if (node->attributes.Exists("name")) {
		entry.Name = XMLParser::TokenToString(node->attributes.Get("name"));
	}
	else {
		entry.Name = StringUtils::Duplicate("Unknown");
	}

	// Folder
	if (node->attributes.Exists("folder")) {
		entry.Folder = XMLParser::TokenToString(node->attributes.Get("folder"));
	}

	// Path
	if (node->attributes.Exists("path")) {
		entry.Path = XMLParser::TokenToString(node->attributes.Get("path"));
	}

	// Resource folder
	if (node->attributes.Exists("resourceFolder")) {
		entry.ResourceFolder =
			XMLParser::TokenToString(node->attributes.Get("resourceFolder"));
	}
	else if (entry.Folder) {
		entry.ResourceFolder = StringUtils::Duplicate(entry.Folder);
	}

	// Sprite folder (backwards compat)
	if (node->attributes.Exists("spriteFolder")) {
		entry.ResourceFolder =
			XMLParser::TokenToString(node->attributes.Get("spriteFolder"));
	}

	// Filetype
	if (node->attributes.Exists("fileExtension")) {
		entry.Filetype = XMLParser::TokenToString(node->attributes.Get("fileExtension"));
	}
	else if (node->attributes.Exists("type")) {
		entry.Filetype = XMLParser::TokenToString(node->attributes.Get("type"));
	}

	// ID
	if (node->attributes.Exists("id")) {
		entry.ID = XMLParser::TokenToString(node->attributes.Get("id"));
	}
	else if (strcmp(entry.Filetype, "bin") == 0) {
		// RSDK compatibility.
		char buf[16];
		snprintf(buf, sizeof(buf), "%d", ((int)id) + 1);
		entry.ID = StringUtils::Duplicate(buf);
	}
	else {
		entry.ID = StringUtils::Duplicate(entry.Name);
	}

	// Fill properties
	entry.Properties = new HashMap<char*>(NULL, 8);
	entry.Properties->Put("name", entry.Name);
	entry.Properties->Put("folder", entry.Folder);
	entry.Properties->Put("id", entry.ID);
	entry.Properties->Put("resourceFolder", entry.ResourceFolder);
	entry.Properties->Put("spriteFolder",
		StringUtils::Duplicate(entry.ResourceFolder)); // backwards compat
	entry.Properties->Put("fileExtension", entry.Filetype);

	FillAttributesHashMap(&node->attributes, entry.Properties);

	return entry;
}

bool SceneInfo::Load(XMLNode* node) {
	size_t numGlobalScenes = 0;

	for (size_t i = 0; i < node->children.size(); i++) {
		XMLNode* listElement = node->children[i];

		if (XMLParser::MatchToken(listElement->name, "category")) {
			SceneListCategory* category = nullptr;

			std::string categoryName = "";
			if (listElement->attributes.Exists("name")) {
				categoryName = XMLParser::TokenToStdString(
					listElement->attributes.Get("name"));
			}

			int categoryID = SceneInfo::GetCategoryID(categoryName.c_str());
			if (categoryID == -1) {
				if (categoryName == "") {
					char buf[32];
					snprintf(buf, sizeof(buf), "Category #%d", ((int)i) + 1);
					categoryName = std::string(buf);
				}

				category = SceneInfo::NewCategory(categoryName);
			}
			else {
				category = &Categories[categoryID];
			}

			// Fill properties
			FillAttributesHashMap(&listElement->attributes, category->Properties);

			for (size_t s = 0; s < listElement->children.size(); ++s) {
				XMLNode* node = listElement->children[s];
				if (XMLParser::MatchToken(node->name, "scene") ||
					XMLParser::MatchToken(node->name,
						"stage")) { // backwards
					// compat
					SceneListEntry entry = ParseEntry(node, s);

					category->Entries.push_back(entry);

					NumTotalScenes++;
				}
			}
		}
		else if (XMLParser::MatchToken(listElement->name, "scene")) {
			SceneListEntry entry = ParseEntry(listElement, numGlobalScenes);

			int globalID = GetCategoryID(SCENEINFO_GLOBAL_CATEGORY_NAME);
			if (globalID == -1) {
				globalID = InitGlobalCategory();
			}

			Categories[globalID].Entries.push_back(entry);

			NumTotalScenes++;

			numGlobalScenes++;
		}
	}

	return Categories.size() > 0;
}
