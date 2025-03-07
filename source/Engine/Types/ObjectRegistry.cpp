#include <Engine/Types/ObjectRegistry.h>

#include <Engine/Application.h>

void ObjectRegistry::Add(Entity* obj) {
	if (!Contains(obj)) {
		List.push_back(obj);
	}
}
bool ObjectRegistry::Contains(Entity* obj) {
	return std::find(List.begin(), List.end(), obj) != List.end();
}
void ObjectRegistry::Remove(Entity* obj) {
	if (obj == NULL) {
		return;
	}

	auto it = std::find(List.begin(), List.end(), obj);
	if (it != List.end()) {
		List.erase(it);
	}
}
void ObjectRegistry::Clear() {
	List.clear();
}
void ObjectRegistry::Iterate(std::function<void(Entity* e)> func) {
	std::for_each(List.begin(), List.end(), func);
}
void ObjectRegistry::RemoveNonPersistentFromLinkedList(Entity* first, int persistence) {
	for (Entity *ent = first, *next; ent; ent = next) {
		// Store the "next" so that when/if the current is
		// removed, it can still be used to point at the end of
		// the loop.
		next = ent->NextEntity;

		if (ent->Persistence <= persistence) {
			Remove(ent);
		}
	}
}
void ObjectRegistry::RemoveNonPersistentFromLinkedList(Entity* first) {
	RemoveNonPersistentFromLinkedList(first, Persistence_NONE);
}
Entity* ObjectRegistry::GetNth(int n) {
	if (n < 0 || n >= (int)List.size()) {
		return NULL;
	}
	return List[n];
}
Entity* ObjectRegistry::GetClosest(int x, int y) {
	if (List.size() == 1) {
		return List[0];
	}

	Entity* closest = NULL;
	int smallestDistance = 0x7FFFFFFF;

	Iterate([x, y, &closest, &smallestDistance](Entity* ent) -> void {
		int xD = ent->X - x;
		xD *= xD;
		int yD = ent->Y - y;
		yD *= yD;
		if (smallestDistance > xD + yD) {
			smallestDistance = xD + yD;
			closest = ent;
		}
	});

	return closest;
}

void ObjectRegistry::Dispose() {
	List.clear();
	List.shrink_to_fit();
}
ObjectRegistry::~ObjectRegistry() {
	Dispose();
}

int ObjectRegistry::Count() {
	return (int)List.size();
}
