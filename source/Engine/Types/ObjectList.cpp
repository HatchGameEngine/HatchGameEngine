#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Types/Entity.h>

class ObjectList {
public:
    int     EntityCount = 0;
    Entity* EntityFirst = NULL;
    Entity* EntityLast = NULL;

    char ObjectName[256];
    char LoadFunctionName[256 + 5];
    char GlobalUpdateFunctionName[256 + 13];
    double AverageUpdateEarlyTime = 0.0;
    double AverageUpdateTime = 0.0;
    double AverageUpdateLateTime = 0.0;
    double AverageUpdateEarlyItemCount = 0;
    double AverageUpdateItemCount = 0;
    double AverageUpdateLateItemCount = 0;
    double AverageRenderTime = 0.0;
    double AverageRenderItemCount = 0;

    Entity* (*SpawnFunction)(const char*) = NULL;
};
#endif

#include <Engine/Types/ObjectList.h>

#include <Engine/Application.h>

PUBLIC         ObjectList::ObjectList(const char* name) {
    StringUtils::Copy(ObjectName, name, sizeof(ObjectName));
    snprintf(LoadFunctionName, sizeof LoadFunctionName, "%s_Load", ObjectName);
    snprintf(GlobalUpdateFunctionName, sizeof GlobalUpdateFunctionName, "%s_GlobalUpdate", ObjectName);
}

// Double linked-list functions
PUBLIC void    ObjectList::Add(Entity* obj) {
    // Set "prev" of obj to last
    obj->PrevEntityInList = EntityLast;
    obj->NextEntityInList = NULL;

    // If the last exists, set that one's "next" to obj
    if (EntityLast)
        EntityLast->NextEntityInList = obj;

    // Set obj as the first if there is not one
    if (!EntityFirst)
        EntityFirst = obj;

    EntityLast = obj;

    EntityCount++;
}
PUBLIC bool    ObjectList::Contains(Entity* obj) {
    for (Entity* search = EntityFirst; search != NULL; search = search->NextEntityInList) {
        if (search == obj)
            return true;
    }

    return false;
}
PUBLIC void    ObjectList::Remove(Entity* obj) {
    if (obj == NULL) return;

    obj->List = NULL;

    if (EntityFirst == obj)
        EntityFirst = obj->NextEntityInList;

    if (EntityLast == obj)
        EntityLast = obj->PrevEntityInList;

    if (obj->PrevEntityInList)
        obj->PrevEntityInList->NextEntityInList = obj->NextEntityInList;

    if (obj->NextEntityInList)
        obj->NextEntityInList->PrevEntityInList = obj->PrevEntityInList;

    obj->PrevEntityInList = NULL;
    obj->NextEntityInList = NULL;

    EntityCount--;
}
PUBLIC void    ObjectList::Clear() {
    EntityCount = 0;
    EntityFirst = NULL;
    EntityLast = NULL;

    AverageUpdateTime = 0.0;
    AverageUpdateItemCount = 0;
    AverageRenderTime = 0.0;
    AverageRenderItemCount = 0;
}

// ObjectList functions
PUBLIC Entity* ObjectList::Spawn() {
    return SpawnFunction(ObjectName);
}
PUBLIC void ObjectList::Iterate(std::function<void(Entity* e)> func) {
    for (Entity* ent = EntityFirst; ent != NULL; ent = ent->NextEntityInList)
        func(ent);
}
PUBLIC void ObjectList::RemoveNonPersistentFromLinkedList(Entity* first, int persistence) {
    for (Entity* ent = first, *next; ent; ent = next) {
        // Store the "next" so that when/if the current is removed,
        // it can still be used to point at the end of the loop.
        next = ent->NextEntity;

        if (ent->Persistence <= persistence && ent->List == this)
            Remove(ent);
    }
}
PUBLIC void ObjectList::RemoveNonPersistentFromLinkedList(Entity* first) {
    RemoveNonPersistentFromLinkedList(first, Persistence_NONE);
}
PUBLIC void ObjectList::ResetPerf() {
    // AverageUpdateTime = 0.0;
    AverageUpdateItemCount = 0.0;
    AverageUpdateEarlyItemCount = 0.0;
    AverageUpdateLateItemCount = 0.0;
    AverageRenderItemCount = 0.0;
}
PUBLIC Entity* ObjectList::GetNth(int n) {
    Entity* ent = EntityFirst;
    for (ent = EntityFirst; ent != NULL && n > 0; ent = ent->NextEntityInList, n--);
    return ent;
}
PUBLIC Entity* ObjectList::GetClosest(int x, int y) {
    if (!EntityCount)
        return NULL;
    else if (EntityCount == 1)
        return EntityFirst;

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

PUBLIC int     ObjectList::Count() {
    return EntityCount;
}
