#ifndef ENGINE_TYPES_OBJECTLIST_H
#define ENGINE_TYPES_OBJECTLIST_H

#include <Engine/Includes/Standard.h>
#include <Engine/Types/Entity.h>

class ObjectList {
public:
	int EntityCount = 0;
	int Activity = ACTIVE_NORMAL;
	Entity* EntityFirst = nullptr;
	Entity* EntityLast = nullptr;
	char* ObjectName;
	char* LoadFunctionName;
	char* GlobalUpdateFunctionName;
	ObjectListPerformance Performance;
	Entity* (*SpawnFunction)(ObjectList*) = nullptr;

	ObjectList(const char* name);
	~ObjectList();
	void Add(Entity* obj);
	bool Contains(Entity* obj);
	void Remove(Entity* obj);
	void Clear();
	Entity* Spawn();
	void Iterate(std::function<void(Entity* e)> func);
	void RemoveNonPersistentFromLinkedList(Entity* first, int persistence);
	void RemoveNonPersistentFromLinkedList(Entity* first);
	void ResetPerf();
	Entity* GetNth(int n);
	Entity* GetClosest(int x, int y);
	int Count();
};

#endif /* ENGINE_TYPES_OBJECTLIST_H */
