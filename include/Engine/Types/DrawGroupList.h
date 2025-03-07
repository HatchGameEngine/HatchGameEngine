#ifndef ENGINE_TYPES_DRAWGROUPLIST_H
#define ENGINE_TYPES_DRAWGROUPLIST_H

#include <Engine/Includes/Standard.h>
#include <Engine/Types/Entity.h>

class DrawGroupList {
public:
	vector<Entity*>* Entities = nullptr;
	bool EntityDepthSortingEnabled = false;
	bool NeedsSorting = false;

	DrawGroupList();
	int Add(Entity* obj);
	bool Contains(Entity* obj);
	int GetEntityIndex(Entity* obj);
	void Remove(Entity* obj);
	void Clear();
	void Sort();
	void Init();
	void Dispose();
	~DrawGroupList();
	int Count();
};

#endif /* ENGINE_TYPES_DRAWGROUPLIST_H */
