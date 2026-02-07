#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Engine/Bytecode/Bytecode.h>
#include <Engine/Bytecode/BytecodeDebugger.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/GarbageCollector.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Exceptions/CompilerErrorException.h>

#include <Engine/Application.h>

Parser Compiler::parser;
Scanner Compiler::scanner;
ParseRule* Compiler::Rules = NULL;
vector<ObjFunction*> Compiler::Functions;
vector<Local> Compiler::ModuleLocals;
vector<Local> Compiler::ModuleConstants;
HashMap<VMValue>* Compiler::StandardConstants = NULL;
HashMap<Token>* Compiler::TokenMap = NULL;

bool Compiler::DoLogging = false;
CompilerSettings Compiler::Settings;

// Order these by C/C++ precedence operators
enum TokenTYPE {
	// Other
	TOKEN_LEFT_BRACE,
	TOKEN_RIGHT_BRACE,
	// Precedence 2
	TOKEN_DECREMENT,
	TOKEN_INCREMENT,
	TOKEN_LEFT_PAREN,
	TOKEN_RIGHT_PAREN,
	TOKEN_LEFT_SQUARE_BRACE,
	TOKEN_RIGHT_SQUARE_BRACE,
	TOKEN_DOT,
	// Precedence 3
	TOKEN_LOGICAL_NOT, // (!)
	TOKEN_BITWISE_NOT, // (~)
	TOKEN_TYPEOF,
	TOKEN_NEW,
	// Precedence 5
	TOKEN_MULTIPLY,
	TOKEN_DIVISION,
	TOKEN_MODULO,
	// Precedence 6
	TOKEN_PLUS,
	TOKEN_MINUS,
	// Precedence 7
	TOKEN_BITWISE_LEFT,
	TOKEN_BITWISE_RIGHT,
	// Precedence 8
	// Precedence 9
	TOKEN_LESS,
	TOKEN_LESS_EQUAL,
	TOKEN_GREATER,
	TOKEN_GREATER_EQUAL,
	// Precedence 10
	TOKEN_EQUALS,
	TOKEN_NOT_EQUALS,
	TOKEN_HAS,
	// Precedence 11
	TOKEN_BITWISE_AND,
	TOKEN_BITWISE_XOR,
	TOKEN_BITWISE_OR,
	// Precedence 14
	TOKEN_LOGICAL_AND,
	TOKEN_LOGICAL_OR,
	// Precedence 16
	TOKEN_TERNARY,
	TOKEN_COLON,
	// Assignments
	TOKEN_ASSIGNMENT,
	TOKEN_ASSIGNMENT_MULTIPLY,
	TOKEN_ASSIGNMENT_DIVISION,
	TOKEN_ASSIGNMENT_MODULO,
	TOKEN_ASSIGNMENT_PLUS,
	TOKEN_ASSIGNMENT_MINUS,
	TOKEN_ASSIGNMENT_BITWISE_LEFT,
	TOKEN_ASSIGNMENT_BITWISE_RIGHT,
	TOKEN_ASSIGNMENT_BITWISE_AND,
	TOKEN_ASSIGNMENT_BITWISE_XOR,
	TOKEN_ASSIGNMENT_BITWISE_OR,
	// Precedence 17
	TOKEN_COMMA,
	TOKEN_SEMICOLON,

	// Others
	TOKEN_IDENTIFIER,
	TOKEN_STRING,
	TOKEN_NUMBER,
	TOKEN_DECIMAL,
	TOKEN_HITBOX,

	// Literals.
	TOKEN_FALSE,
	TOKEN_TRUE,
	TOKEN_NULL,

	// Script Keywords.
	TOKEN_EVENT,
	TOKEN_VAR,
	TOKEN_STATIC,
	TOKEN_LOCAL,
	TOKEN_CONST,

	// Keywords.
	TOKEN_DO,
	TOKEN_IF,
	TOKEN_OR,
	TOKEN_AND,
	TOKEN_FOR,
	TOKEN_FOREACH,
	TOKEN_CASE,
	TOKEN_ELSE,
	TOKEN_THIS,
	TOKEN_WITH,
	TOKEN_SUPER,
	TOKEN_BREAK,
	TOKEN_CLASS,
	TOKEN_ENUM,
	TOKEN_WHILE,
	TOKEN_REPEAT,
	TOKEN_RETURN,
	TOKEN_SWITCH,
	TOKEN_DEFAULT,
	TOKEN_CONTINUE,
	TOKEN_IMPORT,
	TOKEN_AS,
	TOKEN_IN,
	TOKEN_FROM,
	TOKEN_USING,
	TOKEN_NAMESPACE,

	TOKEN_PRINT,
	TOKEN_BREAKPOINT,

	TOKEN_ERROR,
	TOKEN_EOF
};
enum FunctionType {
	TYPE_TOP_LEVEL,
	TYPE_FUNCTION,
	TYPE_CONSTRUCTOR,
	TYPE_METHOD,
};

Token Compiler::MakeToken(int type) {
	Token token;
	token.Type = type;
	token.Start = scanner.Start;
	token.Length = (int)(scanner.Current - scanner.Start);
	token.Line = scanner.Line;
	token.Pos = (scanner.Start - scanner.LinePos) + 1;

	return token;
}
Token Compiler::MakeTokenRaw(int type, const char* message) {
	Token token;
	token.Type = type;
	token.Start = (char*)message;
	token.Length = (int)strlen(message);
	token.Line = 0;
	token.Pos = scanner.Current - scanner.LinePos;

	return token;
}
Token Compiler::ErrorToken(const char* message) {
	Token token;
	token.Type = TOKEN_ERROR;
	token.Start = (char*)message;
	token.Length = (int)strlen(message);
	token.Line = scanner.Line;
	token.Pos = scanner.Current - scanner.LinePos;

	return token;
}

bool Compiler::IsEOF() {
	return *scanner.Current == 0;
}
bool Compiler::IsDigit(char c) {
	return c >= '0' && c <= '9';
}
bool Compiler::IsHexDigit(char c) {
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}
bool Compiler::IsAlpha(char c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
bool Compiler::IsIdentifierStart(char c) {
	return IsAlpha(c) || c == '$' || c == '_';
}
bool Compiler::IsIdentifierBody(char c) {
	return IsIdentifierStart(c) || IsDigit(c);
}

bool Compiler::MatchChar(char expected) {
	if (IsEOF()) {
		return false;
	}
	if (*scanner.Current != expected) {
		return false;
	}

	scanner.Current++;
	return true;
}
char Compiler::AdvanceChar() {
	return *scanner.Current++;
	// scanner.Current++;
	// return *(scanner.Current - 1);
}
char Compiler::PrevChar() {
	return *(scanner.Current - 1);
}
char Compiler::PeekChar() {
	return *scanner.Current;
}
char Compiler::PeekNextChar() {
	if (IsEOF()) {
		return 0;
	}
	return *(scanner.Current + 1);
}

void Compiler::SkipWhitespace() {
	while (true) {
		char c = PeekChar();
		switch (c) {
		case ' ':
		case '\r':
		case '\t':
			AdvanceChar(); // char in 'c'
			break;

		case '\n':
			scanner.Line++;
			AdvanceChar(); // char in 'c'
			scanner.LinePos = scanner.Current;
			break;

		case '/':
			if (PeekNextChar() == '/') {
				AdvanceChar(); // char in 'c'
				AdvanceChar(); // '/'
				while (PeekChar() != '\n' && !IsEOF()) {
					AdvanceChar();
				}
			}
			else if (PeekNextChar() == '*') {
				AdvanceChar(); // char in 'c'
				AdvanceChar(); // '*'
				do {
					if (PeekChar() == '\n') {
						scanner.Line++;
						AdvanceChar();
						scanner.LinePos = scanner.Current;
					}
					else if (PeekChar() == '*') {
						AdvanceChar(); // '*'
						if (PeekChar() == '/') {
							break;
						}
					}
					else {
						AdvanceChar();
					}
				} while (!IsEOF());

				if (!IsEOF()) {
					AdvanceChar();
				}
			}
			else {
				return;
			}
			break;

		default:
			return;
		}
	}
}

// Token functions
int Compiler::CheckKeyword(int start, int length, const char* rest, int type) {
	if (scanner.Current - scanner.Start == start + length &&
		(!rest || memcmp(scanner.Start + start, rest, length) == 0)) {
		return type;
	}

	return TOKEN_IDENTIFIER;
}
int Compiler::GetKeywordType() {
	switch (*scanner.Start) {
	case 'a':
		if (scanner.Current - scanner.Start > 1) {
			switch (*(scanner.Start + 1)) {
			case 'n':
				return CheckKeyword(2, 1, "d", TOKEN_AND);
			case 's':
				return CheckKeyword(2, 0, NULL, TOKEN_AS);
			}
		}
		break;
	case 'b':
		if (scanner.Current - scanner.Start > 8) {
			return CheckKeyword(1, 9, "reakpoint", TOKEN_BREAKPOINT);
		}
		else {
			return CheckKeyword(1, 4, "reak", TOKEN_BREAK);
		}
		break;
	case 'c':
		if (scanner.Current - scanner.Start > 1) {
			switch (*(scanner.Start + 1)) {
			case 'a':
				return CheckKeyword(2, 2, "se", TOKEN_CASE);
			case 'l':
				return CheckKeyword(2, 3, "ass", TOKEN_CLASS);
			case 'o': {
				if (scanner.Current - scanner.Start > 2) {
					switch (*(scanner.Start + 2)) {
					case 'n':
						if (scanner.Current - scanner.Start > 3) {
							switch (*(scanner.Start + 3)) {
							case 't':
								return CheckKeyword(4,
									4,
									"inue",
									TOKEN_CONTINUE);
							case 's':
								return CheckKeyword(
									4, 1, "t", TOKEN_CONST);
							}
						}
					}
				}
			}
			}
		}
		break;
	case 'd':
		if (scanner.Current - scanner.Start > 1) {
			switch (*(scanner.Start + 1)) {
			case 'e':
				return CheckKeyword(2, 5, "fault", TOKEN_DEFAULT);
			case 'o':
				return CheckKeyword(2, 0, NULL, TOKEN_DO);
			}
		}
		break;
	case 'e':
		if (scanner.Current - scanner.Start > 1) {
			switch (*(scanner.Start + 1)) {
			case 'l':
				return CheckKeyword(2, 2, "se", TOKEN_ELSE);
			case 'n':
				return CheckKeyword(2, 2, "um", TOKEN_ENUM);
			case 'v':
				return CheckKeyword(2, 3, "ent", TOKEN_EVENT);
			}
		}
		break;
	case 'f':
		if (scanner.Current - scanner.Start > 1) {
			switch (*(scanner.Start + 1)) {
			case 'a':
				return CheckKeyword(2, 3, "lse", TOKEN_FALSE);
			case 'o':
				if (scanner.Current - scanner.Start > 2) {
					switch (*(scanner.Start + 2)) {
					case 'r':
						if (scanner.Current - scanner.Start > 3) {
							switch (*(scanner.Start + 3)) {
							case 'e':
								return CheckKeyword(
									4, 3, "ach", TOKEN_FOREACH);
							}
						}
						return CheckKeyword(3, 0, NULL, TOKEN_FOR);
					}
				}
				break;
			case 'r':
				return CheckKeyword(2, 2, "om", TOKEN_FROM);
			}
		}
		break;
	case 'h':
		if (scanner.Current - scanner.Start > 1) {
			switch (*(scanner.Start + 1)) {
			case 'a':
				return CheckKeyword(2, 1, "s", TOKEN_HAS);
			case 'i':
				return CheckKeyword(2, 4, "tbox", TOKEN_HITBOX);
			}
		}
		break;
	case 'i':
		if (scanner.Current - scanner.Start > 1) {
			switch (*(scanner.Start + 1)) {
			case 'f':
				return CheckKeyword(2, 0, NULL, TOKEN_IF);
			case 'n':
				return CheckKeyword(2, 0, NULL, TOKEN_IN);
			case 'm':
				return CheckKeyword(2, 4, "port", TOKEN_IMPORT);
			}
		}
		break;
	case 'l':
		return CheckKeyword(1, 4, "ocal", TOKEN_LOCAL);
	case 'n':
		if (scanner.Current - scanner.Start > 1) {
			switch (*(scanner.Start + 1)) {
			case 'a':
				return CheckKeyword(2, 7, "mespace", TOKEN_NAMESPACE);
			case 'u':
				return CheckKeyword(2, 2, "ll", TOKEN_NULL);
			case 'e':
				return CheckKeyword(2, 1, "w", TOKEN_NEW);
			}
		}
		break;
	case 'o':
		if (scanner.Current - scanner.Start > 1) {
			switch (*(scanner.Start + 1)) {
			case 'r':
				return CheckKeyword(2, 0, NULL, TOKEN_OR);
			}
		}
		break;
	case 'p':
		if (scanner.Current - scanner.Start > 1) {
			switch (*(scanner.Start + 1)) {
			case 'r':
				return CheckKeyword(2, 3, "int", TOKEN_PRINT);
			}
		}
		break;
	case 'r':
		if (scanner.Current - scanner.Start > 1) {
			switch (*(scanner.Start + 1)) {
			case 'e':
				if (scanner.Current - scanner.Start > 2) {
					switch (*(scanner.Start + 2)) {
					case 't':
						return CheckKeyword(3, 3, "urn", TOKEN_RETURN);
					case 'p':
						return CheckKeyword(3, 3, "eat", TOKEN_REPEAT);
					}
				}
				break;
			}
		}
		break;
	case 's':
		if (scanner.Current - scanner.Start > 1) {
			switch (*(scanner.Start + 1)) {
			case 't':
				return CheckKeyword(2, 4, "atic", TOKEN_STATIC);
			case 'u':
				if (scanner.Current - scanner.Start > 2) {
					switch (*(scanner.Start + 2)) {
					case 'p':
						return CheckKeyword(3, 2, "er", TOKEN_SUPER);
					}
				}
				break;
			case 'w':
				return CheckKeyword(2, 4, "itch", TOKEN_SWITCH);
			}
		}
		break;
	case 't':
		if (scanner.Current - scanner.Start > 1) {
			switch (*(scanner.Start + 1)) {
			case 'h':
				return CheckKeyword(2, 2, "is", TOKEN_THIS);
			case 'r':
				return CheckKeyword(2, 2, "ue", TOKEN_TRUE);
			case 'y':
				return CheckKeyword(2, 4, "peof", TOKEN_TYPEOF);
			}
		}
		break;
	case 'u':
		return CheckKeyword(1, 4, "sing", TOKEN_USING);
	case 'v':
		return CheckKeyword(1, 2, "ar", TOKEN_VAR);
	case 'w':
		if (scanner.Current - scanner.Start > 1) {
			switch (*(scanner.Start + 1)) {
			case 'i':
				return CheckKeyword(2, 2, "th", TOKEN_WITH);
			case 'h':
				return CheckKeyword(2, 3, "ile", TOKEN_WHILE);
			}
		}
		break;
	}

	return TOKEN_IDENTIFIER;
}

Token Compiler::StringToken() {
	while (PeekChar() != '"' && !IsEOF()) {
		bool lineBreak = false;
		switch (PeekChar()) {
			// Line Break
		case '\n':
			lineBreak = true;
			break;
			// Escaped
		case '\\':
			AdvanceChar();
			break;
		}

		AdvanceChar();

		if (lineBreak) {
			scanner.Line++;
			scanner.LinePos = scanner.Current;
		}
	}

	if (IsEOF()) {
		return ErrorToken("Unterminated string.");
	}

	// The closing double-quote.
	AdvanceChar();
	return MakeToken(TOKEN_STRING);
}
Token Compiler::NumberToken() {
	if (*scanner.Start == '0' && (PeekChar() == 'x' || PeekChar() == 'X')) {
		AdvanceChar(); // x
		while (IsHexDigit(PeekChar())) {
			AdvanceChar();
		}
		return MakeToken(TOKEN_NUMBER);
	}

	while (IsDigit(PeekChar())) {
		AdvanceChar();
	}

	// Look for a fractional part.
	if (PeekChar() == '.' && IsDigit(PeekNextChar())) {
		// Consume the "."
		AdvanceChar();

		while (IsDigit(PeekChar())) {
			AdvanceChar();
		}

		return MakeToken(TOKEN_DECIMAL);
	}

	return MakeToken(TOKEN_NUMBER);
}
Token Compiler::IdentifierToken() {
	while (IsIdentifierBody(PeekChar())) {
		AdvanceChar();
	}

	return MakeToken(GetKeywordType());
}
Token Compiler::ScanToken() {
	SkipWhitespace();

	scanner.Start = scanner.Current;

	if (IsEOF()) {
		return MakeToken(TOKEN_EOF);
	}

	char c = AdvanceChar();

	if (IsDigit(c)) {
		return NumberToken();
	}
	if (IsIdentifierStart(c)) {
		return IdentifierToken();
	}

	switch (c) {
	case '(':
		return MakeToken(TOKEN_LEFT_PAREN);
	case ')':
		return MakeToken(TOKEN_RIGHT_PAREN);
	case '{':
		return MakeToken(TOKEN_LEFT_BRACE);
	case '}':
		return MakeToken(TOKEN_RIGHT_BRACE);
	case '[':
		return MakeToken(TOKEN_LEFT_SQUARE_BRACE);
	case ']':
		return MakeToken(TOKEN_RIGHT_SQUARE_BRACE);
	case ';':
		return MakeToken(TOKEN_SEMICOLON);
	case ',':
		return MakeToken(TOKEN_COMMA);
	case '.':
		return MakeToken(TOKEN_DOT);
	case ':':
		return MakeToken(TOKEN_COLON);
	case '?':
		return MakeToken(TOKEN_TERNARY);
	case '~':
		return MakeToken(TOKEN_BITWISE_NOT);
		// Two-char punctuations
	case '!':
		return MakeToken(MatchChar('=') ? TOKEN_NOT_EQUALS : TOKEN_LOGICAL_NOT);
	case '=':
		return MakeToken(MatchChar('=') ? TOKEN_EQUALS : TOKEN_ASSIGNMENT);

	case '*':
		return MakeToken(MatchChar('=') ? TOKEN_ASSIGNMENT_MULTIPLY : TOKEN_MULTIPLY);
	case '/':
		return MakeToken(MatchChar('=') ? TOKEN_ASSIGNMENT_DIVISION : TOKEN_DIVISION);
	case '%':
		return MakeToken(MatchChar('=') ? TOKEN_ASSIGNMENT_MODULO : TOKEN_MODULO);
	case '+':
		return MakeToken(MatchChar('=')  ? TOKEN_ASSIGNMENT_PLUS
				: MatchChar('+') ? TOKEN_INCREMENT
						 : TOKEN_PLUS);
	case '-':
		return MakeToken(MatchChar('=')  ? TOKEN_ASSIGNMENT_MINUS
				: MatchChar('-') ? TOKEN_DECREMENT
						 : TOKEN_MINUS);
	case '<':
		return MakeToken(MatchChar('<')  ? MatchChar('=') ? TOKEN_ASSIGNMENT_BITWISE_LEFT
								  : TOKEN_BITWISE_LEFT
				: MatchChar('=') ? TOKEN_LESS_EQUAL
						 : TOKEN_LESS);
	case '>':
		return MakeToken(MatchChar('>')  ? MatchChar('=') ? TOKEN_ASSIGNMENT_BITWISE_RIGHT
								  : TOKEN_BITWISE_RIGHT
				: MatchChar('=') ? TOKEN_GREATER_EQUAL
						 : TOKEN_GREATER);
	case '&':
		return MakeToken(MatchChar('=')  ? TOKEN_ASSIGNMENT_BITWISE_AND
				: MatchChar('&') ? TOKEN_LOGICAL_AND
						 : TOKEN_BITWISE_AND);
	case '|':
		return MakeToken(MatchChar('=')  ? TOKEN_ASSIGNMENT_BITWISE_OR
				: MatchChar('|') ? TOKEN_LOGICAL_OR
						 : TOKEN_BITWISE_OR);
	case '^':
		return MakeToken(MatchChar('=') ? TOKEN_ASSIGNMENT_BITWISE_XOR : TOKEN_BITWISE_XOR);
		// String
	case '"':
		return StringToken();
	}

	return ErrorToken("Unexpected character.");
}

void Compiler::AdvanceToken() {
	parser.Previous = parser.Current;

	while (true) {
		parser.Current = ScanToken();
		if (parser.Current.Type != TOKEN_ERROR) {
			break;
		}

		ErrorAtCurrent(parser.Current.Start);
	}
}
Token Compiler::NextToken() {
	AdvanceToken();
	return parser.Previous;
}
Token Compiler::PeekToken() {
	return parser.Current;
}
Token Compiler::PeekNextToken() {
	Parser previousParser = parser;
	Scanner previousScanner = scanner;
	Token next;

	AdvanceToken();
	next = parser.Current;

	parser = previousParser;
	scanner = previousScanner;

	return next;
}
Token Compiler::PrevToken() {
	return parser.Previous;
}
bool Compiler::MatchToken(int expectedType) {
	if (!CheckToken(expectedType)) {
		return false;
	}
	AdvanceToken();
	return true;
}
bool Compiler::MatchAssignmentToken() {
	switch (parser.Current.Type) {
	case TOKEN_ASSIGNMENT:
	case TOKEN_ASSIGNMENT_MULTIPLY:
	case TOKEN_ASSIGNMENT_DIVISION:
	case TOKEN_ASSIGNMENT_MODULO:
	case TOKEN_ASSIGNMENT_PLUS:
	case TOKEN_ASSIGNMENT_MINUS:
	case TOKEN_ASSIGNMENT_BITWISE_LEFT:
	case TOKEN_ASSIGNMENT_BITWISE_RIGHT:
	case TOKEN_ASSIGNMENT_BITWISE_AND:
	case TOKEN_ASSIGNMENT_BITWISE_XOR:
	case TOKEN_ASSIGNMENT_BITWISE_OR:
		AdvanceToken();
		return true;

	case TOKEN_INCREMENT:
	case TOKEN_DECREMENT:
		AdvanceToken();
		return true;

	default:
		break;
	}
	return false;
}
bool Compiler::CheckToken(int expectedType) {
	return parser.Current.Type == expectedType;
}
void Compiler::ConsumeToken(int type, const char* message) {
	if (parser.Current.Type == type) {
		AdvanceToken();
		return;
	}

	ErrorAtCurrent(message);
}
void Compiler::ConsumeIdentifier(const char* message) {
	// Contextual keywords
	switch (parser.Current.Type) {
	case TOKEN_HITBOX:
	case TOKEN_BREAKPOINT:
		AdvanceToken();
		return;
	}

	ConsumeToken(TOKEN_IDENTIFIER, message);
}

// Error handling
bool Compiler::ReportError(int line, int pos, bool fatal, const char* string, ...) {
	if (!fatal && !CurrentSettings.ShowWarnings) {
		return true;
	}

	char message[4096];
	memset(message, 0, sizeof message);

	va_list args;
	va_start(args, string);
	vsnprintf(message, sizeof message, string, args);
	va_end(args);

	char* textBuffer = (char*)malloc(512);

	PrintBuffer buffer;
	buffer.Buffer = &textBuffer;
	buffer.WriteIndex = 0;
	buffer.BufferSize = 512;

	if (scanner.SourceFilename) {
		buffer_printf(&buffer, "In file '%s' on ", scanner.SourceFilename);
	}
	else {
		buffer_printf(&buffer, "On ");
	}

	buffer_printf(&buffer,
		"line %d, position %d:\n    %s\n\n",
		line,
		pos,
		message);

	if (!fatal) {
		if (CurrentSettings.PrintToLog) {
			Log::Print(Log::LOG_WARN, textBuffer);
		}
		else {
			printf("%s\n", textBuffer);
		}
		free(textBuffer);
		return true;
	}

	std::string error = std::string(textBuffer);

	free(textBuffer);

	throw CompilerErrorException(error);
}
void Compiler::ErrorAt(Token* token, const char* message, bool fatal) {
	if (token->Type == TOKEN_EOF) {
		ReportError(token->Line, (int)token->Pos, fatal, "At end of file: %s", message);
	}
	else if (token->Type == TOKEN_ERROR) {
		ReportError(token->Line, (int)token->Pos, fatal, "%s", message);
	}
	else {
		ReportError(token->Line,
			(int)token->Pos,
			fatal,
			"At '%.*s': %s",
			token->Length,
			token->Start,
			message);
	}
}
void Compiler::Error(const char* message) {
	ErrorAt(&parser.Previous, message, true);
}
void Compiler::ErrorAtCurrent(const char* message) {
	ErrorAt(&parser.Current, message, true);
}
void Compiler::Warning(const char* message) {
	ErrorAt(&parser.Current, message, false);
}
void Compiler::WarningInFunction(const char* format, ...) {
	if (!CurrentSettings.ShowWarnings) {
		return;
	}

	char message[4096];
	memset(message, 0, sizeof message);

	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof message, format, args);
	va_end(args);

	char* textBuffer = (char*)malloc(512);

	PrintBuffer buffer;
	buffer.Buffer = &textBuffer;
	buffer.WriteIndex = 0;
	buffer.BufferSize = 512;

	if (strcmp(Function->Name, "main") == 0) {
		buffer_printf(&buffer, "In top level code");
	}
	else if (ClassName.size() > 0) {
		buffer_printf(&buffer, "In method '%s::%s'", ClassName.c_str(), Function->Name);
	}
	else {
		buffer_printf(&buffer, "In function '%s'", Function->Name);
	}

	if (scanner.SourceFilename) {
		buffer_printf(&buffer, " of file '%s'", scanner.SourceFilename);
	}

	buffer_printf(&buffer, ":\n    %s\n",  message);

	if (CurrentSettings.PrintToLog) {
		Log::Print(Log::LOG_WARN, textBuffer);
	}
	else {
		printf("%s\n", textBuffer);
	}

	free(textBuffer);
}

int Compiler::ParseVariable(const char* errorMessage, bool constant) {
	ConsumeIdentifier(errorMessage);
	return DeclareVariable(&parser.Previous, constant);
}
bool Compiler::IdentifiersEqual(Token* a, Token* b) {
	if (a->Length != b->Length) {
		return false;
	}
	return memcmp(a->Start, b->Start, a->Length) == 0;
}
void Compiler::MarkInitialized() {
	if (ScopeDepth == 0) {
		return;
	}
	Locals[LocalCount - 1].Depth = ScopeDepth;
}
void Compiler::DefineVariableToken(Token global, bool constant) {
	if (ScopeDepth > 0) {
		if (!constant) {
			MarkInitialized();
		}
		return;
	}
	EmitByte(constant ? OP_DEFINE_CONSTANT : OP_DEFINE_GLOBAL);
	EmitStringHash(global);
}
int Compiler::DeclareVariable(Token* name, bool constant) {
	if (ScopeDepth == 0) {
		return -1;
	}

	for (int i = LocalCount - 1; i >= 0; i--) {
		Local* local = &Locals[i];

		if (local->Depth != -1 && local->Depth < ScopeDepth) {
			break;
		}

		if (IdentifiersEqual(name, &local->Name)) {
			Error("Variable with this name already declared in this scope.");
		}
	}

	for (int i = Constants.size() - 1; i >= 0; i--) {
		Local* local = &Constants[i];

		if (local->Depth < ScopeDepth) {
			break;
		}

		if (IdentifiersEqual(name, &local->Name)) {
			Error("Constant with this name already declared in this scope.");
		}
	}

	if (!constant) {
		return AddLocal(*name);
	}
	Constants.push_back({*name, ScopeDepth, 0, false, false, true});
	return ((int)Constants.size()) - 1;
}
int Compiler::ParseModuleVariable(const char* errorMessage, bool constant) {
	ConsumeIdentifier(errorMessage);
	return DeclareModuleVariable(&parser.Previous, constant);
}
void Compiler::DefineModuleVariable(int local) {
	EmitByte(OP_DEFINE_MODULE_LOCAL);
	Compiler::ModuleLocals[local].Depth = 0;
}
int Compiler::DeclareModuleVariable(Token* name, bool constant) {
	for (int i = Compiler::ModuleLocals.size() - 1; i >= 0; i--) {
		Local& local = Compiler::ModuleLocals[i];

		if (IdentifiersEqual(name, &local.Name)) {
			Error("Local with this name already declared in this module.");
		}
	}

	for (int i = Compiler::ModuleConstants.size() - 1; i >= 0; i--) {
		Local& local = Compiler::ModuleConstants[i];

		if (IdentifiersEqual(name, &local.Name)) {
			Error("Local with this name already declared in this module.");
		}
	}

	if (!constant) {
		return AddModuleLocal(*name);
	}

	Compiler::ModuleConstants.push_back({*name, 0, 0, false, false, true});
	return ((int)Compiler::ModuleConstants.size()) - 1;
}
void Compiler::WarnVariablesUnusedUnset() {
	if (!CurrentSettings.ShowWarnings) {
		return;
	}

	size_t numUnused = UnusedVariables->size();
	std::string message;
	char temp[4096];
	if (numUnused) {
		for (int i = numUnused - 1; i >= 0; i--) {
			Local& local = (*UnusedVariables)[i];
			snprintf(temp,
				sizeof(temp),
				"Variable '%.*s' is unused. (Declared on line %d)",
				(int)local.Name.Length,
				local.Name.Start,
				local.Name.Line);
			message += std::string(temp);
			if (i != 0) {
				message += "\n    ";
			}
		}
	}

	size_t numUnset = UnsetVariables->size();

	if (numUnset) {
		for (int i = numUnset - 1; i >= 0; i--) {
			Local& local = (*UnsetVariables)[i];
			snprintf(temp,
				sizeof(temp),
				"Variable '%.*s' can be const. (Declared on line %d)",
				(int)local.Name.Length,
				local.Name.Start,
				local.Name.Line);
			message += std::string(temp);
			if (i != 0 || numUnused) {
				message += "\n    ";
			}
		}
	}

	if (numUnset + numUnused != 0) {
		WarningInFunction("%s", message.c_str());
	}
}

void Compiler::EmitSetOperation(Uint8 setOp, int arg, Token name) {
	switch (setOp) {
	case OP_SET_GLOBAL:
	case OP_SET_PROPERTY:
		EmitByte(setOp);
		EmitStringHash(name);
		break;
	case OP_SET_LOCAL:
		EmitBytes(setOp, (Uint8)arg);
		break;
	case OP_SET_ELEMENT:
		EmitByte(setOp);
		break;
	case OP_SET_MODULE_LOCAL:
		EmitByte(setOp);
		EmitUint16((Uint16)arg);
		break;
	default:
		break;
	}
}
void Compiler::EmitGetOperation(Uint8 getOp, int arg, Token name) {
	switch (getOp) {
	case OP_GET_GLOBAL:
	case OP_GET_PROPERTY:
		EmitByte(getOp);
		EmitStringHash(name);
		break;
	case OP_GET_LOCAL:
		EmitBytes(getOp, (Uint8)arg);
		break;
	case OP_GET_ELEMENT:
		EmitByte(getOp);
		break;
	case OP_GET_MODULE_LOCAL:
		EmitByte(getOp);
		EmitUint16((Uint16)arg);
	default:
		break;
	}
}
void Compiler::EmitAssignmentToken(Token assignmentToken) {
	switch (assignmentToken.Type) {
	case TOKEN_ASSIGNMENT_PLUS:
		EmitByte(OP_ADD);
		break;
	case TOKEN_ASSIGNMENT_MINUS:
		EmitByte(OP_SUBTRACT);
		break;
	case TOKEN_ASSIGNMENT_MODULO:
		EmitByte(OP_MODULO);
		break;
	case TOKEN_ASSIGNMENT_DIVISION:
		EmitByte(OP_DIVIDE);
		break;
	case TOKEN_ASSIGNMENT_MULTIPLY:
		EmitByte(OP_MULTIPLY);
		break;
	case TOKEN_ASSIGNMENT_BITWISE_OR:
		EmitByte(OP_BW_OR);
		break;
	case TOKEN_ASSIGNMENT_BITWISE_AND:
		EmitByte(OP_BW_AND);
		break;
	case TOKEN_ASSIGNMENT_BITWISE_XOR:
		EmitByte(OP_BW_XOR);
		break;
	case TOKEN_ASSIGNMENT_BITWISE_LEFT:
		EmitByte(OP_BITSHIFT_LEFT);
		break;
	case TOKEN_ASSIGNMENT_BITWISE_RIGHT:
		EmitByte(OP_BITSHIFT_RIGHT);
		break;
	case TOKEN_INCREMENT:
		EmitByte(OP_INCREMENT);
		break;
	case TOKEN_DECREMENT:
		EmitByte(OP_DECREMENT);
		break;
	default:
		break;
	}
}
void Compiler::EmitCopy(Uint8 count) {
	EmitByte(OP_COPY);
	EmitByte(count);
}

void Compiler::EmitCall(const char* name, int argCount, bool isSuper) {
	EmitCallOpcode(argCount, isSuper);
	EmitStringHash(name);
}
void Compiler::EmitCall(Token name, int argCount, bool isSuper) {
	EmitCallOpcode(argCount, isSuper);
	EmitStringHash(name);
}
void Compiler::EmitCallOpcode(int argCount, bool isSuper) {
	EmitByte(isSuper ? OP_SUPER_INVOKE : OP_INVOKE);
	EmitByte(argCount);
}

bool Compiler::ResolveNamedVariable(Token name,
	Uint8& getOp,
	Uint8& setOp,
	Local& local,
	int& arg) {
	local.Constant = false;

	arg = ResolveLocal(&name, &local);

	// Determine whether local or global
	if (arg != -1) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	}
	else {
		arg = ResolveModuleLocal(&name, &local);
		VMValue value;
		if (arg != -1) {
			getOp = OP_GET_MODULE_LOCAL;
			setOp = OP_SET_MODULE_LOCAL;
		}
		else if (StandardConstants->GetIfExists(name.ToString().c_str(), &value)) {
			EmitConstant(value);
			return true;
		}
		else {
			getOp = OP_GET_GLOBAL;
			setOp = OP_SET_GLOBAL;
		}
	}

	return false;
}
void Compiler::DoVariableAssignment(Token name,
	Local local,
	Uint8 getOp,
	Uint8 setOp,
	int arg,
	Token assignmentToken,
	bool isPrefix) {
	if (local.Constant) {
		ErrorAt(&name, "Attempted to assign to constant!", true);
	}
	else if (getOp == OP_GET_LOCAL) {
		Locals[arg].WasSet = true;
	}
	else if (getOp == OP_GET_MODULE_LOCAL) {
		ModuleLocals[arg].WasSet = true;
	}

	if (assignmentToken.Type == TOKEN_INCREMENT || assignmentToken.Type == TOKEN_DECREMENT) {
		EmitGetOperation(getOp, arg, name);

		// If postfix, we push the value BEFORE the increment/decrement happens
		// If prefix, we leave the incremented/decremented value on the stack
		if (!isPrefix) {
			EmitCopy(1);
			EmitByte(OP_SAVE_VALUE); // Save value. (value)
		}

		EmitAssignmentToken(assignmentToken);
		EmitSetOperation(setOp, arg, name);

		if (!isPrefix) {
			EmitByte(OP_POP);
			EmitByte(OP_LOAD_VALUE); // Load value. (value)
		}
	}
	else {
		if (assignmentToken.Type != TOKEN_ASSIGNMENT) {
			EmitGetOperation(getOp, arg, name);
		}

		GetExpression();

		EmitAssignmentToken(assignmentToken);
		EmitSetOperation(setOp, arg, name);
	}
}
void Compiler::NamedVariable(Token name, bool canAssign) {
	Uint8 getOp, setOp;
	Local local;
	int arg;

	if (ResolveNamedVariable(name, getOp, setOp, local, arg)) {
		return;
	}

	if (canAssign && MatchAssignmentToken()) {
		Token assignmentToken = parser.Previous;
		DoVariableAssignment(name, local, getOp, setOp, arg, assignmentToken, false);
	}
	else if (local.Constant && local.ConstantVal.Type != VAL_ERROR) {
		EmitConstant(local.ConstantVal);
	}
	else {
		EmitGetOperation(getOp, arg, name);
	}
}
void Compiler::ScopeBegin() {
	ScopeDepth++;
}
void Compiler::ScopeEnd() {
	ScopeDepth--;
	ClearToScope(ScopeDepth);
}
void Compiler::ClearToScope(int depth) {
	int popCount = 0;
	while (LocalCount > 0 && Locals[LocalCount - 1].Depth > depth) {
		if (!Locals[LocalCount - 1].Resolved) {
			UnusedVariables->push_back(Locals[LocalCount - 1]);
		}
		else if (Locals[LocalCount - 1].ConstantVal.Type != VAL_ERROR &&
			!Locals[LocalCount - 1].WasSet) {
			UnsetVariables->push_back(Locals[LocalCount - 1]);
		}

		popCount++; // pop locals

		LocalCount--;
	}
	while (Constants.size() > 0 && Constants.back().Depth > depth) {
		Constants.pop_back();
	}

	PopMultiple(popCount);
}
void Compiler::PopToScope(int depth) {
	int lcl = LocalCount;
	int popCount = 0;
	while (lcl > 0 && Locals[lcl - 1].Depth > depth) {
		popCount++; // pop locals
		lcl--;
	}
	PopMultiple(popCount);
}
void Compiler::PopMultiple(int count) {
	if (count == 1) {
		EmitByte(OP_POP);
		return;
	}

	while (count > 0) {
		int max = count;
		if (max > 0xFF) {
			max = 0xFF;
		}
		EmitBytes(OP_POPN, max);
		count -= max;
	}
}
int Compiler::AddLocal() {
	if (LocalCount == 0xFF) {
		Error("Too many local variables in function.");
		return -1;
	}
	Local* local = &Locals[LocalCount++];
	local->Depth = -1;
	local->Index = LocalCount - 1;
	local->Resolved = false;
	local->Constant = false;
	local->ConstantVal = VMValue{VAL_ERROR};
	return local->Index;
}
int Compiler::AddLocal(Token name) {
	int local = AddLocal();

	Locals[local].Name = name;

	AllLocals.push_back(Locals[local]);

	return local;
}
int Compiler::AddLocal(const char* name, size_t len) {
	int local = AddLocal();

	RenameLocal(&Locals[local], name, len);

	AllLocals.push_back(Locals[local]);

	return local;
}
int Compiler::AddHiddenLocal(const char* name, size_t len) {
	int local = AddLocal();

	RenameLocal(&Locals[local], name, len);

	Locals[local].Resolved = true;

	MarkInitialized();

	AllLocals.push_back(Locals[local]);

	return local;
}
void Compiler::RenameLocal(Local* local, const char* name, size_t len) {
	local->Name.Start = (char*)name;
	local->Name.Length = len;
	local->Name.Line = 0;
	local->Name.Pos = 0;
}
void Compiler::RenameLocal(Local* local, const char* name) {
	local->Name.Start = (char*)name;
	local->Name.Length = strlen(name);
	local->Name.Line = 0;
	local->Name.Pos = 0;
}
void Compiler::RenameLocal(Local* local, Token name) {
	local->Name = name;
}
int Compiler::ResolveLocal(Token* name, Local* result) {
	for (int i = LocalCount - 1; i >= 0; i--) {
		Local* local = &Locals[i];
		if (IdentifiersEqual(name, &local->Name)) {
			if (local->Depth == -1) {
				Error("Cannot read local variable in its own initializer.");
			}
			local->Resolved = true;
			if (result) {
				*result = *local;
			}
			return i;
		}
	}

	for (int i = Constants.size() - 1; i >= 0; i--) {
		Local* local = &Constants[i];
		if (IdentifiersEqual(name, &local->Name)) {
			local->Resolved = true;
			if (result) {
				*result = *local;
			}
			return i;
		}
	}

	return -1;
}
int Compiler::AddModuleLocal(Token name) {
	if (Compiler::ModuleLocals.size() == 0xFFFF) {
		Error("Too many locals in module.");
		return -1;
	}
	Local local;
	local.Name = name;
	local.Depth = -1;
	local.Index = (int)Compiler::ModuleLocals.size();
	local.Resolved = false;
	local.Constant = false;
	local.ConstantVal = VMValue{VAL_ERROR};
	Compiler::ModuleLocals.push_back(local);
	return local.Index;
}
int Compiler::ResolveModuleLocal(Token* name, Local* result) {
	for (int i = Compiler::ModuleLocals.size() - 1; i >= 0; i--) {
		Local& local = Compiler::ModuleLocals[i];
		if (IdentifiersEqual(name, &local.Name)) {
			if (local.Depth == -1) {
				Error("Cannot read local variable in its own initializer.");
			}
			local.Resolved = true;
			if (result) {
				*result = local;
			}
			return i;
		}
	}

	for (int i = Compiler::ModuleConstants.size() - 1; i >= 0; i--) {
		Local& local = Compiler::ModuleConstants[i];
		if (IdentifiersEqual(name, &local.Name)) {
			local.Resolved = true;
			if (result) {
				*result = local;
			}
			return i;
		}
	}

	return -1;
}
Uint8 Compiler::GetArgumentList() {
	Uint8 argumentCount = 0;
	if (!CheckToken(TOKEN_RIGHT_PAREN)) {
		do {
			GetExpression();
			if (argumentCount >= 255) {
				Error("Cannot have more than 255 arguments.");
			}
			argumentCount++;
		} while (MatchToken(TOKEN_COMMA));
	}

	ConsumeToken(TOKEN_RIGHT_PAREN, "Expected \")\" after arguments.");
	return argumentCount;
}

Token InstanceToken = Token{0, NULL, 0, 0, 0};
void Compiler::GetThis(bool canAssign) {
	InstanceToken = parser.Previous;
	NamedVariable(parser.Previous, false);
}
void Compiler::GetSuper(bool canAssign) {
	InstanceToken = parser.Previous;
	if (!CheckToken(TOKEN_DOT)) {
		Error("Expect '.' after 'super'.");
	}
	EmitBytes(OP_GET_LOCAL, 0);
}
void Compiler::GetDot(bool canAssign) {
	bool isSuper = InstanceToken.Type == TOKEN_SUPER;
	InstanceToken.Type = -1;

	ConsumeIdentifier("Expect property name after '.'.");
	Token nameToken = parser.Previous;

	if (canAssign && MatchAssignmentToken()) {
		if (isSuper) {
			EmitByte(OP_GET_SUPERCLASS);
		}

		Token assignmentToken = parser.Previous;
		if (assignmentToken.Type == TOKEN_INCREMENT ||
			assignmentToken.Type == TOKEN_DECREMENT) {
			// (this)
			EmitCopy(1); // Copy property holder. (this,
			// this)
			EmitGetOperation(OP_GET_PROPERTY, -1,
				nameToken); // Pops a property holder.
			// (value, this)

			EmitCopy(1); // Copy value. (value, value,
			// this)
			EmitByte(OP_SAVE_VALUE); // Save value.
			// (value, this)
			EmitAssignmentToken(assignmentToken); // OP_DECREMENT
			// (value - 1, this)

			EmitSetOperation(OP_SET_PROPERTY, -1, nameToken);
			// Pops the value and then pops the instance,
			// pushes the value (value - 1)

			EmitByte(OP_POP); // ()
			EmitByte(OP_LOAD_VALUE); // Load value. (value)
		}
		else {
			if (assignmentToken.Type != TOKEN_ASSIGNMENT) {
				EmitCopy(1);
				EmitGetOperation(OP_GET_PROPERTY, -1, nameToken);
			}

			GetExpression();

			EmitAssignmentToken(assignmentToken);
			EmitSetOperation(OP_SET_PROPERTY, -1, nameToken);
		}
	}
	else if (MatchToken(TOKEN_LEFT_PAREN)) {
		uint8_t argCount = GetArgumentList();

		EmitCall(nameToken, argCount, isSuper);
	}
	else {
		if (isSuper) {
			EmitByte(OP_GET_SUPERCLASS);
		}

		EmitGetOperation(OP_GET_PROPERTY, -1, nameToken);
	}
}
void Compiler::GetElement(bool canAssign) {
	Token blank;
	memset(&blank, 0, sizeof(blank));
	GetExpression();
	ConsumeToken(TOKEN_RIGHT_SQUARE_BRACE, "Expected matching ']'.");

	if (canAssign && MatchAssignmentToken()) {
		Token assignmentToken = parser.Previous;
		if (assignmentToken.Type == TOKEN_INCREMENT ||
			assignmentToken.Type == TOKEN_DECREMENT) {
			// (index, array)
			EmitCopy(2); // Copy array & index.
			EmitGetOperation(OP_GET_ELEMENT, -1,
				blank); // Pops a array and index.
			// (value)

			EmitCopy(1); // Copy value. (value, value,
			// index)
			EmitByte(OP_SAVE_VALUE); // Save value.
			// (value, index)
			EmitAssignmentToken(assignmentToken); // OP_DECREMENT
			// (value - 1,
			// index)

			EmitSetOperation(OP_SET_ELEMENT, -1, blank);
			// Pops the value and then pops the instance,
			// pushes the value (value - 1)

			EmitByte(OP_POP); // ()
			EmitByte(OP_LOAD_VALUE); // Load value. (value)
		}
		else {
			if (assignmentToken.Type != TOKEN_ASSIGNMENT) {
				EmitCopy(2);
				EmitGetOperation(OP_GET_ELEMENT, -1, blank);
			}

			// Get right-hand side
			GetExpression();

			EmitAssignmentToken(assignmentToken);
			EmitSetOperation(OP_SET_ELEMENT, -1, blank);
		}
	}
	else {
		EmitGetOperation(OP_GET_ELEMENT, -1, blank);
	}
}

// Reading expressions
bool negateConstant = false;
void Compiler::GetGrouping(bool canAssign) {
	GetExpression();
	ConsumeToken(TOKEN_RIGHT_PAREN, "Expected \")\" after expression.");
}
void Compiler::GetLiteral(bool canAssign) {
	switch (parser.Previous.Type) {
	case TOKEN_NULL:
		EmitByte(OP_NULL);
		break;
	case TOKEN_TRUE:
		EmitByte(OP_TRUE);
		break;
	case TOKEN_FALSE:
		EmitByte(OP_FALSE);
		break;
	default:
		return; // Unreachable.
	}
}
void Compiler::GetInteger(bool canAssign) {
	int value = 0;
	char* start = parser.Previous.Start;
	if (start[0] == '0' && (start[1] == 'x' || start[1] == 'X')) {
		value = (int)strtol(start + 2, NULL, 16);
	}
	else {
		value = (int)atof(start);
	}

	if (negateConstant) {
		value = -value;
	}
	negateConstant = false;

	EmitConstant(INTEGER_VAL(value));
}
void Compiler::GetDecimal(bool canAssign) {
	float value = 0;
	value = (float)atof(parser.Previous.Start);

	if (negateConstant) {
		value = -value;
	}
	negateConstant = false;

	EmitConstant(DECIMAL_VAL(value));
}
ObjString* Compiler::MakeString(Token token) {
	ObjString* string = CopyString(token.Start + 1, token.Length - 2);

	// Escape the string
	char* dst = string->Chars;
	string->Length = 0;

	for (char* src = token.Start + 1; src < token.Start + token.Length - 1; src++) {
		if (*src == '\\') {
			src++;
			switch (*src) {
			case 'n':
				*dst++ = '\n';
				break;
			case '"':
				*dst++ = '"';
				break;
			case '\'':
				*dst++ = '\'';
				break;
			case '\\':
				*dst++ = '\\';
				break;
			default:
				Error("Unknown escape character");
				break;
			}
			string->Length++;
		}
		else {
			*dst++ = *src;
			string->Length++;
		}
	}
	*dst++ = 0;

	return string;
}

void Compiler::GetString(bool canAssign) {
	ObjString* string = Compiler::MakeString(parser.Previous);
	EmitConstant(OBJECT_VAL(string));
}
void Compiler::GetArray(bool canAssign) {
	Uint32 count = 0;

	while (!MatchToken(TOKEN_RIGHT_SQUARE_BRACE)) {
		GetExpression();
		count++;

		if (!MatchToken(TOKEN_COMMA)) {
			ConsumeToken(TOKEN_RIGHT_SQUARE_BRACE, "Expected \"]\" at end of array.");
			break;
		}
	}

	EmitByte(OP_NEW_ARRAY);
	EmitUint32(count);
}
void Compiler::GetMap(bool canAssign) {
	Uint32 count = 0;

	while (!MatchToken(TOKEN_RIGHT_BRACE)) {
		ConsumeToken(TOKEN_STRING, "Expected string for map key.");
		GetString(false);

		ConsumeToken(TOKEN_COLON, "Expected \":\" after key string.");
		GetExpression();
		count++;

		if (!MatchToken(TOKEN_COMMA)) {
			ConsumeToken(TOKEN_RIGHT_BRACE, "Expected \"}\" after map.");
			break;
		}
	}

	EmitByte(OP_NEW_MAP);
	EmitUint32(count);
}
bool Compiler::IsConstant() {
	switch (PeekToken().Type) {
	case TOKEN_NULL:
	case TOKEN_TRUE:
	case TOKEN_FALSE:
		return true;
	case TOKEN_STRING:
		return true;
	case TOKEN_NUMBER:
		return true;
	case TOKEN_DECIMAL:
		return true;
	case TOKEN_MINUS: {
		switch (PeekNextToken().Type) {
		case TOKEN_NUMBER:
			return true;
		case TOKEN_DECIMAL:
			return true;
		default:
			return false;
		}
		break;
	}
	default:
		return false;
	}
}
void Compiler::GetConstant(bool canAssign) {
	switch (NextToken().Type) {
	case TOKEN_NULL:
	case TOKEN_TRUE:
	case TOKEN_FALSE:
		GetLiteral(canAssign);
		break;
	case TOKEN_STRING:
		GetString(canAssign);
		break;
	case TOKEN_NUMBER:
		GetInteger(canAssign);
		break;
	case TOKEN_DECIMAL:
		GetDecimal(canAssign);
		break;
	case TOKEN_MINUS: {
		negateConstant = true;
		switch (NextToken().Type) {
		case TOKEN_NUMBER:
			GetInteger(canAssign);
			break;
		case TOKEN_DECIMAL:
			GetDecimal(canAssign);
			break;
		default:
			Error("Invalid value after negative sign!");
			break;
		}
		break;
	}
	default:
		Error("Invalid value!");
		break;
	}
}
void Compiler::GetVariable(bool canAssign) {
	NamedVariable(parser.Previous, canAssign);
}
void Compiler::GetLogicalAND(bool canAssign) {
	int endJump = EmitJump(OP_JUMP_IF_FALSE);

	EmitByte(OP_POP);
	ParsePrecedence(PREC_AND);

	PatchJump(endJump);
}
void Compiler::GetLogicalOR(bool canAssign) {
	int elseJump = EmitJump(OP_JUMP_IF_FALSE);
	int endJump = EmitJump(OP_JUMP);

	PatchJump(elseJump);
	EmitByte(OP_POP);

	ParsePrecedence(PREC_OR);
	PatchJump(endJump);
}
void Compiler::GetConditional(bool canAssign) {
	int thenJump = EmitJump(OP_JUMP_IF_FALSE);
	EmitByte(OP_POP);
	ParsePrecedence(PREC_TERNARY);

	int elseJump = EmitJump(OP_JUMP);
	ConsumeToken(TOKEN_COLON, "Expected \":\" after conditional condition.");

	PatchJump(thenJump);
	EmitByte(OP_POP);
	ParsePrecedence(PREC_TERNARY);
	PatchJump(elseJump);
}
void Compiler::GetUnary(bool canAssign) {
	Token previousToken = parser.Previous;
	Token currentToken = parser.Current;

	int position = CodePointer();

	ParsePrecedence(PREC_UNARY);

	switch (previousToken.Type) {
	case TOKEN_MINUS:
		EmitByte(OP_NEGATE);
		break;
	case TOKEN_BITWISE_NOT:
		EmitByte(OP_BW_NOT);
		break;
	case TOKEN_LOGICAL_NOT:
		EmitByte(OP_LG_NOT);
		break;
	case TOKEN_TYPEOF:
		EmitByte(OP_TYPEOF);
		break;
	case TOKEN_INCREMENT:
	case TOKEN_DECREMENT:
		if (currentToken.Type == TOKEN_IDENTIFIER) {
			CurrentChunk()->Count = position;
		}

		UnaryIncrement(currentToken, previousToken, canAssign);
		break;
	default:
		return; // Unreachable.
	}
}
void Compiler::UnaryIncrement(Token name, Token assignmentToken, bool canAssign) {
	Uint8 getOp, setOp;
	Local local;
	int arg;

	if (name.Type != TOKEN_IDENTIFIER) {
		EmitAssignmentToken(assignmentToken);
		return;
	}

	if (ResolveNamedVariable(name, getOp, setOp, local, arg)) {
		return;
	}

	if (canAssign) {
		DoVariableAssignment(name, local, getOp, setOp, arg, assignmentToken, true);
	}
	else if (local.Constant && local.ConstantVal.Type != VAL_ERROR) {
		EmitConstant(local.ConstantVal);
	}
	else {
		EmitGetOperation(getOp, arg, name);
		EmitAssignmentToken(assignmentToken);
	}
}
void Compiler::GetNew(bool canAssign) {
	ConsumeIdentifier("Expect class name.");
	NamedVariable(parser.Previous, false);

	uint8_t argCount = 0;
	if (MatchToken(TOKEN_LEFT_PAREN)) {
		argCount = GetArgumentList();
	}
	EmitBytes(OP_NEW, argCount);
}
void Compiler::GetHitbox(bool canAssign) {
	if (!MatchToken(TOKEN_LEFT_BRACE)) {
		Compiler::GetVariable(canAssign);
		return;
	}

	int pre;
	int codePointer = CodePointer();
	bool allConstants = true;
	std::vector<Sint16> values;

	int count = 0;
	while (!MatchToken(TOKEN_RIGHT_BRACE)) {
		if (count == 4) {
			Error("Must construct hitbox with exactly four values.");
		}

		if (allConstants) {
			pre = CodePointer();
		}

		GetExpression();
		count++;

		if (allConstants) {
			VMValue value;
			uint8_t* codePtr = CurrentChunk()->Code + pre;
			if (!(pre + Bytecode::GetTotalOpcodeSize(codePtr) == CodePointer() &&
				    CurrentChunk()->GetConstant(pre, &value))) {
				allConstants = false;
			}
			else {
				if (!IS_INTEGER(value)) {
					Error("Must construct hitbox with integer values.");
				}
				values.push_back((Sint16)(AS_INTEGER(value)));
			}
		}

		if (!MatchToken(TOKEN_COMMA)) {
			ConsumeToken(
				TOKEN_RIGHT_BRACE, "Expected '}' at end of hitbox constructor.");
			break;
		}
	}

	if (count == 0) {
		EmitConstant(HITBOX_VAL(0, 0, 0, 0));
		return;
	}
	else if (count != 4) {
		Error("Must construct hitbox with exactly four values.");
	}

	if (allConstants) {
		CurrentChunk()->Count = codePointer;
		EmitConstant(HITBOX_VAL(values.data()));
		return;
	}

	EmitByte(OP_NEW_HITBOX);
}
void Compiler::GetBinary(bool canAssign) {
	Token operato = parser.Previous;
	int operatorType = operato.Type;

	ParseRule* rule = GetRule(operatorType);
	ParsePrecedence((Precedence)(rule->Precedence + 1));

	switch (operatorType) {
		// Numeric Operations
	case TOKEN_PLUS:
		EmitByte(OP_ADD);
		break;
	case TOKEN_MINUS:
		EmitByte(OP_SUBTRACT);
		break;
	case TOKEN_MULTIPLY:
		EmitByte(OP_MULTIPLY);
		break;
	case TOKEN_DIVISION:
		EmitByte(OP_DIVIDE);
		break;
	case TOKEN_MODULO:
		EmitByte(OP_MODULO);
		break;
		// Bitwise Operations
	case TOKEN_BITWISE_LEFT:
		EmitByte(OP_BITSHIFT_LEFT);
		break;
	case TOKEN_BITWISE_RIGHT:
		EmitByte(OP_BITSHIFT_RIGHT);
		break;
	case TOKEN_BITWISE_OR:
		EmitByte(OP_BW_OR);
		break;
	case TOKEN_BITWISE_AND:
		EmitByte(OP_BW_AND);
		break;
	case TOKEN_BITWISE_XOR:
		EmitByte(OP_BW_XOR);
		break;
		// Logical Operations
	case TOKEN_LOGICAL_AND:
		EmitByte(OP_LG_AND);
		break;
	case TOKEN_LOGICAL_OR:
		EmitByte(OP_LG_OR);
		break;
		// Equality and Comparison Operators
	case TOKEN_NOT_EQUALS:
		EmitByte(OP_EQUAL_NOT);
		break;
	case TOKEN_EQUALS:
		EmitByte(OP_EQUAL);
		break;
	case TOKEN_GREATER:
		EmitByte(OP_GREATER);
		break;
	case TOKEN_GREATER_EQUAL:
		EmitByte(OP_GREATER_EQUAL);
		break;
	case TOKEN_LESS:
		EmitByte(OP_LESS);
		break;
	case TOKEN_LESS_EQUAL:
		EmitByte(OP_LESS_EQUAL);
		break;
	default:
		ErrorAt(&operato, "Unknown binary operator.", true);
		return; // Unreachable.
	}
}
void Compiler::GetHas(bool canAssign) {
	ConsumeIdentifier("Expect property name.");
	EmitByte(OP_HAS_PROPERTY);
	EmitStringHash(parser.Previous);
}
void Compiler::GetSuffix(bool canAssign) {}
void Compiler::GetCall(bool canAssign) {
	Uint8 argCount = GetArgumentList();
	EmitByte(OP_CALL);
	EmitByte(argCount);
}
void Compiler::GetExpression() {
	ParsePrecedence(PREC_ASSIGNMENT);
}
// Reading statements
struct switch_case {
	bool IsDefault;
	Uint32 CasePosition;
	Uint32 JumpPosition;
	Uint32 CodeLength;
	Uint8* CodeBlock;
	int* LineBlock;
};
stack<vector<int>*> BreakJumpListStack;
stack<vector<int>*> ContinueJumpListStack;
stack<vector<switch_case>*> SwitchJumpListStack;
stack<int> BreakScopeStack;
stack<int> ContinueScopeStack;
stack<int> SwitchScopeStack;
void Compiler::GetPrintStatement() {
	GetExpression();
	ConsumeToken(TOKEN_SEMICOLON, "Expected \";\" after value.");
	if (InREPL) {
		EmitCopy(1);
	}
	EmitByte(OP_PRINT);
}
void Compiler::GetBreakpointStatement() {
	ConsumeToken(TOKEN_SEMICOLON, "Expected \";\" after 'breakpoint'.");
	AddBreakpoint();
}
void Compiler::GetExpressionStatement() {
	GetExpression();
	if (!InREPL) {
		EmitByte(OP_POP);
	}
	ConsumeToken(TOKEN_SEMICOLON, "Expected \";\" after expression.");
}
void Compiler::GetContinueStatement() {
	if (ContinueJumpListStack.size() == 0) {
		Error("Can't continue outside of loop.");
	}

	PopToScope(ContinueScopeStack.top());

	int jump = EmitJump(OP_JUMP);
	ContinueJumpListStack.top()->push_back(jump);

	ConsumeToken(TOKEN_SEMICOLON, "Expect ';' after continue.");
}
void Compiler::GetDoWhileStatement() {
	// Set the start of the loop to before the condition
	int loopStart = CodePointer();

	// Push new jump list on break stack
	StartBreakJumpList();

	// Push new jump list on continue stack
	StartContinueJumpList();

	// Execute code block
	GetStatement();

	// Pop jump list off continue stack, patch all continue to this
	// code point
	EndContinueJumpList();

	// Evaluate the condition
	ConsumeToken(TOKEN_WHILE, "Expected 'while' at end of 'do' block.");
	ConsumeToken(TOKEN_LEFT_PAREN, "Expected '(' after 'while'.");
	GetExpression();
	ConsumeToken(TOKEN_RIGHT_PAREN, "Expected ')' after condition.");
	ConsumeToken(TOKEN_SEMICOLON, "Expected ';' after ')'.");

	// Jump if false (or 0)
	int exitJump = EmitJump(OP_JUMP_IF_FALSE);

	// Pop while expression value off the stack.
	EmitByte(OP_POP);

	// After block, return to evaluation of while expression.
	EmitLoop(loopStart);

	// Set the exit jump to this point
	PatchJump(exitJump);

	// Pop value since OP_JUMP_IF_FALSE doesn't pop off expression
	// value
	EmitByte(OP_POP);

	// Pop jump list off break stack, patch all breaks to this code
	// point
	EndBreakJumpList();
}
void Compiler::GetReturnStatement() {
	if (Type == TYPE_TOP_LEVEL) {
		Error("Cannot return from top-level code.");
	}

	if (MatchToken(TOKEN_SEMICOLON)) {
		EmitReturn();
	}
	else {
		if (Type == TYPE_CONSTRUCTOR) {
			Error("Cannot return a value from an initializer.");
		}

		GetExpression();
		ConsumeToken(TOKEN_SEMICOLON, "Expect ';' after return value.");
		EmitByte(OP_RETURN);
	}
}
void Compiler::GetRepeatStatement() {
	ScopeBegin();
	ConsumeToken(TOKEN_LEFT_PAREN, "Expect '(' after 'repeat'.");
	GetExpression();

	Token variableToken = {TOKEN_ERROR};
	int remaining = 0;

	if (MatchToken(TOKEN_COMMA)) {
		ConsumeIdentifier("Expect variable name.");
		variableToken = parser.Previous;
		if (MatchToken(TOKEN_COMMA)) {
			ConsumeIdentifier("Expect variable name.");
			remaining = AddLocal(parser.Previous);
			MarkInitialized();
		}
	}

	ConsumeToken(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	if (!remaining) {
		remaining = AddHiddenLocal(" remaining", 11);
	}
	EmitByte(OP_INCREMENT); // increment remaining as we're about
	// to decrement it, so we can cheat
	// continue

	if (variableToken.Type != TOKEN_ERROR) {
		EmitConstant(INTEGER_VAL(-1));
		AddLocal(variableToken);
		MarkInitialized();
		Locals[LocalCount - 1].Constant = true; // trick the compiler into ensuring it
		// doesn't get modified
	}

	int loopStart = CurrentChunk()->Count;

	if (variableToken.Type != TOKEN_ERROR) {
		// decrement it and set it back, but keep it on the
		// stack
		EmitBytes(OP_GET_LOCAL, remaining);
		EmitByte(OP_DECREMENT);
	}
	else {
		EmitByte(OP_DECREMENT);
	}

	StartBreakJumpList();
	StartContinueJumpList();

	int exitJump = EmitJump(OP_JUMP_IF_FALSE);

	if (variableToken.Type != TOKEN_ERROR) {
		// save, pop the decrement off the stack, and increment
		// our int
		EmitBytes(OP_SET_LOCAL, remaining);
		EmitByte(OP_POP);
		EmitByte(OP_INCREMENT);
	}

	// Repeat Code Body
	GetStatement();

	EndContinueJumpList();

	EmitLoop(loopStart);

	PatchJump(exitJump);

	// if we jumped from ending we have a loose end we have to
	// remove
	if (variableToken.Type != TOKEN_ERROR) {
		EmitByte(OP_POP);
	}

	EndBreakJumpList();

	ScopeEnd();
}
void Compiler::GetSwitchStatement() {
	Chunk* chunk = CurrentChunk();

	StartBreakJumpList();
	StartContinueJumpList();

	// Evaluate the condition
	ConsumeToken(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
	GetExpression();
	ConsumeToken(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	ConsumeToken(TOKEN_LEFT_BRACE, "Expected \"{\" before statements.");

	int code_block_start = CodePointer();
	int code_block_length = code_block_start;
	Uint8* code_block_copy = NULL;
	int* line_block_copy = NULL;

	StartSwitchJumpList();
	ScopeBegin();
	GetBlockStatement();
	ScopeEnd();

	code_block_length = CodePointer() - code_block_start;

	// Copy code block
	code_block_copy = (Uint8*)malloc(code_block_length * sizeof(Uint8));
	memcpy(code_block_copy, &chunk->Code[code_block_start], code_block_length * sizeof(Uint8));

	// Copy line info block
	line_block_copy = (int*)malloc(code_block_length * sizeof(int));
	memcpy(line_block_copy, &chunk->Lines[code_block_start], code_block_length * sizeof(int));

	chunk->Count -= code_block_length;

	switch_case* defaultCase = nullptr;

	int exitJump = -1;

	vector<switch_case> cases = *SwitchJumpListStack.top();
	for (size_t i = 0; i < cases.size(); i++) {
		switch_case& case_info = cases[i];

		if (case_info.IsDefault) {
			defaultCase = &cases[i];
			continue;
		}

		EmitCopy(1);

		for (Uint32 i = 0; i < case_info.CodeLength; i++) {
			chunk->Write(case_info.CodeBlock[i], case_info.LineBlock[i]);
		}

		EmitByte(OP_EQUAL);
		int jumpToPatch = EmitJump(OP_JUMP_IF_FALSE);

		PopMultiple(2);

		case_info.JumpPosition = EmitJump(OP_JUMP);

		PatchJump(jumpToPatch);

		EmitByte(OP_POP);
	}

	EmitByte(OP_POP);

	if (defaultCase) {
		defaultCase->JumpPosition = EmitJump(OP_JUMP);
	}
	else {
		exitJump = EmitJump(OP_JUMP);
	}

	int new_block_pos = CodePointer();
	// We do this here so that if an allocation is needed, it
	// happens.
	for (int i = 0; i < code_block_length; i++) {
		chunk->Write(code_block_copy[i], line_block_copy[i]);
	}
	free(code_block_copy);
	free(line_block_copy);

	if (exitJump != -1) {
		PatchJump(exitJump);
	}

	int code_offset = new_block_pos - code_block_start;

	for (size_t i = 0; i < cases.size(); i++) {
		int jump = cases[i].CasePosition - (cases[i].JumpPosition + 2);

		jump += code_offset;

		if (jump > UINT16_MAX) {
			Error("Too much code to jump over.");
		}

		PatchJump(cases[i].JumpPosition, jump);
	}

	EndSwitchJumpList();

	// Set the old break opcode positions to the newly placed ones
	vector<int>* top = BreakJumpListStack.top();
	for (size_t i = 0; i < top->size(); i++) {
		(*top)[i] += code_offset;
	}

	// Set the old continue opcode positions to the newly placed ones
	top = ContinueJumpListStack.top();
	for (size_t i = 0; i < top->size(); i++) {
		(*top)[i] += code_offset;
	}

	// Pop jump list off continue stack, patch all continue to this
	// code point
	EndContinueJumpList();

	// Pop jump list off break stack, patch all breaks to this code
	// point
	EndBreakJumpList();
}
void Compiler::GetCaseStatement() {
	if (SwitchJumpListStack.size() == 0) {
		Error("Cannot use case label outside of switch statement.");
	}

	Chunk* chunk = CurrentChunk();

	int code_block_start = CodePointer();
	int code_block_length = code_block_start;
	Uint8* code_block_copy = NULL;
	int* line_block_copy = NULL;

	GetExpression();

	ConsumeToken(TOKEN_COLON, "Expected \":\" after \"case\".");

	code_block_length = CodePointer() - code_block_start;

	switch_case case_info;
	case_info.IsDefault = false;
	case_info.CasePosition = code_block_start;
	case_info.CodeLength = code_block_length;

	// Copy code block
	case_info.CodeBlock = (Uint8*)malloc(code_block_length * sizeof(Uint8));
	memcpy(case_info.CodeBlock,
		&chunk->Code[code_block_start],
		code_block_length * sizeof(Uint8));

	// Copy line info block
	case_info.LineBlock = (int*)malloc(code_block_length * sizeof(int));
	memcpy(case_info.LineBlock,
		&chunk->Lines[code_block_start],
		code_block_length * sizeof(int));

	chunk->Count -= code_block_length;

	SwitchJumpListStack.top()->push_back(case_info);
}
void Compiler::GetDefaultStatement() {
	if (SwitchJumpListStack.size() == 0) {
		Error("Cannot use default label outside of switch statement.");
	}

	ConsumeToken(TOKEN_COLON, "Expected \":\" after \"default\".");

	// Check if there already is a default clause, and prevent compilation if so.
	vector<switch_case>* top = SwitchJumpListStack.top();
	for (size_t i = 0; i < top->size(); i++) {
		if ((*top)[i].IsDefault) {
			Error("Cannot have multiple default clauses.");
		}
	}

	switch_case case_info;
	case_info.IsDefault = true;
	case_info.CasePosition = CodePointer();

	top->push_back(case_info);
}
void Compiler::GetWhileStatement() {
	// Set the start of the loop to before the condition
	int loopStart = CodePointer();

	// Evaluate the condition
	ConsumeToken(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
	GetExpression();
	ConsumeToken(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	// Jump if false (or 0)
	int exitJump = EmitJump(OP_JUMP_IF_FALSE);

	// Pop while expression value off the stack.
	EmitByte(OP_POP);

	// Push new jump list on break stack
	StartBreakJumpList();

	// Push new jump list on continue stack
	StartContinueJumpList();

	// Execute code block
	GetStatement();

	// Pop jump list off continue stack, patch all continue to this
	// code point
	EndContinueJumpList();

	// After block, return to evaluation of while expression.
	EmitLoop(loopStart);

	// Set the exit jump to this point
	PatchJump(exitJump);

	// Pop value since OP_JUMP_IF_FALSE doesn't pop off expression
	// value
	EmitByte(OP_POP);

	// Pop jump list off break stack, patch all breaks to this code
	// point
	EndBreakJumpList();
}
void Compiler::GetBreakStatement() {
	if (BreakJumpListStack.size() == 0) {
		Error("Cannot break outside of loop or switch statement.");
	}

	PopToScope(BreakScopeStack.top());

	int jump = EmitJump(OP_JUMP);
	BreakJumpListStack.top()->push_back(jump);

	ConsumeToken(TOKEN_SEMICOLON, "Expect ';' after break.");
}
void Compiler::GetBlockStatement() {
	while (!CheckToken(TOKEN_RIGHT_BRACE) && !CheckToken(TOKEN_EOF)) {
		GetDeclaration();
	}

	ConsumeToken(TOKEN_RIGHT_BRACE, "Expected \"}\" after block.");
}
void Compiler::GetWithStatement() {
	enum { WITH_STATE_INIT, WITH_STATE_ITERATE, WITH_STATE_FINISH, WITH_STATE_INIT_SLOTTED };

	bool useOther = true;
	bool useOtherSlot = false;
	bool hasThis = HasThis();

	// Start new scope
	ScopeBegin();

	// Reserve stack slot for where "other" will be at
	EmitByte(OP_NULL);

	// Add "other"
	int otherSlot = AddHiddenLocal("other", 5);

	// If the function has "this", make a copy of "this" (which is
	// at the first slot) into "other"
	if (hasThis) {
		EmitBytes(OP_GET_LOCAL, 0);
		EmitBytes(OP_SET_LOCAL, otherSlot);
		EmitByte(OP_POP);
	}
	else {
		// If the function does not have "this", we cannot
		// always use frame slot zero (For example, slot zero
		// is invalid in top-level functions.) So we store the
		// slot that will receive the value.
		useOtherSlot = true;
	}

	// For 'as'
	Token receiverName;

	// With "expression"
	ConsumeToken(TOKEN_LEFT_PAREN, "Expect '(' after 'with'.");
	GetExpression();
	if (MatchToken(TOKEN_AS)) {
		ConsumeIdentifier("Expect receiver name.");

		receiverName = parser.Previous;

		// Turns out we're using 'as', so rename "other" to the
		// true receiver name
		RenameLocal(&Locals[otherSlot], receiverName);

		AllLocals.push_back(Locals[otherSlot]);

		// Don't rename "other" anymore
		useOther = false;

		// Using a specific slot for "other", rather than slot
		// zero
		useOtherSlot = true;
	}
	ConsumeToken(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	// Rename "other" to "this" if the function doesn't have "this"
	if (useOther && !hasThis) {
		RenameLocal(&Locals[otherSlot], "this");

		AllLocals.push_back(Locals[otherSlot]);
	}

	// Init "with" iteration
	EmitByte(OP_WITH);

	if (useOtherSlot) {
		EmitByte(WITH_STATE_INIT_SLOTTED);
		EmitByte(otherSlot); // Store the slot where the
		// receiver will land
	}
	else {
		EmitByte(WITH_STATE_INIT);
	}

	EmitByte(0xFF);
	EmitByte(0xFF);

	int loopStart = CurrentChunk()->Count;

	// Push new jump list on break stack
	StartBreakJumpList();

	// Push new jump list on continue stack
	StartContinueJumpList();

	// Execute code block
	GetStatement();

	// Pop jump list off continue stack, patch all continue to this
	// code point
	EndContinueJumpList();

	// Loop back?
	EmitByte(OP_WITH);
	EmitByte(WITH_STATE_ITERATE);

	int offset = CurrentChunk()->Count - loopStart + 2;
	if (offset > UINT16_MAX) {
		Error("Loop body too large.");
	}

	EmitByte(offset & 0xFF);
	EmitByte((offset >> 8) & 0xFF);

	// Pop jump list off break stack, patch all breaks to this code
	// point
	EndBreakJumpList();

	// End
	EmitByte(OP_WITH);
	EmitByte(WITH_STATE_FINISH);
	EmitByte(0xFF);
	EmitByte(0xFF);

	int jump = CurrentChunk()->Count - loopStart;
	CurrentChunk()->Code[loopStart - 2] = jump & 0xFF;
	CurrentChunk()->Code[loopStart - 1] = (jump >> 8) & 0xFF;

	// End scope (will pop "other")
	ScopeEnd();
}
void Compiler::GetForStatement() {
	// Start new scope
	ScopeBegin();

	ConsumeToken(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

	// Initializer (happens only once)
	if (MatchToken(TOKEN_VAR)) {
		GetVariableDeclaration(false);
	}
	else if (MatchToken(TOKEN_SEMICOLON)) {
		// No initializer.
	}
	else {
		// "for (<identifier> in <expression>)"
		if (PeekNextToken().Type == TOKEN_IN) {
			GetForEachBlock();
			ScopeEnd();
			return;
		}
		else {
			// It's a regular 'for'
			GetExpressionStatement();
		}
	}

	int exitJump = -1;
	int loopStart = CurrentChunk()->Count;

	// Conditional
	if (!MatchToken(TOKEN_SEMICOLON)) {
		GetExpression();
		ConsumeToken(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

		// Jump out of the loop if the condition is false.
		exitJump = EmitJump(OP_JUMP_IF_FALSE);
		EmitByte(OP_POP); // Condition.
	}

	// Incremental
	if (!MatchToken(TOKEN_RIGHT_PAREN)) {
		int bodyJump = EmitJump(OP_JUMP);

		int incrementStart = CurrentChunk()->Count;
		GetExpression();
		EmitByte(OP_POP);
		ConsumeToken(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

		EmitLoop(loopStart);
		loopStart = incrementStart;
		PatchJump(bodyJump);
	}

	// Push new jump list on break stack
	StartBreakJumpList();

	// Push new jump list on continue stack
	StartContinueJumpList();

	// Execute code block
	GetStatement();

	// Pop jump list off continue stack, patch all continue to this
	// code point
	EndContinueJumpList();

	// After block, return to evaluation of condition.
	EmitLoop(loopStart);

	if (exitJump != -1) {
		PatchJump(exitJump);
		EmitByte(OP_POP); // Condition.
	}

	// Pop jump list off break stack, patch all break to this code
	// point
	EndBreakJumpList();

	// End new scope
	ScopeEnd();
}
void Compiler::GetForEachStatement() {
	// Start new scope
	ScopeBegin();

	ConsumeToken(TOKEN_LEFT_PAREN, "Expect '(' after 'foreach'.");

	GetForEachBlock();

	// End new scope
	ScopeEnd();
}
void Compiler::GetForEachBlock() {
	// Variable name
	ConsumeIdentifier("Expect variable name.");

	Token variableToken = parser.Previous;

	ConsumeToken(TOKEN_IN, "Expect 'in' after variable name.");

	// Iterator after 'in'
	GetExpression();

	// Add a local for the object to be iterated
	// The script should not be able to refer to it by name, so it begins with a space.
	// The value in it is what GetExpression() left on the top of the stack
	int iterObj = AddHiddenLocal(" iterObj", 8);

	// Add a local for the iteration state
	// Its initial value is null
	EmitByte(OP_NULL);

	int iterValue = AddHiddenLocal(" iterValue", 10);

	ConsumeToken(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");

	int exitJump = -1;
	int loopStart = CurrentChunk()->Count;

	// Call $iterObj.$iterate($iterValue)
	// $iterValue is initially null, which signals that the
	// iteration just began.
	EmitBytes(OP_GET_LOCAL, iterObj);
	EmitBytes(OP_GET_LOCAL, iterValue);
	EmitCall("iterate", 1, false);

	// Set the result to iterValue, updating the iteration state
	EmitBytes(OP_SET_LOCAL, iterValue);

	// If it returns null, the iteration ends
	EmitBytes(OP_NULL, OP_EQUAL_NOT);
	exitJump = EmitJump(OP_JUMP_IF_FALSE);
	EmitByte(OP_POP);

	// Call $iterObj.$iteratorValue($iterValue)
	EmitBytes(OP_GET_LOCAL, iterObj);
	EmitBytes(OP_GET_LOCAL, iterValue);
	EmitCall("iteratorValue", 1, false);

	// Push new jump list on break stack
	StartBreakJumpList();

	// Push new jump list on continue stack
	StartContinueJumpList();

	// Begin a new scope
	ScopeBegin();

	// Make the variable name visible
	AddLocal(variableToken);
	MarkInitialized();

	// Execute code block
	GetStatement();

	// End that new scope
	ScopeEnd();

	// Pop jump list off continue stack, patch all continue to this
	// code point
	EndContinueJumpList();

	// After block, return to evaluation of condition.
	EmitLoop(loopStart);
	PatchJump(exitJump);

	// We land here if $iterate returns null, so we need to pop the
	// value left on the stack
	EmitByte(OP_POP);

	// Pop jump list off break stack, patch all break to this code
	// point
	EndBreakJumpList();
}
void Compiler::GetIfStatement() {
	ConsumeToken(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
	GetExpression();
	ConsumeToken(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	int thenJump = EmitJump(OP_JUMP_IF_FALSE);
	EmitByte(OP_POP);
	GetStatement();

	int elseJump = EmitJump(OP_JUMP);

	PatchJump(thenJump);
	EmitByte(OP_POP); // Only Pop if OP_JUMP_IF_FALSE, as it
	// doesn't pop

	if (MatchToken(TOKEN_ELSE)) {
		GetStatement();
	}

	PatchJump(elseJump);
}
void Compiler::GetStatement() {
	if (MatchToken(TOKEN_PRINT)) {
		GetPrintStatement();
	}
	else if (MatchToken(TOKEN_CONTINUE)) {
		GetContinueStatement();
	}
	else if (MatchToken(TOKEN_DEFAULT)) {
		GetDefaultStatement();
	}
	else if (MatchToken(TOKEN_RETURN)) {
		GetReturnStatement();
	}
	else if (MatchToken(TOKEN_REPEAT)) {
		GetRepeatStatement();
	}
	else if (MatchToken(TOKEN_SWITCH)) {
		GetSwitchStatement();
	}
	else if (MatchToken(TOKEN_WHILE)) {
		GetWhileStatement();
	}
	else if (MatchToken(TOKEN_BREAK)) {
		GetBreakStatement();
	}
	else if (MatchToken(TOKEN_CASE)) {
		GetCaseStatement();
	}
	else if (MatchToken(TOKEN_WITH)) {
		GetWithStatement();
	}
	else if (MatchToken(TOKEN_FOR)) {
		GetForStatement();
	}
	else if (MatchToken(TOKEN_FOREACH)) {
		GetForEachStatement();
	}
	else if (MatchToken(TOKEN_DO)) {
		GetDoWhileStatement();
	}
	else if (MatchToken(TOKEN_IF)) {
		GetIfStatement();
	}
	else if (MatchToken(TOKEN_LEFT_BRACE)) {
		ScopeBegin();
		GetBlockStatement();
		ScopeEnd();
	}
	else if (MatchToken(TOKEN_BREAKPOINT)) {
		GetBreakpointStatement();
	}
	else {
		GetExpressionStatement();
	}
}
// Reading declarations
void Compiler::CompileFunction() {
	// Compile the parameter list.
	ConsumeToken(TOKEN_LEFT_PAREN, "Expect '(' after function name.");

	bool isOptional = false;

	if (!CheckToken(TOKEN_RIGHT_PAREN)) {
		do {
			if (!isOptional && MatchToken(TOKEN_LEFT_SQUARE_BRACE)) {
				isOptional = true;
			}

			ParseVariable("Expect parameter name.", false);
			DefineVariableToken(parser.Previous, false);

			Function->Arity++;
			if (Function->Arity > 255) {
				Error("Cannot have more than 255 parameters.");
			}

			if (!isOptional) {
				Function->MinArity++;
			}
			else if (MatchToken(TOKEN_RIGHT_SQUARE_BRACE)) {
				break;
			}
		} while (MatchToken(TOKEN_COMMA));
	}

	ConsumeToken(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

	// The body.
	ConsumeToken(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
	GetBlockStatement();
}
int Compiler::GetFunction(int type, string className) {
	int index = (int)Compiler::Functions.size();

	Compiler* compiler = new Compiler;
	compiler->ClassName = className;
	compiler->Initialize(this, 1, type);

	try {
		compiler->CompileFunction();
	}
	catch (const CompilerErrorException& error) {
		compiler->Cleanup();

		delete compiler;

		throw error;
	}

	compiler->Finish();

	delete compiler;

	return index;
}
int Compiler::GetFunction(int type) {
	return GetFunction(type, "");
}
void Compiler::GetMethod(Token className) {
	ConsumeIdentifier("Expect method name.");
	Token constantToken = parser.Previous;

	// If the method has the same name as its class, it's an
	// initializer.
	int type = TYPE_METHOD;
	if (IdentifiersEqual(&className, &parser.Previous)) {
		type = TYPE_CONSTRUCTOR;
	}

	// Compile the old instruction if it fits under uint8.
	int index = GetFunction(type, className.ToString());
	if (index <= UINT8_MAX) {
		EmitByte(OP_METHOD_V4);
		EmitByte(index);
	}
	else {
		EmitByte(OP_METHOD);
		EmitUint16(index);
	}

	EmitStringHash(constantToken);
}
void Compiler::GetVariableDeclaration(bool constant) {
	if (SwitchScopeStack.size() != 0) {
		if (SwitchScopeStack.top() == ScopeDepth) {
			Error("Cannot initialize variable inside switch statement.");
		}
	}

	do {
		int variable = ParseVariable("Expected variable name.", constant);

		Token token = parser.Previous;

		int pre = CodePointer();
		if (MatchToken(TOKEN_ASSIGNMENT)) {
			GetExpression();
		}
		else {
			if (constant) { // don't play nice
				ErrorAtCurrent(
					"\"const\" variables must have an explicit constant declaration.");
			}

			EmitByte(OP_NULL);
		}

		VMValue value;
		Local* locals = constant ? Constants.data() : Locals;
		if (pre + Bytecode::GetTotalOpcodeSize(CurrentChunk()->Code + pre) == CodePointer() &&
			CurrentChunk()->GetConstant(pre, &value)) {
			if (variable != -1) {
				locals[variable].ConstantVal = value;
				locals[variable].Constant = constant;
				if (constant) {
					CurrentChunk()->Count = pre;
				}
			}
		}
		else if (constant) {
			ErrorAtCurrent("\"const\" variables must be set to a constant.");
		}

		DefineVariableToken(token, constant);
		if (constant && variable == -1) {
			// treat it like a module constant
			ModuleConstants.push_back({token, 0, 0, false, false, true, value});
		}
	} while (MatchToken(TOKEN_COMMA));

	ConsumeToken(TOKEN_SEMICOLON, "Expected \";\" after variable declaration.");
}
void Compiler::GetModuleVariableDeclaration() {
	if (ScopeDepth > 0) {
		Error("Cannot use local declaration outside of top-level code.");
	}

	if (parser.Current.Type == TOKEN_VAR || parser.Current.Type == TOKEN_CONST) {
		bool constant = parser.Current.Type == TOKEN_CONST;
		vector<Local>* vec = constant ? &ModuleConstants : &ModuleLocals;
		AdvanceToken();

		Token token = parser.Current;
		do {
			int local = ParseModuleVariable("Expected variable name.", constant);

			int pre = CodePointer();
			if (MatchToken(TOKEN_ASSIGNMENT)) {
				GetExpression();
			}
			else {
				if (constant) { // don't play nice
					ErrorAtCurrent(
						"\"const\" variables must have an explicit constant declaration.");
				}

				EmitByte(OP_NULL);
			}

			VMValue value;
			if (pre + Bytecode::GetTotalOpcodeSize(CurrentChunk()->Code + pre) == CodePointer() &&
				CurrentChunk()->GetConstant(pre, &value)) {
				vec->at(local).ConstantVal = value;
				if (constant) {
					CurrentChunk()->Count = pre;
				}
			}
			else if (constant) {
				ErrorAt(&token,
					"\"const\" variables must be set to a constant.",
					true);
			}

			if (!constant) {
				DefineModuleVariable(local);
			}
		} while (MatchToken(TOKEN_COMMA));

		ConsumeToken(TOKEN_SEMICOLON, "Expected \";\" after variable declaration.");
	}
	else {
		ErrorAtCurrent("Expected \"var\" or \"const\" after \"local\" declaration.");
	}
}
void Compiler::GetPropertyDeclaration(Token propertyName) {
	do {
		ParseVariable("Expected property name.", false);

		NamedVariable(propertyName, false);

		Token token = parser.Previous;

		if (MatchToken(TOKEN_ASSIGNMENT)) {
			GetExpression();
		}
		else {
			EmitByte(OP_NULL);
		}

		EmitSetOperation(OP_SET_PROPERTY, -1, token);

		EmitByte(OP_POP);
	} while (MatchToken(TOKEN_COMMA));

	ConsumeToken(TOKEN_SEMICOLON, "Expected \";\" after property declaration.");
}
void Compiler::GetClassDeclaration() {
	ConsumeIdentifier("Expect class name.");

	Token className = parser.Previous;
	DeclareVariable(&className, false);

	EmitByte(OP_CLASS);
	EmitStringHash(className);

	ClassHashList.push_back(GetHash(className));

	// Check for class extension
	if (MatchToken(TOKEN_PLUS)) {
		EmitByte(CLASS_TYPE_EXTENDED);
		ClassExtendedList.push_back(1);
	}
	else {
		EmitByte(CLASS_TYPE_NORMAL);
		ClassExtendedList.push_back(0);
	}

	if (MatchToken(TOKEN_LESS)) {
		ConsumeIdentifier("Expect base class name.");
		Token superName = parser.Previous;

		EmitByte(OP_INHERIT);
		EmitStringHash(superName);
	}

	DefineVariableToken(className, false);

	ConsumeToken(TOKEN_LEFT_BRACE, "Expect '{' before class body.");

	while (!CheckToken(TOKEN_RIGHT_BRACE) && !CheckToken(TOKEN_EOF)) {
		if (MatchToken(TOKEN_EVENT)) {
			NamedVariable(className, false);
			GetMethod(className);
		}
		else if (MatchToken(TOKEN_STATIC)) {
			GetPropertyDeclaration(className);
		}
		else {
			NamedVariable(className, false);
			GetMethod(className);
		}
	}

	ConsumeToken(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
}
void Compiler::GetEnumDeclaration() {
	Token enumName;
	bool isNamed = false;

	if (MatchToken(TOKEN_IDENTIFIER)) {
		enumName = parser.Previous;
		DeclareVariable(&enumName, false);

		EmitByte(OP_NEW_ENUM);
		EmitStringHash(enumName);

		DefineVariableToken(enumName, true);

		isNamed = true;
	}

	ConsumeToken(TOKEN_LEFT_BRACE, "Expect '{' before enum body.");

	while (!CheckToken(TOKEN_RIGHT_BRACE) && !CheckToken(TOKEN_EOF)) {
		bool didStart = false;

		VMValue current = INTEGER_VAL(0);
		do {
			if (CheckToken(TOKEN_RIGHT_BRACE)) {
				break;
			}

			int variable = ParseVariable("Expected constant name.", true);

			Token token = parser.Previous;

			// Push the enum class to the stack
			if (isNamed) {
				NamedVariable(enumName, false);
			}

			if (MatchToken(TOKEN_ASSIGNMENT)) {
				int pre = CodePointer();
				GetExpression();
				if (pre + Bytecode::GetTotalOpcodeSize(CurrentChunk()->Code + pre) !=
						CodePointer() ||
					!CurrentChunk()->GetConstant(pre, &current)) {
					ErrorAt(&token,
						"Manual enum value must be constant.",
						true);
				}
				EmitCopy(1);
				EmitByte(OP_SAVE_VALUE);
			}
			else {
				if (didStart) {
					if (IS_NOT_NUMBER(current)) {
						Warning("Current enum base is a non-number, this enum value will be null!");
						current = NULL_VAL;
					}
					else if (IS_DECIMAL(current)) {
						current.as.Decimal += 1;
					}
					else if (IS_INTEGER(current)) {
						current.as.Integer++;
					}
					EmitByte(OP_LOAD_VALUE);
					EmitConstant(INTEGER_VAL(1));
					EmitByte(OP_ENUM_NEXT);
				}
				else {
					EmitConstant(INTEGER_VAL(0));
				}
				EmitCopy(1);
				EmitByte(OP_SAVE_VALUE);
			}

			didStart = true;

			if (isNamed) {
				EmitByte(OP_ADD_ENUM);
				EmitStringHash(token);
			}
			else {
				DefineVariableToken(token, true);
				if (variable == -1) {
					// treat it as a module
					// constant
					ModuleConstants.push_back(
						{token, 0, 0, false, false, true, current});
				}
			}
		} while (MatchToken(TOKEN_COMMA));
	}

	ConsumeToken(TOKEN_RIGHT_BRACE, "Expect '}' after enum body.");
}
void Compiler::GetImportDeclaration() {
	bool importModules = MatchToken(TOKEN_FROM);

	do {
		ConsumeToken(TOKEN_STRING, "Expect string after 'import'.");

		Token className = parser.Previous;
		VMValue value = OBJECT_VAL(Compiler::MakeString(className));

		EmitByte(importModules ? OP_IMPORT_MODULE : OP_IMPORT);
		EmitUint32(GetConstantIndex(value));
	} while (MatchToken(TOKEN_COMMA));

	ConsumeToken(TOKEN_SEMICOLON, "Expected \";\" after \"import\" declaration.");
}
void Compiler::GetUsingDeclaration() {
	ConsumeToken(TOKEN_NAMESPACE, "Expected \"namespace\" after \"using\" declaration.");

	if (ScopeDepth > 0) {
		Error("Cannot use namespaces outside of top-level code.");
	}

	do {
		ConsumeIdentifier("Expected namespace name.");
		Token nsName = parser.Previous;
		EmitByte(OP_USE_NAMESPACE);
		EmitStringHash(nsName);
	} while (MatchToken(TOKEN_COMMA));

	ConsumeToken(TOKEN_SEMICOLON, "Expected \";\" after \"using\" declaration.");
}
void Compiler::GetEventDeclaration() {
	ConsumeIdentifier("Expected event name.");
	Token constantToken = parser.Previous;

	// FIXME: We don't work with closures and upvalues yet, so
	// we still have to declare functions globally regardless of
	// scope.

	// if (ScopeDepth > 0) {
	//     DeclareVariable(&constantToken);
	//     MarkInitialized();
	// }

	// Compile the old instruction if it fits under uint8.
	int index = GetFunction(TYPE_FUNCTION);
	if (index <= UINT8_MAX) {
		EmitByte(OP_EVENT_V4);
		EmitByte(index);
	}
	else {
		EmitByte(OP_EVENT);
		EmitUint16(index);
	}

	// if (ScopeDepth == 0) {
	EmitByte(OP_DEFINE_GLOBAL);
	EmitStringHash(constantToken);
	// }
}
void Compiler::GetDeclaration() {
	if (MatchToken(TOKEN_CLASS)) {
		GetClassDeclaration();
	}
	else if (MatchToken(TOKEN_ENUM)) {
		GetEnumDeclaration();
	}
	else if (MatchToken(TOKEN_IMPORT)) {
		GetImportDeclaration();
	}
	else if (MatchToken(TOKEN_VAR)) {
		GetVariableDeclaration(false);
	}
	else if (MatchToken(TOKEN_CONST)) {
		GetVariableDeclaration(true);
	}
	else if (MatchToken(TOKEN_LOCAL)) {
		GetModuleVariableDeclaration();
	}
	else if (MatchToken(TOKEN_USING)) {
		GetUsingDeclaration();
	}
	else if (MatchToken(TOKEN_EVENT)) {
		GetEventDeclaration();
	}
	else {
		GetStatement();
	}
}

void Compiler::MakeRules() {
	Rules = (ParseRule*)Memory::TrackedCalloc(
		"Compiler::Rules", TOKEN_EOF + 1, sizeof(ParseRule));
	// Single-character tokens.
	Rules[TOKEN_LEFT_PAREN] =
		ParseRule{&Compiler::GetGrouping, &Compiler::GetCall, NULL, PREC_CALL};
	Rules[TOKEN_RIGHT_PAREN] = ParseRule{NULL, NULL, NULL, PREC_NONE};
	Rules[TOKEN_LEFT_BRACE] = ParseRule{&Compiler::GetMap, NULL, NULL, PREC_CALL};
	Rules[TOKEN_RIGHT_BRACE] = ParseRule{NULL, NULL, NULL, PREC_NONE};
	Rules[TOKEN_LEFT_SQUARE_BRACE] =
		ParseRule{&Compiler::GetArray, &Compiler::GetElement, NULL, PREC_CALL};
	Rules[TOKEN_RIGHT_SQUARE_BRACE] = ParseRule{NULL, NULL, NULL, PREC_NONE};
	Rules[TOKEN_COMMA] = ParseRule{NULL, NULL, NULL, PREC_NONE};
	Rules[TOKEN_DOT] = ParseRule{NULL, &Compiler::GetDot, NULL, PREC_CALL};
	Rules[TOKEN_SEMICOLON] = ParseRule{NULL, NULL, NULL, PREC_NONE};
	// Operators
	Rules[TOKEN_MINUS] = ParseRule{&Compiler::GetUnary, &Compiler::GetBinary, NULL, PREC_TERM};
	Rules[TOKEN_PLUS] = ParseRule{NULL, &Compiler::GetBinary, NULL, PREC_TERM};
	Rules[TOKEN_DECREMENT] =
		ParseRule{&Compiler::GetUnary, NULL, NULL, PREC_CALL}; // &Compiler::GetSuffix
	Rules[TOKEN_INCREMENT] =
		ParseRule{&Compiler::GetUnary, NULL, NULL, PREC_CALL}; // &Compiler::GetSuffix
	Rules[TOKEN_DIVISION] = ParseRule{NULL, &Compiler::GetBinary, NULL, PREC_FACTOR};
	Rules[TOKEN_MULTIPLY] = ParseRule{NULL, &Compiler::GetBinary, NULL, PREC_FACTOR};
	Rules[TOKEN_MODULO] = ParseRule{NULL, &Compiler::GetBinary, NULL, PREC_FACTOR};
	Rules[TOKEN_BITWISE_XOR] = ParseRule{NULL, &Compiler::GetBinary, NULL, PREC_BITWISE_XOR};
	Rules[TOKEN_BITWISE_AND] = ParseRule{NULL, &Compiler::GetBinary, NULL, PREC_BITWISE_AND};
	Rules[TOKEN_BITWISE_OR] = ParseRule{NULL, &Compiler::GetBinary, NULL, PREC_BITWISE_OR};
	Rules[TOKEN_BITWISE_LEFT] = ParseRule{NULL, &Compiler::GetBinary, NULL, PREC_BITWISE_SHIFT};
	Rules[TOKEN_BITWISE_RIGHT] =
		ParseRule{NULL, &Compiler::GetBinary, NULL, PREC_BITWISE_SHIFT};
	Rules[TOKEN_BITWISE_NOT] = ParseRule{&Compiler::GetUnary, NULL, NULL, PREC_UNARY};
	Rules[TOKEN_TERNARY] = ParseRule{NULL, &Compiler::GetConditional, NULL, PREC_TERNARY};
	Rules[TOKEN_COLON] = ParseRule{NULL, NULL, NULL, PREC_NONE};
	Rules[TOKEN_LOGICAL_AND] = ParseRule{NULL, &Compiler::GetLogicalAND, NULL, PREC_AND};
	Rules[TOKEN_LOGICAL_OR] = ParseRule{NULL, &Compiler::GetLogicalOR, NULL, PREC_OR};
	Rules[TOKEN_LOGICAL_NOT] = ParseRule{&Compiler::GetUnary, NULL, NULL, PREC_UNARY};
	Rules[TOKEN_TYPEOF] = ParseRule{&Compiler::GetUnary, NULL, NULL, PREC_UNARY};
	Rules[TOKEN_NEW] = ParseRule{&Compiler::GetNew, NULL, NULL, PREC_UNARY};
	Rules[TOKEN_NOT_EQUALS] = ParseRule{NULL, &Compiler::GetBinary, NULL, PREC_EQUALITY};
	Rules[TOKEN_EQUALS] = ParseRule{NULL, &Compiler::GetBinary, NULL, PREC_EQUALITY};
	Rules[TOKEN_HAS] = ParseRule{NULL, &Compiler::GetHas, NULL, PREC_EQUALITY};
	Rules[TOKEN_GREATER] = ParseRule{NULL, &Compiler::GetBinary, NULL, PREC_COMPARISON};
	Rules[TOKEN_GREATER_EQUAL] = ParseRule{NULL, &Compiler::GetBinary, NULL, PREC_COMPARISON};
	Rules[TOKEN_LESS] = ParseRule{NULL, &Compiler::GetBinary, NULL, PREC_COMPARISON};
	Rules[TOKEN_LESS_EQUAL] = ParseRule{NULL, &Compiler::GetBinary, NULL, PREC_COMPARISON};
	// Assignment
	Rules[TOKEN_ASSIGNMENT] = ParseRule{NULL, NULL, NULL, PREC_NONE};
	Rules[TOKEN_ASSIGNMENT_MULTIPLY] = ParseRule{NULL, NULL, NULL, PREC_NONE};
	Rules[TOKEN_ASSIGNMENT_DIVISION] = ParseRule{NULL, NULL, NULL, PREC_NONE};
	Rules[TOKEN_ASSIGNMENT_MODULO] = ParseRule{NULL, NULL, NULL, PREC_NONE};
	Rules[TOKEN_ASSIGNMENT_PLUS] = ParseRule{NULL, NULL, NULL, PREC_NONE};
	Rules[TOKEN_ASSIGNMENT_MINUS] = ParseRule{NULL, NULL, NULL, PREC_NONE};
	Rules[TOKEN_ASSIGNMENT_BITWISE_LEFT] = ParseRule{NULL, NULL, NULL, PREC_NONE};
	Rules[TOKEN_ASSIGNMENT_BITWISE_RIGHT] = ParseRule{NULL, NULL, NULL, PREC_NONE};
	Rules[TOKEN_ASSIGNMENT_BITWISE_AND] = ParseRule{NULL, NULL, NULL, PREC_NONE};
	Rules[TOKEN_ASSIGNMENT_BITWISE_XOR] = ParseRule{NULL, NULL, NULL, PREC_NONE};
	Rules[TOKEN_ASSIGNMENT_BITWISE_OR] = ParseRule{NULL, NULL, NULL, PREC_NONE};
	// Keywords
	Rules[TOKEN_THIS] = ParseRule{&Compiler::GetThis, NULL, NULL, PREC_NONE};
	Rules[TOKEN_SUPER] = ParseRule{&Compiler::GetSuper, NULL, NULL, PREC_NONE};
	// Constants or whatever
	Rules[TOKEN_NULL] = ParseRule{&Compiler::GetLiteral, NULL, NULL, PREC_NONE};
	Rules[TOKEN_TRUE] = ParseRule{&Compiler::GetLiteral, NULL, NULL, PREC_NONE};
	Rules[TOKEN_FALSE] = ParseRule{&Compiler::GetLiteral, NULL, NULL, PREC_NONE};
	Rules[TOKEN_STRING] = ParseRule{&Compiler::GetString, NULL, NULL, PREC_NONE};
	Rules[TOKEN_NUMBER] = ParseRule{&Compiler::GetInteger, NULL, NULL, PREC_NONE};
	Rules[TOKEN_DECIMAL] = ParseRule{&Compiler::GetDecimal, NULL, NULL, PREC_NONE};
	Rules[TOKEN_IDENTIFIER] = ParseRule{&Compiler::GetVariable, NULL, NULL, PREC_NONE};
	Rules[TOKEN_HITBOX] = ParseRule{&Compiler::GetHitbox, NULL, NULL, PREC_NONE};
}
ParseRule* Compiler::GetRule(int type) {
	return &Compiler::Rules[(int)type];
}

void Compiler::ParsePrecedence(Precedence precedence) {
	AdvanceToken();
	ParseFn prefixRule = GetRule(parser.Previous.Type)->Prefix;
	if (prefixRule == NULL) {
		Error("Expected expression.");
		return;
	}

	int preCount = CurrentChunk()->Count;
	int preConstant = CurrentChunk()->Constants->size();

	bool canAssign = precedence <= PREC_ASSIGNMENT;
	(this->*prefixRule)(canAssign);

	if (CurrentSettings.DoOptimizations) {
		preConstant = CheckPrefixOptimize(preCount, preConstant, prefixRule);
	}

	while (precedence <= GetRule(parser.Current.Type)->Precedence) {
		AdvanceToken();
		ParseFn infixRule = GetRule(parser.Previous.Type)->Infix;
		if (infixRule) {
			(this->*infixRule)(canAssign);
		}
		if (CurrentSettings.DoOptimizations) {
			preConstant = CheckInfixOptimize(preCount, preConstant, infixRule);
		}
	}

	if (canAssign && MatchAssignmentToken()) {
		Error("Invalid assignment target.");
		GetExpression();
	}
}
Uint32 Compiler::GetHash(char* string) {
	return Murmur::EncryptString(string);
}
Uint32 Compiler::GetHash(Token token) {
	return Murmur::EncryptData(token.Start, token.Length);
}

Chunk* Compiler::CurrentChunk() {
	return &Function->Chunk;
}
int Compiler::CodePointer() {
	return CurrentChunk()->Count;
}
void Compiler::EmitByte(Uint8 byte) {
	CurrentChunk()->Write(byte,
		(int)((parser.Previous.Pos & 0xFFFF) << 16 | (parser.Previous.Line & 0xFFFF)));
}
void Compiler::EmitBytes(Uint8 byte1, Uint8 byte2) {
	EmitByte(byte1);
	EmitByte(byte2);
}
void Compiler::EmitUint16(Uint16 value) {
	EmitByte(value & 0xFF);
	EmitByte(value >> 8 & 0xFF);
}
void Compiler::EmitSint16(Sint16 value) {
	EmitUint16((Uint16)value);
}
void Compiler::EmitUint32(Uint32 value) {
	EmitByte(value & 0xFF);
	EmitByte(value >> 8 & 0xFF);
	EmitByte(value >> 16 & 0xFF);
	EmitByte(value >> 24 & 0xFF);
}
void Compiler::EmitSint32(Sint32 value) {
	EmitUint32((Uint32)value);
}
void Compiler::EmitFloat(float value) {
	Uint8* bytes = (Uint8*)(&value);
	EmitByte(*bytes++);
	EmitByte(*bytes++);
	EmitByte(*bytes++);
	EmitByte(*bytes);
}

int Compiler::GetConstantIndex(VMValue value) {
	int index = FindConstant(value);
	if (index < 0) {
		index = MakeConstant(value);
	}
	return index;
}
int Compiler::EmitConstant(VMValue value) {
	if (value.Type == VAL_INTEGER) {
		int i = AS_INTEGER(value);
		if (CurrentSettings.DoOptimizations && (i == 0 || i == 1)) {
			EmitByte(!i ? OP_FALSE : OP_TRUE);
		}
		else {
			EmitByte(OP_INTEGER);
			EmitSint32(value.as.Integer);
		}
		return -1;
	}
	else if (value.Type == VAL_DECIMAL) {
		EmitByte(OP_DECIMAL);
		EmitFloat(value.as.Decimal);
		return -1;
	}
	else if (value.Type == VAL_NULL) {
		EmitByte(OP_NULL);
		return -1;
	}

	// anything else gets added to the const table
	int index = GetConstantIndex(value);

	EmitByte(OP_CONSTANT);
	EmitUint32(index);

	return index;
}

void Compiler::EmitLoop(int loopStart) {
	EmitByte(OP_JUMP_BACK);

	int offset = CurrentChunk()->Count - loopStart + 2;
	if (offset > UINT16_MAX) {
		Error("Loop body too large.");
	}

	EmitByte(offset & 0xFF);
	EmitByte((offset >> 8) & 0xFF);
}
int Compiler::GetJump(int offset) {
	int jump = CurrentChunk()->Count - (offset + 2);
	if (jump > UINT16_MAX) {
		Error("Too much code to jump over.");
	}

	return jump;
}
int Compiler::GetPosition() {
	return CurrentChunk()->Count;
}
int Compiler::EmitJump(Uint8 instruction) {
	return EmitJump(instruction, 0xFFFF);
}
int Compiler::EmitJump(Uint8 instruction, int jump) {
	EmitByte(instruction);
	EmitUint16(jump);
	return CurrentChunk()->Count - 2;
}
void Compiler::PatchJump(int offset, int jump) {
	CurrentChunk()->Code[offset] = jump & 0xFF;
	CurrentChunk()->Code[offset + 1] = (jump >> 8) & 0xFF;
}
void Compiler::PatchJump(int offset) {
	int jump = GetJump(offset);
	PatchJump(offset, jump);
}
void Compiler::EmitStringHash(const char* string) {
	Uint32 hash = GetHash((char*)string);
	if (!TokenMap->Exists(hash)) {
		Token tk;
		tk.Start = (char*)string;
		tk.Length = strlen(string);
		TokenMap->Put(hash, tk);
	}
	EmitUint32(hash);
}
void Compiler::EmitStringHash(Token token) {
	if (!TokenMap->Exists(GetHash(token))) {
		TokenMap->Put(GetHash(token), token);
	}
	EmitUint32(GetHash(token));
}
void Compiler::EmitReturn() {
	if (Type == TYPE_CONSTRUCTOR) {
		// return the new instance built from the constructor
		EmitBytes(OP_GET_LOCAL, 0);
	}
	else if (!InREPL) {
		EmitByte(OP_NULL);
	}
	EmitByte(OP_RETURN);
}

// Advanced Jumping
void Compiler::StartBreakJumpList() {
	BreakJumpListStack.push(new vector<int>());
	BreakScopeStack.push(ScopeDepth);
}
void Compiler::EndBreakJumpList() {
	vector<int>* top = BreakJumpListStack.top();
	for (size_t i = 0; i < top->size(); i++) {
		int offset = (*top)[i];
		PatchJump(offset);
	}
	delete top;
	BreakJumpListStack.pop();
	BreakScopeStack.pop();
}
void Compiler::StartContinueJumpList() {
	ContinueJumpListStack.push(new vector<int>());
	ContinueScopeStack.push(ScopeDepth);
}
void Compiler::EndContinueJumpList() {
	vector<int>* top = ContinueJumpListStack.top();
	for (size_t i = 0; i < top->size(); i++) {
		int offset = (*top)[i];
		PatchJump(offset);
	}
	delete top;
	ContinueJumpListStack.pop();
	ContinueScopeStack.pop();
}
void Compiler::StartSwitchJumpList() {
	SwitchJumpListStack.push(new vector<switch_case>());
	SwitchScopeStack.push(ScopeDepth + 1);
}
void Compiler::EndSwitchJumpList() {
	vector<switch_case>* top = SwitchJumpListStack.top();
	for (size_t i = 0; i < top->size(); i++) {
		if (!(*top)[i].IsDefault) {
			free((*top)[i].CodeBlock);
			free((*top)[i].LineBlock);
		}
	}
	delete top;
	SwitchJumpListStack.pop();
	SwitchScopeStack.pop();
}

int Compiler::FindConstant(VMValue value) {
	for (size_t i = 0; i < CurrentChunk()->Constants->size(); i++) {
		VMValue constant = (*CurrentChunk()->Constants)[i];
		if (IS_STRING(constant) && IS_STRING(value) && Value::SortaEqual(constant, value)) {
			return (int)i;
		}
		if (Value::ExactlyEqual(value, constant)) {
			return (int)i;
		}
	}
	return -1;
}
int Compiler::MakeConstant(VMValue value) {
	int constant = CurrentChunk()->AddConstant(value);
	// if (constant > UINT8_MAX) {
	//     Error("Too many constants in one chunk.");
	//     return 0;
	// }
	return constant;
}

bool Compiler::HasThis() {
	switch (Type) {
	case TYPE_CONSTRUCTOR:
	case TYPE_METHOD:
		return true;
	default:
		return false;
	}
}
void Compiler::SetReceiverName(const char* name) {
	Local* local = &Locals[0];
	local->Name.Start = (char*)name;
	local->Name.Length = strlen(name);
}
void Compiler::SetReceiverName(Token name) {
	Local* local = &Locals[0];
	local->Name = name;
}

int Compiler::CheckPrefixOptimize(int preCount, int preConstant, ParseFn fn) {
	int checkConstant = -1;
	VMValue out = NULL_VAL;

	if (fn == &Compiler::GetUnary) {
		Uint8 unOp = CurrentChunk()->Code[CodePointer() - 1];
		if (unOp == OP_TYPEOF) {
			return preConstant;
		}
		Uint8 op = CurrentChunk()->Code[preCount];
		VMValue constant;
		if (preCount + Bytecode::GetTotalOpcodeSize(CurrentChunk()->Code + preCount) !=
				CodePointer() - 1 ||
			!CurrentChunk()->GetConstant(preCount,
				&constant,
				&checkConstant)) {
			return preConstant;
		}

		if (IS_NOT_NUMBER(constant) && unOp != OP_LG_NOT) {
			return preConstant;
		}

		switch (unOp) {
		case OP_LG_NOT:
			CurrentChunk()->Count = preCount;

			switch (constant.Type) {
			case VAL_NULL:
				EmitByte(OP_TRUE);
				break;
			case VAL_OBJECT:
				EmitByte(OP_FALSE);
				break;
			case VAL_DECIMAL:
				EmitByte((float)(AS_DECIMAL(constant) == 0.0) ? OP_TRUE : OP_FALSE);
				break;
			case VAL_INTEGER:
				EmitByte(!AS_INTEGER(constant) ? OP_TRUE : OP_FALSE);
				break;
			}
			break;
		case OP_NEGATE:
			CurrentChunk()->Count = preCount;

			if (constant.Type == VAL_DECIMAL) {
				out = DECIMAL_VAL(-AS_DECIMAL(constant));
			}
			else {
				out = INTEGER_VAL(-AS_INTEGER(constant));
			}
			break;
		case OP_BW_NOT:
			CurrentChunk()->Count = preCount;

			if (constant.Type == VAL_DECIMAL) {
				out = DECIMAL_VAL((float)(~(int)AS_DECIMAL(constant)));
			}
			else {
				out = INTEGER_VAL(~AS_INTEGER(constant));
			}
			break;
		case OP_INCREMENT: {
			CurrentChunk()->Count = preCount;

			if (constant.Type == VAL_DECIMAL) {
				out = DECIMAL_VAL(++AS_DECIMAL(constant));
			}
			else {
				out = INTEGER_VAL(++AS_INTEGER(constant));
			}
			break;
		}
		case OP_DECREMENT: {
			CurrentChunk()->Count = preCount;

			if (constant.Type == VAL_DECIMAL) {
				out = DECIMAL_VAL(--AS_DECIMAL(constant));
			}
			else {
				out = INTEGER_VAL(--AS_INTEGER(constant));
			}
			break;
		}
		}
	}

	if (checkConstant >= preConstant) {
		CurrentChunk()->Constants->pop_back();
		// Log::PrintSimple("Constant eaten: %d\n",
		// checkConstant);
	}
	if (!IS_NULL(out)) {
		EmitConstant(out);
		preConstant = CurrentChunk()->Constants->size();
		if (out.Type == VAL_INTEGER) {
			preConstant =
				CheckPrefixOptimize(preCount, preConstant, &Compiler::GetInteger);
		}
	}

	///////////
	// printf("------AFTER : @ %d %d\n", preCount,
	// CurrentChunk()->Constants->size()); for (int i = preCount; i
	// < CurrentChunk()->Count;)
	//     i = DebugInstruction(CurrentChunk(), i);
	// printf("----------------- @ %d\n", preCount);
	///////////

	return preConstant;
}

int Compiler::CheckInfixOptimize(int preCount, int preConstant, ParseFn fn) {
	///////////
	// printf("------InfixOptimize @ %d %d\n", preCount,
	// preConstant); for (int i = preCount; i <
	// CurrentChunk()->Count;)
	//     i = DebugInstruction(CurrentChunk(), i);
	///////////

	if (fn == &Compiler::GetBinary) {
		// this is gonna be really basic for now (constant
		// constant OP) some of the stuff that passes through
		// here are much longer than that, but this is a very
		// solid start that already can shrink a good amount

		int off1 = preCount;
		Uint8 op1 = CurrentChunk()->Code[off1];
		int off2 = Bytecode::GetTotalOpcodeSize(CurrentChunk()->Code + off1) + off1;
		if (off2 >= CodePointer()) {
			return preConstant;
		}
		Uint8 op2 = CurrentChunk()->Code[off2];
		int offB = Bytecode::GetTotalOpcodeSize(CurrentChunk()->Code + off2) + off2;
		if (offB != CodePointer() - 1) { // CHANGE TO >= ONCE
			// CASCADING IS ADDED
			return preConstant;
		}
		Uint8 opB = CurrentChunk()->Code[offB];

		VMValue a;
		int checkConstantA = -1;
		VMValue b;
		int checkConstantB = -1;

		if (!CurrentChunk()->GetConstant(off1, &a, &checkConstantA)) {
			return preConstant;
		}

		if (!CurrentChunk()->GetConstant(off2, &b, &checkConstantB)) {
			return preConstant;
		}

		VMValue out;

		switch (opB) {
		// Numeric Operations
		case OP_ADD: {
			if (IS_STRING(a) || IS_STRING(b)) {
				VMValue str_b = Value::CastAsString(b);
				VMValue str_a = Value::CastAsString(a);
				out = Value::Concatenate(str_a, str_b);
				break;
			}
			else if (IS_NOT_NUMBER(a) || IS_NOT_NUMBER(b)) {
				return preConstant;
			}

			if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
				float a_d = AS_DECIMAL(Value::CastAsDecimal(a));
				float b_d = AS_DECIMAL(Value::CastAsDecimal(b));
				out = DECIMAL_VAL(a_d + b_d);
			}
			else {
				int a_d = AS_INTEGER(a);
				int b_d = AS_INTEGER(b);
				out = INTEGER_VAL(a_d + b_d);
			}

			break;
		}
		case OP_SUBTRACT: {
			if (IS_NOT_NUMBER(a) || IS_NOT_NUMBER(b)) {
				return preConstant;
			}

			if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
				float a_d = AS_DECIMAL(Value::CastAsDecimal(a));
				float b_d = AS_DECIMAL(Value::CastAsDecimal(b));
				out = DECIMAL_VAL(a_d - b_d);
			}
			else {
				int a_d = AS_INTEGER(a);
				int b_d = AS_INTEGER(b);
				out = INTEGER_VAL(a_d - b_d);
			}

			break;
		}
		case OP_MULTIPLY: {
			if (IS_NOT_NUMBER(a) || IS_NOT_NUMBER(b)) {
				return preConstant;
			}

			if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
				float a_d = AS_DECIMAL(Value::CastAsDecimal(a));
				float b_d = AS_DECIMAL(Value::CastAsDecimal(b));
				out = DECIMAL_VAL(a_d * b_d);
			}
			else {
				int a_d = AS_INTEGER(a);
				int b_d = AS_INTEGER(b);
				out = INTEGER_VAL(a_d * b_d);
			}

			break;
		}
		case OP_DIVIDE: {
			if (IS_NOT_NUMBER(a) || IS_NOT_NUMBER(b)) {
				return preConstant;
			}

			if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
				float a_d = AS_DECIMAL(Value::CastAsDecimal(a));
				float b_d = AS_DECIMAL(Value::CastAsDecimal(b));

				if (b_d == 0) {
					return preConstant;
				}
				out = DECIMAL_VAL(a_d / b_d);
			}
			else {
				int a_d = AS_INTEGER(a);
				int b_d = AS_INTEGER(b);
				if (b_d == 0) {
					return preConstant;
				}

				out = INTEGER_VAL(a_d / b_d);
			}

			break;
		}
		case OP_MODULO: {
			if (IS_NOT_NUMBER(a) || IS_NOT_NUMBER(b)) {
				return preConstant;
			}

			if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
				float a_d = AS_DECIMAL(Value::CastAsDecimal(a));
				float b_d = AS_DECIMAL(Value::CastAsDecimal(b));
				out = DECIMAL_VAL(fmod(a_d, b_d));
			}
			else {
				int a_d = AS_INTEGER(a);
				int b_d = AS_INTEGER(b);
				out = INTEGER_VAL(a_d % b_d);
			}

			break;
		}
		// Bitwise Operations
		case OP_BITSHIFT_LEFT: {
			if (IS_NOT_NUMBER(a) || IS_NOT_NUMBER(b)) {
				return preConstant;
			}

			if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
				int a_d = AS_INTEGER(Value::CastAsInteger(a));
				int b_d = AS_INTEGER(Value::CastAsInteger(b));
				out = DECIMAL_VAL((float)(a_d << b_d));
			}
			else {
				int a_d = AS_INTEGER(a);
				int b_d = AS_INTEGER(b);
				out = INTEGER_VAL(a_d << b_d);
			}

			break;
		}
		case OP_BITSHIFT_RIGHT: {
			if (IS_NOT_NUMBER(a) || IS_NOT_NUMBER(b)) {
				return preConstant;
			}

			if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
				int a_d = AS_INTEGER(Value::CastAsInteger(a));
				int b_d = AS_INTEGER(Value::CastAsInteger(b));
				out = DECIMAL_VAL((float)(a_d >> b_d));
			}
			else {
				int a_d = AS_INTEGER(a);
				int b_d = AS_INTEGER(b);
				out = INTEGER_VAL(a_d >> b_d);
			}

			break;
		}
		case OP_BW_OR: {
			if (IS_NOT_NUMBER(a) || IS_NOT_NUMBER(b)) {
				return preConstant;
			}

			if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
				int a_d = AS_INTEGER(Value::CastAsInteger(a));
				int b_d = AS_INTEGER(Value::CastAsInteger(b));
				out = DECIMAL_VAL((float)(a_d | b_d));
			}
			else {
				int a_d = AS_INTEGER(a);
				int b_d = AS_INTEGER(b);
				out = INTEGER_VAL(a_d | b_d);
			}

			break;
		}
		case OP_BW_AND: {
			if (IS_NOT_NUMBER(a) || IS_NOT_NUMBER(b)) {
				return preConstant;
			}

			if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
				int a_d = AS_INTEGER(Value::CastAsInteger(a));
				int b_d = AS_INTEGER(Value::CastAsInteger(b));
				out = DECIMAL_VAL((float)(a_d & b_d));
			}
			else {
				int a_d = AS_INTEGER(a);
				int b_d = AS_INTEGER(b);
				out = INTEGER_VAL(a_d & b_d);
			}

			break;
		}
		case OP_BW_XOR: {
			if (IS_NOT_NUMBER(a) || IS_NOT_NUMBER(b)) {
				return preConstant;
			}

			if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
				int a_d = AS_INTEGER(Value::CastAsInteger(a));
				int b_d = AS_INTEGER(Value::CastAsInteger(b));
				out = DECIMAL_VAL((float)(a_d ^ b_d));
			}
			else {
				int a_d = AS_INTEGER(a);
				int b_d = AS_INTEGER(b);
				out = INTEGER_VAL(a_d ^ b_d);
			}

			break;
		}
		// Equality and Comparison Operators
		case OP_EQUAL_NOT:
		case OP_EQUAL: {
			bool equal = Value::SortaEqual(a, b);
			if (opB == OP_EQUAL_NOT) {
				equal = !equal;
			}
			out = INTEGER_VAL(equal ? 1 : 0);
			break;
		}
		case OP_GREATER: {
			if (IS_NOT_NUMBER(a) || IS_NOT_NUMBER(b)) {
				return preConstant;
			}

			if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
				float a_d = AS_DECIMAL(Value::CastAsDecimal(a));
				float b_d = AS_DECIMAL(Value::CastAsDecimal(b));
				out = INTEGER_VAL(a_d > b_d);
			}
			else {
				int a_d = AS_INTEGER(a);
				int b_d = AS_INTEGER(b);
				out = INTEGER_VAL(a_d > b_d);
			}

			break;
		}
		case OP_GREATER_EQUAL: {
			if (IS_NOT_NUMBER(a) || IS_NOT_NUMBER(b)) {
				return preConstant;
			}

			if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
				float a_d = AS_DECIMAL(Value::CastAsDecimal(a));
				float b_d = AS_DECIMAL(Value::CastAsDecimal(b));
				out = INTEGER_VAL(a_d >= b_d);
			}
			else {
				int a_d = AS_INTEGER(a);
				int b_d = AS_INTEGER(b);
				out = INTEGER_VAL(a_d >= b_d);
			}

			break;
		}
		case OP_LESS: {
			if (IS_NOT_NUMBER(a) || IS_NOT_NUMBER(b)) {
				return preConstant;
			}

			if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
				float a_d = AS_DECIMAL(Value::CastAsDecimal(a));
				float b_d = AS_DECIMAL(Value::CastAsDecimal(b));
				out = INTEGER_VAL(a_d < b_d);
			}
			else {
				int a_d = AS_INTEGER(a);
				int b_d = AS_INTEGER(b);
				out = INTEGER_VAL(a_d < b_d);
			}

			break;
		}
		case OP_LESS_EQUAL: {
			if (IS_NOT_NUMBER(a) || IS_NOT_NUMBER(b)) {
				return preConstant;
			}

			if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
				float a_d = AS_DECIMAL(Value::CastAsDecimal(a));
				float b_d = AS_DECIMAL(Value::CastAsDecimal(b));
				out = INTEGER_VAL(a_d <= b_d);
			}
			else {
				int a_d = AS_INTEGER(a);
				int b_d = AS_INTEGER(b);
				out = INTEGER_VAL(a_d <= b_d);
			}

			break;
		}
		}

		CurrentChunk()->Count = preCount;
		if (checkConstantA >= preConstant) {
			CurrentChunk()->Constants->pop_back();
		}
		if (checkConstantB >= preConstant && checkConstantA != checkConstantB) {
			CurrentChunk()->Constants->pop_back();
		}

		preConstant = CurrentChunk()->Constants->size();
		EmitConstant(out);
		if (out.Type == VAL_INTEGER) {
			preConstant =
				CheckPrefixOptimize(preCount, preConstant, &Compiler::GetInteger);
		}
	}

	return preConstant;
}

void Compiler::AddBreakpoint() {
	if (Breakpoints.size() == 255) {
		Error("Cannot have more than 255 breakpoints.");
	}

	Breakpoints.push_back(CurrentChunk()->Count);
}

// Compiling
void Compiler::Init() {
	Compiler::MakeRules();

	Compiler::DoLogging = false;

	Settings.PrintToLog = true;
	Settings.PrintChunks = false;
	Settings.ShowWarnings = false;
#if DEVELOPER_MODE
	Settings.WriteDebugInfo = true;
	Settings.WriteSourceFilename = true;
#else
	Settings.WriteDebugInfo = false;
	Settings.WriteSourceFilename = false;
#endif
	Settings.DoOptimizations = true;

	Application::Settings->GetBool("compiler", "log", &Compiler::DoLogging);
	if (Compiler::DoLogging) {
		Application::Settings->GetBool("compiler", "showWarnings", &Settings.ShowWarnings);
	}

	Application::Settings->GetBool("compiler", "writeDebugInfo", &Settings.WriteDebugInfo);
	Application::Settings->GetBool(
		"compiler", "writeSourceFilename", &Settings.WriteSourceFilename);
	Application::Settings->GetBool("compiler", "optimizations", &Settings.DoOptimizations);

	Application::Settings->GetBool("dev", "debugCompiler", &Settings.PrintChunks);
}
void Compiler::GetStandardConstants() {
	if (Compiler::StandardConstants == NULL) {
		Compiler::StandardConstants =
			new HashMap<VMValue>(NULL, ScriptManager::Constants->Count());
	}
	Compiler::StandardConstants->Clear();

	ScriptManager::Constants->ForAll([](Uint32 hash, VMValue val) {
		if (IS_NUMBER(val) || OBJECT_TYPE(val) == OBJ_STRING) {
			Compiler::StandardConstants->Put(hash, val);
		}
	});
}
void Compiler::PrepareCompiling() {
	if (Compiler::TokenMap == NULL) {
		Compiler::TokenMap = new HashMap<Token>(NULL, 8);
	}
}
void Compiler::Initialize(Compiler* enclosing, int scope, int type) {
	Type = type;
	LocalCount = 0;
	ScopeDepth = scope;
	Function = NewFunction();
	UnusedVariables = new vector<Local>();
	UnsetVariables = new vector<Local>();
	Compiler::Functions.push_back(Function);

	switch (type) {
	case TYPE_CONSTRUCTOR:
	case TYPE_METHOD:
	case TYPE_FUNCTION:
		Function->Name = StringUtils::Create(parser.Previous.Start, parser.Previous.Length);
		break;
	case TYPE_TOP_LEVEL:
		Function->Name = StringUtils::Create("main");
		break;
	}

	Local* local = &Locals[LocalCount++];
	local->Depth = ScopeDepth;
	local->Index = LocalCount - 1;

	if (HasThis()) {
		// In a method, it holds the receiver, "this".
		SetReceiverName("this");
	}
	else {
		// In a function, it holds the function, but cannot be referenced, so it has no name.
		SetReceiverName("");
	}

	AllLocals.push_back(*local);
}
void Compiler::WriteBytecode(Stream* stream, const char* filename) {
	Bytecode* bytecode = new Bytecode();

	for (size_t i = 0; i < Compiler::Functions.size(); i++) {
		bytecode->Functions.push_back(Compiler::Functions[i]);
	}

	bytecode->HasDebugInfo = CurrentSettings.WriteDebugInfo;
	bytecode->SourceFilename = CurrentSettings.WriteSourceFilename ? (char*)filename : nullptr;
	bytecode->Write(stream, TokenMap);

	bytecode->SourceFilename = nullptr;

	delete bytecode;

	if (TokenMap) {
		TokenMap->Clear();
	}
}
bool Compiler::Compile(const char* filename, const char* source, CompilerSettings settings, Stream* output) {
	CurrentSettings = settings;

	scanner.Line = 1;
	scanner.Start = (char*)source;
	scanner.Current = (char*)source;
	scanner.LinePos = (char*)source;
	scanner.SourceFilename = (char*)filename;

	parser.HadError = false;
	parser.PanicMode = false;

	Initialize(NULL, 0, TYPE_TOP_LEVEL);

	try {
		AdvanceToken();
		while (!MatchToken(TOKEN_EOF)) {
			GetDeclaration();
		}

		ConsumeToken(TOKEN_EOF, "Expected end of file.");
	}
	catch (const CompilerErrorException& error) {
		Cleanup();
		DeleteFunctions();

		throw error;
	}

	if (Compiler::ModuleLocals.size() > 0) {
		Function->Chunk.ModuleLocals = new vector<ChunkLocal>;

		for (size_t i = 0; i < Compiler::ModuleLocals.size(); i++) {
			Local moduleLocal = Compiler::ModuleLocals[i];

			ChunkLocal local;
			Token nameToken = moduleLocal.Name;
			local.Name = StringUtils::Create(nameToken.ToString());
			local.Constant = moduleLocal.Constant;
			local.WasSet = moduleLocal.WasSet;
			local.Index = i;
			local.Position = (Uint32)((nameToken.Pos & 0xFFFF) << 16 | (nameToken.Line & 0xFFFF));
			Function->Chunk.ModuleLocals->push_back(local);

			if (UnusedVariables && !moduleLocal.Resolved) {
				UnusedVariables->insert(
					UnusedVariables->begin(), moduleLocal);
			}
			else if (UnsetVariables &&
				moduleLocal.ConstantVal.Type != VAL_ERROR &&
				!moduleLocal.WasSet) {
				UnsetVariables->insert(UnsetVariables->begin(), moduleLocal);
			}
		}
	}

	Finish();

	for (size_t c = 0; c < Compiler::Functions.size(); c++) {
		Chunk* chunk = &Compiler::Functions[c]->Chunk;
		chunk->OpcodeCount = 0;
		for (int offset = 0; offset < chunk->Count;) {
			offset += Bytecode::GetTotalOpcodeSize(chunk->Code + offset);
			chunk->OpcodeCount++;
		}
	}

	if (CurrentSettings.PrintChunks && Compiler::Functions.size() > 0) {
		BytecodeDebugger* debugger = new BytecodeDebugger;

		debugger->PrintToLog = CurrentSettings.PrintToLog;

		if (TokenMap && TokenMap->Count()) {
			HashMap<char*>* tokens = new HashMap<char*>(NULL, TokenMap->Count());

			TokenMap->WithAll([tokens](Uint32 hash, Token token) -> void {
				std::string asString = token.ToString();
				tokens->Put(hash, StringUtils::Create(asString.c_str()));
			});

			debugger->Tokens = tokens;
		}

		for (size_t c = 0; c < Compiler::Functions.size(); c++) {
			Chunk* chunk = &Compiler::Functions[c]->Chunk;

			debugger->DebugChunk(chunk,
				Compiler::Functions[c]->Name,
				Compiler::Functions[c]->MinArity,
				Compiler::Functions[c]->Arity);

			if (CurrentSettings.PrintToLog) {
				Log::PrintSimple("\n");
			}
			else {
				printf("\n");
			}
		}

		delete debugger;
	}

	if (output) {
		WriteBytecode(output, filename);
	}

	Cleanup();
	DeleteFunctions();

	return true;
}
bool Compiler::Compile(const char* filename, const char* source, Stream* output) {
	return Compile(filename, source, CurrentSettings, output);
}
void Compiler::Cleanup() {
	if (UnusedVariables) {
		delete UnusedVariables;
		UnusedVariables = nullptr;
	}
	if (UnsetVariables) {
		delete UnsetVariables;
		UnsetVariables = nullptr;
	}

	while (BreakJumpListStack.size() > 0) {
		delete BreakJumpListStack.top();
		BreakJumpListStack.pop();
		BreakScopeStack.pop();
	}
	while (ContinueJumpListStack.size() > 0) {
		delete ContinueJumpListStack.top();
		ContinueJumpListStack.pop();
		ContinueScopeStack.pop();
	}
	while (SwitchJumpListStack.size() > 0) {
		EndSwitchJumpList();
	}
}
void Compiler::DeleteFunctions() {
	for (size_t c = 0; c < Compiler::Functions.size(); c++) {
		ObjFunction* func = Compiler::Functions[c];
		func->Chunk.Free();
	}
}
void Compiler::Finish() {
	Chunk* chunk = CurrentChunk();

	// NOTE: Top level functions don't have locals.
	// See Compiler::Compile for the logic that handles module locals
	if (AllLocals.size()) {
		chunk->Locals = new vector<ChunkLocal>;

		for (int i = 0; i < AllLocals.size(); i++) {
			Local srcLocal = AllLocals[i];
			Token nameToken = srcLocal.Name;
			Uint32 position = 0;

			if (i >= Function->Arity + 1) {
				position = (Uint32)((nameToken.Pos & 0xFFFF) << 16 | (nameToken.Line & 0xFFFF));
			}

			ChunkLocal local;
			local.Name = StringUtils::Create(nameToken.ToString());
			local.Constant = srcLocal.Constant;
			local.WasSet = srcLocal.WasSet;
			local.Index = srcLocal.Index;
			local.Position = position;
			chunk->Locals->push_back(local);
		}
	}

	size_t numBreakpoints = Breakpoints.size();
	if (numBreakpoints) {
		Function->Chunk.Breakpoints = (Uint8*)Memory::Calloc(Function->Chunk.Count, sizeof(Uint8));
		for (size_t i = 0; i < numBreakpoints; i++) {
			Function->Chunk.Breakpoints[Breakpoints[i]] = 1;
		}
	}

	if (UnusedVariables || UnsetVariables) {
		WarnVariablesUnusedUnset();
		if (UnusedVariables) {
			delete UnusedVariables;
			UnusedVariables = nullptr;
		}
		if (UnsetVariables) {
			delete UnsetVariables;
			UnsetVariables = nullptr;
		}
	}

	EmitReturn();
}

Compiler::~Compiler() {}
void Compiler::FinishCompiling() {
	Compiler::Functions.clear();
	Compiler::ModuleLocals.clear();
	Compiler::ModuleConstants.clear();

	if (TokenMap) {
		delete TokenMap;
		TokenMap = NULL;
	}
}
void Compiler::Dispose() {
	if (StandardConstants) {
		delete StandardConstants;
		StandardConstants = NULL;
	}
	if (Rules) {
		Memory::Free(Rules);
		Rules = NULL;
	}
}
