#ifndef ENGINE_RESOURCETYPES_RESOURCE_H
#define ENGINE_RESOURCETYPES_RESOURCE_H

#include <Engine/ResourceTypes/Resourceable.h>
#include <Engine/ResourceTypes/ResourceType.h>

class Resource {
private:
	static ResourceType* New(Uint8 type, const char* filename, Uint32 hash, int unloadPolicy);
	static void Delete(ResourceType* resource);
	static void AddRef(ResourceType* resource);
	static void DecRef(ResourceType* resource);
	static bool UnloadData(ResourceType* resource);
	static int Search(Uint8 type, const char* filename, Uint32 hash);
	static Uint8 GuessType(const char* filename);
	static Resourceable* LoadData(Uint8 type, const char* filename);

public:
	static vector<ResourceType*>* GetList();
	static void DisposeInScope(Uint32 scope);
	static void DisposeAll();
	static void* GetVMObject(ResourceType* resource);
	static void SetVMObject(ResourceType* resource, void* obj);
	static void ReleaseVMObject(ResourceType* resource);
	static bool CompareVMObjects(void* a, void* b);
	static ResourceType* Load(Uint8 type, const char* filename, int unloadPolicy);
	static void TakeRef(ResourceType* resource);
	static bool Reload(ResourceType* resource);
	static void Unload(ResourceType* resource);
	static void Release(ResourceType* resource);
};

#endif /* ENGINE_RESOURCETYPES_RESOURCE_H */
