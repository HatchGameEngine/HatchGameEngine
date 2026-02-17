#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/ScriptREPL.h>
#include <Engine/Bytecode/VMThread.h>
#include <Engine/Exceptions/ScriptREPLException.h>

#ifdef HSL_COMPILER
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Exceptions/CompilerErrorException.h>
#include <Engine/Utilities/StringUtils.h>
#endif

#ifdef HSL_VM
VMValue ScriptREPL::ExecuteCode(VMThread* thread, CallFrame* frame, const char* code, CompilerSettings settings) {
#ifdef HSL_COMPILER
	ObjFunction* function = nullptr;
	Chunk* chunk = nullptr;
	Chunk* firstChunk = nullptr;

	if (thread->FrameCount == FRAMES_MAX) {
		throw ScriptREPLException("No call frame available for executing code");
	}

	if (frame) {
		function = frame->Function;
		chunk = &function->Chunk;
		firstChunk = &(*function->Module->Functions)[0]->Chunk;
	}

	// Prepare the compiler
	Compiler::PrepareCompiling();

	Compiler* compiler = new Compiler;
	compiler->InREPL = true;
	compiler->CurrentSettings = settings;

	if (!function || function->Index == 0) {
		compiler->Type = FUNCTIONTYPE_TOPLEVEL;
		compiler->ScopeDepth = 0;
	}
	else {
		compiler->Type = FUNCTIONTYPE_FUNCTION;
		compiler->ScopeDepth = 1;
	}

	compiler->Initialize();

	// Only the first chunk in the module contains module locals
	if (firstChunk && firstChunk->ModuleLocals) {
		for (size_t i = 0; i < firstChunk->ModuleLocals->size(); i++) {
			ChunkLocal local = (*firstChunk->ModuleLocals)[i];

			Local* compilerLocal;
			Local constLocal;

			if (local.Constant) {
				if (local.Index >= chunk->Constants->size()) {
					continue;
				}

				VMValue constVal = thread->GetConstant(chunk, local.Index);
				compilerLocal = &constLocal;
				compilerLocal->Index = compiler->MakeConstant(constVal);
				compilerLocal->ConstantVal = constVal;
				compilerLocal->Constant = true;
			}
			else {
				if (Compiler::ModuleLocals.size() == 0xFFFF) {
					continue;
				}

				int index = compiler->AddModuleLocal();
				compilerLocal = &Compiler::ModuleLocals[index];
				compilerLocal->Index = local.Index;
			}

			compilerLocal->Depth = 0;
			compilerLocal->Resolved = true;

			Compiler::RenameLocal(compilerLocal, local.Name);

			if (function->Chunk.Lines) {
				Token* nameToken = &compilerLocal->Name;
				nameToken->Line = function->Chunk.Lines[local.Position] & 0xFFFF;
				nameToken->Pos =
					(function->Chunk.Lines[local.Position] >> 16) & 0xFFFF;
			}

			if (local.Constant) {
				compiler->Constants.push_back(constLocal);
			}
		}
	}

	if (chunk && chunk->Locals) {
		for (size_t i = 0; i < chunk->Locals->size(); i++) {
			ChunkLocal local = (*chunk->Locals)[i];

			Local* compilerLocal;
			Local constLocal;

			if (local.Constant) {
				if (local.Index >= chunk->Constants->size()) {
					continue;
				}

				VMValue constVal = thread->GetConstant(chunk, local.Index);
				compilerLocal = &constLocal;
				compilerLocal->Index = compiler->MakeConstant(constVal);
				compilerLocal->ConstantVal = constVal;
				compilerLocal->Constant = true;
			}
			else {
				if (local.Index >= thread->StackTop - frame->Slots ||
					compiler->LocalCount == 0xFF) {
					continue;
				}

				int index = compiler->AddLocal();
				compilerLocal = &compiler->Locals[index];
				compilerLocal->Index = local.Index;
				compilerLocal->Constant = false;
			}

			compilerLocal->Depth = compiler->ScopeDepth;
			compilerLocal->Resolved = true;

			Compiler::RenameLocal(compilerLocal, local.Name);

			if (function->Chunk.Lines) {
				Token* nameToken = &compilerLocal->Name;
				nameToken->Line = function->Chunk.Lines[local.Position] & 0xFFFF;
				nameToken->Pos =
					(function->Chunk.Lines[local.Position] >> 16) & 0xFFFF;
			}

			if (local.Constant) {
				compiler->Constants.push_back(constLocal);
			}
		}
	}

	compiler->SetupLocals();

	// Compile the code
	ObjModule* module = nullptr;
	try {
		module = ScriptManager::CompileAndLoad(thread, compiler, code, nullptr);
	} catch (const CompilerErrorException& error) {
		delete compiler;
		Compiler::FinishCompiling();
		throw ScriptREPLException(error.what());
	}

	delete compiler;
	Compiler::FinishCompiling();

	VMValue result = NULL_VAL;

	// Execute the code
	if (module) {
		ObjFunction* newFunction = (*module->Functions)[0];

		// Add any new module locals to the current module
		if (firstChunk && firstChunk->ModuleLocals && newFunction->Chunk.ModuleLocals) {
			size_t start = firstChunk->ModuleLocals->size();
			for (size_t i = start; i < newFunction->Chunk.ModuleLocals->size(); i++) {
				ChunkLocal copy = (*newFunction->Chunk.ModuleLocals)[i];
				copy.Name = StringUtils::Duplicate(copy.Name);
				firstChunk->ModuleLocals->push_back(copy);
			}
		}

		VMValue* lastStackTop = thread->StackTop;
		int lastFrame = thread->FrameCount;
		int lastReturnFrame = thread->ReturnFrame;

		thread->ReturnFrame = thread->FrameCount;

		bool didCall = thread->Call(newFunction, 0);
		if (didCall) {
			VMValue lastInterpretResult = thread->InterpretResult;
			thread->InterpretResult = NULL_VAL;

			if (lastFrame > 0) {
				CallFrame* currentCallFrame = &thread->Frames[thread->FrameCount - 1];
				CallFrame* lastCallFrame = &thread->Frames[lastFrame - 1];

				currentCallFrame->Slots = lastCallFrame->Slots;
				currentCallFrame->ModuleLocals = lastCallFrame->ModuleLocals;
			}

#ifdef VM_DEBUG
			ScriptManager::AddSourceFile("repl", StringUtils::Duplicate(code));
#endif

			thread->RunInstructionSet();
			result = thread->InterpretResult;
			thread->InterpretResult = lastInterpretResult;
		}

		thread->ReturnFrame = lastReturnFrame;
		thread->FrameCount = lastFrame;
		thread->StackTop = lastStackTop;

		if (!didCall) {
			throw ScriptREPLException("Could not execute code");
		}
	}
	else {
		throw ScriptREPLException("Could not compile code");
	}

	return result;
#else
	throw ScriptREPLException("No compiler in this build");
#endif
}
#endif
