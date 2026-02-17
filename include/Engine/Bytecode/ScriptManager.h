#ifndef ENGINE_BYTECODE_SCRIPTMANAGER_H
#define ENGINE_BYTECODE_SCRIPTMANAGER_H
class ScriptEntity;

#include <Engine/Bytecode/Bytecode.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/VMThread.h>
#include <Engine/Exceptions/ScriptException.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>

#ifdef HSL_COMPILER
#include <Engine/Bytecode/Compiler.h>
#endif

#ifndef HSL_STANDALONE
#include <Engine/Types/Entity.h>
#include <Engine/ResourceTypes/ISound.h>
#include <Engine/ResourceTypes/ISprite.h>
#endif

#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#endif

#ifdef USE_SDL
#include <Engine/Includes/StandardSDL2.h>
#endif

#define OBJECTS_DIR_NAME "Objects/"

class ScriptManager {
private:
#ifdef HSL_LIBRARY
	static hsl_ImportScriptHandler ImportScriptHandler;
	static hsl_ImportClassHandler ImportClassHandler;
#endif

#if defined(HSL_VM) && defined(VM_DEBUG)
	static Uint32 GetBranchLimit();
	static void LoadSourceCodeLines(SourceFile* sourceFile, char* text);
	static void LoadSourceCodeLines(SourceFile* sourceFile, const char* sourceFilename);
#endif

public:
#ifdef HSL_VM
	static bool LoadAllClasses;
#ifdef VM_DEBUG
	static bool BreakpointsEnabled;
#endif
	static HashMap<VMValue>* Globals;
	static VMThread Threads[8];
	static Uint32 ThreadCount;
	static vector<ObjModule*> ModuleList;
	static vector<ObjModule*> TempModuleList;
	static HashMap<BytecodeContainer>* Sources;
	static HashMap<ObjClass*>* Classes;
	static HashMap<ObjModule*>* Modules;
	static HashMap<char*>* Tokens;
	static vector<ObjNamespace*> AllNamespaces;
	static vector<ObjClass*> ClassImplList;
#ifdef USE_SDL
	static SDL_mutex* GlobalLock;
#endif
#endif
	static HashMap<VMValue>* Constants;

	static void DestroyObject(Obj* object);
	static void FreeFunction(Obj* object);
	static void FreeModule(Obj* object);
	static void FreeClass(Obj* object);
	static void FreeEnumeration(Obj* object);
	static void FreeNamespace(Obj* object);
#ifdef HSL_VM
	static void RemoveTemporaryModules();
	static void RequestGarbageCollection(bool doLog);
	static void ForceGarbageCollection(bool doLog);
	static void ResetStack();
#endif
	static void Init();
	static void Dispose();
	static bool Lock();
	static void Unlock();
#ifdef HSL_VM
	static VMThread* NewThread();
	static void DisposeThread(VMThread* thread);
	static bool DoIntegerConversion(VMValue& value, Uint32 threadID);
	static bool DoDecimalConversion(VMValue& value, Uint32 threadID);
	static void DefineMethod(VMThread* thread, ObjFunction* function, Uint32 hash);
	static void DefineNative(ObjClass* klass, const char* name, NativeFn function);
	static void GlobalLinkInteger(ObjClass* klass, const char* name, int* value);
	static void GlobalLinkDecimal(ObjClass* klass, const char* name, float* value);
	static void GlobalConstInteger(ObjClass* klass, const char* name, int value);
	static void GlobalConstDecimal(ObjClass* klass, const char* name, float value);
	static ObjClass* GetClassParent(Obj* object, ObjClass* klass);
	static bool GetClassMethod(ObjClass* klass, Uint32 hash, VMValue* callable);
	static bool GetClassMethod(Obj* object, ObjClass* klass, Uint32 hash, VMValue* callable);
	static bool ClassHasMethod(ObjClass* klass, Uint32 hash);
#endif
	static void LinkStandardLibrary();
	static void LinkExtensions();
#ifdef HSL_VM
	static Bytecode* ReadBytecode(BytecodeContainer bytecodeContainer);
	static Bytecode* ReadBytecode(Stream* stream);
	static ObjModule* LoadBytecode(VMThread* thread, Bytecode* bytecode, Uint32 filenameHash);
	static ObjModule* LoadBytecode(VMThread* thread, BytecodeContainer bytecodeContainer, Uint32 filenameHash);
	static ObjModule* LoadBytecode(VMThread* thread, Stream* stream, Uint32 filenameHash);
	static bool RunBytecode(VMThread* thread, BytecodeContainer bytecodeContainer, Uint32 filenameHash);
	static bool RunBytecode(VMThread* thread, Stream* stream, Uint32 filenameHash);
	static bool CallFunction(const char* functionName);
	static VMValue FindFunction(const char* functionName);
#ifndef HSL_STANDALONE
	static BytecodeContainer GetBytecodeFromFilenameHash(Uint32 filenameHash);
	static bool BytecodeForFilenameHashExists(Uint32 filenameHash);
#endif
	static bool ScriptExists(const char* name);
	static bool ClassExists(const char* objectName);
	static bool ClassExists(Uint32 hash);
	static bool IsClassLoaded(const char* className);
	static bool IsStandardLibraryClass(const char* className);
	static bool LoadScript(VMThread* thread, const char* filename);
#ifdef HSL_COMPILER
	static bool LoadScriptFromStream(VMThread* thread, Stream* stream, const char* filename);
	static ObjModule* CompileScriptFromStream(VMThread* thread, Stream* stream, const char* filename);
	static ObjModule* CompileAndLoad(VMThread* thread, const char* code, const char* filename, CompilerSettings settings);
	static ObjModule* CompileAndLoad(VMThread* thread, Compiler* compiler, const char* code, const char* filename);
#endif
	static bool IsScriptLoaded(const char* filename);
	static bool IsScriptLoaded(Uint32 filenameHash);
	static ObjModule* GetScriptModule(const char* filename);
	static ObjModule* GetScriptModule(Uint32 filenameHash);
	static ObjFunction* GetFunctionAtScriptLine(ObjModule* module, int lineNum);
#ifdef HSL_LIBRARY
	static void SetImportScriptHandler(hsl_ImportScriptHandler handler);
	static void SetImportClassHandler(hsl_ImportClassHandler handler);
#endif
	static bool LoadObjectClass(VMThread* thread, const char* objectName);
	static ObjClass* GetObjectClass(const char* className);
#ifndef HSL_STANDALONE
	static void LoadClasses();
#endif
#ifdef VM_DEBUG
	static char* GetSourceCodeLine(const char* sourceFilename, int line);
	static void AddSourceFile(const char* sourceFilename, char* text);
	static void RemoveSourceFile(const char* sourceFilename);
#endif
#endif
	static Uint32 MakeFilenameHash(const char* filename);
	static std::string GetBytecodeFilenameForHash(Uint32 filenameHash);
#ifdef HSL_VM
	static int GetInteger(VMValue* args, int index, Uint32 threadID);
	static float GetDecimal(VMValue* args, int index, Uint32 threadID);
	static char* GetString(VMValue* args, int index, Uint32 threadID);
	static ObjString* GetVMString(VMValue* args, int index, Uint32 threadID);
	static ObjArray* GetArray(VMValue* args, int index, Uint32 threadID);
	static ObjMap* GetMap(VMValue* args, int index, Uint32 threadID);
	static ObjInstance* GetInstance(VMValue* args, int index, Uint32 threadID);
	static ObjFunction* GetFunction(VMValue* args, int index, Uint32 threadID);
#ifndef HSL_STANDALONE
	static ISprite* GetSprite(VMValue* args, int index, Uint32 threadID);
	static Image* GetImage(VMValue* args, int index, Uint32 threadID);
	static ISound* GetSound(VMValue* args, int index, Uint32 threadID);
	static ObjEntity* GetEntity(VMValue* args, int index, Uint32 threadID);
	static ObjShader* GetShader(VMValue* args, int index, Uint32 threadID);
	static ObjFont* GetFont(VMValue* args, int index, Uint32 threadID);
#endif
	static void CheckArgCount(int argCount, int expects);
	static void CheckAtLeastArgCount(int argCount, int expects);
#endif
};

#ifdef HSL_VM
#define VM_THROW_ERROR(...) ScriptManager::Threads[threadID].ThrowRuntimeError(false, __VA_ARGS__)

namespace ScriptTypes {
int GetInteger(VMValue* args, int index, Uint32 threadID);
float GetDecimal(VMValue* args, int index, Uint32 threadID);
char* GetString(VMValue* args, int index, Uint32 threadID);
ObjString* GetVMString(VMValue* args, int index, Uint32 threadID);
ObjArray* GetArray(VMValue* args, int index, Uint32 threadID);
ObjMap* GetMap(VMValue* args, int index, Uint32 threadID);
ObjBoundMethod* GetBoundMethod(VMValue* args, int index, Uint32 threadID);
ObjFunction* GetFunction(VMValue* args, int index, Uint32 threadID);
VMValue GetCallable(VMValue* args, int index, Uint32 threadID);
ObjInstance* GetInstance(VMValue* args, int index, Uint32 threadID);
ObjStream* GetStream(VMValue* args, int index, Uint32 threadID);
#ifndef HSL_STANDALONE
CollisionBox GetHitbox(VMValue* args, int index, Uint32 threadID);
ObjEntity* GetEntity(VMValue* args, int index, Uint32 threadID);
ObjShader* GetShader(VMValue* args, int index, Uint32 threadID);
ObjFont* GetFont(VMValue* args, int index, Uint32 threadID);
ISprite* GetSpriteIndex(int where, Uint32 threadID);
ISprite* GetSprite(VMValue* args, int index, Uint32 threadID);
Image* GetImage(VMValue* args, int index, Uint32 threadID);
GameTexture* GetTexture(VMValue* args, int index, Uint32 threadID);
ISound* GetSound(VMValue* args, int index, Uint32 threadID);
ISound* GetMusic(VMValue* args, int index, Uint32 threadID);
IModel* GetModel(VMValue* args, int index, Uint32 threadID);
MediaBag* GetVideo(VMValue* args, int index, Uint32 threadID);
Animator* GetAnimator(VMValue* args, int index, Uint32 threadID);
#endif
} // namespace ScriptTypes
#endif

#endif /* ENGINE_BYTECODE_SCRIPTMANAGER_H */
