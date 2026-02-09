#ifndef ENGINE_BYTECODE_VMTHREADDEBUGGER_H
#define ENGINE_BYTECODE_VMTHREADDEBUGGER_H

#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/VMThread.h>
#include <Engine/Includes/Standard.h>

#ifdef VM_DEBUG
#include <Engine/Bytecode/BytecodeDebugger.h>
#endif

class VMThreadDebugger {
#ifdef VM_DEBUG
private:
	bool ReadLine(std::string& line);
	bool InterpretCommand(std::vector<char*> args, const char* fullLine);
	bool ExecuteCode(const char* code);

	bool Cmd_Help(std::vector<char*> args, const char* fullLine);
	bool Cmd_Continue(std::vector<char*> args, const char* fullLine);
	bool Cmd_Backtrace(std::vector<char*> args, const char* fullLine);
	bool Cmd_Stack(std::vector<char*> args, const char* fullLine);
	bool Cmd_Instruction(std::vector<char*> args, const char* fullLine);
	bool Cmd_Frame(std::vector<char*> args, const char* fullLine);
	bool Cmd_Line(std::vector<char*> args, const char* fullLine);
#if USING_VM_FUNCPTRS
	bool Cmd_Step(std::vector<char*> args, const char* fullLine);
	bool Cmd_NextInstruction(std::vector<char*> args, const char* fullLine);
#endif
	bool Cmd_Chunk(std::vector<char*> args, const char* fullLine);
	bool Cmd_Variable(std::vector<char*> args, const char* fullLine);
	bool Cmd_Breakpoint(std::vector<char*> args, const char* fullLine);

	ObjModule* CompileCode(Compiler* compiler, const char* code);
	CallFrame* GetCallFrame();
	void PrintStackTrace();
	bool PrintSourceLineAndPosition(const char* sourceFilename, int line, int pos);

	VMThread* Thread = nullptr;
	bool Active = false;
	bool Exiting = false;
	int DebugFrame = 0;
	BytecodeDebugger* CodeDebugger = nullptr;
#endif

public:
#ifdef VM_DEBUG
	void Enter();
	void MainLoop();
	void Exit();

	void PrintCallFrameSourceLine(CallFrame* frame, int line, int pos, bool showFunction);
	void PrintCallFrameSourceLine(CallFrame* frame, size_t bpos, bool showFunction);
#endif

	VMThreadDebugger(VMThread* thread);
	~VMThreadDebugger();

	static void Initialize();
	static void Dispose();
};

#endif /* ENGINE_BYTECODE_VMTHREADDEBUGGER_H */
