#ifndef ENGINE_BYTECODE_SCRIPTMANAGER_H
#define ENGINE_BYTECODE_SCRIPTMANAGER_H

#include <Engine/Bytecode/Bytecode.h>
#include <Engine/Bytecode/GarbageCollector.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/TypeImpl/ArrayImpl.h>
#include <Engine/Bytecode/TypeImpl/FunctionImpl.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/MapImpl.h>
#include <Engine/Bytecode/TypeImpl/StreamImpl.h>
#include <Engine/Bytecode/TypeImpl/StringImpl.h>
#include <Engine/Bytecode/VMThread.h>
#include <Engine/Exceptions/ScriptException.h>
#include <Engine/Includes/HashMap.h>
#include <Engine/Includes/Standard.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/Stream.h>
#include <Engine/Types/Property.h>

#ifdef HSL_COMPILER
#include <Engine/Bytecode/Compiler.h>
#endif

#ifndef HSL_STANDALONE
#include <Engine/Bytecode/TypeImpl/FontImpl.h>
#include <Engine/Bytecode/TypeImpl/EntityImpl.h>
#include <Engine/Bytecode/TypeImpl/MaterialImpl.h>
#include <Engine/Bytecode/TypeImpl/ShaderImpl.h>
#include <Engine/ResourceTypes/ISound.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/Types/Entity.h>
#endif

#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#endif

#ifdef USE_SDL
#include <Engine/Includes/StandardSDL2.h>
#endif

#define OBJECTS_DIR_NAME "Objects/"

#define MAX_VM_THREADS 8

class ScriptManager {
private:
#ifdef HSL_LIBRARY
	hsl_ImportScriptHandler ImportScriptHandler = nullptr;
	hsl_ImportClassHandler ImportClassHandler = nullptr;
	hsl_WithIteratorHandler WithIteratorHandler = nullptr;
#endif

#if defined(HSL_VM) && defined(VM_DEBUG)
	HashMap<SourceFile*>* SourceFiles = nullptr;

	static Uint32 GetBranchLimit();
	void InitThread(VMThread* thread);
	void LoadSourceCodeLines(SourceFile* sourceFile, char* text);
	void LoadSourceCodeLines(SourceFile* sourceFile, const char* sourceFilename);
#endif

public:
#ifdef HSL_VM
	static bool LoadAllClasses;
#ifdef VM_DEBUG
	bool BreakpointsEnabled = false;
#endif
#ifdef HSL_LIBRARY
	hsl_RuntimeErrorHandler RuntimeErrorHandler = nullptr;
	bool HasWithIteratorHandler = false;
	char* LastCompileError = nullptr;
#endif
	Obj* RootObject = nullptr;
	HashMap<VMValue>* Globals = nullptr;
	VMThread Threads[MAX_VM_THREADS];
	Uint32 ThreadCount = 0;
	GarbageCollector* GC = nullptr;
	vector<ObjModule*> ModuleList;
	vector<ObjModule*> TempModuleList;
	HashMap<BytecodeContainer>* Sources = nullptr;
	HashMap<ObjClass*>* Classes = nullptr;
	HashMap<ObjModule*>* Modules = nullptr;
	HashMap<char*>* Tokens = nullptr;
	vector<ObjNamespace*> AllNamespaces;
#ifdef USE_SDL
	SDL_mutex* GlobalLock = nullptr;
#endif
#endif

	HashMap<VMValue>* Constants = nullptr;

	ArrayImpl* ImplArray = nullptr;
	FunctionImpl* ImplFunction = nullptr;
	InstanceImpl* ImplInstance = nullptr;
	MapImpl* ImplMap = nullptr;
	StreamImpl* ImplStream = nullptr;
	StringImpl* ImplString = nullptr;
#ifndef HSL_STANDALONE
	FontImpl* ImplFont = nullptr;
	EntityImpl* ImplEntity = nullptr;
	MaterialImpl* ImplMaterial = nullptr;
	ShaderImpl* ImplShader = nullptr;
#endif
	HashMap<ObjClass*>* ImplClasses = nullptr;

	ScriptManager();
	~ScriptManager();

	void DestroyObject(Obj* object);
	void FreeFunction(Obj* object);
	void FreeModule(Obj* object);
	void FreeClass(Obj* object);
	void FreeEnumeration(Obj* object);
	void FreeNamespace(Obj* object);
#ifdef HSL_VM
	void RemoveTemporaryModules();
	void RequestGarbageCollection(bool doLog);
	void ForceGarbageCollection(bool doLog);
	void ResetStack();
#endif
	static void Init();
	static void Dispose();
	bool Lock();
	void Unlock();
#ifdef HSL_VM
	VMThread* NewThread();
	void DisposeThread(VMThread* thread);
	void DefineMethod(VMThread* thread, ObjFunction* function, Uint32 hash);
	void DefineNative(ObjClass* klass, const char* name, NativeFn function);
	void GlobalLinkInteger(ObjClass* klass, const char* name, int* value);
	void GlobalLinkDecimal(ObjClass* klass, const char* name, float* value);
	void GlobalConstInteger(ObjClass* klass, const char* name, int value);
	void GlobalConstDecimal(ObjClass* klass, const char* name, float value);
	ObjClass* GetClassParent(Obj* object, ObjClass* klass);
	bool GetClassMethod(ObjClass* klass, Uint32 hash, VMValue* callable);
	bool GetClassMethod(Obj* object, ObjClass* klass, Uint32 hash, VMValue* callable);
	bool ClassHasMethod(ObjClass* klass, Uint32 hash);
#endif
	void LinkStandardLibrary();
#ifdef HSL_VM
	Bytecode* ReadBytecode(BytecodeContainer bytecodeContainer);
	Bytecode* ReadBytecode(Stream* stream);
	ObjModule* LoadBytecode(VMThread* thread, Bytecode* bytecode, Uint32 filenameHash);
	ObjModule* LoadBytecode(VMThread* thread, BytecodeContainer bytecodeContainer, Uint32 filenameHash);
	ObjModule* LoadBytecode(VMThread* thread, Stream* stream, Uint32 filenameHash);
	bool RunBytecode(VMThread* thread, BytecodeContainer bytecodeContainer, Uint32 filenameHash);
	bool RunBytecode(VMThread* thread, Stream* stream, Uint32 filenameHash);
	bool CallFunction(const char* functionName);
	VMValue FindFunction(const char* functionName);
#ifndef HSL_STANDALONE
	BytecodeContainer GetBytecodeFromFilenameHash(Uint32 filenameHash);
	bool BytecodeForFilenameHashExists(Uint32 filenameHash);
#endif
	bool ScriptExists(const char* name);
	bool ClassExists(const char* objectName);
	bool ClassExists(Uint32 hash);
	bool IsClassLoaded(const char* className);
	bool IsStandardLibraryClass(const char* className);
	bool LoadScript(VMThread* thread, const char* filename);
#ifdef HSL_COMPILER
	bool LoadScriptFromStream(VMThread* thread, Stream* stream, const char* filename);
	ObjModule* CompileScriptFromStream(VMThread* thread, Stream* stream, const char* filename);
	ObjModule* CompileAndLoad(VMThread* thread, const char* code, const char* filename, CompilerSettings settings);
	ObjModule* CompileAndLoad(VMThread* thread, Compiler* compiler, const char* code, const char* filename);
#endif
	bool IsScriptLoaded(const char* filename);
	bool IsScriptLoaded(Uint32 filenameHash);
	ObjModule* GetScriptModule(const char* filename);
	ObjModule* GetScriptModule(Uint32 filenameHash);
	ObjFunction* GetFunctionAtScriptLine(ObjModule* module, int lineNum);
#ifdef HSL_LIBRARY
	void SetImportScriptHandler(hsl_ImportScriptHandler handler);
	void SetImportClassHandler(hsl_ImportClassHandler handler);
	void SetWithIteratorHandler(hsl_WithIteratorHandler handler);
	bool CallWithIteratorHandler(int state, VMValue receiver, int* index, VMValue* newReceiver);
#endif
	bool LoadObjectClass(VMThread* thread, const char* objectName);
	ObjClass* GetObjectClass(const char* className);
#ifndef HSL_STANDALONE
	void LoadClasses();
#endif
#ifdef VM_DEBUG
	char* GetSourceCodeLine(const char* sourceFilename, int line);
	void AddSourceFile(const char* sourceFilename, char* text);
	void RemoveSourceFile(const char* sourceFilename);
#endif
#endif
	static Uint32 MakeFilenameHash(const char* filename);
	static std::string GetBytecodeFilenameForHash(Uint32 filenameHash);
#ifdef HSL_VM
	int GetInteger(VMValue* args, int index, VMThread* thread);
	float GetDecimal(VMValue* args, int index, VMThread* thread);
	char* GetString(VMValue* args, int index, VMThread* thread);
	ObjString* GetVMString(VMValue* args, int index, VMThread* thread);
	ObjArray* GetArray(VMValue* args, int index, VMThread* thread);
	ObjMap* GetMap(VMValue* args, int index, VMThread* thread);
	ObjInstance* GetInstance(VMValue* args, int index, VMThread* thread);
	ObjFunction* GetFunction(VMValue* args, int index, VMThread* thread);
#ifndef HSL_STANDALONE
	ISprite* GetSprite(VMValue* args, int index, VMThread* thread);
	Image* GetImage(VMValue* args, int index, VMThread* thread);
	ISound* GetSound(VMValue* args, int index, VMThread* thread);
	ObjEntity* GetEntity(VMValue* args, int index, VMThread* thread);
	ObjShader* GetShader(VMValue* args, int index, VMThread* thread);
	ObjFont* GetFont(VMValue* args, int index, VMThread* thread);
#endif
	static void CheckArgCount(int argCount, int expects, VMThread* thread);
	static void CheckAtLeastArgCount(int argCount, int expects, VMThread* thread);
#endif

#ifdef HSL_VM
	static VMValue VM_GetClass(int argCount, VMValue* args, VMThread* thread);
#endif

	Obj* AllocateObject(size_t size, ObjType type);
	ObjString* AllocateString(char* chars, size_t length);
	ObjString* TakeString(char* chars, size_t length);
	ObjString* TakeString(char* chars);
	ObjString* CopyString(const char* chars, size_t length);
	ObjString* CopyString(const char* chars);
	ObjString* CopyString(std::string path);
	ObjString* CopyString(ObjString* string);
	ObjString* AllocString(size_t length);
	ObjFunction* NewFunction();
	ObjNative* NewNative(NativeFn function);
	ObjAPINative* NewAPINative(APINativeFn function);
	ObjUpvalue* NewUpvalue(VMValue* slot);
	ObjClosure* NewClosure(ObjFunction* function);
	ObjClass* NewClass(Uint32 hash);
	ObjClass* NewClass(const char* className);
	ObjInstance* NewInstance(ObjClass* klass);
	ObjEntity* NewEntity(ObjClass* klass);
	ObjBoundMethod* NewBoundMethod(VMValue receiver, ObjFunction* method);
	ObjArray* NewArray();
	ObjMap* NewMap();
	ObjNamespace* NewNamespace(Uint32 hash);
	ObjNamespace* NewNamespace(const char* nsName);
	ObjEnum* NewEnum(Uint32 hash);
	ObjModule* NewModule();
	Obj* NewNativeInstance(size_t size);

	std::string GetClassName(Uint32 hash);

	VMValue CastValueAsString(VMValue v);
	VMValue ConcatenateValues(VMValue va, VMValue vb);
	VMValue ValueFromProperty(Property property);
};

#ifdef HSL_VM
#define VM_THROW_ERROR(...) thread->ThrowRuntimeError(false, __VA_ARGS__)

namespace ScriptTypes {
int GetInteger(VMValue* args, int index, VMThread* thread);
float GetDecimal(VMValue* args, int index, VMThread* thread);
char* GetString(VMValue* args, int index, VMThread* thread);
ObjString* GetVMString(VMValue* args, int index, VMThread* thread);
ObjArray* GetArray(VMValue* args, int index, VMThread* thread);
ObjMap* GetMap(VMValue* args, int index, VMThread* thread);
ObjBoundMethod* GetBoundMethod(VMValue* args, int index, VMThread* thread);
ObjFunction* GetFunction(VMValue* args, int index, VMThread* thread);
VMValue GetCallable(VMValue* args, int index, VMThread* thread);
ObjInstance* GetInstance(VMValue* args, int index, VMThread* thread);
ObjStream* GetStream(VMValue* args, int index, VMThread* thread);
#ifndef HSL_STANDALONE
CollisionBox GetHitbox(VMValue* args, int index, VMThread* thread);
ObjEntity* GetEntity(VMValue* args, int index, VMThread* thread);
ObjShader* GetShader(VMValue* args, int index, VMThread* thread);
ObjFont* GetFont(VMValue* args, int index, VMThread* thread);
ISprite* GetSpriteIndex(int where, VMThread* thread);
ISprite* GetSprite(VMValue* args, int index, VMThread* thread);
Image* GetImage(VMValue* args, int index, VMThread* thread);
GameTexture* GetTexture(VMValue* args, int index, VMThread* thread);
ISound* GetSound(VMValue* args, int index, VMThread* thread);
ISound* GetMusic(VMValue* args, int index, VMThread* thread);
IModel* GetModel(VMValue* args, int index, VMThread* thread);
MediaBag* GetVideo(VMValue* args, int index, VMThread* thread);
Animator* GetAnimator(VMValue* args, int index, VMThread* thread);
#endif
} // namespace ScriptTypes
#endif

#endif /* ENGINE_BYTECODE_SCRIPTMANAGER_H */
