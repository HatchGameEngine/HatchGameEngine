#ifndef ENGINE_COMPILER_ENUMS
#define ENGINE_COMPILER_ENUMS

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Token.h>

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

class Compiler;
typedef void (Compiler::*ParseFn)(bool canAssign);

struct Local {
	Token Name;
	int Depth = -1;
	bool Resolved = false;
	bool WasSet = false;
	bool Constant = false;
	VMValue ConstantVal = VMValue{VAL_ERROR};
};

struct ParseRule {
	ParseFn Prefix;
	ParseFn Infix;
	ParseFn Suffix;
	enum Precedence Precedence;
};

#endif /* ENGINE_COMPILER_ENUMS */
