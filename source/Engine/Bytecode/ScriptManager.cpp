#include <Engine/Bytecode/Bytecode.h>
#include <Engine/Bytecode/GarbageCollector.h>
#include <Engine/Bytecode/ScriptEntity.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/SourceFileMap.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/EntityImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Bytecode/ValuePrinter.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Hashing/CombinedHash.h>
#include <Engine/Hashing/FNV1A.h>
#include <Engine/ResourceTypes/ResourceManager.h>
#include <Engine/TextFormats/XML/XMLParser.h>

#include <Engine/Bytecode/Compiler.h>

bool ScriptManager::LoadAllClasses = false;

VMThread ScriptManager::Threads[8];
Uint32 ScriptManager::ThreadCount = 1;

HashMap<VMValue>* ScriptManager::Globals = NULL;
HashMap<VMValue>* ScriptManager::Constants = NULL;

vector<ObjModule*> ScriptManager::ModuleList;

HashMap<BytecodeContainer>* ScriptManager::Sources = NULL;
HashMap<ObjClass*>* ScriptManager::Classes = NULL;
HashMap<char*>* ScriptManager::Tokens = NULL;
vector<ObjNamespace*> ScriptManager::AllNamespaces;
vector<ObjClass*> ScriptManager::ClassImplList;

SDL_mutex* ScriptManager::GlobalLock = NULL;

#ifdef VM_DEBUG
static HashMap<SourceFile*>* SourceFiles = nullptr;
#endif

// #define DEBUG_STRESS_GC

void ScriptManager::RequestGarbageCollection() {
#ifndef DEBUG_STRESS_GC
	if (GarbageCollector::GarbageSize > GarbageCollector::NextGC)
#endif
	{
		size_t startSize = GarbageCollector::GarbageSize;

		ForceGarbageCollection();

		// startSize = GarbageCollector::GarbageSize -
		// startSize;
		Log::Print(Log::LOG_INFO,
			"%04X: Freed garbage from %u to %u (%d), next GC at %d",
			Scene::Frame,
			(Uint32)startSize,
			(Uint32)GarbageCollector::GarbageSize,
			GarbageCollector::GarbageSize - startSize,
			GarbageCollector::NextGC);
	}
}
void ScriptManager::ForceGarbageCollection() {
	if (ScriptManager::Lock()) {
		if (ScriptManager::ThreadCount > 1) {
			ScriptManager::Unlock();
			return;
		}

		GarbageCollector::Collect();

		ScriptManager::Unlock();
	}
}

void ScriptManager::ResetStack() {
	Threads[0].ResetStack();
}

// #region Life Cycle
void ScriptManager::Init() {
	if (Globals == NULL) {
		Globals = new HashMap<VMValue>(NULL, 8);
	}
	if (Constants == NULL) {
		Constants = new HashMap<VMValue>(NULL, 8);
	}
	if (Sources == NULL) {
		Sources = new HashMap<BytecodeContainer>(NULL, 8);
	}
	if (Classes == NULL) {
		Classes = new HashMap<ObjClass*>(NULL, 8);
	}
	if (Tokens == NULL) {
		Tokens = new HashMap<char*>(NULL, 64);
	}

	memset(VMThread::InstructionIgnoreMap, 0, sizeof(VMThread::InstructionIgnoreMap));

	GlobalLock = SDL_CreateMutex();

#ifdef VM_DEBUG
	int branchLimit = GetBranchLimit();
	SourceFiles = new HashMap<SourceFile*>(NULL, 8);
#endif

	for (Uint32 i = 0; i < sizeof(Threads) / sizeof(VMThread); i++) {
		memset(&Threads[i].Stack, 0, sizeof(Threads[i].Stack));
		memset(&Threads[i].RegisterValue, 0, sizeof(Threads[i].RegisterValue));
		memset(&Threads[i].Frames, 0, sizeof(Threads[i].Frames));
		memset(&Threads[i].Name, 0, sizeof(Threads[i].Name));

		Threads[i].FrameCount = 0;
		Threads[i].ReturnFrame = 0;

		Threads[i].ID = i;
		Threads[i].StackTop = Threads[i].Stack;

#ifdef VM_DEBUG
		Threads[i].DebugInfo = false;
		Threads[i].InDebugger = false;
		Threads[i].DebugFrame = 0;
		Threads[i].BranchLimit = branchLimit;
#endif
	}
	ThreadCount = 1;

	ScriptEntity::Init();

	TypeImpl::Init();
}
#ifdef VM_DEBUG
Uint32 ScriptManager::GetBranchLimit() {
	int branchLimit = 0;

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

	return (Uint32)branchLimit;
}
#endif
void ScriptManager::Dispose() {
	if (Globals) {
		Globals->Clear();
		Globals = nullptr;
		delete Globals;
	}

	if (Constants) {
		Constants->Clear();
		Constants = nullptr;
		delete Constants;
	}

	ClassImplList.clear();
	AllNamespaces.clear();
	ModuleList.clear();

	if (ThreadCount) {
#ifdef VM_DEBUG
		for (int i = 0; i < ThreadCount; i++) {
			Threads[i].DisposeBreakpoints();
		}
#endif

		Threads[0].FrameCount = 0;
		Threads[0].ResetStack();
	}
	ThreadCount = 0;

	ForceGarbageCollection();

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
	if (Tokens) {
		Tokens->WithAll([](Uint32 hash, char* token) -> void {
			Memory::Free(token);
		});
		Tokens->Clear();
		delete Tokens;
		Tokens = NULL;
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

	if (GlobalLock) {
		SDL_DestroyMutex(GlobalLock);
		GlobalLock = NULL;
	}
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
// #endregion

// #region ValueFuncs
bool ScriptManager::DoIntegerConversion(VMValue& value, Uint32 threadID) {
	VMValue result = Value::CastAsInteger(value);
	if (IS_NULL(result)) {
		// Conversion failed
		ScriptManager::Threads[threadID].ThrowRuntimeError(false,
			"Expected value to be of type %s; value was of type %s.",
			GetTypeString(VAL_INTEGER),
			GetValueTypeString(value));
		return false;
	}
	value = result;
	return true;
}
bool ScriptManager::DoDecimalConversion(VMValue& value, Uint32 threadID) {
	VMValue result = Value::CastAsDecimal(value);
	if (IS_NULL(result)) {
		// Conversion failed
		ScriptManager::Threads[threadID].ThrowRuntimeError(false,
			"Expected value to be of type %s; value was of type %s.",
			GetTypeString(VAL_DECIMAL),
			GetValueTypeString(value));
		return false;
	}
	value = result;
	return true;
}

void ScriptManager::DestroyObject(Obj* object) {
	if (object->Destructor != nullptr) {
		object->Destructor(object);
	}

	FREE_OBJ(object);
}
// #endregion

// #region GlobalFuncs
bool ScriptManager::Lock() {
	if (ScriptManager::ThreadCount == 1) {
		return true;
	}

	return SDL_LockMutex(GlobalLock) == 0;
}
void ScriptManager::Unlock() {
	if (ScriptManager::ThreadCount > 1) {
		SDL_UnlockMutex(GlobalLock);
	}
}

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
	if (klass->Parent == nullptr && object->Type == OBJ_ENTITY) {
		ObjEntity* entity = (ObjEntity*)object;
		if (entity->EntityPtr && klass != EntityImpl::ParentClass) {
			return EntityImpl::Class;
		}
	}

	return klass->Parent;
}
bool ScriptManager::ClassHasMethod(ObjClass* klass, Uint32 hash) {
	VMValue callable;
	return GetClassMethod(klass, hash, &callable);
}

void ScriptManager::LinkStandardLibrary() {
	StandardLibrary::Link();
}
void ScriptManager::LinkExtensions() {}
// #endregion

#define FG_YELLOW ""
#define FG_RESET ""

#if defined(LINUX)
#undef FG_YELLOW
#undef FG_RESET
#define FG_YELLOW "\x1b[1;93m"
#define FG_RESET "\x1b[m"
#endif

// #region ObjectFuncs
bool ScriptManager::RunBytecode(BytecodeContainer bytecodeContainer, Uint32 filenameHash) {
	Bytecode* bytecode = new Bytecode();
	if (!bytecode->Read(bytecodeContainer, Tokens)) {
		delete bytecode;
		return false;
	}

	VMThread* thread = &Threads[0];

	ObjModule* module = NewModule();

	for (size_t i = 0; i < bytecode->Functions.size(); i++) {
		ObjFunction* function = bytecode->Functions[i];
		Chunk* chunk = &function->Chunk;

		module->Functions->push_back(function);

		function->Module = module;
#if USING_VM_FUNCPTRS
		chunk->SetupOpfuncs();
#endif

#ifdef VM_DEBUG
		if (chunk->Breakpoints) {
			Uint8* breakpoints = (Uint8*)Memory::Calloc(chunk->Count, sizeof(Uint8));

			memcpy(breakpoints, chunk->Breakpoints, chunk->Count);

			thread->Breakpoints[function] = breakpoints;
		}
#endif
	}

	if (bytecode->SourceFilename) {
		module->SourceFilename = StringUtils::Duplicate(bytecode->SourceFilename);
	}
	else {
		char fnHash[13];
		snprintf(fnHash, sizeof(fnHash), "%08X.ibc", filenameHash);
		module->SourceFilename = StringUtils::Duplicate(fnHash);
	}

	ModuleList.push_back(module);

	delete bytecode;

	thread->RunFunction((*module->Functions)[0], 0);

	return true;
}
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
bool ScriptManager::ClassExists(const char* objectName) {
	return SourceFileMap::ClassMap->Exists(objectName);
}
bool ScriptManager::ClassExists(Uint32 hash) {
	return SourceFileMap::ClassMap->Exists(hash);
}
bool ScriptManager::IsClassLoaded(const char* className) {
	return ScriptManager::Classes->Exists(className);
}
bool ScriptManager::IsStandardLibraryClass(const char* className) {
	return IS_CLASS(Constants->Get(className));
}
bool ScriptManager::LoadScript(char* filename) {
	if (!filename || !*filename) {
		return false;
	}

	Uint32 hash = MakeFilenameHash(filename);
	return LoadScript(hash);
}
bool ScriptManager::LoadScript(const char* filename) {
	return LoadScript((char*)filename);
}
bool ScriptManager::LoadScript(Uint32 hash) {
	if (!Sources->Exists(hash)) {
		BytecodeContainer bytecode = ScriptManager::GetBytecodeFromFilenameHash(hash);
		if (!bytecode.Data) {
			return false;
		}

		return RunBytecode(bytecode, hash);
	}

	return true;
}
bool ScriptManager::LoadObjectClass(const char* objectName) {
	if (ScriptManager::IsClassLoaded(objectName)) {
		return true;
	}

	if (!ScriptManager::ClassExists(objectName)) {
		return false;
	}

	// On first load:
	vector<Uint32>* filenameHashList = SourceFileMap::ClassMap->Get(objectName);

	for (size_t fn = 0; fn < filenameHashList->size(); fn++) {
		Uint32 filenameHash = (*filenameHashList)[fn];

		if (!Sources->Exists(filenameHash)) {
			BytecodeContainer bytecode =
				ScriptManager::GetBytecodeFromFilenameHash(filenameHash);
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

			RunBytecode(bytecode, filenameHash);
		}
	}

	if (!IsStandardLibraryClass(objectName) && !Classes->Exists(objectName)) {
		ObjClass* klass = GetObjectClass(objectName);
		if (!klass) {
			Log::Print(Log::LOG_ERROR, "Could not find class of %s!", objectName);
			return false;
		}
		Classes->Put(objectName, klass);
	}

	return true;
}
ObjClass* ScriptManager::GetObjectClass(const char* className) {
	VMValue value = Globals->Get(className);

	if (IS_CLASS(value)) {
		return AS_CLASS(value);
	}

	return nullptr;
}
void ScriptManager::LoadClasses() {
	SourceFileMap::ClassMap->ForAll([](Uint32, vector<Uint32>* filenameHashList) -> void {
		for (size_t fn = 0; fn < filenameHashList->size(); fn++) {
			Uint32 filenameHash = (*filenameHashList)[fn];

			BytecodeContainer bytecode =
				ScriptManager::GetBytecodeFromFilenameHash(filenameHash);
			if (!bytecode.Data) {
				Log::Print(
					Log::LOG_WARN, "Class %08X does not exist!", filenameHash);
				continue;
			}

			RunBytecode(bytecode, filenameHash);
		}
	});

	ScriptManager::ForceGarbageCollection();
}
#ifdef VM_DEBUG
void ScriptManager::LoadSourceCodeLines(SourceFile* sourceFile, const char* sourceFilename) {
	Stream* stream = File::Open(sourceFilename, File::READ_ACCESS);
	if (!stream) {
		return;
	}

	size_t size = stream->Length();
	char* text = (char*)Memory::Calloc(size + 1, sizeof(char));
	stream->ReadBytes(text, size);
	stream->Close();

	char* ptr = text;
	char* lineStart = ptr;
	while (*ptr != '\0') {
		if (*ptr == '\n') {
			sourceFile->Lines.push_back(lineStart);
			lineStart = ptr + 1;
			*ptr = '\0';
		}
		ptr++;
	}

	sourceFile->Text = text;
}
char* ScriptManager::GetSourceCodeLine(const char* sourceFilename, int line) {
	SourceFile* sourceFile = nullptr;

	if (!SourceFiles->Exists(sourceFilename)) {
		std::string filePath = std::string(SCRIPTS_DIRECTORY_NAME) + "/" + std::string(sourceFilename);

		sourceFile = new SourceFile;

		if (File::Exists(filePath.c_str())) {
			LoadSourceCodeLines(sourceFile, filePath.c_str());

			sourceFile->Exists = true;
		}
		else {
			Log::Print(Log::LOG_WARN, "Source file \"%s\" does not exist.", filePath.c_str());
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
#endif
// #endregion
