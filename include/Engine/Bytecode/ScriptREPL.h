#ifndef ENGINE_BYTECODE_SCRIPTREPL_H
#define ENGINE_BYTECODE_SCRIPTREPL_H

#include <Engine/Bytecode/CompilerEnums.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/VMThread.h>

class ScriptREPL {
#ifdef HSL_VM
public:
	static VMValue ExecuteCode(VMThread* thread, CallFrame* frame, const char* code, CompilerSettings settings);
#endif
};

#endif /* ENGINE_BYTECODE_SCRIPTREPL_H */
