#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Scene/SceneConfig.h>
#include <Engine/TextFormats/XML/XMLParser.h>
#include <Engine/TextFormats/XML/XMLNode.h>

class SceneInfo {
public:
    static vector<SceneListEntry>    Entries;
    static vector<SceneListCategory> Categories;
};
#endif

#include <Engine/Scene/SceneInfo.h>
#include <Engine/Utilities/StringUtils.h>

vector<SceneListEntry>    SceneInfo::Entries;
vector<SceneListCategory> SceneInfo::Categories;

PUBLIC STATIC void SceneInfo::Dispose() {
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

PUBLIC STATIC bool SceneInfo::IsCategoryValid(int categoryID) {
    return categoryID >= 0 && categoryID < (int)Categories.size();
}

PUBLIC STATIC bool SceneInfo::IsEntryValid(int entryID) {
    return entryID >= 0 && entryID < (int)Entries.size();
}

PUBLIC STATIC bool SceneInfo::IsEntryValidInCategory(size_t categoryID, size_t entryID) {
    if (!IsCategoryValid((int)categoryID))
        return false;

    return entryID >= Categories[categoryID].OffsetStart && entryID < Categories[categoryID].OffsetEnd;
}

PUBLIC STATIC int SceneInfo::GetCategoryID(const char* categoryName) {
    for (size_t i = 0; i < Categories.size(); i++) {
        if (!strcmp(Categories[i].Name, categoryName))
            return i;
    }

    return -1;
}

PUBLIC STATIC int SceneInfo::GetEntryID(const char* categoryName, const char* entryName) {
    int categoryID = GetCategoryID(categoryName);
    if (categoryID < 0)
        return -1;

    SceneListCategory& category = Categories[categoryID];
    return GetEntryIDWithinRange(category.OffsetStart, category.OffsetEnd, entryName);
}

PUBLIC STATIC int SceneInfo::GetEntryID(const char* categoryName, size_t entryID) {
    int categoryID = GetCategoryID(categoryName);
    if (categoryID < 0)
        return -1;

    return GetEntryID((size_t)categoryID, (size_t)entryID);
}

PUBLIC STATIC int SceneInfo::GetEntryID(size_t categoryID, size_t entryID) {
    if (!SceneInfo::IsCategoryValid((int)categoryID))
        return -1;

    SceneListCategory& category = Categories[categoryID];
    size_t actualEntryID = category.OffsetStart + entryID;
    if (!SceneInfo::IsEntryValidInCategory(categoryID, actualEntryID))
        return -1;

    return (int)actualEntryID;
}

PUBLIC STATIC int SceneInfo::GetEntryPosInCategory(const char *categoryName, const char* entryName) {
    int categoryID = GetCategoryID(categoryName);
    if (categoryID < 0)
        return -1;

    return GetEntryPosInCategory((size_t)categoryID, entryName);
}

PUBLIC STATIC int SceneInfo::GetEntryPosInCategory(size_t categoryID, const char* entryName) {
    if (!SceneInfo::IsCategoryValid((int)categoryID))
        return -1;

    SceneListCategory& category = Categories[categoryID];
    int actualEntryID = GetEntryIDWithinRange(category.OffsetStart, category.OffsetEnd, entryName);
    if (actualEntryID < 0)
        return -1;

    return actualEntryID - (int)category.OffsetStart;
}

PUBLIC STATIC int SceneInfo::GetEntryIDWithinRange(size_t start, size_t end, const char* entryName) {
    for (size_t i = start; i < end; i++) {
        if (!strcmp(Entries[i].Name, entryName))
            return (int)i;
    }

    return -1;
}

PUBLIC STATIC string SceneInfo::GetParentPath(int entryID) {
    SceneListEntry& entry = Entries[entryID];

    char filePath[4096];
    if (!strcmp(entry.Filetype, "bin")) {
        snprintf(filePath, sizeof(filePath), "Stages/%s/", entry.Folder);
    }
    else {
        if (entry.Folder == nullptr)
            snprintf(filePath, sizeof(filePath), "Scenes/");
        else
            snprintf(filePath, sizeof(filePath), "Scenes/%s/", entry.Folder);
    }

    return std::string(filePath);
}

PUBLIC STATIC string SceneInfo::GetFilename(int entryID) {
    SceneListEntry scene = Entries[entryID];

    std::string parentPath = GetParentPath(entryID);

    const char* id = scene.ID != nullptr ? scene.ID : scene.Name;

    char filePath[4096];
    if (!strcmp(scene.Filetype, "bin")) {
        if (scene.Folder == nullptr) {
            if (scene.Filetype == nullptr)
                snprintf(filePath, sizeof(filePath), "Scene%s", id);
            else
                snprintf(filePath, sizeof(filePath), "Scene%s.%s", id, scene.Filetype);
        }
        else if (scene.Filetype == nullptr)
            snprintf(filePath, sizeof(filePath), "%s/Scene%s", scene.Folder, id);
        else
            snprintf(filePath, sizeof(filePath), "%s/Scene%s.%s", scene.Folder, id, scene.Filetype);
    }
    else {
        if (scene.Folder == nullptr) {
            if (scene.Filetype == nullptr)
                snprintf(filePath, sizeof(filePath), "%s", id);
            else
                snprintf(filePath, sizeof(filePath), "%s.%s", id, scene.Filetype);
        }
        else if (scene.Filetype == nullptr)
            snprintf(filePath, sizeof(filePath), "%s/%s", scene.Folder, id);
        else
            snprintf(filePath, sizeof(filePath), "%s/%s.%s", scene.Folder, id, scene.Filetype);
    }

    parentPath += std::string(filePath);

    return parentPath;
}

PUBLIC STATIC string SceneInfo::GetTileConfigFilename(int entryID) {
    return GetParentPath(entryID) + "TileConfig.bin";
}

PUBLIC STATIC char* SceneInfo::GetEntryProperty(int entryID, char* property) {
    if (IsEntryValid(entryID)) {
        SceneListEntry& entry = Entries[entryID];
        if (entry.Properties->Exists(property))
            return entry.Properties->Get(property);
    }
    return nullptr;
}
PUBLIC STATIC char* SceneInfo::GetCategoryProperty(int categoryID, char* property) {
    if (IsCategoryValid(categoryID)) {
        SceneListCategory& category = Categories[categoryID];
        if (category.Properties->Exists(property))
            return category.Properties->Get(property);
    }
    return nullptr;
}

PUBLIC STATIC bool SceneInfo::HasEntryProperty(int entryID, char* property) {
    if (IsEntryValid(entryID))
        return Entries[entryID].Properties->Exists(property);
    return false;
}
PUBLIC STATIC bool SceneInfo::HasCategoryProperty(int categoryID, char* property) {
    if (IsCategoryValid(categoryID))
        return Categories[categoryID].Properties->Exists(property);
    return false;
}

PRIVATE STATIC void SceneInfo::FillAttributesHashMap(XMLAttributes* attr, HashMap<char*>* map) {
    for (size_t i = 0; i < attr->KeyVector.size(); i++) {
        char *key = attr->KeyVector[i];
        char *value = XMLParser::TokenToString(attr->ValueMap.Get(key));
        if (!map->Exists(key))
            map->Put(key, value);
    }
}

PUBLIC STATIC bool SceneInfo::Load(XMLNode* node) {
    for (size_t i = 0; i < node->children.size(); ++i) {
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

                    // Sprite folder
                    if (stgElement->attributes.Exists("spriteFolder"))
                        entry.SpriteFolder = XMLParser::TokenToString(stgElement->attributes.Get("spriteFolder"));

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
                    entry.Properties->Put("spriteFolder", entry.SpriteFolder);
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
