#include <Engine/Bytecode/Bytecode.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/SourceFileMap.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Bytecode/ValuePrinter.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Hashing/CombinedHash.h>
#include <Engine/TextFormats/XML/XMLParser.h>
#include <Engine/Utilities/StringUtils.h>

#ifdef HSL_VM
#include <Engine/Bytecode/GarbageCollector.h>
#endif

#ifdef HSL_COMPILER
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Exceptions/CompilerErrorException.h>
#endif

#ifndef HSL_STANDALONE
#include <Engine/Bytecode/ScriptEntity.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/ResourceManager.h>
#else
#include <Engine/Bytecode/StandaloneMain.h>
#endif

#ifdef HSL_VM
bool ScriptManager::LoadAllClasses = false;

// #define DEBUG_STRESS_GC

void ScriptManager::RequestGarbageCollection(bool doLog) {
	if (!GC) {
		return;
	}

#ifndef DEBUG_STRESS_GC
	if (GC->GarbageSize > GC->NextGC)
#endif
	{
		size_t startSize = GC->GarbageSize;

		ForceGarbageCollection(doLog);

		if (doLog) {
			Log::Print(Log::LOG_INFO,
#ifndef HSL_STANDALONE
				"%04X: Freed garbage from %u to %u (%d), next GC at %d",
				Scene::Frame,
#else
				"Freed garbage from %u to %u (%d), next GC at %d",
#endif
				(Uint32)startSize,
				(Uint32)GC->GarbageSize,
				GC->GarbageSize - startSize,
				GC->NextGC);
		}
	}
}
void ScriptManager::ForceGarbageCollection(bool doLog) {
	if (Lock()) {
		if (ThreadCount > 1) {
			Unlock();
			return;
		}

		GC->Collect(doLog);

		Unlock();
	}
}

void ScriptManager::ResetStack() {
	if (ThreadCount > 0) {
		Threads[0].ResetStack();
	}
}
#endif

// #region Life Cycle
ScriptManager::ScriptManager() {
	Constants = new HashMap<VMValue>(NULL, 8);

#ifdef HSL_VM
	Globals = new HashMap<VMValue>(NULL, 8);
	Sources = new HashMap<BytecodeContainer>(NULL, 8);
	Classes = new HashMap<ObjClass*>(NULL, 8);
	Modules = new HashMap<ObjModule*>(NULL, 8);
	Tokens = new HashMap<char*>(NULL, 64);

#ifdef VM_DEBUG
	SourceFiles = new HashMap<SourceFile*>(NULL, 8);
#endif

	GC = new GarbageCollector(this);
#endif

	ImplClasses = new HashMap<ObjClass*>(NULL, 8);

	ImplArray = new ArrayImpl(this);
	ImplFunction = new FunctionImpl(this);
	ImplInstance = new InstanceImpl(this);
	ImplMap = new MapImpl(this);
	ImplStream = new StreamImpl(this);
	ImplString = new StringImpl(this);

#ifndef HSL_STANDALONE
	ImplFont = new FontImpl(this);
	ImplEntity = new EntityImpl(this);
	ImplMaterial = new MaterialImpl(this);
	ImplShader = new ShaderImpl(this);
#endif

#ifdef HSL_VM
	ThreadCount = 0;

	for (int i = 0; i < MAX_VM_THREADS; i++) {
		InitThread(&Threads[i]);

		Threads[i].Manager = this;
		Threads[i].ID = i;
	}

#if defined(DEVELOPER_MODE) && defined(VM_DEBUG)
#if defined(WIN32) || defined(LINUX) || defined(MACOSX)
	BreakpointsEnabled = true;
#endif
#ifndef HSL_STANDALONE
	Application::Settings->GetBool("dev", "enableScriptBreakpoints", &BreakpointsEnabled);
#endif
#endif
#endif

#ifdef USE_SDL
	GlobalLock = SDL_CreateMutex();
#endif
}
void ScriptManager::Init() {
	TypeImpl::Init();

#ifndef HSL_STANDALONE
	ScriptEntity::Init();
#endif

#ifdef HSL_STDLIB
	StandardLibrary::Init();
#endif
}
#if defined(HSL_VM) && defined(VM_DEBUG)
Uint32 ScriptManager::GetBranchLimit() {
	int branchLimit = 0;

#ifndef HSL_STANDALONE
	if (Application::Settings->GetInteger("dev", "branchLimit", &branchLimit) == true) {
		if (branchLimit < 0) {
			branchLimit = 0;
		}
	}
	else {
		bool useBranchLimit = false;
		if (Application::Settings->GetBool("dev", "branchLimit", &useBranchLimit) &&
			useBranchLimit == true) {
			branchLimit = DEFAULT_BRANCH_LIMIT;
		}
	}
#endif

	return (Uint32)branchLimit;
}
#endif
ScriptManager::~ScriptManager() {
	if (Constants) {
		Constants->Clear();
		delete Constants;
		Constants = nullptr;
	}

#ifdef HSL_LIBRARY
	ImportScriptHandler = NULL;
	ImportClassHandler = NULL;
	WithIteratorHandler = NULL;
	HasWithIteratorHandler = false;

	if (LastCompileError) {
		Memory::Free(LastCompileError);
		LastCompileError = nullptr;
	}
#endif

#ifdef HSL_VM
	if (ImplClasses) {
		ImplClasses->Clear();
		delete ImplClasses;
		ImplClasses = nullptr;
	}

	if (Globals) {
		Globals->Clear();
		delete Globals;
		Globals = nullptr;
	}

	AllNamespaces.clear();
	ModuleList.clear();
	TempModuleList.clear();

	if (ThreadCount) {
		for (int i = 0; i < ThreadCount; i++) {
			if (!Threads[i].Active) {
				continue;
			}

#ifdef VM_DEBUG
			Threads[i].DisposeBreakpoints();
#endif
			Threads[i].Active = false;
		}

		Threads[0].FrameCount = 0;
		Threads[0].ResetStack();
	}

#ifdef HSL_STANDALONE
	ForceGarbageCollection(ShouldShowGarbageCollectionOutput());
#else
	ForceGarbageCollection(true);
#endif

	if (Sources) {
		Sources->WithAll([](Uint32 hash, BytecodeContainer bytecode) -> void {
			Memory::Free(bytecode.Data);
		});
		Sources->Clear();
		delete Sources;
		Sources = NULL;
	}
	if (Classes) {
		Classes->Clear();
		delete Classes;
		Classes = NULL;
	}
	if (Modules) {
		Modules->Clear();
		delete Modules;
		Modules = NULL;
	}
	if (Tokens) {
		Tokens->WithAll([](Uint32 hash, char* token) -> void {
			Memory::Free(token);
		});
		Tokens->Clear();
		delete Tokens;
		Tokens = NULL;
	}

	if (GC) {
		delete GC;
		GC = NULL;
	}

#ifdef VM_DEBUG
	if (SourceFiles) {
		SourceFiles->WithAll([](Uint32, SourceFile* sourceFile) -> void {
			Memory::Free(sourceFile->Text);
			delete sourceFile;
		});
		SourceFiles->Clear();
		delete SourceFiles;
		SourceFiles = NULL;
	}
#endif
#endif

	if (ImplArray) {
		delete ImplArray;
		ImplArray = nullptr;
	}
	if (ImplFunction) {
		delete ImplFunction;
		ImplFunction = nullptr;
	}
	if (ImplInstance) {
		delete ImplInstance;
		ImplInstance = nullptr;
	}
	if (ImplMap) {
		delete ImplMap;
		ImplMap = nullptr;
	}
	if (ImplStream) {
		delete ImplStream;
		ImplStream = nullptr;
	}
	if (ImplString) {
		delete ImplString;
		ImplString = nullptr;
	}

#ifndef HSL_STANDALONE
	if (ImplFont) {
		delete ImplFont;
		ImplFont = nullptr;
	}
	if (ImplEntity) {
		delete ImplEntity;
		ImplEntity = nullptr;
	}
	if (ImplMaterial) {
		delete ImplMaterial;
		ImplMaterial = nullptr;
	}
	if (ImplShader) {
		delete ImplShader;
		ImplShader = nullptr;
	}
#endif

#ifdef USE_SDL
	if (GlobalLock) {
		SDL_DestroyMutex(GlobalLock);
		GlobalLock = nullptr;
	}
#endif
}
void ScriptManager::Dispose() {
	TypeImpl::Dispose();
}
void ScriptManager::FreeFunction(Obj* object) {
	ObjFunction* function = (ObjFunction*)object;

	Memory::Free(function->Name);

	function->Chunk.Free();
}
void ScriptManager::FreeModule(Obj* object) {
	ObjModule* module = (ObjModule*)object;

	for (size_t i = 0; i < module->Functions->size(); i++) {
		FreeFunction((Obj*)(*module->Functions)[i]);
	}

	Memory::Free(module->SourceFilename);

	delete module->Functions;
	delete module->Locals;
}
void ScriptManager::FreeClass(Obj* object) {
	ObjClass* klass = (ObjClass*)object;

	Memory::Free(klass->Name);

	delete klass->Methods;
	delete klass->Fields;

	if (ImplClasses) {
		ImplClasses->Remove(klass->Hash);
	}
}
void ScriptManager::FreeEnumeration(Obj* object) {
	ObjEnum* enumeration = (ObjEnum*)object;

	Memory::Free(enumeration->Name);

	delete enumeration->Fields;
}
void ScriptManager::FreeNamespace(Obj* object) {
	ObjNamespace* ns = (ObjNamespace*)object;

	Memory::Free(ns->Name);

	delete ns->Fields;
}
#ifdef HSL_VM
void ScriptManager::RemoveTemporaryModules() {
	for (size_t i = 0; i < TempModuleList.size(); i++) {
		ObjModule* module = TempModuleList[i];

#ifdef VM_DEBUG
		for (int i = 0; i < ThreadCount; i++) {
			Threads[i].RemoveBreakpointsForModule(module);
		}
#endif

		auto it = std::find(ModuleList.begin(), ModuleList.end(), module);
		if (it != ModuleList.end()) {
			ModuleList.erase(it);
		}
	}

	TempModuleList.clear();
}
void ScriptManager::InitThread(VMThread* thread) {
	memset(&thread->Stack, 0, sizeof(thread->Stack));
	memset(&thread->RegisterValue, 0, sizeof(thread->RegisterValue));
	memset(&thread->Frames, 0, sizeof(thread->Frames));
	memset(&thread->Name, 0, sizeof(thread->Name));
	memset(&thread->InstructionIgnoreMap, 0, sizeof(thread->InstructionIgnoreMap));

	thread->FrameCount = 0;
	thread->ReturnFrame = 0;

	thread->Active = false;
	thread->StackTop = thread->Stack;

#ifdef VM_DEBUG
	thread->DebugInfo = false;
	thread->AttachedDebuggerCount = 0;
	thread->CurrentBreakpointIndex = 0;
	thread->BranchLimit = GetBranchLimit();
#endif
}
VMThread* ScriptManager::NewThread() {
	for (int i = 0; i < MAX_VM_THREADS; i++) {
		VMThread* thread = &Threads[i];
		if (!thread->Active) {
			InitThread(thread);
			thread->Active = true;
			return thread;
		}
	}
	return nullptr;
}
void ScriptManager::DisposeThread(VMThread* thread) {
	thread->ResetStack();
	thread->Active = false;
}
// #endregion

// #region ValueFuncs
#endif
void ScriptManager::DestroyObject(Obj* object) {
	switch (object->Type) {
	case OBJ_STRING:
		ImplString->Dispose(object);
		break;
	case OBJ_ARRAY:
		ImplArray->Dispose(object);
		break;
	case OBJ_MAP:
		ImplMap->Dispose(object);
		break;
	case OBJ_MODULE:
		FreeModule(object);
		break;
	case OBJ_CLASS:
		FreeClass(object);
		break;
	case OBJ_NAMESPACE:
		FreeNamespace(object);
		break;
	case OBJ_ENUM:
		FreeEnumeration(object);
		break;
	case OBJ_INSTANCE:
	case OBJ_NATIVE_INSTANCE:
	case OBJ_ENTITY: {
		ObjInstance* instance = (ObjInstance*)object;
		if (instance->Destructor != nullptr) {
			instance->Destructor(object);
		}
		break;
	}
	default:
		break;
	}

#ifdef HSL_VM
	if (GC) {
		assert(GC->GarbageSize >= object->Size);
		GC->GarbageSize -= object->Size;
	}
#endif

	Memory::Free(object);
}
// #endregion

// #region GlobalFuncs
bool ScriptManager::Lock() {
#ifdef HSL_STANDALONE
	return LockScriptManager((void*)this);
#elif defined(USE_SDL)
	return SDL_LockMutex(GlobalLock) == 0;
#else
	return true;
#endif
}
bool ScriptManager::Unlock() {
#ifdef HSL_STANDALONE
	return UnlockScriptManager((void*)this);
#elif defined(USE_SDL)
	return SDL_UnlockMutex(GlobalLock) == 0;
#else
	return true;
#endif
}

#ifdef HSL_VM
void ScriptManager::DefineMethod(VMThread* thread, ObjFunction* function, Uint32 hash) {
	VMValue methodValue = OBJECT_VAL(function);

	ObjClass* klass = AS_CLASS(thread->Peek(0));
	klass->Methods->Put(hash, methodValue);

	if (hash == klass->Hash) {
		klass->Initializer = methodValue;
	}

	function->Class = klass;

	thread->Pop();
}
void ScriptManager::DefineNative(ObjClass* klass, const char* name, NativeFn function) {
	if (function == NULL || klass == NULL || name == NULL) {
		return;
	}

	klass->Methods->Put(name, OBJECT_VAL(NewNative(function)));
}
void ScriptManager::GlobalLinkInteger(ObjClass* klass, const char* name, int* value) {
	if (name == NULL) {
		return;
	}

	if (klass == NULL) {
		Globals->Put(name, INTEGER_LINK_VAL(value));
	}
	else {
		klass->Methods->Put(name, INTEGER_LINK_VAL(value));
	}
}
void ScriptManager::GlobalLinkDecimal(ObjClass* klass, const char* name, float* value) {
	if (name == NULL) {
		return;
	}

	if (klass == NULL) {
		Globals->Put(name, DECIMAL_LINK_VAL(value));
	}
	else {
		klass->Methods->Put(name, DECIMAL_LINK_VAL(value));
	}
}
void ScriptManager::GlobalConstInteger(ObjClass* klass, const char* name, int value) {
	if (name == NULL) {
		return;
	}
	if (klass == NULL) {
		Constants->Put(name, INTEGER_VAL(value));
	}
	else {
		klass->Methods->Put(name, INTEGER_VAL(value));
	}
}
void ScriptManager::GlobalConstDecimal(ObjClass* klass, const char* name, float value) {
	if (name == NULL) {
		return;
	}
	if (klass == NULL) {
		Constants->Put(name, DECIMAL_VAL(value));
	}
	else {
		klass->Methods->Put(name, DECIMAL_VAL(value));
	}
}
bool ScriptManager::GetClassMethod(ObjClass* klass, Uint32 hash, VMValue* callable) {
	while (klass != nullptr) {
		// Look for a field in the class which may shadow a method.
		if (klass->Fields->GetIfExists(hash, callable)) {
			return true;
		}

		// There is no field with that name, so look for methods.
		if (klass->Methods->GetIfExists(hash, callable)) {
			return true;
		}

		// Otherwise, walk up the inheritance chain until we find the method.
		klass = klass->Parent;
	}

	return false;
}
bool ScriptManager::GetClassMethod(Obj* object, ObjClass* klass, Uint32 hash, VMValue* callable) {
	while (klass != nullptr) {
		// Look for a field in the class which may shadow a method.
		if (klass->Fields->GetIfExists(hash, callable)) {
			return true;
		}

		// There is no field with that name, so look for methods.
		if (klass->Methods->GetIfExists(hash, callable)) {
			return true;
		}

		// Otherwise, walk up the inheritance chain until we find the method.
		klass = GetClassParent(object, klass);
	}

	return false;
}
ObjClass* ScriptManager::GetClassParent(Obj* object, ObjClass* klass) {
#ifndef HSL_STANDALONE
	if (klass->Parent == nullptr && object->Type == OBJ_ENTITY) {
		ObjEntity* entity = (ObjEntity*)object;
		if (entity->EntityPtr && klass != ImplEntity->ParentClass) {
			return ImplEntity->Class;
		}
	}
#endif
	return klass->Parent;
}
bool ScriptManager::ClassHasMethod(ObjClass* klass, Uint32 hash) {
	VMValue callable;
	return GetClassMethod(klass, hash, &callable);
}
#endif

void ScriptManager::LinkStandardLibrary() {
#ifdef HSL_STDLIB
	StandardLibrary::Link(this);
#endif
}
// #endregion

// #region ObjectFuncs
#ifdef HSL_VM
Bytecode* ScriptManager::ReadBytecode(BytecodeContainer bytecodeContainer) {
	Bytecode* bytecode = new Bytecode();
	if (!bytecode->Read(bytecodeContainer, this, Tokens)) {
		delete bytecode;
		return nullptr;
	}

	return bytecode;
}
Bytecode* ScriptManager::ReadBytecode(Stream *stream) {
	Bytecode* bytecode = new Bytecode();
	if (!bytecode->Read(stream, this, Tokens)) {
		delete bytecode;
		return nullptr;
	}

	return bytecode;
}
ObjModule* ScriptManager::LoadBytecode(Bytecode* bytecode, Uint32 filenameHash) {
	ObjModule* module = NewModule();

	for (size_t i = 0; i < bytecode->Functions.size(); i++) {
		ObjFunction* function = bytecode->Functions[i];
		Chunk* chunk = &function->Chunk;

		module->Functions->push_back(function);

		function->Module = module;
#if USING_VM_FUNCPTRS
		chunk->SetupOpfuncs();
#endif
	}

	if (bytecode->SourceFilename) {
		module->SourceFilename = StringUtils::Duplicate(bytecode->SourceFilename);
		module->HasSourceFilename = true;
	}
	else if (filenameHash) {
		char fnHash[13];
		snprintf(fnHash, sizeof(fnHash), "%08X.ibc", filenameHash);
		module->SourceFilename = StringUtils::Duplicate(fnHash);
		module->HasSourceFilename = false;
	}

	ModuleList.push_back(module);

	if (filenameHash) {
		Modules->Put(filenameHash, module);
	}
	else {
		TempModuleList.push_back(module);
	}

	return module;
}
ObjModule* ScriptManager::LoadBytecode(BytecodeContainer bytecodeContainer, Uint32 filenameHash) {
	Bytecode* bytecode = ReadBytecode(bytecodeContainer);
	if (bytecode) {
		ObjModule* module = LoadBytecode(bytecode, filenameHash);
		delete bytecode;
		return module;
	}
	return nullptr;
}
ObjModule* ScriptManager::LoadBytecode(Stream* stream, Uint32 filenameHash) {
	Bytecode* bytecode = ReadBytecode(stream);
	if (bytecode) {
		ObjModule* module = LoadBytecode(bytecode, filenameHash);
		delete bytecode;
		return module;
	}
	return nullptr;
}
bool ScriptManager::RunBytecode(VMThread* thread, BytecodeContainer bytecodeContainer, Uint32 filenameHash) {
	Bytecode* bytecode = ReadBytecode(bytecodeContainer);
	if (!bytecode) {
		return false;
	}

	ObjModule* module = LoadBytecode(bytecode, filenameHash);

	delete bytecode;

	if (!module) {
		return false;
	}

#ifdef VM_DEBUG
	if (BreakpointsEnabled) {
		AddModuleBreakpoints(thread, module);
	}
#endif

	thread->RunFunction((*module->Functions)[0], 0);

	return true;
}
bool ScriptManager::RunBytecode(VMThread* thread, Stream* stream, Uint32 filenameHash) {
	Bytecode* bytecode = ReadBytecode(stream);
	if (!bytecode) {
		return false;
	}

	ObjModule* module = LoadBytecode(bytecode, filenameHash);

	delete bytecode;

	if (!module) {
		return false;
	}

#ifdef VM_DEBUG
	if (BreakpointsEnabled) {
		AddModuleBreakpoints(thread, module);
	}
#endif

	thread->RunFunction((*module->Functions)[0], 0);

	return true;
}
#ifdef VM_DEBUG
void ScriptManager::AddModuleBreakpoints(VMThread* thread, ObjModule* module) {
	for (size_t i = 0; i < module->Functions->size(); i++) {
		thread->AddFunctionBreakpoints((*module->Functions)[i]);
	}
}
#endif
bool ScriptManager::CallFunction(const char* functionName) {
	if (!Globals->Exists(functionName)) {
		return false;
	}

	VMValue callable = Globals->Get(functionName);
	if (!IS_CALLABLE(callable)) {
		return false;
	}

	Threads[0].InvokeForEntity(callable, 0);
	return true;
}
VMValue ScriptManager::FindFunction(const char* functionName) {
	VMValue callable;

	char* methodName = StringUtils::StrCaseStr(functionName, "::");
	if (methodName) {
		std::string name = std::string(functionName, methodName - functionName);

		methodName += 2;

		if (*methodName == '\0') {
			return NULL_VAL;
		}

		if (!Globals->Exists(name.c_str())) {
			return NULL_VAL;
		}

		VMValue value = Globals->Get(name.c_str());
		if (!IS_CLASS(value)) {
			return NULL_VAL;
		}

		ObjClass* klass = AS_CLASS(value);
		Uint32 hash = Murmur::EncryptString(methodName);
		if (!GetClassMethod(klass, hash, &callable)) {
			return NULL_VAL;
		}
	}
	else {
		if (!Globals->Exists(functionName)) {
			return NULL_VAL;
		}

		callable = Globals->Get(functionName);
	}

	if (!IS_CALLABLE(callable)) {
		return NULL_VAL;
	}

	return callable;
}
#ifndef HSL_STANDALONE
BytecodeContainer ScriptManager::GetBytecodeFromFilenameHash(Uint32 filenameHash) {
	if (Sources->Exists(filenameHash)) {
		return Sources->Get(filenameHash);
	}

	BytecodeContainer bytecode;
	bytecode.Data = nullptr;
	bytecode.Size = 0;

	std::string filenameForHash = GetBytecodeFilenameForHash(filenameHash);
	const char* filename = filenameForHash.c_str();

	if (!ResourceManager::ResourceExists(filename)) {
		return bytecode;
	}

	ResourceStream* stream = ResourceStream::New(filename);
	if (!stream) {
		// Object doesn't exist?
		return bytecode;
	}

	bytecode.Size = stream->Length();
	bytecode.Data = (Uint8*)Memory::TrackedMalloc("Bytecode::Data", bytecode.Size);
	stream->ReadBytes(bytecode.Data, bytecode.Size);
	stream->Close();

	Sources->Put(filenameHash, bytecode);

	return bytecode;
}
bool ScriptManager::BytecodeForFilenameHashExists(Uint32 filenameHash) {
	if (Sources->Exists(filenameHash)) {
		return true;
	}

	std::string filenameForHash = GetBytecodeFilenameForHash(filenameHash);
	const char* filename = filenameForHash.c_str();

	if (!ResourceManager::ResourceExists(filename)) {
		return false;
	}

	ResourceStream* stream = ResourceStream::New(filename);
	if (!stream) {
		return false;
	}

	stream->Close();

	return true;
}
#endif
bool ScriptManager::ScriptExists(const char* name) {
#ifdef HSL_STANDALONE
	return true;
#else
	return BytecodeForFilenameHashExists(MakeFilenameHash(name));
#endif
}
bool ScriptManager::ClassExists(const char* objectName) {
#ifdef HSL_STANDALONE
	return true;
#else
	return SourceFileMap::ClassMap->Exists(objectName);
#endif
}
bool ScriptManager::ClassExists(Uint32 hash) {
#ifdef HSL_STANDALONE
	return true;
#else
	return SourceFileMap::ClassMap->Exists(hash);
#endif
}
bool ScriptManager::IsClassLoaded(const char* className) {
	return ScriptManager::Classes->Exists(className);
}
bool ScriptManager::IsStandardLibraryClass(const char* className) {
	return IS_CLASS(Constants->Get(className));
}
bool ScriptManager::LoadScript(VMThread* thread, const char* filename) {
	if (!filename || !*filename) {
		return false;
	}

#ifdef HSL_LIBRARY
	if (ImportScriptHandler && ImportScriptHandler(filename, (hsl_Thread*)thread) != 0) {
		return true;
	}

	return false;
#elif defined(HSL_STANDALONE) && defined(HSL_COMPILER)
	std::string fullPath = "";
	const char* scriptsDir = GetScriptsDirectory();
	if (scriptsDir != nullptr) {
		fullPath = std::string(scriptsDir) + "/";
	}
	fullPath += std::string(filename);
	filename = fullPath.c_str();

	Stream* stream = File::Open(filename, File::READ_ACCESS);
	if (!stream) {
		Log::Print(Log::LOG_ERROR, "Could not open file \"%s\"!", filename);
		return false;
	}

	bool succeeded = LoadScriptFromStream(thread, stream, filename);
	stream->Close();
	return succeeded;
#else
	Uint32 hash = MakeFilenameHash(filename);
	if (!Sources->Exists(hash)) {
		BytecodeContainer bytecode = GetBytecodeFromFilenameHash(hash);
		if (!bytecode.Data) {
			return false;
		}

		return RunBytecode(thread, bytecode, hash);
	}

	return true;
#endif
}
#ifdef HSL_COMPILER
bool ScriptManager::LoadScriptFromStream(VMThread* thread, Stream* stream, const char* filename) {
	ObjModule* module = CompileScriptFromStream(thread, stream, filename);
	if (!module) {
		return false;
	}

	ObjFunction* function = (*module->Functions)[0];
	if (thread->Call(function, 0)) {
		thread->RunInstructionSet();
		return true;
	}

	return false;
}
ObjModule* ScriptManager::CompileScriptFromStream(VMThread* thread, Stream* stream, const char* filename) {
	size_t size = stream->Length();
	char* code = (char*)Memory::Calloc(size + 1, sizeof(char));
	if (!code) {
		Log::Print(Log::LOG_ERROR, "Out of memory reading script \"%s\"!", filename);
		return nullptr;
	}
	stream->ReadBytes(code, size);

	ObjModule* module = nullptr;
	try {
		module = CompileAndLoad(thread, code, filename, Compiler::Settings);
	} catch (const CompilerErrorException& error) {
		Log::Print(Log::LOG_ERROR, "Could not compile script \"%s\"!\n%s", filename, error.what());
	}
	Memory::Free(code);
	return module;
}
ObjModule* ScriptManager::CompileAndLoad(VMThread* thread, const char* code, const char* filename, CompilerSettings settings) {
	Compiler::PrepareCompiling();

	Compiler* compiler = new Compiler(this);
	compiler->InREPL = false;
	compiler->CurrentSettings = settings;
	compiler->Type = FUNCTIONTYPE_TOPLEVEL;
	compiler->ScopeDepth = 0;
	compiler->Initialize();
	compiler->SetupLocals();

	ObjModule* module = nullptr;
	try {
		module = CompileAndLoad(thread, compiler, code, filename);
	} catch (const CompilerErrorException& error) {
		delete compiler;
		Compiler::FinishCompiling();
		throw error;
	}

	delete compiler;
	Compiler::FinishCompiling();

	return module;
}
ObjModule* ScriptManager::CompileAndLoad(VMThread* thread, Compiler* compiler, const char* code, const char* filename) {
	bool didCompile = false;

	MemoryStream* memStream = MemoryStream::New(0x100);
	if (!memStream) {
		return nullptr;
	}

	try {
		didCompile = compiler->Compile(filename, code, memStream);
	} catch (const CompilerErrorException& error) {
		memStream->Close();

		throw error;
	}

	ObjModule* module = nullptr;

	if (didCompile) {
		Uint32 filenameHash = 0x00000000;
		if (filename) {
			filenameHash = MakeFilenameHash(filename);
		}

		memStream->Seek(0);

		module = LoadBytecode(memStream, filenameHash);

#ifdef VM_DEBUG
		if (module && BreakpointsEnabled) {
			AddModuleBreakpoints(thread, module);
		}
#endif
	}

	memStream->Close();

	return module;
}
#endif
bool ScriptManager::IsScriptLoaded(const char* filename) {
	Uint32 hash = MakeFilenameHash(filename);
	return IsScriptLoaded(hash);
}
bool ScriptManager::IsScriptLoaded(Uint32 filenameHash) {
	return Sources->Exists(filenameHash);
}
ObjModule* ScriptManager::GetScriptModule(const char* filename) {
	Uint32 hash = MakeFilenameHash(filename);
	return GetScriptModule(hash);
}
ObjModule* ScriptManager::GetScriptModule(Uint32 filenameHash) {
	if (Modules->Exists(filenameHash)) {
		return Modules->Get(filenameHash);
	}
	return nullptr;
}
ObjFunction* ScriptManager::GetFunctionAtScriptLine(ObjModule* module, int lineNum) {
	if (lineNum == 0) {
		return (*module->Functions)[0];
	}

	for (size_t i = 0; i < module->Functions->size(); i++) {
		ObjFunction* function = (*module->Functions)[i];
		Chunk* chunk = &function->Chunk;
		if (!chunk->Lines) {
			continue;
		}

		for (int offset = 0; offset < chunk->Count;) {
			int line = chunk->Lines[offset] & 0xFFFF;
			if (line == lineNum) {
				return function;
			}

			offset += Bytecode::GetTotalOpcodeSize(chunk->Code + offset);
		}
	}

	return nullptr;
}
#ifdef HSL_LIBRARY
void ScriptManager::SetImportScriptHandler(hsl_ImportScriptHandler handler) {
	ImportScriptHandler = handler;
}
void ScriptManager::SetImportClassHandler(hsl_ImportClassHandler handler) {
	ImportClassHandler = handler;
}
void ScriptManager::SetWithIteratorHandler(hsl_WithIteratorHandler handler) {
	WithIteratorHandler = handler;
	HasWithIteratorHandler = handler != nullptr;
}
bool ScriptManager::CallWithIteratorHandler(int state, VMValue receiver, int* index, VMValue* newReceiver) {
	int result = WithIteratorHandler(state, (hsl_Value*)&receiver, index, (hsl_Value*)newReceiver);

	return result != 0;
}
#endif
bool ScriptManager::LoadObjectClass(VMThread* thread, const char* objectName) {
	bool succeeded = true;

	if (ScriptManager::IsClassLoaded(objectName)) {
		return true;
	}

	if (!ScriptManager::ClassExists(objectName)) {
		return false;
	}

#ifdef HSL_LIBRARY
	if (!ImportClassHandler) {
		return false;
	}

	if (ImportClassHandler(objectName, (hsl_Thread*)thread) != 0) {
		succeeded = true;
	}
#endif

#ifndef HSL_STANDALONE
	vector<Uint32>* filenameHashList = SourceFileMap::ClassMap->Get(objectName);

	for (size_t fn = 0; fn < filenameHashList->size(); fn++) {
		Uint32 filenameHash = (*filenameHashList)[fn];

		if (!Sources->Exists(filenameHash)) {
			BytecodeContainer bytecode = GetBytecodeFromFilenameHash(filenameHash);
			if (!bytecode.Data) {
				Log::Print(Log::LOG_WARN,
					"Code for the object class \"%s\" does not exist!",
					objectName);
				return false;
			}

			if (fn == 0) {
				Log::Print(Log::LOG_VERBOSE,
					"Loading class %s, %d filename(s)...",
					objectName,
					(int)filenameHashList->size());
			}

			RunBytecode(thread, bytecode, filenameHash);
		}
	}
#endif

	if (succeeded && !IsStandardLibraryClass(objectName) && !Classes->Exists(objectName)) {
		ObjClass* klass = GetObjectClass(objectName);
		if (!klass) {
			Log::Print(Log::LOG_ERROR, "Could not find class of %s!", objectName);
			return false;
		}
		Classes->Put(objectName, klass);
	}

	return succeeded;
}
ObjClass* ScriptManager::GetObjectClass(const char* className) {
	VMValue value = Globals->Get(className);

	if (IS_CLASS(value)) {
		return AS_CLASS(value);
	}

	return nullptr;
}
#ifndef HSL_STANDALONE
void ScriptManager::LoadClasses() {
	SourceFileMap::ClassMap->WithAll([this](Uint32, vector<Uint32>* filenameHashList) -> void {
		for (size_t fn = 0; fn < filenameHashList->size(); fn++) {
			Uint32 filenameHash = (*filenameHashList)[fn];

			BytecodeContainer bytecode = GetBytecodeFromFilenameHash(filenameHash);
			if (!bytecode.Data) {
				Log::Print(
					Log::LOG_WARN, "Class %08X does not exist!", filenameHash);
				continue;
			}

			RunBytecode(&Threads[0], bytecode, filenameHash);
		}
	});

	ScriptManager::ForceGarbageCollection(false);
}
#endif
#ifdef VM_DEBUG
void ScriptManager::LoadSourceCodeLines(SourceFile* sourceFile, char* text) {
	char* ptr = text;
	char* end = text + strlen(text);
	char* lineStart = ptr;

	while (true) {
		if (*ptr == '\n' || ptr == end) {
			sourceFile->Lines.push_back(lineStart);

			if (ptr == end) {
				break;
			}
			else {
				*ptr = '\0';
				lineStart = ptr + 1;
			}
		}

		ptr++;
	}

	sourceFile->Text = text;
	sourceFile->Exists = true;
}
void ScriptManager::LoadSourceCodeLines(SourceFile* sourceFile, const char* sourceFilename) {
	Stream* stream = File::Open(sourceFilename, File::READ_ACCESS);
	if (!stream) {
		return;
	}

	size_t size = stream->Length();
	char* text = (char*)Memory::Calloc(size + 1, sizeof(char));
	stream->ReadBytes(text, size);
	stream->Close();

	LoadSourceCodeLines(sourceFile, text);
}
char* ScriptManager::GetSourceCodeLine(const char* sourceFilename, int line) {
	SourceFile* sourceFile = nullptr;

	if (!SourceFiles->Exists(sourceFilename)) {
		sourceFile = new SourceFile;

		if (File::Exists(sourceFilename)) {
			LoadSourceCodeLines(sourceFile, sourceFilename);
		}

		if (!sourceFile->Exists) {
			Log::Print(Log::LOG_WARN, "Source file \"%s\" does not exist.", sourceFilename);
		}

		SourceFiles->Put(sourceFilename, sourceFile);
	}
	else {
		sourceFile = SourceFiles->Get(sourceFilename);
	}

	if (!sourceFile->Exists || line < 1 || line > sourceFile->Lines.size()) {
		return nullptr;
	}

	return sourceFile->Lines[line - 1];
}
void ScriptManager::AddSourceFile(const char* sourceFilename, char* text) {
	SourceFile* sourceFile = new SourceFile;
	sourceFile->Exists = true;

	LoadSourceCodeLines(sourceFile, text);

	RemoveSourceFile(sourceFilename);
	SourceFiles->Put(sourceFilename, sourceFile);
}
void ScriptManager::RemoveSourceFile(const char* sourceFilename) {
	if (SourceFiles->Exists(sourceFilename)) {
		Memory::Free(SourceFiles->Get(sourceFilename));

		SourceFiles->Remove(sourceFilename);
	}
}
#endif
#endif
Uint32 ScriptManager::MakeFilenameHash(const char* filename) {
	size_t length = strlen(filename);
	const char* dot = strrchr(filename, '.');
	if (dot) {
		length = dot - filename;
	}
	return CombinedHash::EncryptData((const void*)filename, length);
}
std::string ScriptManager::GetBytecodeFilenameForHash(Uint32 filenameHash) {
	char filename[sizeof(OBJECTS_DIR_NAME) + 12];
	snprintf(filename, sizeof filename, "%s%08X.ibc", OBJECTS_DIR_NAME, filenameHash);
	return std::string(filename);
}
// #endregion

#ifdef HSL_VM
namespace ScriptTypes {
int GetInteger(VMValue* args, int index, VMThread* thread) {
	int value = 0;
	switch (args[index].Type) {
	case VAL_INTEGER:
	case VAL_LINKED_INTEGER:
		value = AS_INTEGER(args[index]);
		break;
	default:
		if (VM_THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
			    index + 1,
			    GetTypeString(VAL_INTEGER),
			    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
			thread->ReturnFromNative();
		}
	}
	return value;
}
float GetDecimal(VMValue* args, int index, VMThread* thread) {
	float value = 0.0f;
	switch (args[index].Type) {
	case VAL_DECIMAL:
	case VAL_LINKED_DECIMAL:
		value = AS_DECIMAL(args[index]);
		break;
	case VAL_INTEGER:
	case VAL_LINKED_INTEGER:
		value = AS_DECIMAL(Value::CastAsDecimal(args[index]));
		break;
	default:
		if (VM_THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
			    index + 1,
			    GetTypeString(VAL_DECIMAL),
			    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
			thread->ReturnFromNative();
		}
	}
	return value;
}
char* GetString(VMValue* args, int index, VMThread* thread) {
	char* value = NULL;
	if (thread->Manager->Lock()) {
		if (IS_STRING(args[index])) {
			value = AS_CSTRING(args[index]);
		}
		else {
			if (VM_THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    GetObjectTypeString(OBJ_STRING),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				thread->Manager->Unlock();
				thread->ReturnFromNative();
			}
		}
		thread->Manager->Unlock();
	}
	return value;
}
ObjString* GetVMString(VMValue* args, int index, VMThread* thread) {
	ObjString* value = NULL;
	if (thread->Manager->Lock()) {
		if (IS_STRING(args[index])) {
			value = AS_STRING(args[index]);
		}
		else {
			if (VM_THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    GetObjectTypeString(OBJ_STRING),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				thread->Manager->Unlock();
				thread->ReturnFromNative();
			}
		}
		thread->Manager->Unlock();
	}
	return value;
}
ObjArray* GetArray(VMValue* args, int index, VMThread* thread) {
	ObjArray* value = NULL;
	if (thread->Manager->Lock()) {
		if (IS_ARRAY(args[index])) {
			value = (ObjArray*)(AS_OBJECT(args[index]));
		}
		else {
			if (VM_THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    GetObjectTypeString(OBJ_ARRAY),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				thread->ReturnFromNative();
			}
		}
		thread->Manager->Unlock();
	}
	return value;
}
ObjMap* GetMap(VMValue* args, int index, VMThread* thread) {
	ObjMap* value = NULL;
	if (thread->Manager->Lock()) {
		if (IS_MAP(args[index])) {
			value = (ObjMap*)(AS_OBJECT(args[index]));
		}
		else {
			if (VM_THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    GetObjectTypeString(OBJ_MAP),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				thread->ReturnFromNative();
			}
		}
		thread->Manager->Unlock();
	}
	return value;
}
ObjBoundMethod* GetBoundMethod(VMValue* args, int index, VMThread* thread) {
	ObjBoundMethod* value = NULL;
	if (thread->Manager->Lock()) {
		if (IS_BOUND_METHOD(args[index])) {
			value = (ObjBoundMethod*)(AS_OBJECT(args[index]));
		}
		else {
			if (VM_THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    GetObjectTypeString(OBJ_BOUND_METHOD),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				thread->ReturnFromNative();
			}
		}
		thread->Manager->Unlock();
	}
	return value;
}
ObjFunction* GetFunction(VMValue* args, int index, VMThread* thread) {
	ObjFunction* value = NULL;
	if (thread->Manager->Lock()) {
		if (IS_FUNCTION(args[index])) {
			value = (ObjFunction*)(AS_OBJECT(args[index]));
		}
		else {
			if (VM_THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    GetObjectTypeString(OBJ_FUNCTION),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				thread->ReturnFromNative();
			}
		}
		thread->Manager->Unlock();
	}
	return value;
}
VMValue GetCallable(VMValue* args, int index, VMThread* thread) {
	VMValue value = NULL_VAL;
	if (thread->Manager->Lock()) {
		if (IS_CALLABLE(args[index])) {
			value = args[index];
		}
		else {
			if (VM_THROW_ERROR(
				    "Expected argument %d to be of type callable instead of %s.",
				    index + 1,
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				thread->ReturnFromNative();
			}
		}
		thread->Manager->Unlock();
	}
	return value;
}
ObjInstance* GetInstance(VMValue* args, int index, VMThread* thread) {
	ObjInstance* value = NULL;
	if (thread->Manager->Lock()) {
		if (IS_INSTANCEABLE(args[index])) {
			value = AS_INSTANCE(args[index]);
		}
		else {
			if (VM_THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    GetObjectTypeString(OBJ_INSTANCE),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				thread->ReturnFromNative();
			}
		}
		thread->Manager->Unlock();
	}
	return value;
}
ObjStream* GetStream(VMValue* args, int index, VMThread* thread) {
	ObjStream* value = NULL;
	if (thread->Manager->Lock()) {
		if (IS_STREAM(args[index])) {
			value = AS_STREAM(args[index]);
		}
		else {
			if (VM_THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    Value::GetClassObjectName(thread->Manager->ImplStream->Class),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				thread->ReturnFromNative();
			}
		}
		thread->Manager->Unlock();
	}
	return value;
}
#ifndef HSL_STANDALONE
CollisionBox GetHitbox(VMValue* args, int index, VMThread* thread) {
	CollisionBox box;
	if (IS_HITBOX(args[index])) {
		Sint16* values = AS_HITBOX(args[index]);
		box.Left = values[HITBOX_LEFT];
		box.Top = values[HITBOX_TOP];
		box.Right = values[HITBOX_RIGHT];
		box.Bottom = values[HITBOX_BOTTOM];
	}
	else if (IS_ARRAY(args[index])) {
		if (thread->Manager->Lock()) {
			Sint16 values[NUM_HITBOX_SIDES];

			ObjArray* array = AS_ARRAY(args[index]);

			if (array->Values->size() != NUM_HITBOX_SIDES) {
				if (VM_THROW_ERROR("Expected array to have %d elements instead of %d.",
					    NUM_HITBOX_SIDES,
					    array->Values->size()) == ERROR_RES_CONTINUE) {
					thread->ReturnFromNative();
				}
				thread->Manager->Unlock();
				return box;
			}

			for (int i = 0; i < NUM_HITBOX_SIDES; i++) {
				VMValue value = (*array->Values)[i];
				if (!IS_INTEGER(value)) {
					VM_THROW_ERROR(
						"Expected value at index %d to be of type %s instead of %s.",
						i,
						GetTypeString(VAL_INTEGER),
						GetValueTypeString(value));
					thread->Manager->Unlock();
					return box;
				}
				values[i] = AS_INTEGER(value);
			}

			box.Left = values[HITBOX_LEFT];
			box.Top = values[HITBOX_TOP];
			box.Right = values[HITBOX_RIGHT];
			box.Bottom = values[HITBOX_BOTTOM];

			thread->Manager->Unlock();
		}
	}
	else {
		if (VM_THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
			    index + 1,
			    GetTypeString(VAL_HITBOX),
			    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
			thread->ReturnFromNative();
		}
	}
	return box;
}
ObjEntity* GetEntity(VMValue* args, int index, VMThread* thread) {
	ObjEntity* value = NULL;
	if (thread->Manager->Lock()) {
		if (IS_ENTITY(args[index])) {
			value = AS_ENTITY(args[index]);
		}
		else {
			if (VM_THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    GetObjectTypeString(OBJ_ENTITY),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				thread->ReturnFromNative();
			}
		}
		thread->Manager->Unlock();
	}
	return value;
}
ObjShader* GetShader(VMValue* args, int index, VMThread* thread) {
	ObjShader* value = nullptr;
	if (thread->Manager->Lock()) {
		if (IS_SHADER(args[index])) {
			value = AS_SHADER(args[index]);
		}
		else {
			if (VM_THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    Value::GetClassObjectName(thread->Manager->ImplShader->Class),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				thread->ReturnFromNative();
			}
		}
		thread->Manager->Unlock();
	}
	return value;
}
ObjFont* GetFont(VMValue* args, int index, VMThread* thread) {
	ObjFont* value = nullptr;
	if (thread->Manager->Lock()) {
		if (IS_FONT(args[index])) {
			value = AS_FONT(args[index]);
		}
		else {
			if (VM_THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    Value::GetClassObjectName(thread->Manager->ImplFont->Class),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				thread->ReturnFromNative();
			}
		}
		thread->Manager->Unlock();
	}
	return value;
}
ISprite* GetSpriteIndex(int where, VMThread* thread) {
	if (where < 0 || where >= (int)Scene::SpriteList.size()) {
		if (VM_THROW_ERROR("Sprite index \"%d\" outside bounds of list.", where) ==
			ERROR_RES_CONTINUE) {
			thread->ReturnFromNative();
		}

		return NULL;
	}

	if (!Scene::SpriteList[where]) {
		return NULL;
	}

	return Scene::SpriteList[where]->AsSprite;
}
ISprite* GetSprite(VMValue* args, int index, VMThread* thread) {
	int where = GetInteger(args, index, thread);
	return GetSpriteIndex(where, thread);
}
Image* GetImage(VMValue* args, int index, VMThread* thread) {
	int where = GetInteger(args, index, thread);
	if (where < 0 || where >= (int)Scene::ImageList.size()) {
		if (VM_THROW_ERROR("Image index \"%d\" outside bounds of list.", where) ==
			ERROR_RES_CONTINUE) {
			thread->ReturnFromNative();
		}

		return NULL;
	}

	if (!Scene::ImageList[where]) {
		return NULL;
	}

	return Scene::ImageList[where]->AsImage;
}
GameTexture* GetTexture(VMValue* args, int index, VMThread* thread) {
	int where = GetInteger(args, index, thread);
	if (where < 0 || where >= (int)Scene::TextureList.size()) {
		if (VM_THROW_ERROR("Texture index \"%d\" outside bounds of list.", where) ==
			ERROR_RES_CONTINUE) {
			thread->ReturnFromNative();
		}

		return NULL;
	}

	if (!Scene::TextureList[where]) {
		return NULL;
	}

	return Scene::TextureList[where];
}
ISound* GetSound(VMValue* args, int index, VMThread* thread) {
	int where = GetInteger(args, index, thread);
	if (where < 0 || where >= (int)Scene::SoundList.size()) {
		if (VM_THROW_ERROR("Sound index \"%d\" outside bounds of list.", where) ==
			ERROR_RES_CONTINUE) {
			thread->ReturnFromNative();
		}

		return NULL;
	}

	if (!Scene::SoundList[where]) {
		return NULL;
	}

	return Scene::SoundList[where]->AsSound;
}
ISound* GetMusic(VMValue* args, int index, VMThread* thread) {
	int where = GetInteger(args, index, thread);
	if (where < 0 || where >= (int)Scene::MusicList.size()) {
		if (VM_THROW_ERROR("Music index \"%d\" outside bounds of list.", where) ==
			ERROR_RES_CONTINUE) {
			thread->ReturnFromNative();
		}

		return NULL;
	}

	if (!Scene::MusicList[where]) {
		return NULL;
	}

	return Scene::MusicList[where]->AsMusic;
}
IModel* GetModel(VMValue* args, int index, VMThread* thread) {
	int where = GetInteger(args, index, thread);
	if (where < 0 || where >= (int)Scene::ModelList.size()) {
		if (VM_THROW_ERROR("Model index \"%d\" outside bounds of list.", where) ==
			ERROR_RES_CONTINUE) {
			thread->ReturnFromNative();
		}

		return NULL;
	}

	if (!Scene::ModelList[where]) {
		return NULL;
	}

	return Scene::ModelList[where]->AsModel;
}
MediaBag* GetVideo(VMValue* args, int index, VMThread* thread) {
	int where = GetInteger(args, index, thread);
	if (where < 0 || where >= (int)Scene::MediaList.size()) {
		if (VM_THROW_ERROR("Video index \"%d\" outside bounds of list.", where) ==
			ERROR_RES_CONTINUE) {
			thread->ReturnFromNative();
		}

		return NULL;
	}

	if (!Scene::MediaList[where]) {
		return NULL;
	}

	return Scene::MediaList[where]->AsMedia;
}
Animator* GetAnimator(VMValue* args, int index, VMThread* thread) {
	int where = GetInteger(args, index, thread);
	if (where < 0 || where >= (int)Scene::AnimatorList.size()) {
		if (VM_THROW_ERROR("Animator index \"%d\" outside bounds of list.", where) ==
			ERROR_RES_CONTINUE) {
			thread->ReturnFromNative();
		}

		return NULL;
	}

	if (!Scene::AnimatorList[where]) {
		return NULL;
	}

	return Scene::AnimatorList[where];
}
#endif
} // namespace ScriptTypes

// NOTE:
// Integers specifically need to be whole integers.
// Floats can be just any countable real number.
int ScriptManager::GetInteger(VMValue* args, int index, VMThread* thread) {
	return ScriptTypes::GetInteger(args, index, thread);
}
float ScriptManager::GetDecimal(VMValue* args, int index, VMThread* thread) {
	return ScriptTypes::GetDecimal(args, index, thread);
}
char* ScriptManager::GetString(VMValue* args, int index, VMThread* thread) {
	return ScriptTypes::GetString(args, index, thread);
}
ObjString* ScriptManager::GetVMString(VMValue* args, int index, VMThread* thread) {
	return ScriptTypes::GetVMString(args, index, thread);
}
ObjArray* ScriptManager::GetArray(VMValue* args, int index, VMThread* thread) {
	return ScriptTypes::GetArray(args, index, thread);
}
ObjMap* ScriptManager::GetMap(VMValue* args, int index, VMThread* thread) {
	return ScriptTypes::GetMap(args, index, thread);
}
ObjInstance* ScriptManager::GetInstance(VMValue* args, int index, VMThread* thread) {
	return ScriptTypes::GetInstance(args, index, thread);
}
ObjFunction* ScriptManager::GetFunction(VMValue* args, int index, VMThread* thread) {
	return ScriptTypes::GetFunction(args, index, thread);
}
#ifndef HSL_STANDALONE
ISprite* ScriptManager::GetSprite(VMValue* args, int index, VMThread* thread) {
	return ScriptTypes::GetSprite(args, index, thread);
}
Image* ScriptManager::GetImage(VMValue* args, int index, VMThread* thread) {
	return ScriptTypes::GetImage(args, index, thread);
}
ISound* ScriptManager::GetSound(VMValue* args, int index, VMThread* thread) {
	return ScriptTypes::GetSound(args, index, thread);
}
ObjEntity* ScriptManager::GetEntity(VMValue* args, int index, VMThread* thread) {
	return ScriptTypes::GetEntity(args, index, thread);
}
ObjShader* ScriptManager::GetShader(VMValue* args, int index, VMThread* thread) {
	return ScriptTypes::GetShader(args, index, thread);
}
ObjFont* ScriptManager::GetFont(VMValue* args, int index, VMThread* thread) {
	return ScriptTypes::GetFont(args, index, thread);
}
#endif

void ScriptManager::CheckArgCount(int argCount, int expects, VMThread* thread) {
	if (argCount != expects) {
		if (VM_THROW_ERROR("Expected %d arguments but got %d.", expects, argCount) ==
			ERROR_RES_CONTINUE) {
			thread->ReturnFromNative();
		}
	}
}
void ScriptManager::CheckAtLeastArgCount(int argCount, int expects, VMThread* thread) {
	if (argCount < expects) {
		if (VM_THROW_ERROR("Expected at least %d arguments but got %d.", expects, argCount) ==
			ERROR_RES_CONTINUE) {
			thread->ReturnFromNative();
		}
	}
}
#endif

#define ALLOCATE_OBJ(type, objectType) (type*)AllocateObject(sizeof(type), objectType)
#define ALLOCATE(type, size) (type*)Memory::TrackedMalloc(#type, sizeof(type) * size)

Obj* ScriptManager::AllocateObject(size_t size, ObjType type) {
#ifdef HSL_VM
	if (GC) {
		// Only do this when allocating more memory
		GC->GarbageSize += size;
	}
#endif

	Obj* object = (Obj*)Memory::TrackedCalloc("AllocateObject", 1, size);
	object->Size = size;
	object->Type = type;
#ifdef HSL_VM
	object->Next = RootObject;
	RootObject = object;
#endif

	return object;
}

ObjString* ScriptManager::AllocateString(char* chars, size_t length) {
	return (ObjString*)ImplString->New(chars, length);
}

ObjString* ScriptManager::TakeString(char* chars, size_t length) {
	return AllocateString(chars, length);
}
ObjString* ScriptManager::TakeString(char* chars) {
	return TakeString(chars, strlen(chars));
}
ObjString* ScriptManager::CopyString(const char* chars, size_t length) {
	char* heapChars = ALLOCATE(char, length + 1);
	if (!heapChars) {
		return nullptr;
	}

	if (chars) {
		memcpy(heapChars, chars, length);
	}

	heapChars[length] = '\0';

	return AllocateString(heapChars, length);
}
ObjString* ScriptManager::CopyString(const char* chars) {
	return CopyString(chars, strlen(chars));
}
ObjString* ScriptManager::CopyString(std::string string) {
	return CopyString(string.c_str());
}
ObjString* ScriptManager::CopyString(ObjString* string) {
	char* heapChars = ALLOCATE(char, string->Length + 1);
	if (!heapChars) {
		return nullptr;
	}

	memcpy(heapChars, string->Chars, string->Length);
	heapChars[string->Length] = '\0';

	return AllocateString(heapChars, string->Length);
}
ObjString* ScriptManager::AllocString(size_t length) {
	char* heapChars = ALLOCATE(char, length + 1);
	heapChars[length] = '\0';

	return AllocateString(heapChars, length);
}

#ifdef HSL_VM
VMValue ScriptManager::VM_GetClass(int argCount, VMValue* args, VMThread* thread) {
	CheckArgCount(argCount, 1, thread);

	if (IS_OBJECT(args[0])) {
		return OBJECT_VAL(AS_OBJECT(args[0])->Class);
	}

	return NULL_VAL;
}
#endif

ObjFunction* ScriptManager::NewFunction() {
	return (ObjFunction*)ImplFunction->New();
}
ObjNative* ScriptManager::NewNative(NativeFn function) {
	ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE_FUNCTION);
	Memory::Track(native, "NewNative");
	native->Function = function;
	return native;
}
ObjAPINative* ScriptManager::NewAPINative(APINativeFn function) {
	ObjAPINative* native = ALLOCATE_OBJ(ObjAPINative, OBJ_API_NATIVE_FUNCTION);
	Memory::Track(native, "NewAPINative");
	native->Function = function;
	return native;
}
ObjUpvalue* ScriptManager::NewUpvalue(VMValue* slot) {
	ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
	upvalue->Closed = NULL_VAL;
	upvalue->Value = slot;
	return upvalue;
}
ObjClosure* ScriptManager::NewClosure(ObjFunction* function) {
#if 0
	ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->UpvalueCount);
	for (int i = 0; i < function->UpvalueCount; i++) {
		upvalues[i] = NULL;
	}

	ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
	closure->Function = function;
	closure->Upvalues = upvalues;
	closure->UpvalueCount = function->UpvalueCount;
	return closure;
#else
	return nullptr;
#endif
}
ObjClass* ScriptManager::NewClass(Uint32 hash) {
	ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
	Memory::Track(klass, "NewClass");
	klass->Hash = hash;
	klass->Methods = new Table(NULL, 4);
	klass->Fields = new Table(NULL, 16);
	klass->Initializer = NULL_VAL;
	klass->Name = StringUtils::Create(GetClassName(hash));
#ifdef HSL_VM
	DefineNative(klass, "GetClass", VM_GetClass);
#endif
	return klass;
}
ObjClass* ScriptManager::NewClass(const char* className) {
	ObjClass* klass = NewClass(GetClassHash(className));
	klass->Name = (char*)Memory::Realloc(klass->Name, strlen(className) + 1);
	memcpy(klass->Name, className, strlen(className) + 1);
	return klass;
}
ObjInstance* ScriptManager::NewInstance(ObjClass* klass) {
	ObjInstance* instance = (ObjInstance*)ImplInstance->New(sizeof(ObjInstance), OBJ_INSTANCE);
	instance->Object.Class = klass;
	return instance;
}
ObjEntity* ScriptManager::NewEntity(ObjClass* klass) {
#ifndef HSL_STANDALONE
	return (ObjEntity*)ImplEntity->New(klass);
#else
	return nullptr;
#endif
}
ObjBoundMethod* ScriptManager::NewBoundMethod(VMValue receiver, ObjFunction* method) {
	ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);
	Memory::Track(bound, "NewBoundMethod");
	bound->Receiver = receiver;
	bound->Method = method;
	return bound;
}
ObjArray* ScriptManager::NewArray() {
	return (ObjArray*)ImplArray->New();
}
ObjMap* ScriptManager::NewMap() {
	return (ObjMap*)ImplMap->New();
}
ObjNamespace* ScriptManager::NewNamespace(Uint32 hash) {
	ObjNamespace* ns = ALLOCATE_OBJ(ObjNamespace, OBJ_NAMESPACE);
	Memory::Track(ns, "NewNamespace");
	ns->Fields = new Table(NULL, 16);
	ns->Name = StringUtils::Create(GetClassName(hash));
	return ns;
}
ObjNamespace* ScriptManager::NewNamespace(const char* nsName) {
	ObjNamespace* ns = NewNamespace(GetClassHash(nsName));
	ns->Name = (char*)Memory::Realloc(ns->Name, strlen(nsName) + 1);
	memcpy(ns->Name, nsName, strlen(nsName) + 1);
	return ns;
}
ObjEnum* ScriptManager::NewEnum(Uint32 hash) {
	ObjEnum* enumeration = ALLOCATE_OBJ(ObjEnum, OBJ_ENUM);
	Memory::Track(enumeration, "NewEnum");
	enumeration->Fields = new Table(NULL, 16);
	enumeration->Name = StringUtils::Create(GetClassName(hash));
	return enumeration;
}
ObjModule* ScriptManager::NewModule() {
	ObjModule* module = ALLOCATE_OBJ(ObjModule, OBJ_MODULE);
	Memory::Track(module, "NewModule");
	module->Functions = new vector<ObjFunction*>();
	module->Locals = new vector<VMValue>();
	return module;
}
Obj* ScriptManager::NewNativeInstance(size_t size) {
	Obj* obj = ImplInstance->New(size, OBJ_NATIVE_INSTANCE);
	Memory::Track(obj, "NewNativeInstance");
	return obj;
}

bool ScriptManager::GetArity(VMValue callee, int& minArity, int& maxArity) {
	if (IS_OBJECT(callee)) {
		switch (OBJECT_TYPE(callee)) {
		case OBJ_BOUND_METHOD: {
			ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
			minArity = bound->Method->MinArity;
			maxArity = bound->Method->Arity;
			return true;
		}
		case OBJ_FUNCTION: {
			ObjFunction* function = AS_FUNCTION(callee);
			minArity = function->MinArity;
			maxArity = function->Arity;
			return true;
		}
		// No way to know. (Yet)
		case OBJ_NATIVE_FUNCTION:
		case OBJ_API_NATIVE_FUNCTION:
		default:
			break;
		}
	}
	return false;
}

std::string ScriptManager::GetClassName(Uint32 hash) {
#ifdef HSL_VM
	if (Tokens && Tokens->Exists(hash)) {
		char* t = Tokens->Get(hash);
		return std::string(t);
	}
	else
#endif
	{
		char nameHash[9];
		snprintf(nameHash, sizeof(nameHash), "%8X", hash);
		return std::string(nameHash);
	}
}

VMValue ScriptManager::CastValueAsString(VMValue v) {
	if (IS_STRING(v)) {
		return v;
	}

	char* buffer = (char*)malloc(512);
	PrintBuffer buffer_info;
	buffer_info.Buffer = &buffer;
	buffer_info.WriteIndex = 0;
	buffer_info.BufferSize = 512;
	ValuePrinter::Print(&buffer_info, v, false);
	v = OBJECT_VAL(CopyString(buffer, buffer_info.WriteIndex));
	free(buffer);
	return v;
}
VMValue ScriptManager::ConcatenateValues(VMValue va, VMValue vb) {
	ObjString* a = AS_STRING(va);
	ObjString* b = AS_STRING(vb);

	size_t length = a->Length + b->Length;
	ObjString* result = AllocString(length);

	memcpy(result->Chars, a->Chars, a->Length);
	memcpy(result->Chars + a->Length, b->Chars, b->Length);
	result->Chars[length] = 0;
	return OBJECT_VAL(result);
}
VMValue ScriptManager::ValueFromProperty(Property property) {
	switch (property.Type) {
	case PROPERTY_NULL:
		return NULL_VAL;
	case PROPERTY_INTEGER:
		return INTEGER_VAL(property.as.Integer);
	case PROPERTY_DECIMAL:
		return DECIMAL_VAL(property.as.Decimal);
	case PROPERTY_BOOL:
		return INTEGER_VAL(property.as.Bool ? 1 : 0);
	case PROPERTY_STRING:
		return OBJECT_VAL(CopyString(property.as.String));
	case PROPERTY_ARRAY: {
		if (ScriptManager::Lock()) {
			ObjArray* array = NewArray();
			for (size_t i = 0; i < property.as.Array.Count; i++) {
				Property* data = (Property*)property.as.Array.Data;
				array->Values->push_back(ValueFromProperty(data[i]));
			}
			ScriptManager::Unlock();
			return OBJECT_VAL(array);
		}
		return NULL_VAL;
	}
	default:
		break;
	}

	return NULL_VAL;
}
