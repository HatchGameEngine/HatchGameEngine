#include <Engine/Bytecode/VMThreadDebugger.h>

#ifdef VM_DEBUG
#include <Engine/Bytecode/Bytecode.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/ScriptREPL.h>
#include <Engine/Bytecode/SourceFileMap.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Bytecode/ValuePrinter.h>
#include <Engine/Exceptions/ScriptREPLException.h>
#include <Engine/IO/TerminalInput.h>
#include <Engine/Utilities/StringUtils.h>

#ifdef HSL_COMPILER
#include <Engine/Bytecode/Compiler.h>
#endif

#ifdef HSL_STANDALONE
#include <Engine/Bytecode/StandaloneMain.h>
#endif

#ifdef USING_LINENOISE
#include <Libraries/linenoise-ng/linenoise.h>

void DebuggerCompletionCallback(const char *buf, linenoiseCompletions *completions);
#endif

typedef bool (VMThreadDebugger::*DebuggerCommandFunc)(std::vector<char*>, const char*);

struct DebuggerCommand {
	const char* Name;
	const char* Description;
	std::vector<std::string> Aliases;
	DebuggerCommandFunc Function;
};

std::unordered_map<std::string, DebuggerCommand> CommandMap;
std::vector<DebuggerCommand> CommandList;

#define CMD(name, func, aliases, desc) \
	{ \
		DebuggerCommand cmd; \
		cmd.Name = name; \
		cmd.Description = desc; \
		cmd.Aliases = aliases; \
		cmd.Function = func; \
		CommandList.push_back(cmd); \
		CommandMap[name] = cmd; \
	}

void VMThreadDebugger::Initialize() {
	CMD("continue",
		&VMThreadDebugger::Cmd_Continue,
		(std::vector<std::string>{"c", "exit"}),
		"Exits the debugger and resumes execution");

	CMD("backtrace",
		&VMThreadDebugger::Cmd_Backtrace,
		(std::vector<std::string>{"bt", "trace"}),
		"Prints the backtrace");

	CMD("stack",
		&VMThreadDebugger::Cmd_Stack,
		(std::vector<std::string>{"st", "stk"}),
		"Prints the stack");

	CMD("instruction",
		&VMThreadDebugger::Cmd_Instruction,
		(std::vector<std::string>{"ins", "inst", "instr", "op", "opcode"}),
		"Shows the current instruction");

	CMD("frame",
		&VMThreadDebugger::Cmd_Frame,
		(std::vector<std::string>{"f"}),
		"Sets the current frame");

	CMD("line",
		&VMThreadDebugger::Cmd_Line,
		(std::vector<std::string>{"l"}),
		"Shows the current line");

#if USING_VM_FUNCPTRS
	CMD("step",
		&VMThreadDebugger::Cmd_Step,
		(std::vector<std::string>{"s"}),
		"Steps until the next column or line with an instruction");

	CMD("nextinstruction",
		&VMThreadDebugger::Cmd_NextInstruction,
		(std::vector<std::string>{
			"nextins", "nextinst", "nextinstr", "nextop", "nextopcode"}),
		"Steps one instruction");
#endif

	CMD("chunk",
		&VMThreadDebugger::Cmd_Chunk,
		(std::vector<std::string>{"code"}),
		"Shows the bytecode of the function of the current frame");

	CMD("variable",
		&VMThreadDebugger::Cmd_Variable,
		(std::vector<std::string>{"printvar"}),
		"Prints a local or global variable");

	CMD("breakpoint",
		&VMThreadDebugger::Cmd_Breakpoint,
		(std::vector<std::string>{"break", "b"}),
		"Adds a breakpoint");

	CMD("tempbreakpoint",
		&VMThreadDebugger::Cmd_TempBreakpoint,
		(std::vector<std::string>{"tbreak", "tb"}),
		"Adds a temporary breakpoint");

	CMD("disablebreakpoint",
		&VMThreadDebugger::Cmd_DisableBreakpoint,
		(std::vector<std::string>{"disablebreak", "db"}),
		"Removes a breakpoint");

	CMD("enablebreakpoint",
		&VMThreadDebugger::Cmd_EnableBreakpoint,
		(std::vector<std::string>{"enablebreak", "eb"}),
		"Enables a breakpoint");

	CMD("removebreakpoint",
		&VMThreadDebugger::Cmd_RemoveBreakpoint,
		(std::vector<std::string>{"removebreak", "rb"}),
		"Removes a breakpoint");

	CMD("listbreakpoints",
		&VMThreadDebugger::Cmd_ListBreakpoints,
		(std::vector<std::string>{"listbreakpoint", "listbreak", "lb"}),
		"Lists breakpoints");

	CMD("clearbreakpoints",
		&VMThreadDebugger::Cmd_ClearBreakpoints,
		(std::vector<std::string>{"clearbreakpoint", "clearbreak", "cb"}),
		"Clears all breakpoints");

	CMD("help",
		&VMThreadDebugger::Cmd_Help,
		(std::vector<std::string>{"h"}),
		"It's this command");
}
void VMThreadDebugger::Dispose() {
	CommandMap.clear();
	CommandList.clear();
}

VMThreadDebugger::VMThreadDebugger(VMThread* thread) {
	Thread = thread;
}

void VMThreadDebugger::Enter() {
	if (!Thread) {
		printf("No thread to debug");
		return;
	}

	if (Active) {
		printf("Already debugging\n");
		return;
	}

	Active = true;
	Exiting = false;
	DebugFrame = Thread->FrameCount - 1;

	CodeDebugger = new BytecodeDebugger;
	CodeDebugger->Tokens = ScriptManager::Tokens;

	Thread->AttachedDebuggerCount++;

#ifdef USING_LINENOISE
	linenoiseSetCompletionCallback(DebuggerCompletionCallback);
#endif
}

#ifdef USING_LINENOISE
void DebuggerCompletionCallback(const char *buf, linenoiseCompletions *completions) {
	if (buf[0] == '\0') {
		return;
	}

	for (size_t i = 0; i < CommandList.size(); i++) {
		DebuggerCommand& command = CommandList[i];
		if (StringUtils::StartsWith(command.Name, buf)) {
			linenoiseAddCompletion(completions, command.Name);
		}
	}
}
#endif

void VMThreadDebugger::MainLoop() {
	while (Active) {
		// Read the line
		std::string read = TerminalInput::GetLine("> ");
		if (read.size() == 0) {
			continue;
		}

		// Split string by whitespace
		// TODO: This needs to be able to capture text in quotes correctly
		std::vector<char*> args;
		char* line = StringUtils::Create(read.c_str());
		char* tok = strtok(line, REPL_TRIM_CHARS);
		while (tok != NULL) {
			args.push_back(tok);
			tok = strtok(NULL, REPL_TRIM_CHARS);
		}

		// Interpret the line
		InterpretCommand(args, read.c_str());

		Memory::Free(line);
	}
}

void VMThreadDebugger::Exit() {
	if (Exiting) {
		return;
	}

	if (CodeDebugger) {
		CodeDebugger->Tokens = nullptr;
		delete CodeDebugger;
	}

	ScriptManager::RemoveTemporaryModules();
	ScriptManager::RemoveSourceFile("repl");

	if (Thread) {
		Thread->AttachedDebuggerCount--;
		Thread = nullptr;
	}

	Exiting = true;
}

VMThreadDebugger::~VMThreadDebugger() {
	Exit();
}

bool VMThreadDebugger::InterpretCommand(std::vector<char*> args, const char* fullLine) {
	if (args.size() == 0) {
		return false;
	}

	// Find exact match
	if (CommandMap.count(args[0])) {
		DebuggerCommand& command = CommandMap.at(args[0]);
		DebuggerCommandFunc func = command.Function;
		return (this->*func)(args, fullLine);
	}

	// Look for aliases
	for (std::unordered_map<std::string, DebuggerCommand>::iterator it = CommandMap.begin();
		it != CommandMap.end();
		it++) {
		DebuggerCommand& command = it->second;
		for (size_t i = 0; i < command.Aliases.size(); i++) {
			if (strcmp(command.Aliases[i].c_str(), args[0]) == 0) {
				DebuggerCommandFunc func = command.Function;
				return (this->*func)(args, fullLine);
			}
		}
	}

#ifdef HSL_COMPILER
	// Treat it as code
	return ExecuteCode(fullLine);
#else
	printf("Unknown command %s\n", args[0]);

	return false;
#endif
}

bool VMThreadDebugger::Cmd_Continue(std::vector<char*> args, const char* fullLine) {
	Active = false;

	return true;
}

bool VMThreadDebugger::Cmd_Backtrace(std::vector<char*> args, const char* fullLine) {
	if (!Thread->FrameCount) {
		printf("No call frames\n");
		return false;
	}

	for (Uint32 i = 0; i < Thread->FrameCount; i++) {
		CallFrame* frame = &Thread->Frames[i];
		ObjFunction* function = frame->Function;
		int line = -1;

		const char* source = GetModuleName(function->Module);
		if (i > 0 && function->Chunk.Lines && strcmp(source, "repl") != 0) {
			CallFrame* fr2 = &Thread->Frames[i - 1];
			line = fr2->Function->Chunk.Lines[fr2->IPLast - fr2->IPStart] & 0xFFFF;
		}

		std::string functionName = VMThread::GetFunctionName(function);
		printf("  %c (%d) %s", DebugFrame == i ? '>' : ' ', (int)i, functionName.c_str());

		Thread->PrintFunctionArgs(frame, nullptr);

		printf("\n");
		printf("        of %s", source);

		if (line > 0) {
			printf(" on line %d", line);
		}
		if (i < Thread->FrameCount - 1) {
			printf(", then");
		}
		printf("\n");
	}

	return true;
}

bool VMThreadDebugger::Cmd_Stack(std::vector<char*> args, const char* fullLine) {
	Thread->PrintStack();

	return true;
}

bool VMThreadDebugger::Cmd_Instruction(std::vector<char*> args, const char* fullLine) {
	CallFrame* frame = GetCallFrame();
	if (frame && frame->Function) {
		printf("byte   ln\n");
		CodeDebugger->DebugInstruction(&frame->Function->Chunk, frame->IP - frame->IPStart);
	}
	else {
		printf("No function to debug\n");
		return false;
	}

	return true;
}

bool VMThreadDebugger::Cmd_Frame(std::vector<char*> args, const char* fullLine) {
	if (!Thread->FrameCount) {
		printf("No call frames\n");
		return false;
	}

	if (args.size() < 2) {
		printf("Missing argument\n");
		return false;
	}

	int index = 0;
	if (!StringUtils::ToNumber(&index, args[1])) {
		printf("Invalid argument\n");
		return false;
	}

	if (index < 0 || index >= Thread->FrameCount) {
		printf("Frame %d out of range (0-%d)\n", index, Thread->FrameCount - 1);
		return false;
	}

	CallFrame* frame = &Thread->Frames[index];

	DebugFrame = index;

	if (frame->Function) {
		printf("In ");
		PrintCallFrameSourceLine(frame, frame->IP - frame->IPStart, true);
	}
	else {
		printf("No function to show the current line of\n");
		return false;
	}

	return true;
}

bool VMThreadDebugger::Cmd_Line(std::vector<char*> args, const char* fullLine) {
	if (!Thread->FrameCount) {
		printf("No call frames\n");
		return false;
	}

	CallFrame* frame = GetCallFrame();

	if (frame && frame->Function) {
		printf("In ");
		PrintCallFrameSourceLine(frame, frame->IP - frame->IPStart, true);
	}
	else {
		printf("No function to show the current line of\n");
		return false;
	}

	return true;
}

#if USING_VM_FUNCPTRS
bool VMThreadDebugger::Cmd_Step(std::vector<char*> args, const char* fullLine) {
	if (!Thread->FrameCount) {
		printf("No call frames\n");
		return false;
	}

	if (DebugFrame != Thread->FrameCount - 1) {
		printf("Cannot step outside of top frame\n");
		return false;
	}

	CallFrame* frame = GetCallFrame();

	if (frame && frame->Function) {
		if (!frame->Function->Chunk.Lines) {
			Thread->RunOpcodeFunc(frame);
		}
		else {
			size_t bpos = frame->IP - frame->IPStart;
			int lastPos = (frame->Function->Chunk.Lines[bpos] >> 16) & 0xFFFF;
			int currentPos = lastPos;
			while (lastPos == currentPos) {
				Thread->RunOpcodeFunc(frame);
				bpos = frame->IP - frame->IPStart;
				currentPos = (frame->Function->Chunk.Lines[bpos] >> 16) & 0xFFFF;
			}
		}

		if (Thread->FrameCount > 0) {
			DebugFrame = Thread->FrameCount - 1;

			frame = &Thread->Frames[DebugFrame];
			frame->IPLast = frame->IP;

			printf("In ");
			PrintCallFrameSourceLine(frame, frame->IP - frame->IPStart, true);
		}
		else {
			printf("No code left to step through\n");
			return true;
		}
	}
	else {
		printf("No function to step through\n");
		return false;
	}

	return true;
}

bool VMThreadDebugger::Cmd_NextInstruction(std::vector<char*> args, const char* fullLine) {
	if (!Thread->FrameCount) {
		printf("No call frames\n");
		return false;
	}

	if (DebugFrame != Thread->FrameCount - 1) {
		printf("Cannot step outside of top frame\n");
		return false;
	}

	CallFrame* frame = GetCallFrame();

	if (frame && frame->Function) {
		Thread->RunOpcodeFunc(frame);

		if (Thread->FrameCount > 0) {
			DebugFrame = Thread->FrameCount - 1;

			frame = &Thread->Frames[DebugFrame];
			frame->IPLast = frame->IP;

			printf("byte   ln\n");
			CodeDebugger->DebugInstruction(
				&frame->Function->Chunk, frame->IP - frame->IPStart);
		}
		else {
			printf("No code left to step through\n");
			return true;
		}
	}
	else {
		printf("No function to step through\n");
		return false;
	}

	return true;
}
#endif

bool VMThreadDebugger::Cmd_Chunk(std::vector<char*> args, const char* fullLine) {
	if (!Thread->FrameCount) {
		printf("No call frames\n");
		return false;
	}

	CallFrame* frame = GetCallFrame();

	if (frame && frame->Function) {
		CodeDebugger->DebugChunk(&frame->Function->Chunk,
			frame->Function->Name,
			frame->Function->MinArity,
			frame->Function->Arity);
	}
	else {
		printf("No chunk to debug\n");
		return false;
	}

	return true;
}

bool VMThreadDebugger::Cmd_Variable(std::vector<char*> args, const char* fullLine) {
	if (args.size() < 2) {
		printf("Missing argument\n");
		return false;
	}

	const char* varName = args[1];
	int which = -1;
	if (args.size() >= 3) {
		if (!StringUtils::ToNumber(&which, args[2])) {
			printf("Invalid argument\n");
			return false;
		}

		if (which < 1) {
			printf("Invalid argument\n");
			return false;
		}
	}

	CallFrame* frame = GetCallFrame();
	Chunk* chunk = nullptr;

	VMValue value = VMValue{VAL_ERROR};
	std::vector<ChunkLocal> matches;
	ChunkLocal local;

	bool printDeclaration = false;
	bool exit = false;

	if (frame && frame->Function) {
		chunk = &frame->Function->Chunk;
	}

	if (chunk && chunk->Locals) {
		bool printMessage = false;

		for (size_t i = 0; i < chunk->Locals->size(); i++) {
			ChunkLocal local = (*chunk->Locals)[i];
			if (strcmp(local.Name, varName) != 0) {
				continue;
			}

			if (which == -1 && matches.size() == 1) {
				printMessage = true;
			}

			matches.push_back(local);
		}

		if (printMessage) {
			printf("Note: More than one variable in this function is named \"%s\" (%d occurrences)\n",
				varName,
				(int)matches.size());
			printf("Use \"variable %s <index>\" to show the n-th occurrence of \"%s\" (starts from 1)\n",
				varName,
				varName);
		}
	}

	if (matches.size() > 0) {
		if (which == -1) {
			local = matches[0];

			if (matches.size() > 1) {
				for (int i = (int)matches.size() - 1; i >= 0; i--) {
					if (matches[i].Index < Thread->StackTop - frame->Slots) {
						local = matches[i];
						which = i + 1;
						break;
					}
				}

				printf("Showing occurrence %d/%d of variable \"%s\"\n",
					which,
					(int)matches.size(),
					varName);
			}
		}
		else {
			if (which > (int)matches.size()) {
				printf("No occurrence #%d of a variable named \"%s\" in this function\n",
					which,
					varName);
				return false;
			}

			local = matches[which - 1];
		}

		printDeclaration = true;

		if (local.Constant) {
			if (local.Index >= chunk->Constants->size()) {
				printf("Invalid constant \"%s\"\n", varName);
				exit = true;
			}
			else {
				value = Thread->GetConstant(chunk, local.Index);
			}
		}
		else {
			if (local.Index >= Thread->StackTop - frame->Slots) {
				printf("Variable \"%s\" not yet initialized or out of scope\n", varName);
				exit = true;
			}
			else {
				value = frame->Slots[local.Index];
			}
		}

		matches.clear();
	}
	else if (which != -1) {
		printf("No occurrence #%d of a variable named \"%s\" in this function\n",
			which,
			varName);
		return false;
	}

	if (printDeclaration) {
		int line = local.Position & 0xFFFF;
		int pos = (local.Position >> 16) & 0xFFFF;
		if (line != 0) {
			printf("Declared at ");
			PrintCallFrameSourceLine(frame, line, pos, false);
		}
	}

	if (exit) {
		return false;
	}

	if (chunk && value.Type == VAL_ERROR) {
		// Only the first chunk in the module contains module locals
		chunk = &(*frame->Function->Module->Functions)[0]->Chunk;

		if (chunk->ModuleLocals) {
			for (size_t i = 0; i < chunk->ModuleLocals->size(); i++) {
				local = (*chunk->ModuleLocals)[i];
				if (strcmp(local.Name, varName) != 0) {
					continue;
				}

				if ((local.Constant && local.Index >= chunk->Constants->size()) ||
					(local.Index >= frame->Module->Locals->size())) {
					printf("Invalid module local \"%s\"\n", varName);
					return false;
				}

				if (local.Constant) {
					value = Thread->GetConstant(chunk, local.Index);
				}
				else {
					value = (*frame->Module->Locals)[local.Index];
				}
				break;
			}
		}
	}

	if (value.Type == VAL_ERROR && !ScriptManager::Constants->GetIfExists(varName, &value) &&
		!ScriptManager::Globals->GetIfExists(varName, &value)) {
		printf("No variable named \"%s\"\n", varName);
		return false;
	}

	printf("%s = ", varName);
	if (IS_STRING(value)) {
		printf("\"");
	}
	ValuePrinter::Print(value);
	if (IS_STRING(value)) {
		printf("\"");
	}
	printf("\n");

	return true;
}

bool VMThreadDebugger::Cmd_Breakpoint(std::vector<char*> args, const char* fullLine) {
	Uint32 position = 0;
	ObjFunction* function = GetFunctionForBreakpoint(args, position);
	if (!function) {
		return false;
	}

	// Add the breakpoint
	VMThreadBreakpoint* bp = Thread->AddBreakpoint(function, position, BREAKPOINT_ONHIT_KEEP);

	if (bp == nullptr) {
		// Must already have been added
		bp = Thread->FindBreakpoint(function, position);
		if (!bp) {
			return false;
		}

		printf("Already set ");
	}
	else {
		printf("Set ");
	}

	printf("breakpoint %d at %s", bp->Index, GetModuleName(function->Module));

	Chunk* chunk = &function->Chunk;
	if (chunk->Lines) {
		int line = chunk->Lines[position] & 0xFFFF;
		int pos = (chunk->Lines[position] >> 16) & 0xFFFF;

		printf(", line %d, column %d\n", line, pos);

		if (function->Module->HasSourceFilename) {
			PrintSourceLineAndPosition(function->Module->SourceFilename, line, pos);
		}
	}
	else {
		printf("\n");
	}

	return true;
}

bool VMThreadDebugger::Cmd_TempBreakpoint(std::vector<char*> args, const char* fullLine) {
	Uint32 position = 0;
	ObjFunction* function = GetFunctionForBreakpoint(args, position);
	if (!function) {
		return false;
	}

	const int type = BREAKPOINT_ONHIT_REMOVE;

	// Add the breakpoint
	VMThreadBreakpoint* bp = Thread->AddBreakpoint(function, position, type);

	if (bp == nullptr) {
		// Must already have been added
		bp = Thread->FindBreakpoint(function, position);
		if (!bp) {
			return false;
		}

		// Change what to do when it's hit
		bp->OnHit = type;
	}

	printf("Set temporary breakpoint %d at %s", bp->Index, GetModuleName(function->Module));

	Chunk* chunk = &function->Chunk;
	if (chunk->Lines) {
		int line = chunk->Lines[position] & 0xFFFF;
		int pos = (chunk->Lines[position] >> 16) & 0xFFFF;

		printf(", line %d, column %d\n", line, pos);

		if (function->Module->HasSourceFilename) {
			PrintSourceLineAndPosition(function->Module->SourceFilename, line, pos);
		}
	}
	else {
		printf("\n");
	}

	return true;
}

bool VMThreadDebugger::Cmd_DisableBreakpoint(std::vector<char*> args, const char* fullLine) {
	int index = 0;
	if (!StringUtils::ToNumber(&index, args[1]) || index < 0) {
		printf("Invalid argument\n");
		return false;
	}

	VMThreadBreakpoint* breakpoint = Thread->GetBreakpoint(index);

	if (!breakpoint) {
		printf("No breakpoint %d\n", index);
		return false;
	}

	breakpoint->Enabled = false;

	printf("Disabled breakpoint %d\n", index);

	return false;
}

bool VMThreadDebugger::Cmd_EnableBreakpoint(std::vector<char*> args, const char* fullLine) {
	int index = 0;
	if (!StringUtils::ToNumber(&index, args[1]) || index < 0) {
		printf("Invalid argument\n");
		return false;
	}

	VMThreadBreakpoint* breakpoint = Thread->GetBreakpoint(index);

	if (!breakpoint) {
		printf("No breakpoint %d\n", index);
		return false;
	}

	breakpoint->Enabled = true;

	printf("Enabled breakpoint %d\n", index);

	return false;
}

bool VMThreadDebugger::Cmd_RemoveBreakpoint(std::vector<char*> args, const char* fullLine) {
	int index = 0;
	if (!StringUtils::ToNumber(&index, args[1]) || index < 0) {
		printf("Invalid argument\n");
		return false;
	}

	if (Thread->RemoveBreakpoint(index)) {
		printf("Removed breakpoint %d\n", index);
		return true;
	}

	printf("No breakpoint %d\n", index);

	return false;
}

bool VMThreadDebugger::Cmd_ListBreakpoints(std::vector<char*> args, const char* fullLine) {
	if (Thread->Breakpoints.size() == 0) {
		printf("No breakpoints in this thread\n");
		return false;
	}

	printf("Breakpoints:\n");

	for (size_t i = 0; i < Thread->Breakpoints.size(); i++) {
		VMThreadBreakpoint* breakpoint = Thread->Breakpoints[i];
		ObjFunction* function = breakpoint->Function;
		Chunk* chunk = &function->Chunk;

		int line = 0;
		int pos = 0;

		printf("%d: %s", breakpoint->Index, GetModuleName(function->Module));

		if (chunk->Lines) {
			line = chunk->Lines[breakpoint->CodeOffset] & 0xFFFF;
			pos = (chunk->Lines[breakpoint->CodeOffset] >> 16) & 0xFFFF;

			printf(", line %d, column %d", line, pos);
		}
		else {
			printf(", code offset %d", breakpoint->CodeOffset);
		}

		if (breakpoint->Enabled) {
			printf(", enabled");
		}
		else {
			printf(", disabled");
		}

		if (breakpoint->OnHit == BREAKPOINT_ONHIT_DISABLE) {
			printf(", disable on hit");
		}
		else if (breakpoint->OnHit == BREAKPOINT_ONHIT_REMOVE) {
			printf(", remove on hit");
		}

		printf("\n");

		if (line > 0 && function->Module->HasSourceFilename) {
			PrintSourceLineAndPosition(function->Module->SourceFilename, line, pos);
		}
	}

	return true;
}

bool VMThreadDebugger::Cmd_ClearBreakpoints(std::vector<char*> args, const char* fullLine) {
	Thread->DisposeBreakpoints();

	printf("Removed all breakpoints.\n");

	return true;
}

bool VMThreadDebugger::Cmd_Help(std::vector<char*> args, const char* fullLine) {
	printf("Commands:\n");

	for (size_t i = 0; i < CommandList.size(); i++) {
		DebuggerCommand& command = CommandList[i];

		printf("    %s: %s\n", command.Name, command.Description);

		if (command.Aliases.size() > 0) {
			printf("      Aliases: ");
			for (size_t j = 0; j < command.Aliases.size(); j++) {
				printf("%s", command.Aliases[j].c_str());
				if (j < command.Aliases.size() - 1) {
					printf(", ");
				}
			}
			printf("\n");
		}

		printf("\n");
	}

	return true;
}

bool VMThreadDebugger::ExecuteCode(const char* code) {
#ifdef HSL_COMPILER
	if (Thread->FrameCount && DebugFrame != Thread->FrameCount - 1) {
		printf("Cannot execute code outside of top frame\n");
		return false;
	}

	CallFrame* frame = GetCallFrame();

	CompilerSettings settings = Compiler::Settings;
	settings.PrintToLog = false;

	VMValue result;
	try {
		result = ScriptREPL::ExecuteCode(Thread, frame, code, settings);
	} catch (const ScriptREPLException& error) {
		printf("%s\n", error.what());
		return false;
	}

	// Print what was left on the stack
	if (IS_STRING(result)) {
		printf("\"");
	}
	ValuePrinter::Print(result);
	if (IS_STRING(result)) {
		printf("\"");
	}
	printf("\n");

	ScriptManager::RequestGarbageCollection(false);

	return true;
#else
	return false;
#endif
}

ObjFunction* VMThreadDebugger::GetFunctionForBreakpoint(std::vector<char*> args, Uint32& position) {
	CallFrame* frame = GetCallFrame();

	ObjFunction* function = nullptr;
	ObjFunction* currentFunction = nullptr;

	int lineNum = 0;

	if (args.size() < 2) {
		if (!frame || !frame->Function) {
			printf("No current function to set breakpoint\n");
			return nullptr;
		}

		currentFunction = frame->Function;
		function = currentFunction;
	}
	else {
		const char* arg = args[1];

		if (args.size() >= 3) {
			StringUtils::ToNumber(&lineNum, args[2]);

			if (lineNum < 1) {
				printf("Invalid argument\n");
				return nullptr;
			}
		}

		Uint32 hash = ScriptManager::MakeFilenameHash(arg);

		if (ScriptManager::BytecodeForFilenameHashExists(hash)) {
			if (!ScriptManager::IsScriptLoaded(hash)) {
				printf("Script \"%s\" is not loaded\n", arg);
				return nullptr;
			}

			ObjModule* module = ScriptManager::GetScriptModule(hash);
			if (!module) {
				printf("Script \"%s\" is not loaded\n", arg);
				return nullptr;
			}

			function = ScriptManager::GetFunctionAtScriptLine(module, lineNum);
			if (!function) {
				printf("Cannot set breakpoint at line %d\n", lineNum);
				return nullptr;
			}
		}
		else {
			VMValue callable = ScriptManager::FindFunction(arg);

			if (IS_NULL(callable)) {
				printf("No function named \"%s\"\n", arg);
				return nullptr;
			}
			else if (!IS_FUNCTION(callable)) {
				printf("\"%s\" must be a function, not a bound method or a native\n",
					arg);
				return nullptr;
			}

			function = AS_FUNCTION(callable);
		}
	}

	Chunk* chunk = &function->Chunk;

	position = 0;

	if (function == currentFunction) {
		position = frame->IP - frame->IPStart;
		position += Bytecode::GetTotalOpcodeSize(chunk->Code + position);
	}
	else if (lineNum > 0) {
		if (chunk->Lines) {
			bool foundPosition = false;

			for (int offset = 0; offset < chunk->Count;) {
				int line = chunk->Lines[offset] & 0xFFFF;
				if (line == lineNum) {
					position = (Uint32)offset;
					foundPosition = true;
					break;
				}

				offset += Bytecode::GetTotalOpcodeSize(chunk->Code + offset);
			}

			if (!foundPosition) {
				printf("Cannot set breakpoint at line %d\n", lineNum);
				return nullptr;
			}
		}
		else {
			printf("Note: No line information present for this function. Setting breakpoint at the beginning\n");
		}
	}

	if (position >= chunk->Count) {
		printf("No next instruction to set breakpoint\n");
		return nullptr;
	}

	return function;
}

CallFrame* VMThreadDebugger::GetCallFrame() {
	if (Thread && Thread->FrameCount > 0 && DebugFrame >= 0) {
		return &Thread->Frames[DebugFrame];
	}

	return nullptr;
}

void VMThreadDebugger::PrintCallFrameSourceLine(CallFrame* frame,
	int line,
	int pos,
	bool showFunction) {
	ObjFunction* function = frame->Function;

	const char* sourceFilename = GetModuleName(function->Module);

	if (showFunction) {
		std::string functionName = VMThread::GetFunctionName(function);

		printf("%s", functionName.c_str());

		Thread->PrintFunctionArgs(frame, nullptr);

		printf("\nat %s", sourceFilename);
	}
	else {
		printf("%s", sourceFilename);
	}

	if (line > -1 && pos > -1) {
		printf(", line %d, column %d\n", line, pos);

		if (function->Module->HasSourceFilename) {
			PrintSourceLineAndPosition(function->Module->SourceFilename, line, pos);
		}
	}
	else {
		printf("\n");
	}
}

void VMThreadDebugger::PrintCallFrameSourceLine(CallFrame* frame, size_t bpos, bool showFunction) {
	int line = -1;
	int pos = -1;

	ObjFunction* function = frame->Function;
	if (function->Chunk.Lines) {
		line = function->Chunk.Lines[bpos] & 0xFFFF;
		pos = (function->Chunk.Lines[bpos] >> 16) & 0xFFFF;
	}

	PrintCallFrameSourceLine(frame, line, pos, showFunction);
}

bool VMThreadDebugger::PrintSourceLineAndPosition(const char* sourceFilename, int line, int pos) {
	std::string filePath = "";
	if (sourceFilename) {
#ifndef HSL_STANDALONE
		filePath = std::string(SCRIPTS_DIRECTORY_NAME) + "/";
#else
		const char* scriptsDir = GetScriptsDirectory();
		if (scriptsDir) {
			filePath = std::string(scriptsDir) + "/";
		}
#endif
		filePath += std::string(sourceFilename);
	}
	else {
		filePath = "repl";
	}

	// TODO: This doesn't watch for changes in the source file
	char* sourceLine = ScriptManager::GetSourceCodeLine(filePath.c_str(), line);
	if (!sourceLine) {
		return false;
	}

	printf("%5d | %s\n", line, sourceLine);
	printf("       ");
	while (pos--) {
		printf(" ");
	}
	printf("^\n");

	return true;
}
#else
void VMThreadDebugger::Initialize() {}
void VMThreadDebugger::Dispose() {}
#endif
