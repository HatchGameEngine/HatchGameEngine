#if INTERFACE

#include <Engine/Includes/Standard.h>

#include <Engine/Types/Entity.h>

class ObjectList {
public:
    int     EntityCount = 0;
    Entity* EntityFirst = NULL;
    Entity* EntityLast = NULL;

    bool            Registry = false;
    vector<Entity*> List;

    char ObjectName[256];
    double AverageUpdateEarlyTime = 0.0;
    double AverageUpdateTime = 0.0;
    double AverageUpdateLateTime = 0.0;
    double AverageUpdateEarlyItemCount = 0;
    double AverageUpdateItemCount = 0;
    double AverageUpdateLateItemCount = 0;
    double AverageRenderTime = 0.0;
    double AverageRenderItemCount = 0;

    Entity* (*SpawnFunction)() = NULL;
};
#endif

#include <Engine/Types/ObjectList.h>

#include <Engine/Application.h>
#include <Engine/Diagnostics/Memory.h>

// Double linked-list functions
PUBLIC void    ObjectList::Add(Entity* obj) {
    if (Registry) {
        List.push_back(obj);
        EntityCount++;
        return;
    }

    // Set "prev" of obj to last
    obj->PrevEntityInList = EntityLast;
    obj->NextEntityInList = NULL;

    // If the last exists, set that ones "next" to obj
    if (EntityLast)
        EntityLast->NextEntityInList = obj;

    // Set obj as the first if there is not one
    if (!EntityFirst)
        EntityFirst = obj;

    EntityLast = obj;

    EntityCount++;
}
PUBLIC bool    ObjectList::Contains(Entity* obj) {
    if (Registry)
        return std::find(List.begin(), List.end(), obj) != List.end();

    for (Entity* search = EntityFirst; search != NULL; search = search->NextEntityInList) {
        if (search == obj)
            return true;
    }

    return false;
}
PUBLIC void    ObjectList::Remove(Entity* obj) {
    if (obj == NULL) return;

    if (Registry) {
        for (int i = 0, iSz = (int)List.size(); i < iSz; i++) {
            if (List[i] == obj) {
                List.erase(List.begin() + i);
                EntityCount--;
                break;
            }
        }
        return;
    }

    obj->List = NULL;

    if (EntityFirst == obj)
        EntityFirst = obj->NextEntityInList;

    if (EntityLast == obj)
        EntityLast = obj->PrevEntityInList;

    if (obj->PrevEntityInList)
        obj->PrevEntityInList->NextEntityInList = obj->NextEntityInList;
    obj->PrevEntityInList = NULL;

    if (obj->NextEntityInList)
        obj->NextEntityInList->PrevEntityInList = obj->PrevEntityInList;
    obj->NextEntityInList = NULL;

    EntityCount--;
}
PUBLIC void    ObjectList::Clear() {
    EntityCount = 0;

    if (Registry) {
        List.clear();
        return;
    }

    EntityFirst = NULL;
    EntityLast = NULL;

    AverageUpdateTime = 0.0;
    AverageUpdateItemCount = 0;
    AverageRenderTime = 0.0;
    AverageRenderItemCount = 0;
}

// ObjectList functions
PRIVATE void ObjectList::Iterate(std::function<void(Entity* e)> func) {
    if (Registry) {
        std::for_each(List.begin(), List.end(), func);
        return;
    }

    for (Entity* ent = EntityFirst; ent != NULL; ent = ent->NextEntityInList)
        func(ent);
}
PRIVATE void ObjectList::IterateLinkedList(Entity* first, std::function<void(Entity* e)> func) {
    for (Entity* ent = first, *next; ent; ent = next) {
        // Store the "next" so that when/if the current is removed,
        // it can still be used to point at the end of the loop.
        next = ent->NextEntity;

        if (ent->List == this)
            func(ent);
    }
}
PUBLIC void ObjectList::RemoveNonPersistentFromLinkedList(Entity* first) {
    IterateLinkedList(first, [this](Entity* ent) -> void {
        if (!ent->Persistent)
            Remove(ent);
    });
}
PUBLIC void ObjectList::ResetPerf() {
    // AverageUpdateTime = 0.0;
    AverageUpdateItemCount = 0.0;
    AverageUpdateEarlyItemCount = 0.0;
    AverageUpdateLateItemCount = 0.0;
    AverageRenderItemCount = 0.0;
}
PUBLIC Entity* ObjectList::GetNth(int n) {
    if (Registry) {
        if (n < 0 || n >= EntityCount)
            return NULL;
        return List[n];
    }

    Entity* ent = EntityFirst;
    for (ent = EntityFirst; ent != NULL && n > 0; ent = ent->NextEntityInList, n--);
    return ent;
}
PUBLIC Entity* ObjectList::GetClosest(int x, int y) {
    if (!EntityCount)
        return NULL;
    else if (EntityCount == 1) {
        if (Registry)
            return List[0];
        return EntityFirst;
    }

    Entity* closest = NULL;
    int smallestDistance = 0x7FFFFFFF;

    Iterate([x, y, &closest, &smallestDistance](Entity* ent) -> void {
        int xD = ent->X - x; xD *= xD;
        int yD = ent->Y - y; yD *= yD;
        if (smallestDistance > xD + yD) {
            smallestDistance = xD + yD;
            closest = ent;
        }
    });

    return closest;
}

PUBLIC void    ObjectList::Dispose() {
    List.clear();
    List.shrink_to_fit();
}
PUBLIC         ObjectList::~ObjectList() {
    Dispose();
}

PUBLIC int     ObjectList::Count() {
    return EntityCount;
}
