#ifndef ENGINE_DIAGNOSTICS_MEMORY_H
#define ENGINE_DIAGNOSTICS_MEMORY_H

#include <Engine/Includes/Standard.h>

class Memory {
private:
	static vector<void*> TrackedMemory;
	static vector<size_t> TrackedSizes;
	static vector<const char*> TrackedMemoryNames;

public:
	static size_t MemoryUsage;
	static bool IsTracking;

	static void Memset4(void* dst, Uint32 val, size_t dwords);
	static void* Malloc(size_t size);
	static void* Calloc(size_t count, size_t size);
	static void* Realloc(void* pointer, size_t size);
	static void* TrackedMalloc(const char* identifier, size_t size);
	static void* TrackedCalloc(const char* identifier, size_t count, size_t size);
	static void Track(void* pointer, const char* identifier);
	static void Track(void* pointer, size_t size, const char* identifier);
	static void TrackLast(const char* identifier);
	static void Free(void* pointer);
	static void Remove(void* pointer);
	static const char* GetName(void* pointer);
	static void ClearTrackedMemory();
	static size_t CheckLeak();
	static void PrintLeak();
};

#endif /* ENGINE_DIAGNOSTICS_MEMORY_H */
