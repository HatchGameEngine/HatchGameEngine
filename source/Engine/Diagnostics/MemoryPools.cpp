#include <Engine/Diagnostics/Log.h>
#include <Engine/Includes/Standard.h>

#include <Engine/Diagnostics/MemoryPools.h>

namespace MemoryPools {
MemoryPool MemoryPools[MEMPOOL_COUNT];

bool Init() {
	memset(MemoryPools, 0, sizeof(MemoryPools));

	MemoryPools[MEMPOOL_HASHMAP].BlocksDataSize = 0x1000000;
	MemoryPools[MEMPOOL_SUBOBJECT].BlocksDataSize = 0x200000;
	MemoryPools[MEMPOOL_STRING].BlocksDataSize = 0x200000;
	MemoryPools[MEMPOOL_TEMP].BlocksDataSize = 0x200000;
	MemoryPools[MEMPOOL_IMAGEDATA].BlocksDataSize = 0x400;

	int totalPoolSize = 0;

	auto memPool = &MemoryPools[0];
	for (int i = 0; i < MEMPOOL_COUNT; i++) {
		memPool->Blocks = (Uint32*)malloc(memPool->BlocksDataSize);
		if (!memPool->Blocks) {
			Log::Print(Log::LOG_ERROR, "Pool %d could not be allocated.", i);
			return false;
		}

		totalPoolSize += memPool->BlocksDataSize;

		memPool->BlockCount = 0;
		memPool->ReferenceCount = 0;
		memPool->GC_CallCount = 0;
		memPool++;
	}

	const char* dataLabels[] = {
		"B",
		"KiB",
		"MiB",
		"GiB",
		"Err1",
		"Err2",
		"Err3",
		"Err4",
		"Err5",
	};
	int dataLabel = 0;

	while (totalPoolSize > 1024) {
		totalPoolSize /= 1024;
		dataLabel++;
	}

	printf("Successfully allocated %d %s for memory pools.\n",
		totalPoolSize,
		dataLabels[dataLabel]);

	return true;
}

void* Alloc(void** mem, size_t size, int pool, bool clearMem) {
	if (!mem) {
		return NULL;
	}

	size = (size & 0xFFFFFFFCU) + 4;

	MemoryPool* memPool = &MemoryPools[pool];
	if (memPool->ReferenceCount >= MAX_REFERENCE_COUNT) {
		CleanupReferences(pool);
	}

	if (memPool->ReferenceCount < MAX_REFERENCE_COUNT) {
		// Is there enough space in the pool for this
		// allocation?
		Uint32 blockDataCurrentSize = memPool->BlockCount * sizeof(Uint32);
		if (size + blockDataCurrentSize < memPool->BlocksDataSize) {
			// BlockStruct: isUsed
			memPool->Blocks[memPool->BlockCount++] = true;
			// BlockStruct: dataStartIndex
			//     Set index to the block after the next
			//     one.
			memPool->Blocks[memPool->BlockCount] = memPool->BlockCount + 2;
			memPool->BlockCount++;
			// BlockStruct: dataSize
			memPool->Blocks[memPool->BlockCount++] = (Uint32)size;
			// BlockStruct: data
			*mem = (void*)&memPool->Blocks[memPool->BlockCount];
			memPool->BlockCount += size / sizeof(Uint32);

			// Add reference
			memPool->ReferenceList[memPool->ReferenceCount] = mem;
			memPool->PointerList[memPool->ReferenceCount] = *mem;
			memPool->ReferenceCount++;
		}
		else {
			// Otherwise, run the GC to attempt to clear up
			// space.
			RunGC(pool);

			// Update the current pool size
			blockDataCurrentSize = memPool->BlockCount * sizeof(Uint32);

			// Is there enough space in the pool for this
			// allocation?
			if (size + blockDataCurrentSize < memPool->BlocksDataSize) {
				// BlockStruct: isUsed
				memPool->Blocks[memPool->BlockCount++] = true;
				// BlockStruct: dataStartIndex
				//     Set index to the block after the
				//     next one.
				memPool->Blocks[memPool->BlockCount] = memPool->BlockCount + 2;
				memPool->BlockCount++;
				// BlockStruct: dataSize
				memPool->Blocks[memPool->BlockCount++] = (Uint32)size;
				// BlockStruct: data
				*mem = (void*)&memPool->Blocks[memPool->BlockCount];
				memPool->BlockCount += size / sizeof(Uint32);

				// Add reference
				memPool->ReferenceList[memPool->ReferenceCount] = mem;
				memPool->PointerList[memPool->ReferenceCount] = *mem;
				memPool->ReferenceCount++;
			}
			else {
				Log::Print(Log::LOG_ERROR,
					"Not enough space in pool %d for 0x%X bytes (0x%X total, 0x%X remaining)",
					pool,
					size,
					memPool->BlocksDataSize,
					memPool->BlocksDataSize - blockDataCurrentSize);
			}
		}

		if (memPool->ReferenceCount >= MAX_REFERENCE_COUNT) {
			CleanupReferences(pool);
		}

		if (clearMem && *mem) {
			memset(*mem, 0, size);
		}
	}
	else {
		Log::Print(Log::LOG_ERROR, "Too many references!");
	}

	return *mem;
}

void ClearPool(int pool) {
	auto memPool = &MemoryPools[pool];
	memPool->BlockCount = 0;
	memPool->ReferenceCount = 0;
	for (int v = 0; v < MAX_REFERENCE_COUNT; v++) {
		memPool->ReferenceList[v] = NULL;
		memPool->PointerList[v] = NULL;
	}
}
void RunGC(int pool) {
	CleanupReferences(pool);

	MemoryPool* memPool = &MemoryPools[pool];
	memPool->GC_CallCount++;

	struct BlockHeader {
		Uint32 isUsed;
		Uint32 dataStartIndex;
		Uint32 dataSize;
		// Data would be here
		// Uint32 data[];
	};

	Uint32 freeBlockIndex = 0;
	Uint32 freedBlocks = 0;
	for (Uint32 i = 0; i < memPool->BlockCount;) {
		Uint32* block = &memPool->Blocks[i];
		BlockHeader* header = (BlockHeader*)block;

		int blockSizeInBytes = header->dataSize + 3 * sizeof(Uint32); // 3 =
			// sizeof(BlockHeader) /
			// sizeof(Uint32)
		int blockSize = blockSizeInBytes / sizeof(Uint32);
		int nextBlockIndex = i + blockSize;

		// Set block to unused until proven used
		header->isUsed = false;

		// Shortcut: If there are no references, then block
		// logically cannot
		//   be used. Therefore, no change is needed, continue
		//   to next block
		if (!memPool->ReferenceCount) {
			freedBlocks += blockSize;
			goto NextBlock;
		}

		// Check if block is used
		for (Uint32 r = 0; r < memPool->ReferenceCount; r++) {
			if (&header[1] == memPool->PointerList[r]) {
				header->isUsed = true;
				break;
			}
		}

		if (!header->isUsed) {
			freedBlocks += blockSize;
			goto NextBlock;
		}
		else {
			// If the current block is ahead of the free
			// block, move current block back to free block
			if (i > freeBlockIndex) {
				memcpy(&memPool->Blocks[freeBlockIndex],
					&memPool->Blocks[i],
					blockSizeInBytes);
			}

			// Move free block index to next block since
			// this one isn't free
			freeBlockIndex += blockSize;
		}

	NextBlock:
		// Next block
		i = nextBlockIndex;
	}

	if (freedBlocks) {
		printf("Freed 0x%X blocks out of 0x%X possible (0x%X max)\n",
			freedBlocks,
			memPool->BlockCount,
			(int)(memPool->BlocksDataSize / sizeof(Uint32)));
	}

	if (freedBlocks) {
		memPool->BlockCount -= freedBlocks;
		if (memPool->BlockCount) {
			for (Uint32 i = 0; i < memPool->BlockCount;) {
				Uint32* block = &memPool->Blocks[i];
				BlockHeader* header = (BlockHeader*)block;

				int blockSizeInBytes = header->dataSize + 3 * sizeof(Uint32); // 3 =
					// sizeof(BlockHeader)
					// /
					// sizeof(Uint32)
				int blockSize = blockSizeInBytes / sizeof(Uint32);
				int nextBlockIndex = i + blockSize;

				// For any pointers that point to this
				// allocblock's data
				void* oldAllocBlockDataPtr =
					(void*)&memPool->Blocks[header->dataStartIndex];
				void* newAllocBlockDataPtr = (void*)&header[1];
				for (Uint32 r = 0; r < memPool->ReferenceCount; r++) {
					if (oldAllocBlockDataPtr == memPool->PointerList[r]) {
						*memPool->ReferenceList[r] =
							memPool->PointerList[r] =
								newAllocBlockDataPtr;
					}
				}

				header->dataStartIndex = i + 3; // 3 = sizeof(BlockHeader) /
					// sizeof(Uint32)

				// Next block
				i = nextBlockIndex;
			}
		}
	}
}
void CleanupReferences(int pool) {
	MemoryPool* memPool = &MemoryPools[pool];

	// Clear stale references
	for (Uint32 r = 0; r < memPool->ReferenceCount; r++) {
		void** ref = memPool->ReferenceList[r];
		void* ptr = memPool->PointerList[r];
		if (ref && *ref != ptr) {
			memPool->ReferenceList[r] = 0;
		}
	}

	// Move all valid references to top of list
	Uint32 v = 0;
	// v: Valid Reference Destination Index
	// r: Reference Index (could be either stale or not)
	for (Uint32 r = 0; r < memPool->ReferenceCount; r++) {
		void** ref = memPool->ReferenceList[r];
		void* ptr = memPool->PointerList[r];
		if (ref) {
			if (r > v) {
				memPool->ReferenceList[v] = ref;
				memPool->PointerList[v] = ptr;
				// Clear the old spot
				memPool->ReferenceList[r] = 0;
				memPool->PointerList[r] = 0;
			}
			v++;
		}
	}

	if ((memPool->ReferenceCount - v) != 0) {
		printf("Freed 0x%X refs out of 0x%X possible (0x%X max)\n",
			memPool->ReferenceCount - v,
			memPool->ReferenceCount,
			MAX_REFERENCE_COUNT);
	}

	// Set new reference count
	memPool->ReferenceCount = v;

	// Cleanup references outside of bounds
	for (; v < MAX_REFERENCE_COUNT; v++) {
		memPool->ReferenceList[v] = NULL;
		memPool->PointerList[v] = NULL;
	}
}
void Dispose() {
	auto memPool = &MemoryPools[0];
	for (int i = 0; i < MEMPOOL_COUNT; i++) {
		if (memPool->Blocks) {
			free(memPool->Blocks);
		}

		memPool->BlockCount = 0;
		memPool->ReferenceCount = 0;
		memPool->GC_CallCount = 0;
		memPool++;
	}
}

void PassReference(void** newReference, void** oldReference, int pool) {
	if (oldReference) {
		*newReference = *oldReference;

		auto mPool = &MemoryPools[pool];
		if (mPool->ReferenceCount < MAX_REFERENCE_COUNT) {
			mPool->ReferenceList[mPool->ReferenceCount] = newReference;
			mPool->PointerList[mPool->ReferenceCount] = *newReference;
			mPool->ReferenceCount++;
			if (mPool->ReferenceCount >= MAX_REFERENCE_COUNT) {
				CleanupReferences(pool);
			}
		}
	}
}
} // namespace MemoryPools
