#ifndef ENGINE_RESOURCETYPES_RESOURCE_H
#define ENGINE_RESOURCETYPES_RESOURCE_H

#include <Engine/ResourceTypes/ResourceType.h>

class Resource {
private:
	static ResourceType* New(Uint8 type, const char* filename, Uint32 hash, int unloadPolicy);
	static void Delete(ResourceType* resource);
	static void AddRef(ResourceType* resource);
	static void DecRef(ResourceType* resource);
	static bool UnloadData(ResourceType* resource);
	static int Search(Uint8 type, const char* filename, Uint32 hash);
	static ResourceType* LoadInternal(Uint8 type, const char* filename, int unloadPolicy);
	static Uint8 GuessType(const char* filename);
	static void* LoadData(Uint8 type, const char* filename);
	static ISprite* LoadFontData(const char* filename, int pixel_sz);
	static MediaBag* LoadMediaBag(const char* filename);

public:
	static vector<ResourceType*>* GetList();
	static void DisposeInScope(Uint32 scope);
	static void DisposeAll();
	static void* GetVMObject(ResourceType* resource);
	static ResourceType* Load(Uint8 type, const char* filename, int unloadPolicy);
	static ResourceType* LoadFont(const char* filename, int pixel_sz, int unloadPolicy);
	static void TakeRef(ResourceType* resource);
	static bool Reload(ResourceType* resource);
	static void Unload(ResourceType* resource);
	static void Release(ResourceType* resource);
};

#endif /* ENGINE_RESOURCETYPES_RESOURCE_H */
