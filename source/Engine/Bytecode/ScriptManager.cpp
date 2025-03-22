#include <Engine/Bytecode/Bytecode.h>
#include <Engine/Bytecode/GarbageCollector.h>
#include <Engine/Bytecode/ScriptEntity.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/SourceFileMap.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/ArrayImpl.h>
#include <Engine/Bytecode/TypeImpl/FunctionImpl.h>
#include <Engine/Bytecode/TypeImpl/MapImpl.h>
#include <Engine/Bytecode/TypeImpl/MaterialImpl.h>
#include <Engine/Bytecode/TypeImpl/StringImpl.h>
#include <Engine/Bytecode/Values.h>
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

std::set<Obj*> ScriptManager::FreedGlobals;

vector<ObjModule*> ScriptManager::ModuleList;

HashMap<BytecodeContainer>* ScriptManager::Sources = NULL;
HashMap<ObjClass*>* ScriptManager::Classes = NULL;
HashMap<char*>* ScriptManager::Tokens = NULL;
vector<ObjNamespace*> ScriptManager::AllNamespaces;
vector<ObjClass*> ScriptManager::ClassImplList;

SDL_mutex* ScriptManager::GlobalLock = NULL;

static Uint32 VMBranchLimit = 0;

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
	VMBranchLimit = GetBranchLimit();
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

		Threads[i].DebugInfo = false;
		Threads[i].BranchLimit = VMBranchLimit;
	}
	ThreadCount = 1;

	ArrayImpl::Init();
	MapImpl::Init();
	FunctionImpl::Init();
	StringImpl::Init();
	MaterialImpl::Init();
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
void ScriptManager::DisposeGlobalValueTable(HashMap<VMValue>* globals) {
	globals->ForAll(FreeGlobalValue);
	globals->Clear();
	delete globals;
}
void ScriptManager::Dispose() {
	// NOTE: Remove GC-able values from these tables so they may be
	// cleaned up.
	if (Globals) {
		Globals->ForAll(RemoveNonGlobalableValue);
	}
	if (Constants) {
		Constants->ForAll(RemoveNonGlobalableValue);
	}

	ClassImplList.clear();
	AllNamespaces.clear();

	FreeModules();

	Threads[0].FrameCount = 0;
	Threads[0].ResetStack();
	ForceGarbageCollection();

	FreedGlobals.clear();

	if (Globals) {
		Log::Print(Log::LOG_VERBOSE, "Freeing values in Globals list...");
		DisposeGlobalValueTable(Globals);
		Log::Print(Log::LOG_VERBOSE, "Done!");
		Globals = NULL;
	}

	if (Constants) {
		Log::Print(Log::LOG_VERBOSE, "Freeing values in Constants list...");
		DisposeGlobalValueTable(Constants);
		Log::Print(Log::LOG_VERBOSE, "Done!");
		Constants = NULL;
	}

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

	SDL_DestroyMutex(GlobalLock);
}
void ScriptManager::RemoveNonGlobalableValue(Uint32 hash, VMValue value) {
	if (IS_OBJECT(value)) {
		switch (OBJECT_TYPE(value)) {
		case OBJ_CLASS:
		case OBJ_ENUM:
		case OBJ_FUNCTION:
		case OBJ_NATIVE:
		case OBJ_NAMESPACE:
		case OBJ_MODULE:
			break;
		default:
			if (hash) {
				Globals->Remove(hash);
			}
			break;
		}
	}
}
void ScriptManager::FreeNativeValue(Uint32 hash, VMValue value) {
	if (IS_OBJECT(value)) {
		switch (OBJECT_TYPE(value)) {
		case OBJ_NATIVE:
			FREE_OBJ(AS_OBJECT(value), ObjNative);
			break;

		default:
			break;
		}
	}
}
void ScriptManager::FreeFunction(ObjFunction* function) {
	/*
	printf("OBJ_FUNCTION: %p (%s)\n", function,
	    function->Name ?
	        (function->Name->Chars ? function->Name->Chars :
	"NULL") : "NULL");
	//*/
	if (function->Name != NULL) {
		FreeValue(OBJECT_VAL(function->Name));
	}
	if (function->ClassName != NULL) {
		FreeValue(OBJECT_VAL(function->ClassName));
	}

	for (size_t i = 0; i < function->Chunk.Constants->size(); i++) {
		FreeValue((*function->Chunk.Constants)[i]);
	}
	function->Chunk.Constants->clear();
	function->Chunk.Free();

	FREE_OBJ(function, ObjFunction);
}
void ScriptManager::FreeModule(ObjModule* module) {
	delete module->Functions;
	delete module->Locals;
}
void ScriptManager::FreeClass(ObjClass* klass) {
	// Subfunctions are already freed as a byproduct of the
	// ModuleList, so just do natives.
	klass->Methods->ForAll(FreeNativeValue);
	delete klass->Methods;

	// A class does not own its values, so it's not allowed
	// to free them.
	delete klass->Fields;

	if (klass->Name) {
		FreeValue(OBJECT_VAL(klass->Name));
	}

	FREE_OBJ(klass, ObjClass);
}
void ScriptManager::FreeEnumeration(ObjEnum* enumeration) {
	// An enumeration does not own its values, so it's not allowed
	// to free them.
	delete enumeration->Fields;

	if (enumeration->Name) {
		FreeValue(OBJECT_VAL(enumeration->Name));
	}

	FREE_OBJ(enumeration, ObjEnum);
}
void ScriptManager::FreeNamespace(ObjNamespace* ns) {
	// A namespace does not own its values, so it's not allowed
	// to free them.
	delete ns->Fields;

	if (ns->Name) {
		FreeValue(OBJECT_VAL(ns->Name));
	}

	FREE_OBJ(ns, ObjNamespace);
}
void ScriptManager::FreeString(ObjString* string) {
	if (string->Chars != NULL) {
		Memory::Free(string->Chars);
	}
	string->Chars = NULL;

	FREE_OBJ(string, ObjString);
}
void ScriptManager::FreeGlobalValue(Uint32 hash, VMValue value) {
	if (IS_OBJECT(value)) {
		Obj* object = AS_OBJECT(value);
		if (FreedGlobals.find(object) != FreedGlobals.end()) {
			return;
		}

#ifdef DEBUG_FREE_GLOBALS
		if (Tokens->Get(hash)) {
			Log::Print(Log::LOG_VERBOSE,
				"Freeing global %s, type %s",
				Tokens->Get(hash),
				GetValueTypeString(value));
		}
		else {
			Log::Print(Log::LOG_VERBOSE,
				"Freeing global %08X, type %s",
				hash,
				GetValueTypeString(value));
		}
#endif

		switch (OBJECT_TYPE(value)) {
		case OBJ_CLASS: {
			ObjClass* klass = AS_CLASS(value);
			FreeClass(klass);
			FreedGlobals.insert(object);
			break;
		}
		case OBJ_ENUM: {
			ObjEnum* enumeration = AS_ENUM(value);
			FreeEnumeration(enumeration);
			FreedGlobals.insert(object);
			break;
		}
		case OBJ_NAMESPACE: {
			ObjNamespace* ns = AS_NAMESPACE(value);
			FreeNamespace(ns);
			FreedGlobals.insert(object);
			break;
		}
		case OBJ_NATIVE: {
			FREE_OBJ(AS_OBJECT(value), ObjNative);
			FreedGlobals.insert(object);
			break;
		}
		default:
			break;
		}
	}
}
void ScriptManager::FreeModules() {
	Log::Print(Log::LOG_VERBOSE, "Freeing %d modules...", ModuleList.size());
	for (size_t i = 0; i < ModuleList.size(); i++) {
		FreeModule(ModuleList[i]);
	}
	ModuleList.clear();
	Log::Print(Log::LOG_VERBOSE, "Done!");
}
// #endregion

// #region ValueFuncs
VMValue ScriptManager::CastValueAsString(VMValue v, bool prettyPrint) {
	if (IS_STRING(v)) {
		return v;
	}

	char* buffer = (char*)malloc(512);
	PrintBuffer buffer_info;
	buffer_info.Buffer = &buffer;
	buffer_info.WriteIndex = 0;
	buffer_info.BufferSize = 512;
	Values::PrintValue(&buffer_info, v, prettyPrint);
	v = OBJECT_VAL(CopyString(buffer, buffer_info.WriteIndex));
	free(buffer);
	return v;
}
VMValue ScriptManager::CastValueAsString(VMValue v) {
	return CastValueAsString(v, false);
}
VMValue ScriptManager::CastValueAsInteger(VMValue v) {
	float a;
	switch (v.Type) {
	case VAL_DECIMAL:
	case VAL_LINKED_DECIMAL:
		a = AS_DECIMAL(v);
		return INTEGER_VAL((int)a);
	case VAL_INTEGER:
		return v;
	case VAL_LINKED_INTEGER:
		return INTEGER_VAL(AS_INTEGER(v));
	default:
		// NOTE: Should error here instead.
		break;
	}
	return NULL_VAL;
}
VMValue ScriptManager::CastValueAsDecimal(VMValue v) {
	int a;
	switch (v.Type) {
	case VAL_DECIMAL:
		return v;
	case VAL_LINKED_DECIMAL:
		return DECIMAL_VAL(AS_DECIMAL(v));
	case VAL_INTEGER:
	case VAL_LINKED_INTEGER:
		a = AS_INTEGER(v);
		return DECIMAL_VAL((float)a);
	default:
		// NOTE: Should error here instead.
		break;
	}
	return NULL_VAL;
}
VMValue ScriptManager::Concatenate(VMValue va, VMValue vb) {
	ObjString* a = AS_STRING(va);
	ObjString* b = AS_STRING(vb);

	size_t length = a->Length + b->Length;
	ObjString* result = AllocString(length);

	memcpy(result->Chars, a->Chars, a->Length);
	memcpy(result->Chars + a->Length, b->Chars, b->Length);
	result->Chars[length] = 0;
	return OBJECT_VAL(result);
}

bool ScriptManager::ValuesSortaEqual(VMValue a, VMValue b) {
	if ((a.Type == VAL_DECIMAL && b.Type == VAL_INTEGER) ||
		(a.Type == VAL_INTEGER && b.Type == VAL_DECIMAL)) {
		float a_d = AS_DECIMAL(CastValueAsDecimal(a));
		float b_d = AS_DECIMAL(CastValueAsDecimal(b));
		return (a_d == b_d);
	}

	if (IS_STRING(a) && IS_STRING(b)) {
		ObjString* astr = AS_STRING(a);
		ObjString* bstr = AS_STRING(b);
		return astr->Length == bstr->Length &&
			!memcmp(astr->Chars, bstr->Chars, astr->Length);
	}

	if (IS_BOUND_METHOD(a) && IS_BOUND_METHOD(b)) {
		ObjBoundMethod* abm = AS_BOUND_METHOD(a);
		ObjBoundMethod* bbm = AS_BOUND_METHOD(b);
		return ValuesEqual(abm->Receiver, bbm->Receiver) && abm->Method == bbm->Method;
	}

	return ScriptManager::ValuesEqual(a, b);
}
bool ScriptManager::ValuesEqual(VMValue a, VMValue b) {
	if (a.Type == VAL_LINKED_INTEGER) {
		goto SKIP_CHECK;
	}
	if (a.Type == VAL_LINKED_DECIMAL) {
		goto SKIP_CHECK;
	}
	if (b.Type == VAL_LINKED_INTEGER) {
		goto SKIP_CHECK;
	}
	if (b.Type == VAL_LINKED_DECIMAL) {
		goto SKIP_CHECK;
	}

	if (a.Type != b.Type) {
		return false;
	}

SKIP_CHECK:

	switch (a.Type) {
	case VAL_LINKED_INTEGER:
	case VAL_INTEGER:
		return AS_INTEGER(a) == AS_INTEGER(b);

	case VAL_LINKED_DECIMAL:
	case VAL_DECIMAL:
		return AS_DECIMAL(a) == AS_DECIMAL(b);

	case VAL_OBJECT:
		return AS_OBJECT(a) == AS_OBJECT(b);

	case VAL_NULL:
		return true;
	}
	return false;
}
bool ScriptManager::ValueFalsey(VMValue a) {
	if (a.Type == VAL_NULL) {
		return true;
	}

	switch (a.Type) {
	case VAL_LINKED_INTEGER:
	case VAL_INTEGER:
		return AS_INTEGER(a) == 0;
	case VAL_LINKED_DECIMAL:
	case VAL_DECIMAL:
		return AS_DECIMAL(a) == 0.0f;
	case VAL_OBJECT:
		return false;
	}
	return false;
}

VMValue ScriptManager::DelinkValue(VMValue val) {
	if (IS_LINKED_DECIMAL(val)) {
		return DECIMAL_VAL(AS_DECIMAL(val));
	}
	if (IS_LINKED_INTEGER(val)) {
		return INTEGER_VAL(AS_INTEGER(val));
	}

	return val;
}

bool ScriptManager::DoIntegerConversion(VMValue& value, Uint32 threadID) {
	VMValue result = ScriptManager::CastValueAsInteger(value);
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
	VMValue result = ScriptManager::CastValueAsDecimal(value);
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

void ScriptManager::FreeValue(VMValue value) {
	if (IS_OBJECT(value)) {
		Obj* objectPointer = AS_OBJECT(value);
		switch (OBJECT_TYPE(value)) {
		case OBJ_BOUND_METHOD: {
			FREE_OBJ(objectPointer, ObjBoundMethod);
			break;
		}
		case OBJ_INSTANCE: {
			ObjInstance* instance = AS_INSTANCE(value);

			// An instance does not own its values, so it's
			// not allowed to free them.
			delete instance->Fields;

			FREE_OBJ(instance, ObjInstance);
			break;
		}
		case OBJ_STRING: {
			ObjString* string = AS_STRING(value);
			FreeString(string);
			break;
		}
		case OBJ_ARRAY: {
			ObjArray* array = AS_ARRAY(value);

			// An array does not own its values, so it's
			// not allowed to free them.
			array->Values->clear();
			delete array->Values;

			FREE_OBJ(array, ObjArray);
			break;
		}
		case OBJ_MAP: {
			ObjMap* map = AS_MAP(value);

			// Free keys
			map->Keys->WithAll([](Uint32, char* ptr) -> void {
				Memory::Free(ptr);
			});

			// Free Keys table
			delete map->Keys;

			// Free Values table
			delete map->Values;

			FREE_OBJ(map, ObjMap);
			break;
		}
		case OBJ_STREAM: {
			ObjStream* stream = AS_STREAM(value);

			if (!stream->Closed) {
				stream->StreamPtr->Close();
			}

			FREE_OBJ(stream, ObjStream);
			break;
		}
		case OBJ_MATERIAL: {
			ObjMaterial* material = AS_MATERIAL(value);

			Material::Remove(material->MaterialPtr);

			FREE_OBJ(material, ObjMaterial);
			break;
		}
		default:
			break;
		}
	}
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

	function->ClassName = CopyString(klass->Name);

	thread->Pop();
}
void ScriptManager::DefineNative(ObjClass* klass, const char* name, NativeFn function) {
	if (function == NULL) {
		return;
	}
	if (klass == NULL) {
		return;
	}
	if (name == NULL) {
		return;
	}

	if (!klass->Methods->Exists(name)) {
		klass->Methods->Put(name, OBJECT_VAL(NewNative(function)));
	}
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
ObjClass* ScriptManager::GetClassParent(ObjClass* klass) {
	if (!klass->Parent && klass->ParentHash) {
		VMValue parent;
		if (ScriptManager::Globals->GetIfExists(klass->ParentHash, &parent) &&
			IS_CLASS(parent)) {
			klass->Parent = AS_CLASS(parent);
		}
	}
	return klass->Parent;
}
VMValue ScriptManager::GetClassMethod(ObjClass* klass, Uint32 hash) {
	VMValue method;
	if (klass->Methods->GetIfExists(hash, &method)) {
		return method;
	}
	else {
		ObjClass* parentClass = ScriptManager::GetClassParent(klass);
		if (parentClass) {
			return GetClassMethod(parentClass, hash);
		}
	}
	return NULL_VAL;
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

	ObjModule* module = NewModule();

	for (size_t i = 0; i < bytecode->Functions.size(); i++) {
		ObjFunction* function = bytecode->Functions[i];

		module->Functions->push_back(function);

		function->Module = module;
#if USING_VM_FUNCPTRS
		function->Chunk.SetupOpfuncs();
#endif
	}

	if (bytecode->SourceFilename) {
		module->SourceFilename = CopyString(bytecode->SourceFilename);
	}
	else {
		char fnHash[256];
		snprintf(fnHash, sizeof(fnHash), "%08X", filenameHash);
		module->SourceFilename = CopyString(fnHash);
	}

	ModuleList.push_back(module);

	delete bytecode;

	Threads[0].RunFunction((*module->Functions)[0], 0);

	return true;
}
bool ScriptManager::CallFunction(char* functionName) {
	if (!Globals->Exists(functionName)) {
		return false;
	}

	VMValue functionValue = Globals->Get(functionName);
	if (!IS_FUNCTION(functionValue)) {
		return false;
	}

	ObjFunction* function = AS_FUNCTION(functionValue);
	Threads[0].RunEntityFunction(function, 0);
	return true;
}
Entity* ScriptManager::SpawnObject(const char* objectName) {
	ObjClass* klass = GetObjectClass(objectName);
	if (!klass) {
		Log::Print(Log::LOG_ERROR, "Could not find class of %s!", objectName);
		return nullptr;
	}

	ScriptEntity* object = new ScriptEntity;

	ObjInstance* instance = NewInstance(klass);
	object->Link(instance);

	return object;
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
bool ScriptManager::LoadObjectClass(const char* objectName, bool addNativeFunctions) {
	if (!objectName || !*objectName) {
		return false;
	}

	if (!ScriptManager::ClassExists(objectName)) {
		Log::Print(Log::LOG_VERBOSE,
			"Could not find classmap for %s%s%s! (Hash: 0x%08X)",
			FG_YELLOW,
			objectName,
			FG_RESET,
			SourceFileMap::ClassMap->HashFunction(objectName, strlen(objectName)));
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
					"Loading class %s%s%s, %d filename(s)...",
					Log::WriteToFile ? "" : FG_YELLOW,
					objectName,
					Log::WriteToFile ? "" : FG_RESET,
					(int)filenameHashList->size());
			}

			RunBytecode(bytecode, filenameHash);
		}
	}

	// Set native functions for that new object class
	if (!IsStandardLibraryClass(objectName) && !Classes->Exists(objectName)) {
		// Log::Print(Log::LOG_VERBOSE, "Setting native
		// functions for class %s...", objectName);
		ObjClass* klass = GetObjectClass(objectName);
		if (!klass) {
			Log::Print(Log::LOG_ERROR, "Could not find class of %s!", objectName);
			return false;
		}
		// FIXME: Do this in a better way. Probably just remove
		// CLASS_TYPE_EXTENDED to begin with.
		if (klass->Type != CLASS_TYPE_EXTENDED && addNativeFunctions) {
			ScriptManager::AddNativeObjectFunctions(klass);
		}
		Classes->Put(objectName, klass);
	}

	return true;
}
void ScriptManager::AddNativeObjectFunctions(ObjClass* klass) {
#define DEF_NATIVE(name) ScriptManager::DefineNative(klass, #name, ScriptEntity::VM_##name)
	DEF_NATIVE(InView);
	DEF_NATIVE(Animate);
	DEF_NATIVE(ApplyPhysics);
	DEF_NATIVE(SetAnimation);
	DEF_NATIVE(ResetAnimation);
	DEF_NATIVE(GetHitboxFromSprite);
	DEF_NATIVE(ReturnHitboxFromSprite);
	DEF_NATIVE(AddToRegistry);
	DEF_NATIVE(IsInRegistry);
	DEF_NATIVE(RemoveFromRegistry);
	DEF_NATIVE(CollidedWithObject);
	DEF_NATIVE(CollideWithObject);
	DEF_NATIVE(SolidCollideWithObject);
	DEF_NATIVE(TopSolidCollideWithObject);
	DEF_NATIVE(PropertyGet);
	DEF_NATIVE(PropertyExists);
	DEF_NATIVE(SetViewVisibility);
	DEF_NATIVE(SetViewOverride);
	DEF_NATIVE(AddToDrawGroup);
	DEF_NATIVE(IsInDrawGroup);
	DEF_NATIVE(RemoveFromDrawGroup);
	DEF_NATIVE(GetIDWithinClass);
	DEF_NATIVE(PlaySound);
	DEF_NATIVE(LoopSound);
	DEF_NATIVE(StopSound);
	DEF_NATIVE(StopAllSounds);
#undef DEF_NATIVE
}
ObjClass* ScriptManager::GetObjectClass(const char* className) {
	VMValue value = Globals->Get(className);

	if (IS_CLASS(value)) {
		return AS_CLASS(value);
	}

	return nullptr;
}
Entity* ScriptManager::ObjectSpawnFunction(ObjectList* list) {
	return ScriptManager::SpawnObject(list->ObjectName);
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
// #endregion
