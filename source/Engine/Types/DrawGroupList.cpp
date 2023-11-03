#if INTERFACE

#include <Engine/Includes/Standard.h>

#include <Engine/Types/Entity.h>

class DrawGroupList {
public:
    vector<Entity*>* Entities = nullptr;
    bool             EntityDepthSortingEnabled = false;
    bool             NeedsSorting = false;
};
#endif

#include <Engine/Types/DrawGroupList.h>

#include <Engine/Application.h>

PUBLIC         DrawGroupList::DrawGroupList() {
    Init();
}

// Double linked-list functions
PUBLIC int    DrawGroupList::Add(Entity* obj) {
    Entities->push_back(obj);
    if (EntityDepthSortingEnabled)
        NeedsSorting = true;
    return Entities->size() - 1;
}
PUBLIC int    DrawGroupList::Contains(Entity* obj) {
    for (size_t i = 0, iSz = Entities->size(); i < iSz; i++) {
        if ((*Entities)[i] == obj)
            return (int)i;
    }
    return -1;
}
PUBLIC void    DrawGroupList::Remove(Entity* obj) {
    for (size_t i = 0, iSz = Entities->size(); i < iSz; i++) {
        if ((*Entities)[i] == obj) {
            Entities->erase(Entities->begin() + i);
            return;
        }
    }
}
PUBLIC void    DrawGroupList::Clear() {
    Entities->clear();
    NeedsSorting = false;
}

PUBLIC void    DrawGroupList::Sort() {
    std::stable_sort(Entities->begin(), Entities->end(), [](const Entity* entA, const Entity* entB) {
        return entA->Depth < entB->Depth;
    });
    NeedsSorting = false;
}

PUBLIC void    DrawGroupList::Init() {
    Entities = new vector<Entity*>();
}
PUBLIC void    DrawGroupList::Dispose() {
    delete Entities;
    Entities = nullptr;
}
PUBLIC         DrawGroupList::~DrawGroupList() {
    // Dispose();
}

PUBLIC int     DrawGroupList::Count() {
    return Entities->size();
}
