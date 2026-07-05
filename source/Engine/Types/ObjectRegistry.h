#ifndef ENGINE_TYPES_OBJECTREGISTRY_H
#define ENGINE_TYPES_OBJECTREGISTRY_H

#include <Engine/Includes/Standard.h>
#include <Engine/Types/Entity.h>

class ObjectRegistry {
public:
	vector<Entity*> List;

	void Add(Entity* obj);
	bool Contains(Entity* obj);
	void Remove(Entity* obj);
	void Clear();
	void Iterate(std::function<void(Entity* e)> func);
	void RemoveNonPersistentFromLinkedList(Entity* first, int persistence);
	void RemoveNonPersistentFromLinkedList(Entity* first);
	Entity* GetNth(int n);
	Entity* GetClosest(int x, int y);
	void Dispose();
	~ObjectRegistry();
	int Count();
};

#endif /* ENGINE_TYPES_OBJECTREGISTRY_H */
