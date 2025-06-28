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
#ifdef VM_DEBUG
	static Uint32 GetBranchLimit();
#endif
	static void FreeModules();

public:
	static bool LoadAllClasses;
	static HashMap<VMValue>* Globals;
	static HashMap<VMValue>* Constants;
	static VMThread Threads[8];
	static Uint32 ThreadCount;
	static vector<ObjModule*> ModuleList;
	static HashMap<BytecodeContainer>* Sources;
	static HashMap<ObjClass*>* Classes;
	static HashMap<char*>* Tokens;
	static vector<ObjNamespace*> AllNamespaces;
	static vector<ObjClass*> ClassImplList;
	static SDL_mutex* GlobalLock;

	static void DestroyObject(Obj* object);
	static void FreeFunction(Obj* object);
	static void FreeModule(Obj* object);
	static void FreeClass(Obj* object);
	static void FreeEnumeration(Obj* object);
	static void FreeNamespace(Obj* object);
	static void RequestGarbageCollection();
	static void ForceGarbageCollection();
	static void ResetStack();
	static void Init();
	static void Dispose();
	static bool DoIntegerConversion(VMValue& value, Uint32 threadID);
	static bool DoDecimalConversion(VMValue& value, Uint32 threadID);
	static bool Lock();
	static void Unlock();
	static void DefineMethod(VMThread* thread, ObjFunction* function, Uint32 hash);
	static void DefineNative(ObjClass* klass, const char* name, NativeFn function);
	static void GlobalLinkInteger(ObjClass* klass, const char* name, int* value);
	static void GlobalLinkDecimal(ObjClass* klass, const char* name, float* value);
	static void GlobalConstInteger(ObjClass* klass, const char* name, int value);
	static void GlobalConstDecimal(ObjClass* klass, const char* name, float value);
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
	static bool ClassExists(Uint32 hash);
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
