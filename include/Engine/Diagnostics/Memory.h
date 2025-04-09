#ifndef ENGINE_DIAGNOSTICS_MEMORY_H
#define ENGINE_DIAGNOSTICS_MEMORY_H

#include <Engine/Includes/Standard.h>

namespace Memory {
//private:
	extern vector<void*> TrackedMemory;
	extern vector<size_t> TrackedSizes;
	extern vector<const char*> TrackedMemoryNames;

//public:
	extern size_t MemoryUsage;
	extern bool IsTracking;

	void Memset4(void* dst, Uint32 val, size_t dwords);
	void* Malloc(size_t size);
	void* Calloc(size_t count, size_t size);
	void* Realloc(void* pointer, size_t size);
	void* TrackedMalloc(const char* identifier, size_t size);
	void* TrackedCalloc(const char* identifier, size_t count, size_t size);
	void Track(void* pointer, const char* identifier);
	void Track(void* pointer, size_t size, const char* identifier);
	void TrackLast(const char* identifier);
	void Free(void* pointer);
	void Remove(void* pointer);
	const char* GetName(void* pointer);
	void ClearTrackedMemory();
	size_t CheckLeak();
	void PrintLeak();
};

#endif /* ENGINE_DIAGNOSTICS_MEMORY_H */
