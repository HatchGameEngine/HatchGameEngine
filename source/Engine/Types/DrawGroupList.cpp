#include <Engine/Types/DrawGroupList.h>

#include <Engine/Application.h>

DrawGroupList::DrawGroupList() {
	Init();
}

// Double linked-list functions
int DrawGroupList::Add(Entity* obj) {
	Entities->push_back(obj);
	if (EntityDepthSortingEnabled) {
		NeedsSorting = true;
	}
	return Entities->size() - 1;
}
bool DrawGroupList::Contains(Entity* obj) {
	return GetEntityIndex(obj) == -1 ? false : true;
}
int DrawGroupList::GetEntityIndex(Entity* obj) {
	for (size_t i = 0, iSz = Entities->size(); i < iSz; i++) {
		if ((*Entities)[i] == obj) {
			return (int)i;
		}
	}
	return -1;
}
void DrawGroupList::Remove(Entity* obj) {
	for (size_t i = 0, iSz = Entities->size(); i < iSz; i++) {
		if ((*Entities)[i] == obj) {
			Entities->erase(Entities->begin() + i);
			return;
		}
	}
}
void DrawGroupList::Clear() {
	Entities->clear();
	NeedsSorting = false;
}

void DrawGroupList::Sort() {
	std::stable_sort(
		Entities->begin(), Entities->end(), [](const Entity* entA, const Entity* entB) {
			return entA->Depth < entB->Depth;
		});
	NeedsSorting = false;
}

void DrawGroupList::Init() {
	Entities = new vector<Entity*>();
}
void DrawGroupList::Dispose() {
	delete Entities;
	Entities = nullptr;
}
DrawGroupList::~DrawGroupList() {
	// Dispose();
}

int DrawGroupList::Count() {
	return Entities->size();
}
