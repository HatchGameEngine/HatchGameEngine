#include <Engine/Application.h>
#include <Engine/Bytecode/Bytecode.h>
#include <Engine/Bytecode/BytecodeDebugger.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/GarbageCollector.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Exceptions/CompilerErrorException.h>
#include <Engine/Hashing/CRC32.h>

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

Local VariableLocal;
Token VariableToken = Token{0, NULL, 0, 0, 0};
Token InstanceToken = Token{0, NULL, 0, 0, 0};

bool ShouldEmitValue = false;

std::unordered_map<std::string, std::function<bool(Compiler*, Uint8*, int)>> Intrinsics;

struct VariableWarning {
	int Line;
	std::string VariableName;
	bool IsUnused = false;
	bool IsUnset = false;
};

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
bool Compiler::IsDot(char c) {
	return c == '.';
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
	// If it starts with "0x", it's in hexadecimal notation.
	if (*scanner.Start == '0' && (PeekChar() == 'x' || PeekChar() == 'X')) {
		AdvanceChar(); // x
		while (IsHexDigit(PeekChar())) {
			AdvanceChar();
		}
		return MakeToken(TOKEN_NUMBER);
	}
	// If it starts with a dot, it's a decimal with an implicit integer part.
	else if (IsDot(*scanner.Start) && IsDigit(PeekChar())) {
		while (IsDigit(PeekChar())) {
			AdvanceChar();
		}
		return MakeToken(TOKEN_DECIMAL);
	}

	while (IsDigit(PeekChar())) {
		AdvanceChar();
	}

	// If there's a dot after the digits, it's a decimal.
	// The fractional part can be omitted.
	if (IsDot(PeekChar())) {
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

	if (IsDigit(c) || (IsDot(c) && IsDigit(PeekChar()))) {
		return NumberToken();
	}
	else if (IsIdentifierStart(c)) {
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
	while (true) {
		parser.Current = ScanToken();
		if (parser.Current.Type != TOKEN_ERROR) {
			break;
		}

		ErrorAtCurrent(parser.Current.Start);
	}
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
void Compiler::WarningAt(Token* token, const char* message) {
	ErrorAt(token, message, false);
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
void Compiler::MarkResolved() {
	if (ScopeDepth == 0) {
		return;
	}
	Locals[LocalCount - 1].Resolved = true;
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
	Constants.push_back({*name, VARTYPE_LOCAL, -1, ScopeDepth, false, false, true});
	return ((int)Constants.size()) - 1;
}
int Compiler::ParseModuleVariable(const char* errorMessage, bool constant) {
	ConsumeIdentifier(errorMessage);
	return DeclareModuleVariable(&parser.Previous, constant);
}
void Compiler::DefineModuleVariable(int local) {
	EmitOpcode(OP_DEFINE_MODULE_LOCAL);
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

	Compiler::ModuleConstants.push_back({*name, VARTYPE_MODULE_LOCAL, -1, 0, false, false, true});
	return ((int)Compiler::ModuleConstants.size()) - 1;
}
void Compiler::WarnVariablesUnusedUnset() {
	std::vector<VariableWarning> warningList;
	std::string message;
	char temp[4096];

	if (!CurrentSettings.ShowWarnings) {
		return;
	}

	for (size_t i = 0; i < UnusedVariables->size(); i++) {
		VariableWarning warning;
		Local& local = (*UnusedVariables)[i];
		warning.Line = local.Name.Line;
		warning.VariableName = local.Name.ToString();
		warning.IsUnused = true;
		warningList.push_back(warning);
	}

	for (size_t i = 0; i < UnsetVariables->size(); i++) {
		VariableWarning warning;
		Local& local = (*UnsetVariables)[i];
		warning.Line = local.Name.Line;
		warning.VariableName = local.Name.ToString();
		warning.IsUnset = true;
		warningList.push_back(warning);
	}

	if (warningList.size() == 0) {
		return;
	}

	std::sort(
		warningList.begin(), warningList.end(), [](VariableWarning& a, VariableWarning& b) -> bool {
			return a.Line < b.Line;
		});

	for (size_t i = 0; i < warningList.size(); i++) {
		VariableWarning& warning = warningList[i];

		if (warning.IsUnset) {
			snprintf(temp,
				sizeof(temp),
				"Variable '%s' can be const. (Declared on line %d)",
				warning.VariableName.c_str(),
				warning.Line);
		}
		else if (warning.IsUnused) {
			snprintf(temp,
				sizeof(temp),
				"Variable '%s' is unused. (Declared on line %d)",
				warning.VariableName.c_str(),
				warning.Line);
		}

		message += std::string(temp);
		if (i < warningList.size() - 1) {
			message += "\n    ";
		}
	}

	WarningInFunction("%s", message.c_str());
}

void Compiler::EmitSetOperation(Uint8 setOp, int arg, Token name) {
	switch (setOp) {
	case OP_SET_GLOBAL:
	case OP_SET_PROPERTY:
		EmitOpcode(setOp);
		EmitStringHash(name);
		break;
	case OP_SET_LOCAL:
		EmitOpcode(setOp, (Uint8)arg);
		break;
	case OP_SET_ELEMENT:
		EmitOpcode(setOp);
		break;
	case OP_SET_MODULE_LOCAL:
		EmitOpcode(setOp);
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
	case OP_LOCATION_PROPERTY:
	case OP_LOCATION_SUPER_PROPERTY:
	case OP_LOCATION_GLOBAL:
		EmitOpcode(getOp);
		EmitStringHash(name);
		break;
	case OP_GET_LOCAL:
	case OP_LOCATION_STACK:
		EmitOpcode(getOp, (Uint8)arg);
		break;
	case OP_GET_MODULE_LOCAL:
	case OP_LOCATION_MODULE_LOCAL:
		EmitOpcode(getOp);
		EmitUint16((Uint16)arg);
	default:
		break;
	}
}
void Compiler::EmitAssignmentToken(Token assignmentToken) {
	switch (assignmentToken.Type) {
	case TOKEN_ASSIGNMENT_PLUS:
		EmitOpcode(OP_ADD);
		break;
	case TOKEN_ASSIGNMENT_MINUS:
		EmitOpcode(OP_SUBTRACT);
		break;
	case TOKEN_ASSIGNMENT_MODULO:
		EmitOpcode(OP_MODULO);
		break;
	case TOKEN_ASSIGNMENT_DIVISION:
		EmitOpcode(OP_DIVIDE);
		break;
	case TOKEN_ASSIGNMENT_MULTIPLY:
		EmitOpcode(OP_MULTIPLY);
		break;
	case TOKEN_ASSIGNMENT_BITWISE_OR:
		EmitOpcode(OP_BW_OR);
		break;
	case TOKEN_ASSIGNMENT_BITWISE_AND:
		EmitOpcode(OP_BW_AND);
		break;
	case TOKEN_ASSIGNMENT_BITWISE_XOR:
		EmitOpcode(OP_BW_XOR);
		break;
	case TOKEN_ASSIGNMENT_BITWISE_LEFT:
		EmitOpcode(OP_BITSHIFT_LEFT);
		break;
	case TOKEN_ASSIGNMENT_BITWISE_RIGHT:
		EmitOpcode(OP_BITSHIFT_RIGHT);
		break;
	case TOKEN_INCREMENT:
		EmitOpcode(OP_INCREMENT);
		break;
	case TOKEN_DECREMENT:
		EmitOpcode(OP_DECREMENT);
		break;
	default:
		break;
	}
}
void Compiler::EmitCopy(Uint8 count) {
	EmitOpcode(OP_COPY, count);
}
void Compiler::EmitCopyAndLoadIndirect() {
	Uint8* op = GetLastOpcodePtr(CurrentChunk(), 0);
	if (*op == OP_LOCATION_PROPERTY || *op == OP_LOCATION_SUPER_PROPERTY) {
		EmitCopy(2);
	}
	else if (*op == OP_LOCATION_ELEMENT) {
		EmitCopy(3);
	}
	else {
		EmitCopy(1);
	}
	EmitOpcode(OP_LOAD_INDIRECT);
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
	EmitOpcode(isSuper ? OP_SUPER_INVOKE : OP_INVOKE);
	EmitByte(argCount);
}

void Compiler::ConvertLvalueToRvalue() {
	Chunk* chunk = CurrentChunk();
	Uint8* op = GetLastOpcodePtr(chunk, 0);
	if (*op == OP_LOCATION_PROPERTY) {
		if (!MakeIndirectPropertyChainDirect(chunk, op, 0)) {
			EmitOpcode(OP_LOAD_INDIRECT);
		}
	}
	else if (*op == OP_LOCATION_SUPER_PROPERTY) {
		Uint8* last = GetLastOpcodePtr(chunk, 1);
		Uint8 direct = IndirectLoadOpcodeToDirect(*last);
		if (direct == OP_NOP) {
			EmitOpcode(OP_LOAD_INDIRECT);
			return;
		}

		Uint32 hash = *(Uint32*)(op + 1);
		chunk->Count = (last - chunk->Code) + Bytecode::GetTotalOpcodeSize(last);
		*last = direct;
		EmitOpcode(OP_GET_SUPERCLASS);
		EmitOpcode(OP_GET_PROPERTY);
		EmitUint32(hash);
	}
	else {
		Uint8 direct = IndirectLoadOpcodeToDirect(*op);
		if (direct == OP_NOP) {
			EmitOpcode(OP_LOAD_INDIRECT);
		}
		else {
			*op = direct;
		}
	}
}
bool Compiler::MakeIndirectPropertyChainDirect(Chunk* chunk, Uint8* op, int index) {
	std::vector<size_t> propOp;
	propOp.push_back(op - chunk->Code);

	index++;

	while (true) {
		Uint8* ptr = GetLastOpcodePtr(chunk, index);
		index++;

		if (!ptr) {
			return false;
		}

		if (*ptr == OP_LOCATION_SUPER_PROPERTY) {
			Uint8* last = GetLastOpcodePtr(chunk, index);
			if (!last) {
				return false;
			}

			Uint8 direct = IndirectLoadOpcodeToDirect(*last);
			if (direct != OP_GET_GLOBAL &&
				direct != OP_GET_LOCAL &&
				direct != OP_GET_MODULE_LOCAL) {
				return false;
			}

			Uint8* code_block_copy = NULL;
			int* line_block_copy = NULL;
			int code_block_start = ptr - chunk->Code;
			int code_block_length = chunk->Count - code_block_start;

			// Copy code block
			code_block_copy = (Uint8*)malloc(code_block_length * sizeof(Uint8));
			memcpy(code_block_copy, &chunk->Code[code_block_start], code_block_length * sizeof(Uint8));
			code_block_copy[0] = OP_GET_PROPERTY;

			// Copy line info block
			line_block_copy = (int*)malloc(code_block_length * sizeof(int));
			memcpy(line_block_copy, &chunk->Lines[code_block_start], code_block_length * sizeof(int));

			*last = direct;

			chunk->Count -= code_block_length;
			EmitOpcode(OP_GET_SUPERCLASS);
			for (int i = 0; i < code_block_length; i++) {
				chunk->Write(code_block_copy[i], line_block_copy[i]);
			}
			free(code_block_copy);
			free(line_block_copy);

			for (size_t i = 0; i < propOp.size(); i++) {
				propOp[i]++;
			}

			break;
		}
		else {
			Uint8 direct = IndirectLoadOpcodeToDirect(*ptr);
			if (direct == OP_GET_PROPERTY) {
				propOp.push_back(ptr - chunk->Code);
			}
			else if (direct == OP_NOP) {
				return false;
			}
			else {
				*ptr = direct;
				break;
			}
		}
	}

	for (size_t i = 0; i < propOp.size(); i++) {
		chunk->Code[propOp[i]] = OP_GET_PROPERTY;
	}

	return true;
}
Uint8 Compiler::IndirectLoadOpcodeToDirect(Uint8 op) {
	if (op == OP_LOCATION_GLOBAL) {
		return OP_GET_GLOBAL;
	}
	else if (op == OP_LOCATION_STACK) {
		return OP_GET_LOCAL;
	}
	else if (op == OP_LOCATION_MODULE_LOCAL) {
		return OP_GET_MODULE_LOCAL;
	}
	else if (op == OP_LOCATION_PROPERTY) {
		return OP_GET_PROPERTY;
	}

	return OP_NOP;
}

ExprContext Compiler::NamedVariable(Token name, Local& currentLocal, ExprContext context) {
	bool emitValue = ShouldEmitValue || context == EXPRCONTEXT_VALUE;
	Uint8 getOp;

	VariableToken = name;

	Local local;
	int arg = ResolveLocal(&name, &local);

	// Determine whether local or global
	if (arg != -1) {
		if (emitValue) {
			getOp = OP_GET_LOCAL;
		}
		else {
			getOp = OP_LOCATION_STACK;
		}

		if (currentLocal.Type == VARTYPE_UNKNOWN) {
			currentLocal = local;
			currentLocal.Type = VARTYPE_LOCAL;
			currentLocal.Index = arg;
		}
	}
	else {
		arg = ResolveModuleLocal(&name, &local);
		VMValue value;
		if (arg != -1) {
			if (emitValue) {
				getOp = OP_GET_MODULE_LOCAL;
			}
			else {
				getOp = OP_LOCATION_MODULE_LOCAL;
			}

			if (currentLocal.Type == VARTYPE_UNKNOWN) {
				currentLocal = local;
				currentLocal.Type = VARTYPE_MODULE_LOCAL;
				currentLocal.Index = arg;
			}
		}
		else if (emitValue && StandardConstants->GetIfExists(name.ToString().c_str(), &value)) {
			EmitConstant(value);
			return EXPRCONTEXT_VALUE;
		}
		else if (emitValue) {
			getOp = OP_GET_GLOBAL;
		}
		else {
			getOp = OP_LOCATION_GLOBAL;
		}
	}

	if (currentLocal.Type == VARTYPE_UNKNOWN) {
		currentLocal = local;
		currentLocal.Type = VARTYPE_GLOBAL;
		currentLocal.Index = -1;
	}

	if (local.Constant && local.ConstantVal.Type != VAL_ERROR) {
		EmitConstant(local.ConstantVal);
		return EXPRCONTEXT_VALUE;
	}
	else {
		EmitGetOperation(getOp, arg, name);
	}

	return emitValue ? EXPRCONTEXT_VALUE : EXPRCONTEXT_LOCATION;
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
		CheckLocalUnusedOrUnset(Locals[LocalCount - 1]);

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
		EmitOpcode(OP_POP);
		return;
	}

	while (count > 0) {
		int max = count;
		if (max > 0xFF) {
			max = 0xFF;
		}
		EmitOpcode(OP_POPN, max);
		count -= max;
	}
}
void Compiler::CheckLocalUnusedOrUnset(Local& local) {
	if (!local.Resolved) {
		UnusedVariables->push_back(local);
	}
	else if (local.ConstantVal.Type != VAL_ERROR && !local.WasSet) {
		UnsetVariables->push_back(local);
	}
}
int Compiler::AddLocal() {
	if (LocalCount == 0xFF) {
		Error("Too many local variables in function.");
		return -1;
	}
	if (LocalCount < 0) {
		LocalCount = 0;
	}
	Local* local = &Locals[LocalCount++];
	local->Depth = -1;
	local->Index = LocalCount - 1;
	local->Resolved = false;
	local->WasSet = false;
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
void Compiler::MarkLocalAsSet(Local& local) {
	if (local.Type == VARTYPE_LOCAL) {
		Locals[local.Index].WasSet = true;
	}
	else if (local.Type == VARTYPE_MODULE_LOCAL) {
		ModuleLocals[local.Index].WasSet = true;
	}
}
int Compiler::AddModuleLocal() {
	if (Compiler::ModuleLocals.size() == 0xFFFF) {
		Error("Too many locals in module.");
		return -1;
	}
	Local local;
	local.Depth = -1;
	local.Index = -1;
	local.Resolved = false;
	local.Constant = false;
	local.ConstantVal = VMValue{VAL_ERROR};
	Compiler::ModuleLocals.push_back(local);
	return (int)Compiler::ModuleLocals.size() - 1;
}
int Compiler::AddModuleLocal(Token name) {
	int index = AddModuleLocal();
	Compiler::ModuleLocals[index].Name = name;
	return index;
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

	ConsumeToken(TOKEN_RIGHT_PAREN, "Expected ')' after arguments.");
	return argumentCount;
}
std::function<bool(Compiler*, Uint8*, int)> Compiler::DetectInstrinsic(Token name) {
	std::vector<Token> tokens;
	Uint8* start = nullptr;

	for (int i = (int)EmittedOpcodes.size() - 1; i >= 0; i--) {
		Uint8* opcode = EmittedOpcodes[i];
		if (*opcode == OP_GET_GLOBAL || *opcode == OP_GET_PROPERTY) {
			Uint32 hash = *(Uint32*)(opcode + 1);
			tokens.insert(tokens.begin(), TokenMap->Get(hash));
			if (*opcode == OP_GET_GLOBAL) {
				start = opcode;
				break;
			}
		}
		else {
			return nullptr;
		}
	}

	if (tokens.size() == 0) {
		return nullptr;
	}

	std::string instrinsic;
	for (size_t i = 0; i < tokens.size(); i++) {
		instrinsic += tokens[i].ToString();
		if (i < tokens.size() - 1) {
			instrinsic += ".";
		}
	}
	instrinsic += "." + name.ToString();

	if (Intrinsics.count(instrinsic)) {
		CurrentChunk()->Count = start - CurrentChunk()->Code;
		return Intrinsics[instrinsic];
	}

	return nullptr;
}

#define CHECK_LVALUE() \
	if (context != EXPRCONTEXT_LOCATION) { \
		Error("Not an lvalue."); \
	}
#define CHECK_RVALUE() \
	if (context != EXPRCONTEXT_VALUE) { \
		Error("Not an rvalue."); \
	}
#define CONVERT_LVALUE_TO_RVALUE() \
	if (context == EXPRCONTEXT_LOCATION) { \
		ConvertLvalueToRvalue(); \
	}

ExprContext Compiler::GetThis(ExprContext context) {
	InstanceToken = parser.Previous;
	VariableLocal = Locals[0];
	return NamedVariable(parser.Previous, VariableLocal, EXPRCONTEXT_LOCATION);
}
ExprContext Compiler::GetSuper(ExprContext context) {
	InstanceToken = parser.Previous;
	VariableLocal = Locals[0];
	if (!CheckToken(TOKEN_DOT)) {
		Error("Expect '.' after 'super'.");
	}

	if (ShouldEmitValue) {
		EmitOpcode(OP_GET_LOCAL, 0);
		return EXPRCONTEXT_VALUE;
	}
	else {
		EmitOpcode(OP_LOCATION_STACK, 0);
		return EXPRCONTEXT_LOCATION;
	}
}
ExprContext Compiler::GetDot(ExprContext context) {
	bool isSuper = InstanceToken.Type == TOKEN_SUPER;
	InstanceToken.Type = -1;

	ConsumeIdentifier("Expect property name after '.'.");
	Token nameToken = parser.Previous;

	if (MatchToken(TOKEN_LEFT_PAREN)) {
		CONVERT_LVALUE_TO_RVALUE();

		Chunk* chunk = CurrentChunk();
		int pre = chunk->Count;

		std::function<bool(Compiler*, Uint8*, int)> intrinsic = nullptr;

		if (CurrentSettings.UseIntrinsics) {
			intrinsic = DetectInstrinsic(nameToken);
		}

		if (intrinsic) {
			// DetectInstrinsic deletes code, so chunk->Count < pre
			int code_block_start = chunk->Count;
			int code_block_length = pre - code_block_start;

			// Copy code block
			Uint8* code_block_copy = (Uint8*)malloc(code_block_length * sizeof(Uint8));
			memcpy(code_block_copy, &chunk->Code[code_block_start], code_block_length * sizeof(Uint8));

			// Copy line info block
			int* line_block_copy = (int*)malloc(code_block_length * sizeof(int));
			memcpy(line_block_copy, &chunk->Lines[code_block_start], code_block_length * sizeof(int));

			uint8_t argCount = GetArgumentList();
			int arg_code_block_length = chunk->Count - code_block_start;

			bool didUseIntrinsic = intrinsic(this, chunk->Code + code_block_start, argCount);
			if (!didUseIntrinsic) {
				// Copy code block
				Uint8* arg_code_block_copy = (Uint8*)malloc(arg_code_block_length * sizeof(Uint8));
				memcpy(arg_code_block_copy, &chunk->Code[code_block_start], arg_code_block_length * sizeof(Uint8));

				// Copy line info block
				int* arg_line_block_copy = (int*)malloc(arg_code_block_length * sizeof(int));
				memcpy(arg_line_block_copy, &chunk->Lines[code_block_start], arg_code_block_length * sizeof(int));

				chunk->Count = code_block_start;

				for (int i = 0; i < code_block_length; i++) {
					chunk->Write(code_block_copy[i], line_block_copy[i]);
				}
				for (int i = 0; i < arg_code_block_length; i++) {
					chunk->Write(arg_code_block_copy[i], arg_line_block_copy[i]);
				}

				free(arg_code_block_copy);
				free(arg_line_block_copy);

				EmitCall(nameToken, argCount, isSuper);
			}

			free(code_block_copy);
			free(line_block_copy);
		}
		else {
			uint8_t argCount = GetArgumentList();
			EmitCall(nameToken, argCount, isSuper);
		}

		return EXPRCONTEXT_VALUE;
	}

	if (ShouldEmitValue) {
		if (isSuper) {
			EmitOpcode(OP_GET_SUPERCLASS);
		}
		EmitGetOperation(OP_GET_PROPERTY, -1, nameToken);
		return EXPRCONTEXT_VALUE;
	}
	else {
		Uint8 opcode = isSuper ? OP_LOCATION_SUPER_PROPERTY : OP_LOCATION_PROPERTY;
		EmitGetOperation(opcode, -1, nameToken);
		return EXPRCONTEXT_LOCATION;
	}
}
ExprContext Compiler::GetElement(ExprContext context) {
	Token blank;
	memset(&blank, 0, sizeof(blank));
	GetExpression();
	ConsumeToken(TOKEN_RIGHT_SQUARE_BRACE, "Expected matching ']'.");

	if (ShouldEmitValue) {
		EmitOpcode(OP_GET_ELEMENT);
		return EXPRCONTEXT_VALUE;
	}
	else {
		EmitOpcode(OP_LOCATION_ELEMENT);
		return EXPRCONTEXT_LOCATION;
	}
}

// Reading expressions
bool negateConstant = false;
ExprContext Compiler::GetGrouping(ExprContext context) {
	ExprContext result = GetExpression();
	ConsumeToken(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
	return result;
}
ExprContext Compiler::GetLiteral(ExprContext context) {
	switch (parser.Previous.Type) {
	case TOKEN_NULL:
		EmitOpcode(OP_NULL);
		break;
	case TOKEN_TRUE:
		EmitOpcode(OP_TRUE);
		break;
	case TOKEN_FALSE:
		EmitOpcode(OP_FALSE);
		break;
	default:
		break;
	}

	return EXPRCONTEXT_VALUE;
}
ExprContext Compiler::GetInteger(ExprContext context) {
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

	return EXPRCONTEXT_VALUE;
}
ExprContext Compiler::GetDecimal(ExprContext context) {
	float value = (float)atof(parser.Previous.Start);

	if (negateConstant) {
		value = -value;
	}
	negateConstant = false;

	EmitConstant(DECIMAL_VAL(value));

	return EXPRCONTEXT_VALUE;
}
int Compiler::ParseHexChars(Uint32* codepoint, char* src, char* srcEnd, int maxChars) {
	int count = 0;

	do {
		int chr = *src++;
		if (!IsHexDigit(chr)) {
			break;
		}

		*codepoint <<= 4;

		if (chr <= '9') {
			*codepoint |= chr - '0';
		}
		else {
			*codepoint |= (tolower(chr) - 'a') + 10;
		}

		count++;
		if (count == maxChars) {
			break;
		}
	}
	while (src < srcEnd);

	return count;
}
std::string Compiler::ParseUnicodeString(char* src, char* srcEnd, int maxChars) {
	Uint32 codepoint = 0;

	int count = Compiler::ParseHexChars(&codepoint, src, srcEnd, maxChars);
	if (count != maxChars) {
		Error("Invalid Unicode escape sequence.");
	}

	std::string result;
	try {
		result = StringUtils::FromCodepoint(codepoint);
	} catch (const std::runtime_error& error) {
		Error(error.what());
	}

	return result;
}
ObjString* Compiler::MakeString(Token token) {
	ObjString* string = CopyString(token.Start + 1, token.Length - 2);

	// Escape the string
	char* dst = string->Chars;
	char* srcEnd = token.Start + token.Length - 1;
	string->Length = 0;

	for (char* src = token.Start + 1; src < srcEnd; src++) {
		if (*src == '\\') {
			src++;
			switch (*src) {
			case 'n':
				*dst++ = '\n';
				string->Length++;
				break;
			case '"':
				*dst++ = '"';
				string->Length++;
				break;
			case '\'':
				*dst++ = '\'';
				string->Length++;
				break;
			case '\\':
				*dst++ = '\\';
				string->Length++;
				break;
			case 'x': {
				Uint32 digits = 0;

				int count = Compiler::ParseHexChars(&digits, src + 1, srcEnd, 2);
				if (count != 2) {
					Error("Invalid escape sequence.");
				}

				*dst++ = digits & 0xFF;
				src += 2;

				string->Length++;
				break;
			}
			case 'u': {
				std::string result = ParseUnicodeString(src + 1, srcEnd, 4);

				memcpy(dst, result.c_str(), result.size());
				dst += result.size();
				src += 4;

				string->Length += result.size();
				break;
			}
			case 'U': {
				std::string result = ParseUnicodeString(src + 1, srcEnd, 8);

				memcpy(dst, result.c_str(), result.size());
				dst += result.size();
				src += 8;

				string->Length += result.size();
				break;
			}
			default:
				Error("Unknown escape character.");
				break;
			}
		}
		else {
			*dst++ = *src;
			string->Length++;
		}
	}
	*dst++ = 0;

	return string;
}

ExprContext Compiler::GetString(ExprContext context) {
	ObjString* string = Compiler::MakeString(parser.Previous);
	EmitConstant(OBJECT_VAL(string));
	return EXPRCONTEXT_VALUE;
}
ExprContext Compiler::GetArray(ExprContext context) {
	Uint32 count = 0;

	while (!MatchToken(TOKEN_RIGHT_SQUARE_BRACE)) {
		GetExpression();
		count++;

		if (!MatchToken(TOKEN_COMMA)) {
			ConsumeToken(TOKEN_RIGHT_SQUARE_BRACE, "Expected ']' at end of array.");
			break;
		}
	}

	EmitOpcode(OP_NEW_ARRAY);
	EmitUint32(count);

	return EXPRCONTEXT_VALUE;
}
ExprContext Compiler::GetMap(ExprContext context) {
	Uint32 count = 0;

	while (!MatchToken(TOKEN_RIGHT_BRACE)) {
		ConsumeToken(TOKEN_STRING, "Expected string for map key.");
		GetString(EXPRCONTEXT_VALUE);

		ConsumeToken(TOKEN_COLON, "Expected ':' after key string.");
		GetExpression();
		count++;

		if (!MatchToken(TOKEN_COMMA)) {
			ConsumeToken(TOKEN_RIGHT_BRACE, "Expected '}' after map.");
			break;
		}
	}

	EmitOpcode(OP_NEW_MAP);
	EmitUint32(count);

	return EXPRCONTEXT_VALUE;
}
ExprContext Compiler::GetVariable(ExprContext context) {
	return NamedVariable(parser.Previous, VariableLocal, EXPRCONTEXT_LOCATION);
}
ExprContext Compiler::GetLogicalAND(ExprContext context) {
	CONVERT_LVALUE_TO_RVALUE();

	int endJump = EmitJump(OP_JUMP_IF_FALSE);

	EmitOpcode(OP_POP);
	ParsePrecedence(PREC_AND);

	PatchJump(endJump);

	return EXPRCONTEXT_VALUE;
}
ExprContext Compiler::GetLogicalOR(ExprContext context) {
	CONVERT_LVALUE_TO_RVALUE();

	int elseJump = EmitJump(OP_JUMP_IF_FALSE);
	int endJump = EmitJump(OP_JUMP);

	PatchJump(elseJump);
	EmitOpcode(OP_POP);

	ParsePrecedence(PREC_OR);
	PatchJump(endJump);

	return EXPRCONTEXT_VALUE;
}
ExprContext Compiler::GetConditional(ExprContext context) {
	CONVERT_LVALUE_TO_RVALUE();

	int thenJump = EmitJump(OP_JUMP_IF_FALSE);
	EmitOpcode(OP_POP);
	ParsePrecedence(PREC_TERNARY);

	int elseJump = EmitJump(OP_JUMP);
	ConsumeToken(TOKEN_COLON, "Expected ':' after conditional condition.");

	PatchJump(thenJump);
	EmitOpcode(OP_POP);
	ParsePrecedence(PREC_TERNARY);
	PatchJump(elseJump);

	return EXPRCONTEXT_VALUE;
}
ExprContext Compiler::GetUnary(ExprContext context) {
	Token previousToken = parser.Previous;

	context = ParsePrecedence(PREC_UNARY, EXPRCONTEXT_LOCATION);

	switch (previousToken.Type) {
	case TOKEN_MINUS:
		CONVERT_LVALUE_TO_RVALUE();
		EmitOpcode(OP_NEGATE);
		break;
	case TOKEN_BITWISE_NOT:
		CONVERT_LVALUE_TO_RVALUE();
		EmitOpcode(OP_BW_NOT);
		break;
	case TOKEN_LOGICAL_NOT:
		CONVERT_LVALUE_TO_RVALUE();
		EmitOpcode(OP_LG_NOT);
		break;
	case TOKEN_TYPEOF:
		CONVERT_LVALUE_TO_RVALUE();
		EmitOpcode(OP_TYPEOF);
		break;
	case TOKEN_INCREMENT:
		CHECK_LVALUE();
		MarkLocalAsSet(VariableLocal);
		EmitCopyAndLoadIndirect();
		EmitOpcode(OP_INCREMENT);
		EmitOpcode(OP_STORE_INDIRECT);
		break;
	case TOKEN_DECREMENT:
		CHECK_LVALUE();
		MarkLocalAsSet(VariableLocal);
		EmitCopyAndLoadIndirect();
		EmitOpcode(OP_DECREMENT);
		EmitOpcode(OP_STORE_INDIRECT);
		break;
	}

	ResetVariableLocal();

	return EXPRCONTEXT_VALUE;
}
ExprContext Compiler::GetNew(ExprContext context) {
	CHECK_RVALUE();

	Local local;

	ConsumeIdentifier("Expect class name.");
	NamedVariable(parser.Previous, local, EXPRCONTEXT_VALUE);

	uint8_t argCount = 0;
	if (MatchToken(TOKEN_LEFT_PAREN)) {
		argCount = GetArgumentList();
	}
	EmitOpcode(OP_NEW, argCount);

	return EXPRCONTEXT_VALUE;
}
ExprContext Compiler::GetHitbox(ExprContext context) {
	if (!MatchToken(TOKEN_LEFT_BRACE)) {
		return GetVariable(EXPRCONTEXT_LOCATION);
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
		return EXPRCONTEXT_VALUE;
	}
	else if (count != 4) {
		Error("Must construct hitbox with exactly four values.");
	}

	if (allConstants) {
		CurrentChunk()->Count = codePointer;
		EmitConstant(HITBOX_VAL(values.data()));
		return EXPRCONTEXT_VALUE;
	}

	EmitOpcode(OP_NEW_HITBOX);

	return EXPRCONTEXT_VALUE;
}
ExprContext Compiler::GetBinary(ExprContext context) {
	CONVERT_LVALUE_TO_RVALUE();

	Token operato = parser.Previous;
	int operatorType = operato.Type;

	ParseRule* rule = GetRule(operatorType);
	ParsePrecedence((Precedence)(rule->Precedence + 1));

	switch (operatorType) {
		// Numeric Operations
	case TOKEN_PLUS:
		EmitOpcode(OP_ADD);
		break;
	case TOKEN_MINUS:
		EmitOpcode(OP_SUBTRACT);
		break;
	case TOKEN_MULTIPLY:
		EmitOpcode(OP_MULTIPLY);
		break;
	case TOKEN_DIVISION:
		EmitOpcode(OP_DIVIDE);
		break;
	case TOKEN_MODULO:
		EmitOpcode(OP_MODULO);
		break;
		// Bitwise Operations
	case TOKEN_BITWISE_LEFT:
		EmitOpcode(OP_BITSHIFT_LEFT);
		break;
	case TOKEN_BITWISE_RIGHT:
		EmitOpcode(OP_BITSHIFT_RIGHT);
		break;
	case TOKEN_BITWISE_OR:
		EmitOpcode(OP_BW_OR);
		break;
	case TOKEN_BITWISE_AND:
		EmitOpcode(OP_BW_AND);
		break;
	case TOKEN_BITWISE_XOR:
		EmitOpcode(OP_BW_XOR);
		break;
		// Logical Operations
	case TOKEN_LOGICAL_AND:
		EmitOpcode(OP_LG_AND);
		break;
	case TOKEN_LOGICAL_OR:
		EmitOpcode(OP_LG_OR);
		break;
		// Equality and Comparison Operators
	case TOKEN_NOT_EQUALS:
		EmitOpcode(OP_EQUAL_NOT);
		break;
	case TOKEN_EQUALS:
		EmitOpcode(OP_EQUAL);
		break;
	case TOKEN_GREATER:
		EmitOpcode(OP_GREATER);
		break;
	case TOKEN_GREATER_EQUAL:
		EmitOpcode(OP_GREATER_EQUAL);
		break;
	case TOKEN_LESS:
		EmitOpcode(OP_LESS);
		break;
	case TOKEN_LESS_EQUAL:
		EmitOpcode(OP_LESS_EQUAL);
		break;
	default:
		ErrorAt(&operato, "Unknown binary operator.", true);
		break;
	}

	return EXPRCONTEXT_VALUE;
}
ExprContext Compiler::GetAssignment(ExprContext context) {
	if (VariableLocal.Constant) {
		ErrorAt(&VariableToken, "Attempted to assign to constant!", true);
	}

	CHECK_LVALUE();

	MarkLocalAsSet(VariableLocal);

	Token assignmentToken = parser.Previous;
	if (assignmentToken.Type != TOKEN_ASSIGNMENT) {
		EmitCopyAndLoadIndirect();
	}

	bool incOrDec = false;

	if (assignmentToken.Type == TOKEN_INCREMENT || assignmentToken.Type == TOKEN_DECREMENT) {
		EmitCopy(1);
		EmitOpcode(OP_SAVE_VALUE);
		incOrDec = true;
	}
	else {
		GetExpression();
	}

	EmitAssignmentToken(assignmentToken);
	EmitOpcode(OP_STORE_INDIRECT);

	if (incOrDec) {
		EmitOpcode(OP_POP);
		EmitOpcode(OP_LOAD_VALUE);
	}

	return EXPRCONTEXT_VALUE;
}
ExprContext Compiler::GetHas(ExprContext context) {
	CONVERT_LVALUE_TO_RVALUE();
	ConsumeIdentifier("Expect property name.");
	EmitOpcode(OP_HAS_PROPERTY);
	EmitStringHash(parser.Previous);
	return EXPRCONTEXT_VALUE;
}
ExprContext Compiler::GetCall(ExprContext context) {
	CONVERT_LVALUE_TO_RVALUE();
	Uint8 argCount = GetArgumentList();
	EmitOpcode(OP_CALL);
	EmitByte(argCount);
	return EXPRCONTEXT_VALUE;
}
ExprContext Compiler::GetExpression() {
	ExprContext context = ParsePrecedence(PREC_ASSIGNMENT, EXPRCONTEXT_VALUE);

	ResetVariableLocal();

	return context;
}
void Compiler::GetValueExpression() {
	ShouldEmitValue = true;
	GetExpression();
	ShouldEmitValue = false;
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
stack<vector<Uint32>*> BreakpointListStack;
stack<int> BreakScopeStack;
stack<int> ContinueScopeStack;
stack<int> SwitchScopeStack;
void Compiler::GetPrintStatement() {
	GetExpression();
	ConsumeToken(TOKEN_SEMICOLON, "Expected ';' after value.");
	EmitOpcode(OP_PRINT);
}
void Compiler::GetBreakpointStatement() {
	Token previousToken = parser.Previous;
	ConsumeToken(TOKEN_SEMICOLON, "Expected ';' after 'breakpoint'.");
	AddBreakpoint(previousToken);
}
void Compiler::GetExpressionStatement() {
	GetExpression();

	if (InREPL && MatchToken(TOKEN_EOF)) {
		EmitNullOnReturn = false;
		return;
	}

	EmitOpcode(OP_POP);

	ConsumeToken(TOKEN_SEMICOLON, "Expected ';' after expression.");
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
	EmitOpcode(OP_POP);

	// After block, return to evaluation of while expression.
	EmitLoop(loopStart);

	// Set the exit jump to this point
	PatchJump(exitJump);

	// Pop value since OP_JUMP_IF_FALSE doesn't pop off expression
	// value
	EmitOpcode(OP_POP);

	// Pop jump list off break stack, patch all breaks to this code
	// point
	EndBreakJumpList();
}
void Compiler::GetReturnStatement() {
	if (Type == FUNCTIONTYPE_TOPLEVEL) {
		Error("Cannot return from top-level code.");
	}

	if (MatchToken(TOKEN_SEMICOLON)) {
		EmitReturn();
	}
	else {
		if (Type == FUNCTIONTYPE_CONSTRUCTOR) {
			Error("Cannot return a value from an initializer.");
		}

		GetExpression();
		ConsumeToken(TOKEN_SEMICOLON, "Expect ';' after return value.");
		EmitOpcode(OP_RETURN);
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
	EmitOpcode(OP_INCREMENT); // increment remaining as we're about
	// to decrement it, so we can cheat
	// continue

	if (variableToken.Type != TOKEN_ERROR) {
		EmitConstant(INTEGER_VAL(-1));
		AddLocal(variableToken);
		MarkInitialized();
		// trick the compiler into ensuring it doesn't get modified
		Locals[LocalCount - 1].Constant = true;
	}

	int loopStart = CurrentChunk()->Count;

	if (variableToken.Type != TOKEN_ERROR) {
		// decrement it and set it back, but keep it on the
		// stack
		EmitOpcode(OP_GET_LOCAL, remaining);
		EmitOpcode(OP_DECREMENT);
	}
	else {
		EmitOpcode(OP_DECREMENT);
	}

	StartBreakJumpList();
	StartContinueJumpList();

	int exitJump = EmitJump(OP_JUMP_IF_FALSE);

	if (variableToken.Type != TOKEN_ERROR) {
		// save, pop the decrement off the stack, and increment
		// our int
		EmitOpcode(OP_SET_LOCAL, remaining);
		EmitOpcode(OP_POP);
		EmitOpcode(OP_INCREMENT);
	}

	// Repeat Code Body
	GetStatement();

	EndContinueJumpList();

	EmitLoop(loopStart);

	PatchJump(exitJump);

	// if we jumped from ending we have a loose end we have to
	// remove
	if (variableToken.Type != TOKEN_ERROR) {
		EmitOpcode(OP_POP);
	}

	EndBreakJumpList();

	ScopeEnd();
}
void Compiler::GetSwitchStatement() {
	Chunk* chunk = CurrentChunk();

	StartBreakJumpList();
	StartContinueJumpList();
	StartBreakpointList();

	// Evaluate the condition
	ConsumeToken(TOKEN_LEFT_PAREN, "Expect '(' after 'switch'.");
	GetExpression();
	ConsumeToken(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	ConsumeToken(TOKEN_LEFT_BRACE, "Expected '{' before statements.");

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

		EmitOpcode(OP_EQUAL);
		int jumpToPatch = EmitJump(OP_JUMP_IF_FALSE);

		PopMultiple(2);

		case_info.JumpPosition = EmitJump(OP_JUMP);

		PatchJump(jumpToPatch);

		EmitOpcode(OP_POP);
	}

	EmitOpcode(OP_POP);

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

	// Set the old breakpoint positions to the newly placed ones and pop list off breakpoint stack
	EndBreakpointList(code_offset);

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

	ConsumeToken(TOKEN_COLON, "Expected ':' after 'case'.");

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

	// Check if there already is a default clause, and prevent compilation if so.
	vector<switch_case>* top = SwitchJumpListStack.top();
	for (size_t i = 0; i < top->size(); i++) {
		if ((*top)[i].IsDefault) {
			Error("Cannot have multiple default clauses.");
		}
	}

	ConsumeToken(TOKEN_COLON, "Expected ':' after 'default'.");

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
	EmitOpcode(OP_POP);

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
	EmitOpcode(OP_POP);

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

	ConsumeToken(TOKEN_RIGHT_BRACE, "Expected '}' after block.");
}
void Compiler::GetWithStatement() {
	enum { WITH_STATE_INIT, WITH_STATE_ITERATE, WITH_STATE_FINISH, WITH_STATE_INIT_SLOTTED };

	bool useOther = true;
	bool useOtherSlot = false;
	bool hasThis = HasThis();

	// Start new scope
	ScopeBegin();

	// Reserve stack slot for where "other" will be at
	EmitOpcode(OP_NULL);

	// Add "other"
	int otherSlot = AddHiddenLocal("other", 5);

	// If the function has "this", make a copy of "this" (which is
	// at the first slot) into "other"
	if (hasThis) {
		EmitOpcode(OP_GET_LOCAL, 0);
		EmitOpcode(OP_SET_LOCAL, otherSlot);
		EmitOpcode(OP_POP);
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
	EmitOpcode(OP_WITH);

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
	EmitOpcode(OP_WITH);
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
	EmitOpcode(OP_WITH);
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
		EmitOpcode(OP_POP); // Condition.
	}

	// Incremental
	if (!MatchToken(TOKEN_RIGHT_PAREN)) {
		int bodyJump = EmitJump(OP_JUMP);

		int incrementStart = CurrentChunk()->Count;
		GetExpression();
		EmitOpcode(OP_POP);
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
		EmitOpcode(OP_POP); // Condition.
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
	EmitOpcode(OP_NULL);

	int iterValue = AddHiddenLocal(" iterValue", 10);

	ConsumeToken(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");

	int exitJump = -1;
	int loopStart = CurrentChunk()->Count;

	// Call $iterObj.$iterate($iterValue)
	// $iterValue is initially null, which signals that the
	// iteration just began.
	EmitOpcode(OP_GET_LOCAL, iterObj);
	EmitOpcode(OP_GET_LOCAL, iterValue);
	EmitCall("iterate", 1, false);

	// Set the result to iterValue, updating the iteration state
	EmitOpcode(OP_SET_LOCAL, iterValue);

	// If it returns null, the iteration ends
	EmitOpcode(OP_NULL, OP_EQUAL_NOT);
	exitJump = EmitJump(OP_JUMP_IF_FALSE);
	EmitOpcode(OP_POP);

	// Call $iterObj.$iteratorValue($iterValue)
	EmitOpcode(OP_GET_LOCAL, iterObj);
	EmitOpcode(OP_GET_LOCAL, iterValue);
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
	EmitOpcode(OP_POP);

	// Pop jump list off break stack, patch all break to this code
	// point
	EndBreakJumpList();
}
void Compiler::GetIfStatement() {
	ConsumeToken(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
	GetExpression();
	ConsumeToken(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	int thenJump = EmitJump(OP_JUMP_IF_FALSE);
	EmitOpcode(OP_POP);
	GetStatement();

	int elseJump = EmitJump(OP_JUMP);

	PatchJump(thenJump);
	EmitOpcode(OP_POP); // Only Pop if OP_JUMP_IF_FALSE, as it
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
	bool matchedLeftSquareBrace = false;

	int arity = 0;
	int minArity = 0;

	if (!CheckToken(TOKEN_RIGHT_PAREN)) {
		do {
			if (!isOptional && MatchToken(TOKEN_LEFT_SQUARE_BRACE)) {
				isOptional = true;
				matchedLeftSquareBrace = true;
			}

			ParseVariable("Expect parameter name.", false);
			DefineVariableToken(parser.Previous, false);
			MarkResolved();

			arity++;
			if (arity > 255) {
				Error("Cannot have more than 255 parameters.");
			}

			if (MatchToken(TOKEN_ASSIGNMENT)) {
				isOptional = true;
				GetValueExpression();
				EmitOpcode(OP_SET_ARGUMENT_SLOT, arity);
			}
			else if (isOptional && !matchedLeftSquareBrace) {
				Error("Cannot have required parameters after optional parameters.");
			}

			if (!isOptional) {
				minArity++;
			}
			else if (matchedLeftSquareBrace && MatchToken(TOKEN_RIGHT_SQUARE_BRACE)) {
				break;
			}
		} while (MatchToken(TOKEN_COMMA));
	}

	Function->Arity = (Uint8)arity;
	Function->MinArity = (Uint8)minArity;

	ConsumeToken(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

	// The body.
	ConsumeToken(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
	GetBlockStatement();
}
int Compiler::GetFunction(int type, string className) {
	int index = (int)Compiler::Functions.size();

	char* name = StringUtils::Create(parser.Previous.Start, parser.Previous.Length);

	Compiler* compiler = new Compiler;
	compiler->CurrentSettings = CurrentSettings;
	compiler->ClassName = className;
	compiler->Type = type;
	compiler->ScopeDepth = 1;

	compiler->Initialize(name);
	compiler->SetupLocals();

	try {
		compiler->CompileFunction();
	}
	catch (const CompilerErrorException& error) {
		compiler->Cleanup();

		delete compiler;

		throw error;
	}

	for (int i = 0; i < compiler->LocalCount; i++) {
		compiler->CheckLocalUnusedOrUnset(compiler->Locals[i]);
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
	int type = FUNCTIONTYPE_METHOD;
	if (IdentifiersEqual(&className, &parser.Previous)) {
		type = FUNCTIONTYPE_CONSTRUCTOR;
	}

	// Compile the old instruction if it fits under uint8.
	int index = GetFunction(type, className.ToString());
	if (index <= UINT8_MAX) {
		EmitOpcode(OP_METHOD_V4);
		EmitByte(index);
	}
	else {
		EmitOpcode(OP_METHOD);
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
			if (constant) {
				GetValueExpression();
			}
			else {
				GetExpression();
			}
		}
		else {
			if (constant) { // don't play nice
				ErrorAtCurrent(
					"'const' variables must have an explicit constant declaration.");
			}

			EmitOpcode(OP_NULL);
		}

		VMValue value;
		Local* locals = constant ? Constants.data() : Locals;
		int constantIndex = -1;
		if (pre + Bytecode::GetTotalOpcodeSize(CurrentChunk()->Code + pre) == CodePointer() &&
			CurrentChunk()->GetConstant(pre, &value, &constantIndex)) {
			if (variable != -1) {
				locals[variable].ConstantVal = value;
				locals[variable].Constant = constant;
				if (constant) {
					CurrentChunk()->Count = pre;
					locals[variable].Index = constantIndex;
					AllLocals.push_back(locals[variable]);
				}
			}
		}
		else if (constant) {
			ErrorAtCurrent("'const' variables must be set to a constant.");
		}

		DefineVariableToken(token, constant);
		if (constant && variable == -1) {
			// treat it like a module constant
			ModuleConstants.push_back({token, VARTYPE_MODULE_LOCAL, constantIndex, 0, false, false, true, value});
		}
	} while (MatchToken(TOKEN_COMMA));

	ConsumeToken(TOKEN_SEMICOLON, "Expected ';' after variable declaration.");
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
				if (constant) {
					GetValueExpression();
				}
				else {
					GetExpression();
				}
			}
			else {
				if (constant) { // don't play nice
					ErrorAtCurrent(
						"'const' variables must have an explicit constant declaration.");
				}

				EmitOpcode(OP_NULL);
			}

			int constantIndex = -1;

			VMValue value;
			if (pre + Bytecode::GetTotalOpcodeSize(CurrentChunk()->Code + pre) == CodePointer() &&
				CurrentChunk()->GetConstant(pre, &value, &constantIndex)) {
				vec->at(local).ConstantVal = value;
				if (constant) {
					CurrentChunk()->Count = pre;
					vec->at(local).Index = constantIndex;
				}
			}
			else if (constant) {
				ErrorAt(&token,
					"'const' variables must be set to a constant.",
					true);
			}

			if (!constant) {
				DefineModuleVariable(local);
			}
		} while (MatchToken(TOKEN_COMMA));

		ConsumeToken(TOKEN_SEMICOLON, "Expected ';' after variable declaration.");
	}
	else {
		ErrorAtCurrent("Expected 'var' or 'const' after 'local' declaration.");
	}
}
void Compiler::GetPropertyDeclaration(Token propertyName) {
	do {
		ParseVariable("Expected property name.", false);

		Local local;
		NamedVariable(propertyName, local, EXPRCONTEXT_VALUE);

		Token token = parser.Previous;

		if (MatchToken(TOKEN_ASSIGNMENT)) {
			GetExpression();
		}
		else {
			EmitOpcode(OP_NULL);
		}

		EmitSetOperation(OP_SET_PROPERTY, -1, token);

		EmitOpcode(OP_POP);
	} while (MatchToken(TOKEN_COMMA));

	ConsumeToken(TOKEN_SEMICOLON, "Expected ';' after property declaration.");
}
void Compiler::GetClassDeclaration() {
	ConsumeIdentifier("Expect class name.");

	Token className = parser.Previous;
	DeclareVariable(&className, false);

	EmitOpcode(OP_CLASS);
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

		EmitOpcode(OP_INHERIT);
		EmitStringHash(superName);
	}

	DefineVariableToken(className, false);

	ConsumeToken(TOKEN_LEFT_BRACE, "Expect '{' before class body.");

	while (!CheckToken(TOKEN_RIGHT_BRACE) && !CheckToken(TOKEN_EOF)) {
		Local local;
		if (MatchToken(TOKEN_EVENT)) {
			NamedVariable(className, local, EXPRCONTEXT_VALUE);
			GetMethod(className);
		}
		else if (MatchToken(TOKEN_STATIC)) {
			GetPropertyDeclaration(className);
		}
		else {
			NamedVariable(className, local, EXPRCONTEXT_VALUE);
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

		EmitOpcode(OP_NEW_ENUM);
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
				Local local;
				NamedVariable(enumName, local, EXPRCONTEXT_VALUE);
			}

			int constantIndex = -1;

			if (MatchToken(TOKEN_ASSIGNMENT)) {
				int pre = CodePointer();
				GetValueExpression();
				if (pre + Bytecode::GetTotalOpcodeSize(CurrentChunk()->Code + pre) !=
						CodePointer() ||
					!CurrentChunk()->GetConstant(pre, &current, &constantIndex)) {
					ErrorAt(&token,
						"Manual enum value must be constant.",
						true);
				}
				EmitCopy(1);
				EmitOpcode(OP_SAVE_VALUE);
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
					EmitOpcode(OP_LOAD_VALUE);
					EmitConstant(INTEGER_VAL(1));
					EmitOpcode(OP_ENUM_NEXT);
				}
				else {
					EmitConstant(INTEGER_VAL(0));
				}
				EmitCopy(1);
				EmitOpcode(OP_SAVE_VALUE);
			}

			didStart = true;

			if (isNamed) {
				EmitOpcode(OP_ADD_ENUM);
				EmitStringHash(token);
			}
			else {
				DefineVariableToken(token, true);
				if (variable == -1) {
					// treat it as a module constant
					ModuleConstants.push_back(
						{token, VARTYPE_MODULE_LOCAL, constantIndex, 0, false, false, true, current});
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

	ConsumeToken(TOKEN_SEMICOLON, "Expected ';' after 'import' declaration.");
}
void Compiler::GetUsingDeclaration() {
	ConsumeToken(TOKEN_NAMESPACE, "Expected 'namespace' after 'using' declaration.");

	if (ScopeDepth > 0) {
		Error("Cannot use namespaces outside of top-level code.");
	}

	do {
		ConsumeIdentifier("Expected namespace name.");
		Token nsName = parser.Previous;
		EmitOpcode(OP_USE_NAMESPACE);
		EmitStringHash(nsName);
	} while (MatchToken(TOKEN_COMMA));

	ConsumeToken(TOKEN_SEMICOLON, "Expected ';' after 'using' declaration.");
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
	//     MarkResolved();
	// }

	// Compile the old instruction if it fits under uint8.
	int index = GetFunction(FUNCTIONTYPE_FUNCTION);
	if (index <= UINT8_MAX) {
		EmitOpcode(OP_EVENT_V4);
		EmitByte(index);
	}
	else {
		EmitOpcode(OP_EVENT);
		EmitUint16(index);
	}

	// if (ScopeDepth == 0) {
	EmitOpcode(OP_DEFINE_GLOBAL);
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
		ParseRule{&Compiler::GetGrouping, &Compiler::GetCall, PREC_CALL};
	Rules[TOKEN_RIGHT_PAREN] = ParseRule{NULL, NULL, PREC_NONE};
	Rules[TOKEN_LEFT_BRACE] = ParseRule{&Compiler::GetMap, NULL, PREC_CALL};
	Rules[TOKEN_RIGHT_BRACE] = ParseRule{NULL, NULL, PREC_NONE};
	Rules[TOKEN_LEFT_SQUARE_BRACE] =
		ParseRule{&Compiler::GetArray, &Compiler::GetElement, PREC_CALL};
	Rules[TOKEN_RIGHT_SQUARE_BRACE] = ParseRule{NULL, NULL, PREC_NONE};
	Rules[TOKEN_COMMA] = ParseRule{NULL, NULL, PREC_NONE};
	Rules[TOKEN_DOT] = ParseRule{NULL, &Compiler::GetDot, PREC_CALL};
	Rules[TOKEN_SEMICOLON] = ParseRule{NULL, NULL, PREC_NONE};
	// Operators
	Rules[TOKEN_MINUS] = ParseRule{&Compiler::GetUnary, &Compiler::GetBinary, PREC_TERM};
	Rules[TOKEN_PLUS] = ParseRule{NULL, &Compiler::GetBinary, PREC_TERM};
	Rules[TOKEN_DECREMENT] =
		ParseRule{&Compiler::GetUnary, &Compiler::GetAssignment, PREC_CALL};
	Rules[TOKEN_INCREMENT] =
		ParseRule{&Compiler::GetUnary, &Compiler::GetAssignment, PREC_CALL};
	Rules[TOKEN_DIVISION] = ParseRule{NULL, &Compiler::GetBinary, PREC_FACTOR};
	Rules[TOKEN_MULTIPLY] = ParseRule{NULL, &Compiler::GetBinary, PREC_FACTOR};
	Rules[TOKEN_MODULO] = ParseRule{NULL, &Compiler::GetBinary, PREC_FACTOR};
	Rules[TOKEN_BITWISE_XOR] = ParseRule{NULL, &Compiler::GetBinary, PREC_BITWISE_XOR};
	Rules[TOKEN_BITWISE_AND] = ParseRule{NULL, &Compiler::GetBinary, PREC_BITWISE_AND};
	Rules[TOKEN_BITWISE_OR] = ParseRule{NULL, &Compiler::GetBinary, PREC_BITWISE_OR};
	Rules[TOKEN_BITWISE_LEFT] = ParseRule{NULL, &Compiler::GetBinary, PREC_BITWISE_SHIFT};
	Rules[TOKEN_BITWISE_RIGHT] =
		ParseRule{NULL, &Compiler::GetBinary, PREC_BITWISE_SHIFT};
	Rules[TOKEN_BITWISE_NOT] = ParseRule{&Compiler::GetUnary, NULL, PREC_UNARY};
	Rules[TOKEN_TERNARY] = ParseRule{NULL, &Compiler::GetConditional, PREC_TERNARY};
	Rules[TOKEN_COLON] = ParseRule{NULL, NULL, PREC_NONE};
	Rules[TOKEN_LOGICAL_AND] = ParseRule{NULL, &Compiler::GetLogicalAND, PREC_AND};
	Rules[TOKEN_LOGICAL_OR] = ParseRule{NULL, &Compiler::GetLogicalOR, PREC_OR};
	Rules[TOKEN_LOGICAL_NOT] = ParseRule{&Compiler::GetUnary, NULL, PREC_UNARY};
	Rules[TOKEN_TYPEOF] = ParseRule{&Compiler::GetUnary, NULL, PREC_UNARY};
	Rules[TOKEN_NEW] = ParseRule{&Compiler::GetNew, NULL, PREC_UNARY};
	Rules[TOKEN_NOT_EQUALS] = ParseRule{NULL, &Compiler::GetBinary, PREC_EQUALITY};
	Rules[TOKEN_EQUALS] = ParseRule{NULL, &Compiler::GetBinary, PREC_EQUALITY};
	Rules[TOKEN_HAS] = ParseRule{NULL, &Compiler::GetHas, PREC_EQUALITY};
	Rules[TOKEN_GREATER] = ParseRule{NULL, &Compiler::GetBinary, PREC_COMPARISON};
	Rules[TOKEN_GREATER_EQUAL] = ParseRule{NULL, &Compiler::GetBinary, PREC_COMPARISON};
	Rules[TOKEN_LESS] = ParseRule{NULL, &Compiler::GetBinary, PREC_COMPARISON};
	Rules[TOKEN_LESS_EQUAL] = ParseRule{NULL, &Compiler::GetBinary, PREC_COMPARISON};
	// Assignment
	Rules[TOKEN_ASSIGNMENT] = ParseRule{NULL, &Compiler::GetAssignment, PREC_ASSIGNMENT};
	Rules[TOKEN_ASSIGNMENT_MULTIPLY] = ParseRule{NULL, &Compiler::GetAssignment, PREC_ASSIGNMENT};
	Rules[TOKEN_ASSIGNMENT_DIVISION] = ParseRule{NULL, &Compiler::GetAssignment, PREC_ASSIGNMENT};
	Rules[TOKEN_ASSIGNMENT_MODULO] = ParseRule{NULL, &Compiler::GetAssignment, PREC_ASSIGNMENT};
	Rules[TOKEN_ASSIGNMENT_PLUS] = ParseRule{NULL, &Compiler::GetAssignment, PREC_ASSIGNMENT};
	Rules[TOKEN_ASSIGNMENT_MINUS] = ParseRule{NULL, &Compiler::GetAssignment, PREC_ASSIGNMENT};
	Rules[TOKEN_ASSIGNMENT_BITWISE_LEFT] = ParseRule{NULL, &Compiler::GetAssignment, PREC_ASSIGNMENT};
	Rules[TOKEN_ASSIGNMENT_BITWISE_RIGHT] = ParseRule{NULL, &Compiler::GetAssignment, PREC_ASSIGNMENT};
	Rules[TOKEN_ASSIGNMENT_BITWISE_AND] = ParseRule{NULL, &Compiler::GetAssignment, PREC_ASSIGNMENT};
	Rules[TOKEN_ASSIGNMENT_BITWISE_XOR] = ParseRule{NULL, &Compiler::GetAssignment, PREC_ASSIGNMENT};
	Rules[TOKEN_ASSIGNMENT_BITWISE_OR] = ParseRule{NULL, &Compiler::GetAssignment, PREC_ASSIGNMENT};
	// Keywords
	Rules[TOKEN_THIS] = ParseRule{&Compiler::GetThis, NULL, PREC_NONE};
	Rules[TOKEN_SUPER] = ParseRule{&Compiler::GetSuper, NULL, PREC_NONE};
	// Constants or whatever
	Rules[TOKEN_NULL] = ParseRule{&Compiler::GetLiteral, NULL, PREC_NONE};
	Rules[TOKEN_TRUE] = ParseRule{&Compiler::GetLiteral, NULL, PREC_NONE};
	Rules[TOKEN_FALSE] = ParseRule{&Compiler::GetLiteral, NULL, PREC_NONE};
	Rules[TOKEN_STRING] = ParseRule{&Compiler::GetString, NULL, PREC_NONE};
	Rules[TOKEN_NUMBER] = ParseRule{&Compiler::GetInteger, NULL, PREC_NONE};
	Rules[TOKEN_DECIMAL] = ParseRule{&Compiler::GetDecimal, NULL, PREC_NONE};
	Rules[TOKEN_IDENTIFIER] = ParseRule{&Compiler::GetVariable, NULL, PREC_NONE};
	Rules[TOKEN_HITBOX] = ParseRule{&Compiler::GetHitbox, NULL, PREC_NONE};
}
ParseRule* Compiler::GetRule(int type) {
	return &Compiler::Rules[(int)type];
}

ExprContext Compiler::ParsePrecedence(Precedence precedence, ExprContext context) {
	AdvanceToken();
	ParseFn prefixRule = GetRule(parser.Previous.Type)->Prefix;
	if (prefixRule == NULL) {
		Error("Expected expression.");
	}

	int preCount = CurrentChunk()->Count;
	int preConstant = CurrentChunk()->Constants->size();

	ExprContext initialContext = context;
	context = (this->*prefixRule)(initialContext);

	if (CurrentSettings.DoOptimizations) {
		preConstant = CheckPrefixOptimize(preCount, preConstant, prefixRule);
	}

	while (precedence <= GetRule(parser.Current.Type)->Precedence) {
		AdvanceToken();
		ParseFn infixRule = GetRule(parser.Previous.Type)->Infix;
		if (infixRule) {
			context = (this->*infixRule)(context);
		}
		if (CurrentSettings.DoOptimizations) {
			preConstant = CheckInfixOptimize(preCount, preConstant, infixRule);
		}
	}

	if (initialContext == EXPRCONTEXT_VALUE && context == EXPRCONTEXT_LOCATION) {
		ConvertLvalueToRvalue();
		return EXPRCONTEXT_VALUE;
	}

	return context;
}
ExprContext Compiler::ParsePrecedence(Precedence precedence) {
	ExprContext context = ParsePrecedence(precedence, EXPRCONTEXT_VALUE);

	ResetVariableLocal();

	return context;
}
void Compiler::ResetVariableLocal() {
	VariableLocal.Type = VARTYPE_UNKNOWN;
	VariableLocal.Constant = false;
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
Uint8* Compiler::GetLastOpcodePtr(Chunk* chunk, int n) {
	if (chunk->Count == 0) {
		return nullptr;
	}

	int opcodeCount = 0;
	for (int offset = 0; offset < chunk->Count;) {
		offset += Bytecode::GetTotalOpcodeSize(chunk->Code + offset);
		opcodeCount++;
	}

	if (n >= opcodeCount) {
		return nullptr;
	}

	Uint8* ptr = chunk->Code;
	for (int op = 0; op < opcodeCount - n - 1; op++) {
		ptr += Bytecode::GetTotalOpcodeSize(ptr);
	}

	return ptr;
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
void Compiler::EmitOpcode(Uint8 byte) {
	EmitByte(byte);
	EmittedOpcodes.push_back(CurrentChunk()->Code + (GetPosition() - 1));
}
void Compiler::EmitOpcode(Uint8 opcode, Uint8 byte) {
	EmitOpcode(opcode);
	EmitByte(byte);
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
			EmitOpcode(OP_INTEGER);
			EmitSint32(value.as.Integer);
		}
		return -1;
	}
	else if (value.Type == VAL_DECIMAL) {
		EmitOpcode(OP_DECIMAL);
		EmitFloat(value.as.Decimal);
		return -1;
	}
	else if (value.Type == VAL_NULL) {
		EmitOpcode(OP_NULL);
		return -1;
	}

	// anything else gets added to the const table
	int index = GetConstantIndex(value);

	EmitOpcode(OP_CONSTANT);
	EmitUint32(index);

	return index;
}

void Compiler::EmitLoop(int loopStart) {
	EmitOpcode(OP_JUMP_BACK);

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
	if (Type == FUNCTIONTYPE_CONSTRUCTOR) {
		// return the new instance built from the constructor
		EmitOpcode(OP_GET_LOCAL, 0);
	}
	else if (EmitNullOnReturn) {
		EmitOpcode(OP_NULL);
	}
	EmitOpcode(OP_RETURN);
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
void Compiler::StartBreakpointList() {
	BreakpointListStack.push(new vector<Uint32>());
}
void Compiler::EndBreakpointList(Uint32 offset) {
	vector<Uint32>* top = BreakpointListStack.top();
	for (size_t i = 0; i < top->size(); i++) {
		Breakpoints.push_back((*top)[i] + offset);
	}
	delete top;
	BreakpointListStack.pop();
}
void Compiler::EndBreakpointList() {
	EndBreakpointList(0);
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
	if (CurrentChunk()->Constants->size() == 0xFFFFFFFF) {
		Error("Too many constants in one chunk.");
		return -1;
	}

	return CurrentChunk()->AddConstant(value);
}

bool Compiler::HasThis() {
	switch (Type) {
	case FUNCTIONTYPE_CONSTRUCTOR:
	case FUNCTIONTYPE_METHOD:
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
				EmitOpcode(OP_TRUE);
				break;
			case VAL_OBJECT:
				EmitOpcode(OP_FALSE);
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
	}
	if (!IS_NULL(out)) {
		EmitConstant(out);
		preConstant = CurrentChunk()->Constants->size();
		if (out.Type == VAL_INTEGER) {
			preConstant =
				CheckPrefixOptimize(preCount, preConstant, &Compiler::GetInteger);
		}
	}

	return preConstant;
}

int Compiler::CheckInfixOptimize(int preCount, int preConstant, ParseFn fn) {
	if (fn == &Compiler::GetBinary) {
		// this is gonna be really basic for now (constant constant OP)
		// some of the stuff that passes through here are much longer than that,
		// but this is a very solid start that already can shrink a good amount

		int off1 = preCount;
		Uint8 op1 = CurrentChunk()->Code[off1];
		int off2 = Bytecode::GetTotalOpcodeSize(CurrentChunk()->Code + off1) + off1;
		if (off2 >= CodePointer()) {
			return preConstant;
		}
		Uint8 op2 = CurrentChunk()->Code[off2];
		int offB = Bytecode::GetTotalOpcodeSize(CurrentChunk()->Code + off2) + off2;
		if (offB != CodePointer() - 1) { // CHANGE TO >= ONCE CASCADING IS ADDED
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
					WarningAt(&parser.Previous, "Division by zero will raise a runtime error!");
					return preConstant;
				}
				out = DECIMAL_VAL(a_d / b_d);
			}
			else {
				int a_d = AS_INTEGER(a);
				int b_d = AS_INTEGER(b);
				if (b_d == 0) {
					WarningAt(&parser.Previous, "Division by zero will raise a runtime error!");
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

				if (b_d == 0) {
					WarningAt(&parser.Previous, "Modulo by zero will raise a runtime error!");
					return preConstant;
				}
				out = DECIMAL_VAL(fmod(a_d, b_d));
			}
			else {
				int a_d = AS_INTEGER(a);
				int b_d = AS_INTEGER(b);
				if (b_d == 0) {
					WarningAt(&parser.Previous, "Modulo by zero will raise a runtime error!");
					return preConstant;
				}

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

void Compiler::AddBreakpoint(Token at) {
	std::vector<Uint32>* breakpoints = BreakpointListStack.top();
	if (breakpoints->size() == MAX_CHUNK_BREAKPOINTS) {
		char msg[64];
		snprintf(msg, sizeof msg, "Cannot have more than %d breakpoints.", MAX_CHUNK_BREAKPOINTS);
		ErrorAt(&at, msg, false); // It's not fatal to have too many breakpoints
		return;
	}

	Uint32 offset = CurrentChunk()->Count;

	breakpoints->push_back(offset);
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
	Settings.UseIntrinsics = true;
	Settings.UseResourcesIntrinsics = false;

	Application::Settings->GetBool("compiler", "log", &Compiler::DoLogging);
	if (Compiler::DoLogging) {
		Application::Settings->GetBool("compiler", "showWarnings", &Settings.ShowWarnings);
	}

	Application::Settings->GetBool("compiler", "writeDebugInfo", &Settings.WriteDebugInfo);
	Application::Settings->GetBool(
		"compiler", "writeSourceFilename", &Settings.WriteSourceFilename);
	Application::Settings->GetBool("compiler", "optimizations", &Settings.DoOptimizations);
	Application::Settings->GetBool("compiler", "intrinsics", &Settings.UseIntrinsics);
	Application::Settings->GetBool("compiler", "useResourcesIntrinsics", &Settings.UseResourcesIntrinsics);

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
void Compiler::SetupIntrinsics() {
	Intrinsics.clear();

	if (!Settings.UseIntrinsics) {
		return;
	}

#define CHECK_ARGCOUNT(count) { \
	if (argCount != count) { \
		return false; \
	} \
}

	Intrinsics["String.Length"] = [](Compiler* compiler, Uint8* argStart, int argCount) {
		CHECK_ARGCOUNT(1);

		compiler->EmitOpcode(OP_LENGTH);

		return true;
	};

	Intrinsics["Array.Length"] = [](Compiler* compiler, Uint8* argStart, int argCount) {
		CHECK_ARGCOUNT(1);

		compiler->EmitOpcode(OP_LENGTH);

		return true;
	};

	if (Settings.UseResourcesIntrinsics) {
		Intrinsics["Resources.LoadSprite"] = [](Compiler* compiler, Uint8* argStart, int argCount) {
			CHECK_ARGCOUNT(2);
			return compiler->Intrinsic_LoadResource(argStart, argCount, INTRINSIC_RESOURCE_SPRITE);
		};
		Intrinsics["Resources.LoadImage"] = [](Compiler* compiler, Uint8* argStart, int argCount) {
			CHECK_ARGCOUNT(2);
			return compiler->Intrinsic_LoadResource(argStart, argCount, INTRINSIC_RESOURCE_IMAGE);
		};
		Intrinsics["Resources.LoadModel"] = [](Compiler* compiler, Uint8* argStart, int argCount) {
			CHECK_ARGCOUNT(2);
			return compiler->Intrinsic_LoadResource(argStart, argCount, INTRINSIC_RESOURCE_MODEL);
		};
		Intrinsics["Resources.LoadMusic"] = [](Compiler* compiler, Uint8* argStart, int argCount) {
			CHECK_ARGCOUNT(2);
			return compiler->Intrinsic_LoadResource(argStart, argCount, INTRINSIC_RESOURCE_MUSIC);
		};
		Intrinsics["Resources.LoadSound"] = [](Compiler* compiler, Uint8* argStart, int argCount) {
			CHECK_ARGCOUNT(2);
			return compiler->Intrinsic_LoadResource(argStart, argCount, INTRINSIC_RESOURCE_SOUND);
		};
		Intrinsics["Resources.LoadVideo"] = [](Compiler* compiler, Uint8* argStart, int argCount) {
			CHECK_ARGCOUNT(2);
			return compiler->Intrinsic_LoadResource(argStart, argCount, INTRINSIC_RESOURCE_MEDIA);
		};
		Intrinsics["Resources.ReadAllText"] = [](Compiler* compiler, Uint8* argStart, int argCount) {
			CHECK_ARGCOUNT(1);
			return compiler->Intrinsic_LoadResource(argStart, argCount, INTRINSIC_RESOURCE_TEXT);
		};
		Intrinsics["Resources.FileExists"] = [](Compiler* compiler, Uint8* argStart, int argCount) {
			CHECK_ARGCOUNT(1);
			return compiler->Intrinsic_ResourceExists(argStart, argCount);
		};
	}

#undef CHECK_ARGCOUNT
}
bool Compiler::Intrinsic_LoadResource(Uint8* argStart, int argCount, Uint8 type) {
	Chunk* chunk = CurrentChunk();
	Uint32 argStartOffset = argStart - chunk->Code;
	Uint32 offset = argStartOffset;
	size_t opcodeSize = Bytecode::GetTotalOpcodeSize(chunk->Code + offset);

	VMValue nameString;
	if (!chunk->GetConstant(offset, &nameString, nullptr)) {
		return false;
	}

	if (!IS_STRING(nameString)) {
		return false;
	}

	for (int i = 0; i < argCount; i++) {
		offset += Bytecode::GetTotalOpcodeSize(chunk->Code + offset);
	}

	if (offset < chunk->Count) {
		return false;
	}

	EraseChunkCode(chunk, argStartOffset, opcodeSize);

	Uint32 hash = CRC32::EncryptString(AS_CSTRING(nameString));

	EmitOpcode(OP_LOAD_GAME_RESOURCE);
	EmitByte(type);
	EmitUint32(hash);

	return true;
}
bool Compiler::Intrinsic_ResourceExists(Uint8* argStart, int argCount) {
	Chunk* chunk = CurrentChunk();
	Uint32 argStartOffset = argStart - chunk->Code;
	Uint32 offset = argStartOffset;
	size_t opcodeSize = Bytecode::GetTotalOpcodeSize(chunk->Code + offset);

	VMValue nameString;
	if (!chunk->GetConstant(offset, &nameString, nullptr)) {
		return false;
	}

	if (!IS_STRING(nameString)) {
		return false;
	}

	for (int i = 0; i < argCount; i++) {
		offset += Bytecode::GetTotalOpcodeSize(chunk->Code + offset);
	}

	if (offset < chunk->Count) {
		return false;
	}

	EraseChunkCode(chunk, argStartOffset, opcodeSize);

	Uint32 hash = CRC32::EncryptString(AS_CSTRING(nameString));

	EmitOpcode(OP_CHECK_GAME_RESOURCE);
	EmitUint32(hash);

	return true;
}
void Compiler::EraseChunkCode(Chunk* chunk, size_t offset, size_t length) {
	Uint32 block_copy_length = (chunk->Count - offset) - length;

	Uint8* dest_code_block = &chunk->Code[offset];
	int* dest_line_block = &chunk->Lines[offset];
	Uint8* src_code_block = &chunk->Code[offset + length];
	int* src_line_block = &chunk->Lines[offset + length];

	memmove(dest_code_block, src_code_block, block_copy_length * sizeof(Uint8));
	memmove(dest_line_block, src_line_block, block_copy_length * sizeof(int));

	chunk->Count -= length;
}
void Compiler::PrepareCompiling() {
	if (Compiler::TokenMap == NULL) {
		Compiler::TokenMap = new HashMap<Token>(NULL, 8);
	}
}
void Compiler::Initialize(char* name) {
	Function = NewFunction();
	Function->Name = name;

	UnusedVariables = new vector<Local>();
	UnsetVariables = new vector<Local>();

	StartBreakpointList();

	Compiler::Functions.push_back(Function);
}
void Compiler::Initialize() {
	char* name = StringUtils::Create("main");

	Initialize(name);
}
void Compiler::SetupLocals() {
	if (LocalCount >= 0) {
		return;
	}

	LocalCount = 0;

	Local* local = &Locals[LocalCount++];
	local->Depth = ScopeDepth;
	local->Index = LocalCount - 1;
	local->Resolved = true;

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
bool Compiler::Compile(const char* filename, const char* source, Stream* output) {
	scanner.Line = 1;
	scanner.Start = (char*)source;
	scanner.Current = (char*)source;
	scanner.LinePos = (char*)source;
	scanner.SourceFilename = (char*)filename;

	parser.HadError = false;
	parser.PanicMode = false;

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
			local.Constant = false;
			local.Resolved = moduleLocal.Resolved;
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

	if (Compiler::ModuleConstants.size() > 0) {
		if (!Function->Chunk.ModuleLocals) {
			Function->Chunk.ModuleLocals = new vector<ChunkLocal>;
		}

		for (size_t i = 0; i < Compiler::ModuleConstants.size(); i++) {
			Local moduleLocal = Compiler::ModuleConstants[i];
			if (moduleLocal.Index < 0) {
				continue;
			}

			ChunkLocal local;
			Token nameToken = moduleLocal.Name;
			local.Name = StringUtils::Create(nameToken.ToString());
			local.Constant = true;
			local.Resolved = true;
			local.Index = moduleLocal.Index;
			local.Position = (Uint32)((nameToken.Pos & 0xFFFF) << 16 | (nameToken.Line & 0xFFFF));
			Function->Chunk.ModuleLocals->push_back(local);
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
	while (BreakpointListStack.size() > 0) {
		delete BreakpointListStack.top();
		BreakpointListStack.pop();
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
			if (srcLocal.Index < 0) {
				continue;
			}

			Token nameToken = srcLocal.Name;
			Uint32 position = 0;
			if (i >= Function->Arity + 1) {
				position = (Uint32)((nameToken.Pos & 0xFFFF) << 16 | (nameToken.Line & 0xFFFF));
			}

			ChunkLocal local;
			local.Name = StringUtils::Create(nameToken.ToString());
			local.Constant = srcLocal.Constant;
			local.Resolved = srcLocal.Resolved;
			local.Index = srcLocal.Index;
			local.Position = position;
			chunk->Locals->push_back(local);
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
	EndBreakpointList();
	AddBreakpointsToChunk(chunk);
	RebuildConstantsList();
}
void Compiler::AddBreakpointsToChunk(Chunk* chunk) {
	chunk->BreakpointCount = (Uint16)Breakpoints.size();

	if (!chunk->BreakpointCount) {
		return;
	}

	chunk->Breakpoints = (Uint32*)Memory::Calloc(chunk->BreakpointCount, sizeof(Uint32));

	for (Uint16 i = 0; i < chunk->BreakpointCount; i++) {
		chunk->Breakpoints[i] = Breakpoints[i];
	}
}
// This removes any unused constants.
void Compiler::RebuildConstantsList() {
	Chunk* chunk = CurrentChunk();
	size_t numConstants = chunk->Constants->size();
	if (numConstants == 0) {
		return;
	}

	std::map<Uint32*, VMValue> constantToValue;

	for (int offset = 0; offset < chunk->Count;) {
		Uint8* opcode = chunk->Code + offset;
		if (Bytecode::IsConstantIndexOpcode(*opcode)) {
			Uint32* index = (Uint32*)(opcode + 1);
			constantToValue[index] = (*chunk->Constants)[*index];
		}
		offset += Bytecode::GetTotalOpcodeSize(opcode);
	}

	chunk->Constants->clear();

	for (auto it = constantToValue.begin(); it != constantToValue.end(); it++) {
		*it->first = GetConstantIndex(it->second);
	}
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
