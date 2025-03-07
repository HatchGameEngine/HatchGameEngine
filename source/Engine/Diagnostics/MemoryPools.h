#pragma once

namespace MemoryPools {
#define MAX_REFERENCE_COUNT 0xF000
struct MemoryPool {
	Uint32* Blocks;
	Uint32 BlockCount;
	Uint32 BlocksDataSize;
	void** ReferenceList[MAX_REFERENCE_COUNT];
	void* PointerList[MAX_REFERENCE_COUNT];
	Uint32 ReferenceCount;
	Uint32 GC_CallCount;
};
enum {
	MEMPOOL_HASHMAP,
	MEMPOOL_SUBOBJECT,
	MEMPOOL_STRING,
	MEMPOOL_TEMP,
	MEMPOOL_IMAGEDATA,

	MEMPOOL_COUNT,
};

extern MemoryPool MemoryPools[MEMPOOL_COUNT];

bool Init();

void* Alloc(void** mem, size_t size, int pool, bool clearMem);
void ClearPool(int pool);
void RunGC(int pool);
void CleanupReferences(int pool);
void Dispose();
void PassReference(void** newReference, void** oldReference, int pool);

template<typename M>
M* Alloc(M** mem, size_t size, int pool, bool clearMem) {
	return (M*)Alloc((void**)mem, size, pool, clearMem);
}
template<typename M>
void PassReference(M** newReference, M** oldReference, int pool) {
	PassReference((void**)newReference, (void**)oldReference, pool);
}
} // namespace MemoryPools
