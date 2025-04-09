#ifndef ENGINE_BYTECODE_GARBAGECOLLECTOR_H
#define ENGINE_BYTECODE_GARBAGECOLLECTOR_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/HashMap.h>

namespace GarbageCollector {
//private:
	void FreeValue(VMValue value);
	void GrayValue(VMValue value);
	void GrayObject(void* obj);
	void GrayHashMapItem(Uint32, VMValue value);
	void GrayHashMap(void* pointer);
	void BlackenObject(Obj* object);
	void CollectResources();

//public:
	extern vector<Obj*> GrayList;
	extern Obj* RootObject;
	extern size_t NextGC;
	extern size_t GarbageSize;
	extern double MaxTimeAlotted;
	extern bool Print;
	extern bool FilterSweepEnabled;
	extern int FilterSweepType;

	extern void Init();
	extern void Collect();
};

#endif /* ENGINE_BYTECODE_GARBAGECOLLECTOR_H */
