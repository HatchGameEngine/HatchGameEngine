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

PUBLIC STATIC void SceneInfo::Init() {
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
    SceneListEntry scene = Entries[entryID];

    char filePath[4096];
    if (!strcmp(scene.Filetype, "bin")) {
        snprintf(filePath, sizeof(filePath), "Stages/%s/", scene.Folder);
    }
    else {
        if (scene.Folder[0] == '\0')
            snprintf(filePath, sizeof(filePath), "Scenes/");
        else
            snprintf(filePath, sizeof(filePath), "Scenes/%s/", scene.Folder);
    }

    return std::string(filePath);
}

PUBLIC STATIC string SceneInfo::GetFilename(int entryID) {
    SceneListEntry scene = Entries[entryID];

    std::string parentPath = GetParentPath(entryID);

    const char* id = scene.ID[0] ? scene.ID : scene.Name;

    char filePath[4096];
    if (!strcmp(scene.Filetype, "bin")) {
        snprintf(filePath, sizeof(filePath), "Scene%s.%s", scene.Folder, scene.ID, scene.Filetype);
    }
    else {
        if (scene.Folder[0] == '\0') {
            if (scene.Filetype[0] == '\0')
                snprintf(filePath, sizeof(filePath), "%s", id);
            else
                snprintf(filePath, sizeof(filePath), "%s.%s", id, scene.Filetype);
        }
        else
            snprintf(filePath, sizeof(filePath), "%s/%s.%s", scene.Folder, id, scene.Filetype);
    }

    parentPath += std::string(filePath);

    return parentPath;
}

PUBLIC STATIC string SceneInfo::GetTileConfigFilename(int entryID) {
    return GetParentPath(entryID) + "TileConfig.bin";
}

PUBLIC STATIC bool SceneInfo::Load(XMLNode* node) {
    for (size_t i = 0; i < node->children.size(); ++i) {
        XMLNode* listElement = node->children[i];
        if (XMLParser::MatchToken(listElement->name, "category")) {
            SceneListCategory category;
            if (listElement->attributes.Exists("name"))
                XMLParser::CopyTokenToString(listElement->attributes.Get("name"), category.Name, sizeof(category.Name));
            else
                snprintf(category.Name, sizeof(category.Name), "unknown list %d", (int)i);

            category.OffsetStart = Entries.size();
            category.Count = 0;

            for (size_t s = 0; s < listElement->children.size(); ++s) {
                XMLNode* stgElement = listElement->children[s];
                if (XMLParser::MatchToken(stgElement->name, "stage")) {
                    SceneListEntry scene;
                    if (stgElement->attributes.Exists("name"))
                        XMLParser::CopyTokenToString(stgElement->attributes.Get("name"), scene.Name, sizeof(scene.Name));
                    else
                        snprintf(scene.Name, sizeof(scene.Name), "Unknown");

                    if (stgElement->attributes.Exists("folder"))
                        XMLParser::CopyTokenToString(stgElement->attributes.Get("folder"), scene.Folder, sizeof(scene.Folder));
                    else {
                        // Accounts for scenes placed in the root of the Scenes folder if the file type is not "bin"
                        scene.Folder[0] = '\0';
                    }

                    if (stgElement->attributes.Exists("id"))
                        XMLParser::CopyTokenToString(stgElement->attributes.Get("id"), scene.ID, sizeof(scene.ID));
                    else
                        snprintf(scene.Name, sizeof(scene.Name), "%d", (int)s);

                    if (stgElement->attributes.Exists("spriteFolder"))
                        XMLParser::CopyTokenToString(stgElement->attributes.Get("spriteFolder"), scene.SpriteFolder, sizeof(scene.SpriteFolder));
                    else
                        scene.SpriteFolder[0] = '\0';

                    if (stgElement->attributes.Exists("type"))
                        XMLParser::CopyTokenToString(stgElement->attributes.Get("type"), scene.Filetype, sizeof(scene.Filetype));
                    else
                        scene.Filetype[0] = '\0';

                    scene.ParentCategoryID = Categories.size();
                    scene.CategoryPos = category.Count;

                    Entries.push_back(scene);
                    category.Count++;
                }

                category.OffsetEnd = category.OffsetStart + category.Count;
            }

            Categories.push_back(category);
        }
    }

    return Categories.size() > 0;
}
