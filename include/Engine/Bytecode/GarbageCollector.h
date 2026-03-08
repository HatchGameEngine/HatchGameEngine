#ifndef ENGINE_BYTECODE_GARBAGECOLLECTOR_H
#define ENGINE_BYTECODE_GARBAGECOLLECTOR_H

#include <Engine/Bytecode/Types.h>

#include <set>

class ScriptManager;

class GarbageCollector {
private:
	void FreeObject(Obj* object);
	void GrayValue(VMValue value);
	void BlackenObject(Obj* object);
#ifndef HSL_STANDALONE
	void CollectResources();
#endif

public:
	std::vector<Obj*> GrayList;
	ScriptManager* Manager;
	size_t NextGC;
	size_t GarbageSize;

	GarbageCollector(ScriptManager* manager);
	void Collect(bool doLog);
	void GrayObject(void* obj);
	void GrayHashMap(void* pointer);
};

#endif /* ENGINE_BYTECODE_GARBAGECOLLECTOR_H */
