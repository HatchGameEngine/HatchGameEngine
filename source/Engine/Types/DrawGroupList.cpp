#if INTERFACE

#include <Engine/Includes/Standard.h>

#include <Engine/Types/Entity.h>

class DrawGroupList {
public:
    int      EntityCount = 0;
    int      EntityCapacity = 0x1000;
    Entity** Entities = NULL;
    bool     NeedsSorting = false;
};
#endif

#include <Engine/Types/DrawGroupList.h>

#include <Engine/Application.h>
#include <Engine/Diagnostics/Memory.h>

PUBLIC         DrawGroupList::DrawGroupList() {
    Init();
}

// Double linked-list functions
PUBLIC int    DrawGroupList::Add(Entity* obj) {
    for (int i = 0, iSz = EntityCapacity; i < iSz; i++) {
        if (Entities[i] == NULL) {
            Entities[i] = obj;
            EntityCount++;
            NeedsSorting = true;
            return i;
        }
    }
    return -1;
}
PUBLIC void    DrawGroupList::Remove(Entity* obj) {
    for (int i = 0, iSz = EntityCapacity; i < iSz; i++) {
        if (Entities[i] == obj) {
            Entities[i] = NULL;
            EntityCount--;
            NeedsSorting = true;
            return;
        }
    }
}
PUBLIC void    DrawGroupList::Clear() {
    for (int i = 0, iSz = EntityCapacity; i < iSz; i++) {
        Entities[i] = NULL;
    }
    EntityCount = 0;
    NeedsSorting = false;
}

PRIVATE STATIC int DrawGroupList::SortFunc(const void *a, const void *b) {
    const Entity* entA = *(const Entity **)a;
    const Entity* entB = *(const Entity **)b;
    if (entA == NULL && entB == NULL) return 0;
    else if (entA == NULL) return 1;
    else if (entB == NULL) return -1;
    return entA->Depth - entB->Depth;
}
PUBLIC void    DrawGroupList::Sort() {
    qsort(Entities, EntityCapacity, sizeof(Entity*), DrawGroupList::SortFunc);
    NeedsSorting = false;
}

PUBLIC void    DrawGroupList::Init() {
    EntityCapacity = 0x1000;
    Entities = (Entity**)Memory::TrackedCalloc("DrawGroupList", sizeof(Entity*), EntityCapacity);
    EntityCount = 0;
}
PUBLIC void    DrawGroupList::Dispose() {
    Memory::Free(Entities);
}
PUBLIC         DrawGroupList::~DrawGroupList() {
    // Dispose();
}

PUBLIC int     DrawGroupList::Count() {
    return EntityCount;
}
