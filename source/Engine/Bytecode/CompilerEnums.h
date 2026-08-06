#ifndef ENGINE_COMPILER_ENUMS
#define ENGINE_COMPILER_ENUMS

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Token.h>

struct CompilerSettings {
	bool PrintToLog;
	bool ShowWarnings;
	bool WriteDebugInfo;
	bool WriteSourceFilename;
	bool DoOptimizations;
	bool PrintChunks;
};

class Parser {
public:
	Token Current;
	Token Previous;
	bool HadError;
	bool PanicMode;
};

class Scanner {
public:
	int Line;
	char* Start;
	char* Current;
	char* LinePos;
	char* SourceFilename;
	char* SourceStart;
};

enum Precedence {
	PREC_NONE,
	PREC_ASSIGNMENT, // =
	PREC_TERNARY,
	PREC_OR, // or (logical)
	PREC_AND, // and (logical)
	PREC_BITWISE_OR,
	PREC_BITWISE_XOR,
	PREC_BITWISE_AND,
	PREC_EQUALITY, // == !=
	PREC_COMPARISON, // < > <= >=
	PREC_BITWISE_SHIFT, // << >>
	PREC_TERM, // + -
	PREC_FACTOR, // * / %
	PREC_UNARY, // ! - ~ ++x --x
	PREC_CALL, // . () [] x++ x--
	PREC_PRIMARY
};

enum ExprContext {
	EXPRCONTEXT_VALUE,
	EXPRCONTEXT_LOCATION
};

class Compiler;
typedef ExprContext (Compiler::*ParseFn)(ExprContext context);

enum VariableType {
	VARTYPE_UNKNOWN,
	VARTYPE_LOCAL,
	VARTYPE_MODULE_LOCAL,
	VARTYPE_GLOBAL
};

struct Local {
	Token Name;
	VariableType Type = VARTYPE_UNKNOWN;
	int Index = -1;
	int Depth = -1;
	bool Resolved = false;
	bool WasSet = false;
	bool Constant = false;
	VMValue ConstantVal = VMValue{VAL_ERROR};
};

struct ParseRule {
	ParseFn Prefix;
	ParseFn Infix;
	enum Precedence Precedence;
};

enum FunctionType {
	FUNCTIONTYPE_TOPLEVEL,
	FUNCTIONTYPE_FUNCTION,
	FUNCTIONTYPE_CONSTRUCTOR,
	FUNCTIONTYPE_METHOD
};

#endif /* ENGINE_COMPILER_ENUMS */
