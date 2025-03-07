#ifndef ENGINE_BYTECODE_GARBAGECOLLECTOR_H
#define ENGINE_BYTECODE_GARBAGECOLLECTOR_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/HashMap.h>

class GarbageCollector {
private:
	static void FreeValue(VMValue value);
	static void GrayValue(VMValue value);
	static void GrayObject(void* obj);
	static void GrayHashMapItem(Uint32, VMValue value);
	static void GrayHashMap(void* pointer);
	static void BlackenObject(Obj* object);
	static void CollectResources();

public:
	static vector<Obj*> GrayList;
	static Obj* RootObject;
	static size_t NextGC;
	static size_t GarbageSize;
	static double MaxTimeAlotted;
	static bool Print;
	static bool FilterSweepEnabled;
	static int FilterSweepType;

	static void Init();
	static void Collect();
};

#endif /* ENGINE_BYTECODE_GARBAGECOLLECTOR_H */
