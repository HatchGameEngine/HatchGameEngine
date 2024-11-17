#include <Engine/Scene/SceneInfo.h>
#include <Engine/Utilities/StringUtils.h>

vector<SceneListEntry>    SceneInfo::Entries;
vector<SceneListCategory> SceneInfo::Categories;

void SceneInfo::Dispose() {
    for (size_t i = 0; i < Categories.size(); i++) {
        Categories[i].Properties->WithAll([](Uint32 hash, char* string) -> void {
            Memory::Free(string);
        });
        delete Categories[i].Properties;
    }

    for (size_t i = 0; i < Entries.size(); i++) {
        Entries[i].Properties->WithAll([](Uint32 hash, char* string) -> void {
            Memory::Free(string);
        });
        delete Entries[i].Properties;
    }

    Categories.clear();
    Categories.shrink_to_fit();

    Entries.clear();
    Entries.shrink_to_fit();
}

bool SceneInfo::IsCategoryValid(int categoryID) {
    return categoryID >= 0 && categoryID < (int)Categories.size();
}

bool SceneInfo::IsEntryValid(int entryID) {
    return entryID >= 0 && entryID < (int)Entries.size();
}

bool SceneInfo::IsEntryValidInCategory(size_t categoryID, size_t entryID) {
    if (!IsCategoryValid((int)categoryID))
        return false;

    return entryID >= Categories[categoryID].OffsetStart && entryID < Categories[categoryID].OffsetEnd;
}

int SceneInfo::GetCategoryID(const char* categoryName) {
    for (size_t i = 0; i < Categories.size(); i++) {
        if (!strcmp(Categories[i].Name, categoryName))
            return i;
    }

    return -1;
}

int SceneInfo::GetEntryID(const char* categoryName, const char* entryName) {
    int categoryID = GetCategoryID(categoryName);
    if (categoryID < 0)
        return -1;

    SceneListCategory& category = Categories[categoryID];
    return GetEntryIDWithinRange(category.OffsetStart, category.OffsetEnd, entryName);
}

int SceneInfo::GetEntryID(const char* categoryName, size_t entryID) {
    int categoryID = GetCategoryID(categoryName);
    if (categoryID < 0)
        return -1;

    return GetEntryID((size_t)categoryID, (size_t)entryID);
}

int SceneInfo::GetEntryID(size_t categoryID, size_t entryID) {
    if (!SceneInfo::IsCategoryValid((int)categoryID))
        return -1;

    SceneListCategory& category = Categories[categoryID];
    size_t actualEntryID = category.OffsetStart + entryID;
    if (!SceneInfo::IsEntryValidInCategory(categoryID, actualEntryID))
        return -1;

    return (int)actualEntryID;
}

int SceneInfo::GetEntryPosInCategory(const char *categoryName, const char* entryName) {
    int categoryID = GetCategoryID(categoryName);
    if (categoryID < 0)
        return -1;

    return GetEntryPosInCategory((size_t)categoryID, entryName);
}

int SceneInfo::GetEntryPosInCategory(size_t categoryID, const char* entryName) {
    if (!SceneInfo::IsCategoryValid((int)categoryID))
        return -1;

    SceneListCategory& category = Categories[categoryID];
    int actualEntryID = GetEntryIDWithinRange(category.OffsetStart, category.OffsetEnd, entryName);
    if (actualEntryID < 0)
        return -1;

    return actualEntryID - (int)category.OffsetStart;
}

int SceneInfo::GetEntryIDWithinRange(size_t start, size_t end, const char* entryName) {
    for (size_t i = start; i < end; i++) {
        if (!strcmp(Entries[i].Name, entryName))
            return (int)i;
    }

    return -1;
}

string SceneInfo::GetParentPath(int entryID) {
    SceneListEntry& entry = Entries[entryID];

    std::string filePath;

    if (entry.Filetype != nullptr && strcmp(entry.Filetype, "bin") == 0) {
        filePath += "Stages/";
    }
    else {
        filePath += "Scenes/";
    }

    if (entry.Folder != nullptr)
        filePath += std::string(entry.Folder) + "/";

    return filePath;
}

string SceneInfo::GetFilename(int entryID) {
    SceneListEntry scene = Entries[entryID];

    std::string parentPath = GetParentPath(entryID);

    std::string id = scene.ID != nullptr ? std::string(scene.ID) : std::string(scene.Name);

    std::string filePath = "";

    // RSDK compatibility.
    if (scene.Filetype != nullptr && strcmp(scene.Filetype, "bin") == 0) {
        filePath = "Scene";
    }

    filePath += id;

    if (scene.Filetype != nullptr)
        filePath += "." + std::string(scene.Filetype);

    parentPath += std::string(filePath);

    return parentPath;
}

string SceneInfo::GetTileConfigFilename(int entryID) {
    return GetParentPath(entryID) + "TileConfig.bin";
}

char* SceneInfo::GetEntryProperty(int entryID, char* property) {
    if (IsEntryValid(entryID)) {
        SceneListEntry& entry = Entries[entryID];
        if (entry.Properties->Exists(property))
            return entry.Properties->Get(property);
    }
    return nullptr;
}
char* SceneInfo::GetCategoryProperty(int categoryID, char* property) {
    if (IsCategoryValid(categoryID)) {
        SceneListCategory& category = Categories[categoryID];
        if (category.Properties->Exists(property))
            return category.Properties->Get(property);
    }
    return nullptr;
}

bool SceneInfo::HasEntryProperty(int entryID, char* property) {
    if (IsEntryValid(entryID))
        return Entries[entryID].Properties->Exists(property);
    return false;
}
bool SceneInfo::HasCategoryProperty(int categoryID, char* property) {
    if (IsCategoryValid(categoryID))
        return Categories[categoryID].Properties->Exists(property);
    return false;
}

void SceneInfo::FillAttributesHashMap(XMLAttributes* attr, HashMap<char*>* map) {
    for (size_t i = 0; i < attr->KeyVector.size(); i++) {
        char *key = attr->KeyVector[i];
        char *value = XMLParser::TokenToString(attr->ValueMap.Get(key));
        if (!map->Exists(key))
            map->Put(key, value);
    }
}

bool SceneInfo::Load(XMLNode* node) {
    for (size_t i = 0; i < node->children.size(); i++) {
        XMLNode* listElement = node->children[i];
        if (XMLParser::MatchToken(listElement->name, "category")) {
            SceneListCategory category;

            category.OffsetStart = Entries.size();
            category.Count = 0;

            if (listElement->attributes.Exists("name"))
                category.Name = XMLParser::TokenToString(listElement->attributes.Get("name"));
            else {
                char buf[32];
                snprintf(buf, sizeof(buf), "Unknown category #%d", ((int)i) + 1);
                category.Name = StringUtils::Duplicate(buf);
            }

            // Fill properties
            category.Properties = new HashMap<char*>(NULL, 8);
            category.Properties->Put("name", category.Name);

            FillAttributesHashMap(&listElement->attributes, category.Properties);

            for (size_t s = 0; s < listElement->children.size(); ++s) {
                XMLNode* stgElement = listElement->children[s];
                if (XMLParser::MatchToken(stgElement->name, "stage")) {
                    SceneListEntry entry;

                    // Name
                    if (stgElement->attributes.Exists("name"))
                        entry.Name = XMLParser::TokenToString(stgElement->attributes.Get("name"));
                    else
                        entry.Name = StringUtils::Duplicate("Unknown");

                    // Folder
                    if (stgElement->attributes.Exists("folder"))
                        entry.Folder = XMLParser::TokenToString(stgElement->attributes.Get("folder"));

                    // ID
                    if (stgElement->attributes.Exists("id"))
                        entry.ID = XMLParser::TokenToString(stgElement->attributes.Get("id"));
                    else {
                        char buf[16];
                        snprintf(buf, sizeof(buf), "%d", ((int)s) + 1);
                        entry.ID = StringUtils::Duplicate(buf);
                    }

                    // Resource folder
                    if (stgElement->attributes.Exists("resourceFolder"))
                        entry.ResourceFolder = XMLParser::TokenToString(stgElement->attributes.Get("resourceFolder"));

                    // Sprite folder (backwards compat)
                    if (stgElement->attributes.Exists("spriteFolder"))
                        entry.ResourceFolder = XMLParser::TokenToString(stgElement->attributes.Get("spriteFolder"));

                    // Filetype
                    if (stgElement->attributes.Exists("fileExtension"))
                        entry.Filetype = XMLParser::TokenToString(stgElement->attributes.Get("fileExtension"));
                    else if (stgElement->attributes.Exists("type"))
                        entry.Filetype = XMLParser::TokenToString(stgElement->attributes.Get("type"));

                    entry.ParentCategoryID = Categories.size();
                    entry.CategoryPos = category.Count;

                    // Fill properties
                    entry.Properties = new HashMap<char*>(NULL, 8);
                    entry.Properties->Put("name", entry.Name);
                    entry.Properties->Put("folder", entry.Folder);
                    entry.Properties->Put("id", entry.ID);
                    entry.Properties->Put("resourceFolder", entry.ResourceFolder);
                    entry.Properties->Put("spriteFolder", entry.ResourceFolder); // backwards compat
                    entry.Properties->Put("fileExtension", entry.Filetype);

                    FillAttributesHashMap(&stgElement->attributes, entry.Properties);

                    Entries.push_back(entry);
                    category.Count++;
                }

                category.OffsetEnd = category.OffsetStart + category.Count;
            }

            Categories.push_back(category);
        }
    }

    return Categories.size() > 0;
}
