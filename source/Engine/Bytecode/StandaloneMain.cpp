#ifdef HSL_STANDALONE
#include <Engine/Bytecode/BytecodeDebugger.h>
#include <Engine/Bytecode/CompilerEnums.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandaloneMain.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Exceptions/CompilerErrorException.h>
#include <Engine/Exceptions/ScriptREPLException.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/Version.h>
#include <Engine/Utilities/StringUtils.h>

#ifdef USE_CLOCK
#include <Engine/Diagnostics/Clock.h>
#endif

#ifdef HSL_LIBRARY
hsl_RuntimeErrorHandler VMRuntimeErrorHandler;
#endif

#ifdef HSL_COMPILER
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/SourceFileMap.h>
#else
#undef HSL_STANDALONE_REPL
#endif

#ifdef HSL_STANDALONE_RUNNER
#include <Engine/Bytecode/GarbageCollector.h>
#else
#ifdef HSL_COMPILER
#define HSL_STANDALONE_COMPILER
#endif
#undef HSL_STANDALONE_REPL
#undef VM_DEBUG
#endif

#ifdef HSL_STANDALONE_REPL
#include <Engine/Bytecode/ScriptREPL.h>
#include <Engine/Bytecode/ValuePrinter.h>
#include <Engine/IO/TerminalInput.h>

#define REPL_PROMPT "> "
#endif

#ifdef VM_DEBUG
#include <Engine/Bytecode/VMThreadDebugger.h>
#endif

#include <cstdio>

#ifndef HSL_LIBRARY
ScriptManager* GlobalScriptManager = nullptr;
#endif

bool CompileCode = false;
bool PrintChunks = false;
bool PrintGarbageCollectionOutput = false;

const char* InputFilename = nullptr;
const char* OutputFilename = nullptr;
std::string ScriptsDirectory;

#ifdef HSL_COMPILER
const char* SourceFilename = nullptr;
bool ShowWarnings = true;
bool DoOptimizations = true;
bool WriteDebugInfo = true;
bool WriteSourceFilename = true;
#endif

#ifdef HSL_STANDALONE_RUNNER
bool ExecuteCode = true;
#endif

#ifdef USE_LOG_FILE
const char* LogFilename = nullptr;
#endif

#ifdef HSL_STANDALONE_REPL
bool StartREPL = false;
#endif

#ifdef VM_DEBUG
bool StartDebugger = false;
#endif

bool IsInREPL = false;

void InitSubsystems() {
	Log::Init();
#ifdef USE_CLOCK
	Clock::Init();
#endif

#ifdef HSL_COMPILER
	Compiler::Init();
#endif

	ScriptManager::Init();

#ifdef VM_DEBUG
	VMThreadDebugger::Initialize();
#endif

#ifdef HSL_COMPILER
	SourceFileMap::CheckForUpdate();
#endif
}

void DisposeSubsystems() {
	ScriptManager::Dispose();
#ifdef HSL_COMPILER
	SourceFileMap::Dispose();
	Compiler::Dispose();
#endif
#ifdef VM_DEBUG
	VMThreadDebugger::Dispose();
#endif

	Memory::ClearTrackedMemory();

	Log::Close();
}

void StandaloneExit(std::string error) {
#ifdef HSL_LIBRARY
	throw std::runtime_error(error);
#else
	DisposeSubsystems();
	exit(-1);
#endif
}

#ifdef HSL_LIBRARY
hsl_ErrorResponse HandleVMRuntimeError(ScriptManager* manager, hsl_Result error, std::string text) {
	if (manager->RuntimeErrorHandler) {
		return manager->RuntimeErrorHandler(error, text.c_str());
	}

	Log::Print(Log::LOG_ERROR, text.c_str());

	return HSL_ERROR_RES_EXIT;
}
#endif

bool InStandaloneREPL() {
	return IsInREPL;
}

bool ShouldShowGarbageCollectionOutput() {
	return PrintGarbageCollectionOutput;
}

const char* GetScriptsDirectory() {
	if (ScriptsDirectory.size() > 0) {
		return ScriptsDirectory.c_str();
	}

	return nullptr;
}

void SetScriptsDirectory(const char* path) {
	if (path == nullptr) {
		ScriptsDirectory = "";
		return;
	}

	ScriptsDirectory = std::string(path);
}

const char* GetVersionText() {
#ifdef GIT_COMMIT_HASH
	return "HSL " HSL_VERSION " (commit " GIT_COMMIT_HASH ")";
#else
	return "HSL " HSL_VERSION;
#endif
}

#if defined(VM_DEBUG) && !defined(HSL_LIBRARY)
void DebuggerLoop() {
	VMThreadDebugger* debugger;

	printf("Entering debugger.\n");

	debugger = new VMThreadDebugger(&GlobalScriptManager->Threads[0]);
	debugger->Enter();
	debugger->MainLoop();
	debugger->Exit();

	printf("Exiting debugger.\n");

	delete debugger;
}
#endif

#ifndef HSL_LIBRARY
void PrintVersionText() {
	Log::PrintSimple("%s\n", GetVersionText());
}

#ifdef HSL_STANDALONE_RUNNER
bool RunnerMain(const char* filename) {
	Stream* stream = File::Open(filename, File::READ_ACCESS);
	if (!stream) {
		Log::Print(Log::LOG_ERROR, "Could not open file \"%s\"!", filename);
		return false;
	}

	bool succeeded = false;

#ifdef HSL_COMPILER
	bool isBytecode = false;

	Uint8 magic[4];
	stream->ReadBytes(magic, 4);
	if (memcmp(BYTECODE_MAGIC, magic, 4) == 0) {
		isBytecode = true;
	}
	stream->Seek(0);
#endif

	VMThread* thread = &GlobalScriptManager->Threads[0];
	ObjModule* module = nullptr;

#ifdef HSL_COMPILER
	if (isBytecode)
#endif
	{
		Uint32 hash = 0xABCDABCD;

		Uint32 hashFromFilename = 0x00000000;
		sscanf(filename, "%08X", &hashFromFilename);
		if (hashFromFilename != 0x00000000) {
			hash = hashFromFilename;
		}

		module = GlobalScriptManager->LoadBytecode(stream, hash);
	}
#ifdef HSL_COMPILER
	else {
		module = GlobalScriptManager->CompileScriptFromStream(thread, stream, filename);
	}
#endif

#ifdef VM_DEBUG
	if (module && GlobalScriptManager->BreakpointsEnabled) {
		GlobalScriptManager->AddModuleBreakpoints(thread, module);
	}
#endif

	if (!module) {
		return false;
	}

	if (PrintChunks) {
		BytecodeDebugger* debugger = new BytecodeDebugger;
		debugger->Tokens = GlobalScriptManager->Tokens;

		for (size_t i = 0; i < module->Functions->size(); i++) {
			Chunk* chunk = &(*module->Functions)[i]->Chunk;

			debugger->DebugChunk(chunk,
				(*module->Functions)[i]->Name,
				(*module->Functions)[i]->MinArity,
				(*module->Functions)[i]->Arity);

			Log::PrintSimple("\n");
		}

		debugger->Tokens = nullptr;
	}

	if (ExecuteCode) {
		VMThread* thread = &GlobalScriptManager->Threads[0];
		ObjFunction* function = (*module->Functions)[0];
		if (thread->Call(function, 0)) {
#ifdef VM_DEBUG
			if (StartDebugger) {
				DebuggerLoop();
			}
			if (thread->FrameCount > 0)
#endif
			{
				thread->RunInstructionSet();
			}
			succeeded = true;
		}
	}
#ifdef VM_DEBUG
	else if (StartDebugger) {
		DebuggerLoop();
	}
#endif

	return succeeded;
}
#endif

#ifdef HSL_COMPILER
bool CompilerMain(const char* inputFilename, const char* outputFilename) {
	Stream* stream = File::Open(inputFilename, File::READ_ACCESS);
	if (!stream) {
		Log::Print(Log::LOG_ERROR, "Could not open file \"%s\"!", inputFilename);
		return false;
	}

	size_t size = stream->Length();
	char* code = (char*)Memory::Calloc(size + 1, sizeof(char));
	if (!code) {
		Log::Print(Log::LOG_ERROR, "Out of memory reading file!");
		stream->Close();
		return false;
	}

	stream->ReadBytes(code, size);
	stream->Close();

	Compiler::PrepareCompiling();

	MemoryStream* memStream = MemoryStream::New(0x100);
	if (!memStream) {
		Log::Print(Log::LOG_ERROR, "Not enough memory for compiling code!");
		Memory::Free(code);
		return false;
	}

	Compiler* compiler = new Compiler(nullptr);
	compiler->InREPL = false;
	compiler->CurrentSettings = Compiler::Settings;
	compiler->Type = FUNCTIONTYPE_TOPLEVEL;
	compiler->ScopeDepth = 0;
	compiler->Initialize();
	compiler->SetupLocals();

	bool didCompile = false;
	try {
		didCompile = compiler->Compile(SourceFilename, code, memStream);
	} catch (const CompilerErrorException& error) {
		Log::Print(Log::LOG_ERROR, "%s", error.what());
	}

	delete compiler;
	Compiler::FinishCompiling();

	Memory::Free(code);

	if (!didCompile) {
		memStream->Close();
		return false;
	}

	if (outputFilename == nullptr) {
		static char filenameBuffer[13];
		Uint32 filenameHash = GlobalScriptManager->MakeFilenameHash(SourceFilename);
		snprintf(filenameBuffer, sizeof filenameBuffer, "%08X.ibc", filenameHash);
		outputFilename = filenameBuffer;
	}

	Stream* output = File::Open(outputFilename, File::WRITE_ACCESS);
	if (!output) {
		Log::Print(Log::LOG_ERROR, "Could not open file \"%s\"!", outputFilename);
		memStream->Close();
		return false;
	}

	output->WriteBytes(memStream->pointer_start, memStream->Position());
	output->Close();
	memStream->Close();

	return true;
}
#endif

#ifdef HSL_STANDALONE_REPL
void REPLMain() {
	VMThread* thread = &GlobalScriptManager->Threads[0];

	Compiler::Settings.WriteDebugInfo = true;
	Compiler::Settings.PrintToLog = Log::IsLoggingToFile();

	PrintVersionText();

	IsInREPL = true;

	while (true) {
		// Read the line
		std::string read = TerminalInput::GetLine(REPL_PROMPT);
		if (read.size() == 0) {
			continue;
		}

		if (Log::IsLoggingToFile()) {
			// Writes to the log, but doesn't print anything to stdout.
			Log::Write("%s%s\n", REPL_PROMPT, read.c_str());
		}

		if (read == "q" || read == "quit" || read == "exit") {
			break;
		}

		CallFrame* frame = nullptr;
		if (thread->FrameCount > 0) {
			&thread->Frames[thread->FrameCount - 1];
		}

		try {
			VMValue result = ScriptREPL::ExecuteCode(thread, frame, read.c_str(), Compiler::Settings);

			// Print what was left on the stack
			if (!IS_NULL(result)) {
				if (IS_STRING(result)) {
					Log::PrintSimple("\"");
				}
				ValuePrinter::Print(result);
				if (IS_STRING(result)) {
					Log::PrintSimple("\"");
				}
				Log::PrintSimple("\n");
			}

			GlobalScriptManager->RequestGarbageCollection(PrintGarbageCollectionOutput);
		} catch (const ScriptREPLException& error) {
			Log::PrintSimple("%s\n", error.what());
		}
	}

	IsInREPL = false;
}
#endif

void PrintHelp() {
	PrintVersionText();

	printf("Usage:\n");
#ifdef HSL_STANDALONE_REPL
	printf("    %s [options] [--] [<input>]\n\n", TARGET_NAME);
#else
	printf("    %s [options] [--] <input>\n\n", TARGET_NAME);
#endif

#ifdef HSL_COMPILER
#ifndef HSL_STANDALONE_COMPILER
	printf("    -c [<output>], --compile [<output>]: Compile the input script into the\n");
	printf("                                     specified output file, or \"XXXXXXXX.ibc\"\n");
	printf("                                     if an argument is not passed.\n\n");
#endif

	printf("    --source-filename <filename>: Specify the source filename for\n");
	printf("                                  the compiled bytecode.\n\n");

	printf("    --no-warnings: Disable compiler warnings.\n\n");

	printf("    --no-optimizations: Disable compiler optimizations.\n\n");

	printf("    --no-debug-info: Compile the bytecode without any debug information.\n\n");

	printf("    --no-source-filename: Do not write the filename of the script into\n");
	printf("                          the bytecode.\n\n");
#endif

#ifdef HSL_STANDALONE_RUNNER
	printf("    --no-exec: Load the bytecode, but don't execute it.\n\n");

	printf("    --print-chunks: Print chunks when the script is loaded or compiled.\n\n");

	printf("    --print-gc-output: Print garbage collection results.\n\n");

	printf("    --scripts-dir <path>: Which path to use for imported scripts.\n\n");
#endif

#ifdef USE_LOG_FILE
	printf("    --log <filename>: Log output into the specified filename.\n\n");
#endif

#ifdef HSL_STANDALONE_REPL
	printf("    --repl: Manually start the REPL.\n\n");
#endif

#ifdef VM_DEBUG
	printf("    --debugger: Manually start the debugger.\n\n");
#endif

	printf("    -h, --help: Show this help text.\n\n");

#ifdef HSL_STANDALONE_REPL
	printf("If %s is ran without any arguments, a REPL is launched.\n", TARGET_NAME);
#endif
}

void ParseArgs(int argc, char* args[]) {
	for (int i = 1; i < argc; i++) {
		std::string arg = std::string(args[i]);
		std::string nextArg = "";
		if (i + 1 < argc) {
			nextArg = args[i + 1];
		}

#ifdef HSL_COMPILER
#ifndef HSL_STANDALONE_COMPILER
		if (arg == "-c" || arg == "--compile") {
			CompileCode = true;
			if (i + 1 < argc && nextArg != "--") {
				i++;
				OutputFilename = args[i];
			}
		}
#endif
		if (arg == "--source-filename") {
			if (i + 1 < argc && nextArg != "--") {
				i++;
				SourceFilename = args[i];
			}
			else {
				printf("Must specify path\n");
				exit(-1);
			}
		}
		else if (arg == "--no-warnings") {
			ShowWarnings = false;
		}
		else if (arg == "--no-optimizations") {
			DoOptimizations = false;
		}
		else if (arg == "--no-debug-info") {
			WriteDebugInfo = false;
		}
		else if (arg == "--no-source-filename") {
			WriteSourceFilename = false;
		}
#endif
#ifdef HSL_STANDALONE_RUNNER
		if (arg == "--no-execution") {
			ExecuteCode = false;
		}
		else if (arg == "--print-chunks") {
			PrintChunks = true;
		}
		else if (arg == "--print-gc-output") {
			PrintGarbageCollectionOutput = true;
		}
		else if (arg == "--scripts-dir") {
			if (i + 1 < argc && nextArg != "--") {
				i++;
				ScriptsDirectory = nextArg;
			}
			else {
				printf("Must specify path\n");
				exit(-1);
			}
		}
#endif
#ifdef HSL_STANDALONE_REPL
		if (arg == "--repl") {
			StartREPL = true;
		}
#endif
#ifdef VM_DEBUG
		if (arg == "--debugger") {
			StartDebugger = true;
		}
#endif
#ifdef USE_LOG_FILE
		if (arg == "--log") {
			if (i + 1 < argc && nextArg != "--") {
				i++;
				LogFilename = args[i];
			}
			else {
				printf("Must specify log filename\n");
				exit(-1);
			}
		}
#endif
		if (arg == "-h" || arg == "--help") {
			PrintHelp();
			exit(0);
		}
		else {
			if (arg.size() >= 3 && StringUtils::StartsWith(args[i], "--")) {
				printf("Unrecognized option %s\n", args[i]);
				exit(-1);
			}
			else if (arg == "--") {
				i++;
			}
			if (i < argc) {
				InputFilename = args[i];
			}
		}
	}
}

int InterpretArguments(int argc, char* args[]) {
	bool success = true;

	if (!InputFilename) {
		Log::Print(Log::LOG_ERROR, "Must specify input file!");
		return -1;
	}

#ifdef HSL_STANDALONE_COMPILER
	CompileCode = true;
#endif

#ifdef HSL_COMPILER
	if (CompileCode) {
		if (SourceFilename == nullptr) {
			SourceFilename = InputFilename;
		}

		success = CompilerMain(SourceFilename, OutputFilename);
	}
	else
#endif
	{
#ifdef HSL_STANDALONE_RUNNER
		success = RunnerMain(InputFilename);
#endif
	}

	if (!success) {
		return -1;
	}

	return 0;
}

int StandaloneMain(int argc, char* args[]) {
	if (argc < 2) {
#ifndef HSL_STANDALONE_REPL
		PrintHelp();
		return -1;
#else
		StartREPL = true;
#endif
	}

	int result = 0;

	InitSubsystems();

	GlobalScriptManager = new ScriptManager;
#ifdef HSL_VM
	GlobalScriptManager->NewThread();
#endif
	GlobalScriptManager->LinkStandardLibrary();

#ifdef HSL_COMPILER
	Compiler::GetStandardConstants(GlobalScriptManager);
#endif

	ParseArgs(argc, args);

#ifdef USE_LOG_FILE
	if (LogFilename) {
		Log::OpenFile(LogFilename);
	}
#endif

#ifdef HSL_COMPILER
	Compiler::Settings.ShowWarnings = ShowWarnings;
	Compiler::Settings.DoOptimizations = DoOptimizations;
	Compiler::Settings.WriteDebugInfo = WriteDebugInfo;
	Compiler::Settings.WriteSourceFilename = WriteSourceFilename;
	Compiler::Settings.PrintChunks = PrintChunks;
#endif

#ifdef HSL_STANDALONE_REPL
#ifdef VM_DEBUG
	if (StartDebugger && !StartREPL && !InputFilename) {
		DebuggerLoop();
	}
	else
#endif
	if (StartREPL) {
#ifdef VM_DEBUG
		if (StartDebugger) {
			DebuggerLoop();
		}
		else
#endif
		{
			REPLMain();
		}
	}
	else
#endif
	{
		result = InterpretArguments(argc, args);
	}

	if (GlobalScriptManager) {
		delete GlobalScriptManager;
		GlobalScriptManager = nullptr;
	}

	DisposeSubsystems();

	return result;
}
#endif
#endif
