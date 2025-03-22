#ifndef ENGINE_BYTECODE_SCRIPTMANAGER_H
#define ENGINE_BYTECODE_SCRIPTMANAGER_H
class ScriptEntity;

#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/VMThread.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Types/Entity.h>
#include <set>

#define OBJECTS_DIR_NAME "Objects/"

class ScriptManager {
private:
	static Uint32 GetBranchLimit();
	static void RemoveNonGlobalableValue(Uint32 hash, VMValue value);
	static void FreeNativeValue(Uint32 hash, VMValue value);
	static void FreeFunction(ObjFunction* function);
	static void FreeModule(ObjModule* module);
	static void FreeClass(ObjClass* klass);
	static void FreeEnumeration(ObjEnum* enumeration);
	static void FreeNamespace(ObjNamespace* ns);
	static void FreeModules();

public:
	static bool LoadAllClasses;
	static HashMap<VMValue>* Globals;
	static HashMap<VMValue>* Constants;
	static std::set<Obj*> FreedGlobals;
	static VMThread Threads[8];
	static Uint32 ThreadCount;
	static vector<ObjModule*> ModuleList;
	static HashMap<BytecodeContainer>* Sources;
	static HashMap<ObjClass*>* Classes;
	static HashMap<char*>* Tokens;
	static vector<ObjNamespace*> AllNamespaces;
	static vector<ObjClass*> ClassImplList;
	static SDL_mutex* GlobalLock;

	static void RequestGarbageCollection();
	static void ForceGarbageCollection();
	static void ResetStack();
	static void Init();
	static void DisposeGlobalValueTable(HashMap<VMValue>* globals);
	static void Dispose();
	static void FreeString(ObjString* string);
	static void FreeGlobalValue(Uint32 hash, VMValue value);
	static VMValue CastValueAsString(VMValue v, bool prettyPrint);
	static VMValue CastValueAsString(VMValue v);
	static VMValue CastValueAsInteger(VMValue v);
	static VMValue CastValueAsDecimal(VMValue v);
	static VMValue Concatenate(VMValue va, VMValue vb);
	static bool ValuesSortaEqual(VMValue a, VMValue b);
	static bool ValuesEqual(VMValue a, VMValue b);
	static bool ValueFalsey(VMValue a);
	static VMValue DelinkValue(VMValue val);
	static bool DoIntegerConversion(VMValue& value, Uint32 threadID);
	static bool DoDecimalConversion(VMValue& value, Uint32 threadID);
	static void FreeValue(VMValue value);
	static bool Lock();
	static void Unlock();
	static void DefineMethod(VMThread* thread, ObjFunction* function, Uint32 hash);
	static void DefineNative(ObjClass* klass, const char* name, NativeFn function);
	static void GlobalLinkInteger(ObjClass* klass, const char* name, int* value);
	static void GlobalLinkDecimal(ObjClass* klass, const char* name, float* value);
	static void GlobalConstInteger(ObjClass* klass, const char* name, int value);
	static void GlobalConstDecimal(ObjClass* klass, const char* name, float value);
	static ObjClass* GetClassParent(ObjClass* klass);
	static VMValue GetClassMethod(ObjClass* klass, Uint32 hash);
	static void LinkStandardLibrary();
	static void LinkExtensions();
	static bool RunBytecode(BytecodeContainer bytecodeContainer, Uint32 filenameHash);
	static bool CallFunction(char* functionName);
	static Entity* SpawnObject(const char* objectName);
	static Uint32 MakeFilenameHash(const char* filename);
	static std::string GetBytecodeFilenameForHash(Uint32 filenameHash);
	static BytecodeContainer GetBytecodeFromFilenameHash(Uint32 filenameHash);
	static bool ClassExists(const char* objectName);
	static bool IsStandardLibraryClass(const char* className);
	static bool LoadScript(char* filename);
	static bool LoadScript(const char* filename);
	static bool LoadScript(Uint32 hash);
	static bool LoadObjectClass(const char* objectName, bool addNativeFunctions);
	static void AddNativeObjectFunctions(ObjClass* klass);
	static ObjClass* GetObjectClass(const char* className);
	static Entity* ObjectSpawnFunction(ObjectList* list);
	static void LoadClasses();
};

#endif /* ENGINE_BYTECODE_SCRIPTMANAGER_H */
