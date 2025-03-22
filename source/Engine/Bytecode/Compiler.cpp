#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Engine/Bytecode/Bytecode.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/GarbageCollector.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/Values.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/IO/FileStream.h>

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
bool Compiler::ShowWarnings = false;
bool Compiler::WriteDebugInfo = false;
bool Compiler::WriteSourceFilename = false;
bool Compiler::DoOptimizations = false;

#define Panic(returnMe) \
	if (parser.PanicMode) { \
		SynchronizeToken(); \
		return returnMe; \
	}

static const char* opcodeNames[] = {"OP_ERROR",
	"OP_CONSTANT",
	"OP_DEFINE_GLOBAL",
	"OP_GET_PROPERTY",
	"OP_SET_PROPERTY",
	"OP_GET_GLOBAL",
	"OP_SET_GLOBAL",
	"OP_GET_LOCAL",
	"OP_SET_LOCAL",
	"OP_PRINT_STACK",
	"OP_INHERIT",
	"OP_RETURN",
	"OP_METHOD",
	"OP_CLASS",
	"OP_CALL",
	"OP_SUPER",
	"OP_INVOKE",
	"OP_JUMP",
	"OP_JUMP_IF_FALSE",
	"OP_JUMP_BACK",
	"OP_POP",
	"OP_COPY",
	"OP_ADD",
	"OP_SUBTRACT",
	"OP_MULTIPLY",
	"OP_DIVIDE",
	"OP_MODULO",
	"OP_NEGATE",
	"OP_INCREMENT",
	"OP_DECREMENT",
	"OP_BITSHIFT_LEFT",
	"OP_BITSHIFT_RIGHT",
	"OP_NULL",
	"OP_TRUE",
	"OP_FALSE",
	"OP_BW_NOT",
	"OP_BW_AND",
	"OP_BW_OR",
	"OP_BW_XOR",
	"OP_LG_NOT",
	"OP_LG_AND",
	"OP_LG_OR",
	"OP_EQUAL",
	"OP_EQUAL_NOT",
	"OP_GREATER",
	"OP_GREATER_EQUAL",
	"OP_LESS",
	"OP_LESS_EQUAL",
	"OP_PRINT",
	"OP_ENUM_NEXT",
	"OP_SAVE_VALUE",
	"OP_LOAD_VALUE",
	"OP_WITH",
	"OP_GET_ELEMENT",
	"OP_SET_ELEMENT",
	"OP_NEW_ARRAY",
	"OP_NEW_MAP",
	"OP_SWITCH_TABLE",
	"OP_FAILSAFE",
	"OP_EVENT",
	"OP_TYPEOF",
	"OP_NEW",
	"OP_IMPORT",
	"OP_SWITCH",
	"OP_POPN",
	"OP_HAS_PROPERTY",
	"OP_IMPORT_MODULE",
	"OP_ADD_ENUM",
	"OP_NEW_ENUM",
	"OP_GET_SUPERCLASS",
	"OP_GET_MODULE_LOCAL",
	"OP_SET_MODULE_LOCAL",
	"OP_DEFINE_MODULE_LOCAL",
	"OP_USE_NAMESPACE",
	"OP_DEFINE_CONSTANT",
	"OP_INTEGER",
	"OP_DECIMAL"};

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
		return CheckKeyword(1, 4, "reak", TOKEN_BREAK);
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
		return CheckKeyword(1, 2, "as", TOKEN_HAS);
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

void Compiler::SynchronizeToken() {
	parser.PanicMode = false;

	while (PeekToken().Type != TOKEN_EOF) {
		if (PrevToken().Type == TOKEN_SEMICOLON) {
			return;
		}

		switch (PeekToken().Type) {
		case TOKEN_IF:
		case TOKEN_WHILE:
		case TOKEN_DO:
		case TOKEN_FOR:
		case TOKEN_SWITCH:
		case TOKEN_CASE:
		case TOKEN_DEFAULT:
		case TOKEN_RETURN:
		case TOKEN_BREAK:
		case TOKEN_CONTINUE:
		case TOKEN_IMPORT:
		case TOKEN_VAR:
		case TOKEN_EVENT:
		case TOKEN_PRINT:
			return;
		default:
			break;
		}

		AdvanceToken();
	}
}

// Error handling
bool Compiler::ReportError(int line, int pos, bool fatal, const char* string, ...) {
	if (!fatal && !Compiler::ShowWarnings) {
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

	buffer_printf(&buffer,
		"In file '%s' on line %d, position %d:\n    %s\n\n",
		scanner.SourceFilename,
		line,
		pos,
		message);

	if (!fatal) {
		Log::Print(Log::LOG_WARN, textBuffer);
		free(textBuffer);
		return true;
	}

	Log::Print(Log::LOG_ERROR, textBuffer);

	const SDL_MessageBoxButtonData buttonsFatal[] = {
		{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Exit"},
	};

	const SDL_MessageBoxData messageBoxData = {
		SDL_MESSAGEBOX_ERROR,
		NULL,
		"Syntax Error",
		textBuffer,
		SDL_arraysize(buttonsFatal),
		buttonsFatal,
		NULL,
	};

	int buttonClicked;
	SDL_ShowMessageBox(&messageBoxData, &buttonClicked);

	free(textBuffer);

	Application::Cleanup();
	exit(-1);

	return false;
}
void Compiler::ErrorAt(Token* token, const char* message, bool fatal) {
	if (fatal) {
		if (parser.PanicMode) {
			return;
		}
		parser.PanicMode = true;
	}

	if (token->Type == TOKEN_EOF) {
		ReportError(token->Line, (int)token->Pos, fatal, " at end of file: %s", message);
	}
	else if (token->Type == TOKEN_ERROR) {
		ReportError(token->Line, (int)token->Pos, fatal, "%s", message);
	}
	else {
		ReportError(token->Line,
			(int)token->Pos,
			fatal,
			" at '%.*s': %s",
			token->Length,
			token->Start,
			message);
	}

	if (fatal) {
		parser.HadError = true;
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
	if (!Compiler::ShowWarnings) {
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

	if (strcmp(Function->Name->Chars, "main") == 0) {
		buffer_printf(&buffer,
			"In top level code of file '%s':\n    %s\n",
			scanner.SourceFilename,
			message);
	}
	else if (ClassName.size() > 0) {
		buffer_printf(&buffer,
			"In method '%s::%s' of file '%s':\n    %s\n",
			ClassName.c_str(),
			Function->Name->Chars,
			scanner.SourceFilename,
			message);
	}
	else {
		buffer_printf(&buffer,
			"In function '%s' of file '%s':\n    %s\n",
			Function->Name->Chars,
			scanner.SourceFilename,
			message);
	}

	Log::Print(Log::LOG_WARN, textBuffer);

	free(textBuffer);
}

int Compiler::ParseVariable(const char* errorMessage, bool constant) {
	ConsumeToken(TOKEN_IDENTIFIER, errorMessage);
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
	Constants.push_back({*name, ScopeDepth, false, false, true});
	return ((int)Constants.size()) - 1;
}
int Compiler::ParseModuleVariable(const char* errorMessage, bool constant) {
	ConsumeToken(TOKEN_IDENTIFIER, errorMessage);
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

	Compiler::ModuleConstants.push_back({*name, 0, false, false, true});
	return ((int)Compiler::ModuleConstants.size()) - 1;
}
void Compiler::WarnVariablesUnusedUnset() {
	if (!Compiler::ShowWarnings) {
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
	EmitBytes(OP_INVOKE, argCount);
	EmitStringHash(name);
	EmitByte(isSuper ? 1 : 0);
}
void Compiler::EmitCall(Token name, int argCount, bool isSuper) {
	EmitBytes(OP_INVOKE, argCount);
	EmitStringHash(name);
	EmitByte(isSuper ? 1 : 0);
}

void Compiler::NamedVariable(Token name, bool canAssign) {
	Uint8 getOp, setOp;
	Local local;
	local.Constant = false;
	int arg = ResolveLocal(&name, &local);

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
			return;
		}
		else {
			getOp = OP_GET_GLOBAL;
			setOp = OP_SET_GLOBAL;
		}
	}

	if (canAssign && MatchAssignmentToken()) {
		if (local.Constant) {
			ErrorAt(&name, "Attempted to assign to constant!", true);
		}
		else if (getOp == OP_GET_LOCAL) {
			Locals[arg].WasSet = true;
		}
		else if (getOp == OP_GET_MODULE_LOCAL) {
			ModuleLocals[arg].WasSet = true;
		}

		Token assignmentToken = parser.Previous;
		if (assignmentToken.Type == TOKEN_INCREMENT ||
			assignmentToken.Type == TOKEN_DECREMENT) {
			EmitGetOperation(getOp, arg, name);

			EmitCopy(1);
			EmitByte(OP_SAVE_VALUE); // Save value. (value)
			EmitAssignmentToken(assignmentToken); // OP_DECREMENT
				// (value - 1, this)

			EmitSetOperation(setOp, arg, name);
			EmitByte(OP_POP);

			EmitByte(OP_LOAD_VALUE); // Load value. (value)
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
int Compiler::AddLocal(Token name) {
	if (LocalCount == 0xFF) {
		Error("Too many local variables in function.");
		return -1;
	}
	Local* local = &Locals[LocalCount++];
	local->Name = name;
	local->Depth = -1;
	local->Resolved = false;
	local->Constant = false;
	local->ConstantVal = VMValue{VAL_ERROR};
	return LocalCount - 1;
}
int Compiler::AddLocal(const char* name, size_t len) {
	if (LocalCount == 0xFF) {
		Error("Too many local variables in function.");
		return -1;
	}
	Local* local = &Locals[LocalCount++];
	local->Depth = -1;
	local->Resolved = false;
	local->Constant = false;
	local->ConstantVal = VMValue{VAL_ERROR};
	;
	RenameLocal(local, name, len);
	return LocalCount - 1;
}
int Compiler::AddHiddenLocal(const char* name, size_t len) {
	int local = AddLocal(name, len);
	Locals[local].Resolved = true;
	MarkInitialized();
	return local;
}
void Compiler::RenameLocal(Local* local, const char* name, size_t len) {
	local->Name.Start = (char*)name;
	local->Name.Length = len;
}
void Compiler::RenameLocal(Local* local, const char* name) {
	local->Name.Start = (char*)name;
	local->Name.Length = strlen(name);
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
	local.Resolved = false;
	local.Constant = false;
	local.ConstantVal = VMValue{VAL_ERROR};
	Compiler::ModuleLocals.push_back(local);
	return ((int)Compiler::ModuleLocals.size()) - 1;
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
	GetVariable(false);
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

	ConsumeToken(TOKEN_IDENTIFIER, "Expect property name after '.'.");
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
		AdvanceToken();
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
int Compiler::GetConstantValue() {
	int position, constant_index;
	position = CodePointer();
	GetConstant(false);
	constant_index = CurrentChunk()->Code[position + 1];
	CurrentChunk()->Count = position;
	return constant_index;
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
	int operatorType = parser.Previous.Type;

	ParsePrecedence(PREC_UNARY);

	switch (operatorType) {
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

		// HACK: replace these with prefix version of OP
		// case TOKEN_INCREMENT:   EmitByte(OP_INCREMENT);
		// break; case TOKEN_DECREMENT: EmitByte(OP_DECREMENT);
		// break;
	default:
		return; // Unreachable.
	}
}
void Compiler::GetNew(bool canAssign) {
	ConsumeToken(TOKEN_IDENTIFIER, "Expect class name.");
	NamedVariable(parser.Previous, false);

	uint8_t argCount = 0;
	if (MatchToken(TOKEN_LEFT_PAREN)) {
		argCount = GetArgumentList();
	}
	EmitBytes(OP_NEW, argCount);
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
	ConsumeToken(TOKEN_IDENTIFIER, "Expect property name.");
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
	EmitByte(OP_PRINT);
}
void Compiler::GetExpressionStatement() {
	GetExpression();
	EmitByte(OP_POP);
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
		ConsumeToken(TOKEN_IDENTIFIER, "Expect variable name.");
		variableToken = parser.Previous;
		if (MatchToken(TOKEN_COMMA)) {
			ConsumeToken(TOKEN_IDENTIFIER, "Expect variable name.");
			remaining = AddLocal(parser.Previous);
			MarkInitialized();
		}
	}

	ConsumeToken(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	if (!remaining) {
		remaining = AddHiddenLocal("$remaining", 11);
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

	switch_case case_info;
	case_info.IsDefault = true;
	case_info.CasePosition = CodePointer();

	SwitchJumpListStack.top()->push_back(case_info);
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
		ConsumeToken(TOKEN_IDENTIFIER, "Expect receiver name.");

		receiverName = parser.Previous;

		// Turns out we're using 'as', so rename "other" to the
		// true receiver name
		RenameLocal(&Locals[otherSlot], receiverName);

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
		GetExpressionStatement();
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

	// Variable name
	ConsumeToken(TOKEN_IDENTIFIER, "Expect variable name.");

	Token variableToken = parser.Previous;

	ConsumeToken(TOKEN_IN, "Expect 'in' after variable name.");

	// Iterator after 'in'
	GetExpression();

	// Add a local for the object to be iterated
	// The programmer cannot refer to it by name, so it begins with
	// a dollar sign. The value in it is what GetExpression() left
	// on the top of the stack
	int iterObj = AddHiddenLocal("$iterObj", 8);

	// Add a local for the iteration state
	// Its initial value is null
	EmitByte(OP_NULL);

	int iterValue = AddHiddenLocal("$iterValue", 10);

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

	// End new scope
	ScopeEnd();
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
	else {
		GetExpressionStatement();
	}
}
// Reading declarations
int Compiler::GetFunction(int type, string className) {
	int index = (int)Compiler::Functions.size();

	Compiler* compiler = new Compiler;
	compiler->ClassName = className;
	compiler->Initialize(this, 1, type);

	// Compile the parameter list.
	compiler->ConsumeToken(TOKEN_LEFT_PAREN, "Expect '(' after function name.");

	bool isOptional = false;

	if (!compiler->CheckToken(TOKEN_RIGHT_PAREN)) {
		do {
			if (!isOptional && compiler->MatchToken(TOKEN_LEFT_SQUARE_BRACE)) {
				isOptional = true;
			}

			compiler->ParseVariable("Expect parameter name.", false);
			compiler->DefineVariableToken(parser.Previous, false);

			compiler->Function->Arity++;
			if (compiler->Function->Arity > 255) {
				compiler->Error("Cannot have more than 255 parameters.");
			}

			if (!isOptional) {
				compiler->Function->MinArity++;
			}
			else if (compiler->MatchToken(TOKEN_RIGHT_SQUARE_BRACE)) {
				break;
			}
		} while (compiler->MatchToken(TOKEN_COMMA));
	}

	compiler->ConsumeToken(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

	// The body.
	compiler->ConsumeToken(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
	compiler->GetBlockStatement();

	compiler->Finish();

	delete compiler;

	return index;
}
int Compiler::GetFunction(int type) {
	return GetFunction(type, "");
}
void Compiler::GetMethod(Token className) {
	ConsumeToken(TOKEN_IDENTIFIER, "Expect method name.");
	Token constantToken = parser.Previous;

	// If the method has the same name as its class, it's an
	// initializer.
	int type = TYPE_METHOD;
	if (IdentifiersEqual(&className, &parser.Previous)) {
		type = TYPE_CONSTRUCTOR;
	}

	int index = GetFunction(type, className.ToString());

	EmitByte(OP_METHOD);
	EmitByte(index);
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
		if (pre + GetTotalOpcodeSize(CurrentChunk()->Code + pre) == CodePointer() &&
			GetEmittedConstant(CurrentChunk(), CurrentChunk()->Code + pre, &value)) {
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
			ModuleConstants.push_back({token, 0, false, false, true, value});
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
			if (pre + GetTotalOpcodeSize(CurrentChunk()->Code + pre) == CodePointer() &&
				GetEmittedConstant(
					CurrentChunk(), CurrentChunk()->Code + pre, &value)) {
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
	ConsumeToken(TOKEN_IDENTIFIER, "Expect class name.");

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
		ConsumeToken(TOKEN_IDENTIFIER, "Expect base class name.");
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
				if (pre + GetTotalOpcodeSize(CurrentChunk()->Code + pre) !=
						CodePointer() ||
					!GetEmittedConstant(CurrentChunk(),
						CurrentChunk()->Code + pre,
						&current)) {
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
						{token, 0, false, false, true, current});
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
		ConsumeToken(TOKEN_IDENTIFIER, "Expected namespace name.");
		Token nsName = parser.Previous;
		EmitByte(OP_USE_NAMESPACE);
		EmitStringHash(nsName);
	} while (MatchToken(TOKEN_COMMA));

	ConsumeToken(TOKEN_SEMICOLON, "Expected \";\" after \"using\" declaration.");
}
void Compiler::GetEventDeclaration() {
	ConsumeToken(TOKEN_IDENTIFIER, "Expected event name.");
	Token constantToken = parser.Previous;

	// FIXME: We don't work with closures and upvalues yet, so
	// we still have to declare functions globally regardless of
	// scope.

	// if (ScopeDepth > 0) {
	//     DeclareVariable(&constantToken);
	//     MarkInitialized();
	// }

	int index = GetFunction(TYPE_FUNCTION);

	EmitByte(OP_EVENT);
	EmitByte(index);

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

	if (parser.PanicMode) {
		SynchronizeToken();
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
	//
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

	if (DoOptimizations) {
		preConstant = CheckPrefixOptimize(preCount, preConstant, prefixRule);
	}

	while (precedence <= GetRule(parser.Current.Type)->Precedence) {
		AdvanceToken();
		ParseFn infixRule = GetRule(parser.Previous.Type)->Infix;
		if (infixRule) {
			(this->*infixRule)(canAssign);
		}
		if (DoOptimizations) {
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
		if (DoOptimizations && (i == 0 || i == 1)) {
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
bool Compiler::GetEmittedConstant(Chunk* chunk, Uint8* code, VMValue* value, int* index) {
	if (index) {
		*index = -1;
	}
	switch (*code) {
	case OP_CONSTANT:
		if (value) {
			*value = (*chunk->Constants)[*(Uint32*)(code + 1)];
		}
		if (index) {
			*index = *(Uint32*)(code + 1);
		}
		return true;
	case OP_FALSE:
	case OP_TRUE:
		if (value) {
			*value = INTEGER_VAL(*code == OP_FALSE ? 0 : 1);
		}
		return true;
	case OP_NULL:
		if (value) {
			*value = NULL_VAL;
		}
		return true;
	case OP_INTEGER:
		if (value) {
			*value = INTEGER_VAL(*(Sint32*)(code + 1));
		}
		return true;
	case OP_DECIMAL:
		if (value) {
			*value = DECIMAL_VAL(*(float*)(code + 1));
		}
		return true;
	}

	return false;
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
		EmitBytes(OP_GET_LOCAL,
			0); // return the new instance built from the
		// constructor
	}
	else {
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
		if (IS_STRING(constant) && IS_STRING(value) &&
			ScriptManager::ValuesSortaEqual(constant, value)) {
			return (int)i;
		}
		if (ValuesEqual(value, constant)) {
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
	///////////
	// printf("------PrefixOptimize @ %d %d\n", preCount,
	// preConstant); for (int i = preCount; i <
	// CurrentChunk()->Count;)
	//     i = DebugInstruction(CurrentChunk(), i);
	///////////

	int checkConstant = -1;
	VMValue out = NULL_VAL;

	if (fn == &Compiler::GetUnary) {
		// printf("GetUnary\n");

		Uint8 unOp = CurrentChunk()->Code[CodePointer() - 1];
		if (unOp == OP_TYPEOF) {
			return preConstant;
		}
		Uint8 op = CurrentChunk()->Code[preCount];
		VMValue constant;
		if (preCount + GetTotalOpcodeSize(CurrentChunk()->Code + preCount) !=
				CodePointer() - 1 ||
			!GetEmittedConstant(CurrentChunk(),
				CurrentChunk()->Code + preCount,
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
		int off2 = GetTotalOpcodeSize(CurrentChunk()->Code + off1) + off1;
		if (off2 >= CodePointer()) {
			return preConstant;
		}
		Uint8 op2 = CurrentChunk()->Code[off2];
		int offB = GetTotalOpcodeSize(CurrentChunk()->Code + off2) + off2;
		if (offB != CodePointer() - 1) { // CHANGE TO >= ONCE
			// CASCADING IS ADDED
			return preConstant;
		}
		Uint8 opB = CurrentChunk()->Code[offB];

		VMValue a;
		int checkConstantA = -1;
		VMValue b;
		int checkConstantB = -1;

		if (!GetEmittedConstant(
			    CurrentChunk(), CurrentChunk()->Code + off1, &a, &checkConstantA)) {
			return preConstant;
		}

		if (!GetEmittedConstant(
			    CurrentChunk(), CurrentChunk()->Code + off2, &b, &checkConstantB)) {
			return preConstant;
		}

		VMValue out;

		switch (opB) {
			// Numeric Operations
		case OP_ADD: {
			if (IS_STRING(a) || IS_STRING(b)) {
				VMValue str_b = ScriptManager::CastValueAsString(b);
				VMValue str_a = ScriptManager::CastValueAsString(a);
				out = ScriptManager::Concatenate(str_a, str_b);
				break;
			}
			else if (IS_NOT_NUMBER(a) || IS_NOT_NUMBER(b)) {
				return preConstant;
			}

			if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
				float a_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(a));
				float b_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(b));
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
				float a_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(a));
				float b_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(b));
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
				float a_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(a));
				float b_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(b));
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
				float a_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(a));
				float b_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(b));

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
				float a_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(a));
				float b_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(b));
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
				float a_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(a));
				float b_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(b));
				out = DECIMAL_VAL((float)((int)a_d << (int)b_d));
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
				float a_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(a));
				float b_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(b));
				out = DECIMAL_VAL((float)((int)a_d >> (int)b_d));
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
				float a_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(a));
				float b_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(b));
				out = DECIMAL_VAL((float)((int)a_d | (int)b_d));
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
				float a_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(a));
				float b_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(b));
				out = DECIMAL_VAL((float)((int)a_d & (int)b_d));
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
				float a_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(a));
				float b_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(b));
				out = DECIMAL_VAL((float)((int)a_d ^ (int)b_d));
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
			bool equal = ScriptManager::ValuesSortaEqual(a, b);
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
				float a_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(a));
				float b_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(b));
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
				float a_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(a));
				float b_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(b));
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
				float a_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(a));
				float b_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(b));
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
				float a_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(a));
				float b_d = AS_DECIMAL(ScriptManager::CastValueAsDecimal(b));
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
		// Log::PrintSimple("Constants eaten: %d %d\n",
		// checkConstantA, checkConstantB);

		preConstant = CurrentChunk()->Constants->size();
		EmitConstant(out);
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

int Compiler::GetTotalOpcodeSize(uint8_t* op) {
	switch (*op) {
		// ConstantInstruction
	case OP_CONSTANT:
	case OP_INTEGER:
	case OP_DECIMAL:
	case OP_IMPORT:
	case OP_IMPORT_MODULE:
		return 5;
	case OP_NULL:
	case OP_TRUE:
	case OP_FALSE:
	case OP_POP:
	case OP_INCREMENT:
	case OP_DECREMENT:
	case OP_BITSHIFT_LEFT:
	case OP_BITSHIFT_RIGHT:
	case OP_EQUAL:
	case OP_EQUAL_NOT:
	case OP_LESS:
	case OP_LESS_EQUAL:
	case OP_GREATER:
	case OP_GREATER_EQUAL:
	case OP_ADD:
	case OP_SUBTRACT:
	case OP_MULTIPLY:
	case OP_MODULO:
	case OP_DIVIDE:
	case OP_BW_NOT:
	case OP_BW_AND:
	case OP_BW_OR:
	case OP_BW_XOR:
	case OP_LG_NOT:
	case OP_LG_AND:
	case OP_LG_OR:
	case OP_GET_ELEMENT:
	case OP_SET_ELEMENT:
	case OP_NEGATE:
	case OP_PRINT:
	case OP_TYPEOF:
	case OP_RETURN:
	case OP_SAVE_VALUE:
	case OP_LOAD_VALUE:
	case OP_GET_SUPERCLASS:
	case OP_DEFINE_MODULE_LOCAL:
	case OP_ENUM_NEXT:
		return 1;
	case OP_COPY:
	case OP_CALL:
	case OP_NEW:
	case OP_EVENT:
	case OP_POPN:
		return 2;
	case OP_GET_LOCAL:
	case OP_SET_LOCAL:
		return 2;
	case OP_GET_GLOBAL:
	case OP_DEFINE_GLOBAL:
	case OP_DEFINE_CONSTANT:
	case OP_SET_GLOBAL:
	case OP_GET_PROPERTY:
	case OP_SET_PROPERTY:
	case OP_HAS_PROPERTY:
	case OP_USE_NAMESPACE:
	case OP_INHERIT:
		return 5;
	case OP_SET_MODULE_LOCAL:
	case OP_GET_MODULE_LOCAL:
		return 3;
	case OP_NEW_ARRAY:
	case OP_NEW_MAP:
		return 5;
	case OP_JUMP:
	case OP_JUMP_IF_FALSE:
	case OP_JUMP_BACK:
		return 3;
	case OP_INVOKE:
		return 7;
	case OP_WITH:
		if (*(op + 1) == 3) {
			return 5;
		}
		return 4;
	case OP_CLASS:
		return 6;
	case OP_ADD_ENUM:
	case OP_NEW_ENUM:
		return 5;
	case OP_METHOD:
		return 6;
	}
	return 1;
}

// Debugging functions
int Compiler::HashInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	uint32_t hash = *(uint32_t*)&chunk->Code[offset + 1];
	Log::PrintSimple("%-16s #%08X", opcodeNames[opcode], hash);
	if (TokenMap->Exists(hash)) {
		Token t = TokenMap->Get(hash);
		Log::PrintSimple(" (%.*s)", (int)t.Length, t.Start);
	}
	Log::PrintSimple("\n");
	return offset + GetTotalOpcodeSize(chunk->Code + offset);
}
int Compiler::ConstantInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	int constant;
	VMValue value;
	if (GetEmittedConstant(chunk, chunk->Code + offset, &value, &constant)) {
		if (constant != -1) {
			Log::PrintSimple("%-16s %9d '", opcodeNames[opcode], constant);
		}
		else {
			Log::PrintSimple("%-16s           '", opcodeNames[opcode]);
		}
	}
	else {
		constant = *(int*)&chunk->Code[offset + 1];
		value = (*chunk->Constants)[constant];
		Log::PrintSimple("%-16s %9d '", opcodeNames[opcode], constant);
	}
	Values::PrintValue(NULL, value);
	Log::PrintSimple("'\n");
	return offset + GetTotalOpcodeSize(chunk->Code + offset);
}
int Compiler::SimpleInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	Log::PrintSimple("%s\n", opcodeNames[opcode]);
	return offset + GetTotalOpcodeSize(chunk->Code + offset);
}
int Compiler::ByteInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	Log::PrintSimple("%-16s %9d\n", opcodeNames[opcode], chunk->Code[offset + 1]);
	return offset + GetTotalOpcodeSize(chunk->Code + offset);
}
int Compiler::ShortInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	uint16_t data = (uint16_t)(chunk->Code[offset + 1]);
	data |= chunk->Code[offset + 2] << 8;
	Log::PrintSimple("%-16s %9d\n", opcodeNames[opcode], data);
	return offset + GetTotalOpcodeSize(chunk->Code + offset);
}
int Compiler::LocalInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	uint8_t slot = chunk->Code[offset + 1];
	if (slot > 0) {
		Log::PrintSimple("%-16s %9d\n", opcodeNames[opcode], slot);
	}
	else {
		Log::PrintSimple("%-16s %9d 'this'\n", opcodeNames[opcode], slot);
	}
	return offset + GetTotalOpcodeSize(chunk->Code + offset);
}
int Compiler::MethodInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	uint8_t slot = chunk->Code[offset + 1];
	uint32_t hash = *(uint32_t*)&chunk->Code[offset + 2];
	Log::PrintSimple("%-13s %2d", opcodeNames[opcode], slot);
	Log::PrintSimple(" #%08X", hash);
	if (TokenMap->Exists(hash)) {
		Token t = TokenMap->Get(hash);
		Log::PrintSimple(" (%.*s)", (int)t.Length, t.Start);
	}
	Log::PrintSimple("\n");
	return offset + GetTotalOpcodeSize(chunk->Code + offset);
}
int Compiler::InvokeInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	return Compiler::MethodInstruction(opcode, chunk, offset);
}
int Compiler::JumpInstruction(uint8_t opcode, int sign, Chunk* chunk, int offset) {
	uint16_t jump = (uint16_t)(chunk->Code[offset + 1]);
	jump |= chunk->Code[offset + 2] << 8;
	Log::PrintSimple(
		"%-16s %9d -> %d\n", opcodeNames[opcode], offset, offset + 3 + sign * jump);
	return offset + GetTotalOpcodeSize(chunk->Code + offset);
}
int Compiler::ClassInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	return Compiler::HashInstruction(opcode, chunk, offset);
}
int Compiler::EnumInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	return Compiler::HashInstruction(opcode, chunk, offset);
}
int Compiler::WithInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	uint8_t type = chunk->Code[offset + 1];
	uint8_t slot = 0;
	if (type == 3) {
		slot = chunk->Code[offset + 2];
		offset++;
	}
	uint16_t jump = (uint16_t)(chunk->Code[offset + 2]);
	jump |= chunk->Code[offset + 3] << 8;
	if (slot > 0) {
		Log::PrintSimple("%-16s %1d %7d -> %d\n", opcodeNames[opcode], type, slot, jump);
	}
	else {
		Log::PrintSimple(
			"%-16s %1d %7d 'this' -> %d\n", opcodeNames[opcode], type, slot, jump);
	}
	if (type == 3) {
		offset--;
	}
	return offset + GetTotalOpcodeSize(chunk->Code + offset);
}
int Compiler::DebugInstruction(Chunk* chunk, int offset) {
	Log::PrintSimple("%04d ", offset);
	if (offset > 0 && (chunk->Lines[offset] & 0xFFFF) == (chunk->Lines[offset - 1] & 0xFFFF)) {
		Log::PrintSimple("   | ");
	}
	else {
		Log::PrintSimple("%4d ", chunk->Lines[offset] & 0xFFFF);
	}

	uint8_t instruction = chunk->Code[offset];
	switch (instruction) {
	case OP_CONSTANT:
	case OP_INTEGER:
	case OP_DECIMAL:
	case OP_IMPORT:
	case OP_IMPORT_MODULE:
		return ConstantInstruction(instruction, chunk, offset);
	case OP_NULL:
	case OP_TRUE:
	case OP_FALSE:
	case OP_POP:
	case OP_INCREMENT:
	case OP_DECREMENT:
	case OP_BITSHIFT_LEFT:
	case OP_BITSHIFT_RIGHT:
	case OP_EQUAL:
	case OP_EQUAL_NOT:
	case OP_LESS:
	case OP_LESS_EQUAL:
	case OP_GREATER:
	case OP_GREATER_EQUAL:
	case OP_ADD:
	case OP_SUBTRACT:
	case OP_MULTIPLY:
	case OP_MODULO:
	case OP_DIVIDE:
	case OP_BW_NOT:
	case OP_BW_AND:
	case OP_BW_OR:
	case OP_BW_XOR:
	case OP_LG_NOT:
	case OP_LG_AND:
	case OP_LG_OR:
	case OP_GET_ELEMENT:
	case OP_SET_ELEMENT:
	case OP_NEGATE:
	case OP_PRINT:
	case OP_TYPEOF:
	case OP_RETURN:
	case OP_SAVE_VALUE:
	case OP_LOAD_VALUE:
	case OP_GET_SUPERCLASS:
	case OP_DEFINE_MODULE_LOCAL:
	case OP_ENUM_NEXT:
		return SimpleInstruction(instruction, chunk, offset);
	case OP_COPY:
	case OP_CALL:
	case OP_NEW:
	case OP_EVENT:
	case OP_POPN:
		return ByteInstruction(instruction, chunk, offset);
	case OP_GET_LOCAL:
	case OP_SET_LOCAL:
		return LocalInstruction(instruction, chunk, offset);
	case OP_GET_GLOBAL:
	case OP_DEFINE_GLOBAL:
	case OP_DEFINE_CONSTANT:
	case OP_SET_GLOBAL:
	case OP_GET_PROPERTY:
	case OP_SET_PROPERTY:
	case OP_HAS_PROPERTY:
	case OP_USE_NAMESPACE:
	case OP_INHERIT:
		return HashInstruction(instruction, chunk, offset);
	case OP_SET_MODULE_LOCAL:
	case OP_GET_MODULE_LOCAL:
		return ShortInstruction(instruction, chunk, offset);
	case OP_NEW_ARRAY:
	case OP_NEW_MAP:
		return SimpleInstruction(instruction, chunk, offset);
	case OP_JUMP:
	case OP_JUMP_IF_FALSE:
		return JumpInstruction(instruction, 1, chunk, offset);
	case OP_JUMP_BACK:
		return JumpInstruction(instruction, -1, chunk, offset);
	case OP_INVOKE:
		return InvokeInstruction(instruction, chunk, offset);

	case OP_PRINT_STACK: {
		offset++;
		uint8_t constant = chunk->Code[offset++];
		Log::PrintSimple("%-16s %4d ", opcodeNames[instruction], constant);
		Values::PrintValue(NULL, (*chunk->Constants)[constant]);
		Log::PrintSimple("\n");

		ObjFunction* function = AS_FUNCTION((*chunk->Constants)[constant]);
		for (int j = 0; j < function->UpvalueCount; j++) {
			int isLocal = chunk->Code[offset++];
			int index = chunk->Code[offset++];
			Log::PrintSimple("%04d   |                     %s %d\n",
				offset - 2,
				isLocal ? "local" : "upvalue",
				index);
		}

		return offset;
	}
	case OP_WITH:
		return WithInstruction(instruction, chunk, offset);
	case OP_CLASS:
		return ClassInstruction(instruction, chunk, offset);
	case OP_ADD_ENUM:
	case OP_NEW_ENUM:
		return EnumInstruction(instruction, chunk, offset);
	case OP_METHOD:
		return MethodInstruction(instruction, chunk, offset);
	default:
		if (instruction < OP_LAST) {
			Log::PrintSimple("No viewer for opcode %s\n", opcodeNames[instruction]);
		}
		else {
			Log::PrintSimple("Unknown opcode %d\n", instruction);
		}
		return chunk->Count + 1;
	}
}
void Compiler::DebugChunk(Chunk* chunk, const char* name, int minArity, int maxArity) {
	int optArgCount = maxArity - minArity;
	if (optArgCount) {
		Log::PrintSimple(
			"== %s (argCount: %d, optArgCount: %d) ==\n", name, maxArity, optArgCount);
	}
	else {
		Log::PrintSimple("== %s (argCount: %d) ==\n", name, maxArity);
	}
	Log::PrintSimple("byte   ln\n");
	for (int offset = 0; offset < chunk->Count;) {
		offset = DebugInstruction(chunk, offset);
	}

	Log::PrintSimple("\nConstants: (%d count)\n", (int)(*chunk->Constants).size());
	for (size_t i = 0; i < (*chunk->Constants).size(); i++) {
		Log::PrintSimple(" %2d '", (int)i);
		Values::PrintValue(NULL, (*chunk->Constants)[i]);
		Log::PrintSimple("'\n");
	}
}

// Compiling
void Compiler::Init() {
	Compiler::MakeRules();

	Compiler::DoLogging = false;
	Compiler::ShowWarnings = false;
#if DEVELOPER_MODE
	Compiler::WriteDebugInfo = true;
	Compiler::WriteSourceFilename = true;
#else
	Compiler::WriteDebugInfo = false;
	Compiler::WriteSourceFilename = false;
#endif
	Compiler::DoOptimizations = true;

	Application::Settings->GetBool("compiler", "log", &Compiler::DoLogging);
	if (Compiler::DoLogging) {
		Application::Settings->GetBool("compiler", "showWarnings", &Compiler::ShowWarnings);
	}

	Application::Settings->GetBool("compiler", "writeDebugInfo", &Compiler::WriteDebugInfo);
	Application::Settings->GetBool(
		"compiler", "writeSourceFilename", &Compiler::WriteSourceFilename);
	Application::Settings->GetBool("compiler", "optimizations", &Compiler::DoOptimizations);
}
void Compiler::GetStandardConstants() {
	if (Compiler::StandardConstants == NULL) {
		Compiler::StandardConstants =
			new HashMap<VMValue>(NULL, ScriptManager::Constants->Capacity);
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
	Enclosing = enclosing;
	Function = NewFunction();
	UnusedVariables = new vector<Local>();
	UnsetVariables = new vector<Local>();
	Compiler::Functions.push_back(Function);

	switch (type) {
	case TYPE_CONSTRUCTOR:
	case TYPE_METHOD:
	case TYPE_FUNCTION:
		Function->Name = CopyString(parser.Previous.Start, parser.Previous.Length);
		break;
	case TYPE_TOP_LEVEL:
		Function->Name = CopyString("main", 4);
		break;
	}

	Local* local = &Locals[LocalCount++];
	local->Depth = ScopeDepth;

	if (HasThis()) {
		// In a method, it holds the receiver, "this".
		SetReceiverName("this");
	}
	else {
		// In a function, it holds the function, but cannot be
		// referenced, so it has no name.
		SetReceiverName("");
	}
}
void Compiler::WriteBytecode(Stream* stream, const char* filename) {
	Bytecode* bytecode = new Bytecode();

	for (size_t i = 0; i < Compiler::Functions.size(); i++) {
		bytecode->Functions.push_back(Compiler::Functions[i]);
	}

	bytecode->HasDebugInfo = Compiler::WriteDebugInfo;
	bytecode->Write(stream, Compiler::WriteSourceFilename ? filename : nullptr, TokenMap);

	delete bytecode;

	if (TokenMap) {
		TokenMap->Clear();
	}
}
bool Compiler::Compile(const char* filename, const char* source, Stream* output) {
	bool debugCompiler = false;
	Application::Settings->GetBool("dev", "debugCompiler", &debugCompiler);

	scanner.Line = 1;
	scanner.Start = (char*)source;
	scanner.Current = (char*)source;
	scanner.LinePos = (char*)source;
	scanner.SourceFilename = (char*)filename;

	parser.HadError = false;
	parser.PanicMode = false;

	Initialize(NULL, 0, TYPE_TOP_LEVEL);

	AdvanceToken();
	while (!MatchToken(TOKEN_EOF)) {
		GetDeclaration();
	}

	ConsumeToken(TOKEN_EOF, "Expected end of file.");

	for (size_t i = 0; i < Compiler::ModuleLocals.size(); i++) {
		if (UnusedVariables && !Compiler::ModuleLocals[i].Resolved) {
			UnusedVariables->insert(
				UnusedVariables->begin(), Compiler::ModuleLocals[i]);
		}
		else if (UnsetVariables &&
			Compiler::ModuleLocals[i].ConstantVal.Type != VAL_ERROR &&
			!Compiler::ModuleLocals[i].WasSet) {
			UnsetVariables->insert(UnsetVariables->begin(), Compiler::ModuleLocals[i]);
		}
	}

	Finish();

	for (size_t c = 0; c < Compiler::Functions.size(); c++) {
		Chunk* chunk = &Compiler::Functions[c]->Chunk;
		chunk->OpcodeCount = 0;
		for (int offset = 0; offset < chunk->Count;) {
			offset += GetTotalOpcodeSize(chunk->Code + offset);
			chunk->OpcodeCount++;
		}
	}

	if (debugCompiler) {
		for (size_t c = 0; c < Compiler::Functions.size(); c++) {
			Chunk* chunk = &Compiler::Functions[c]->Chunk;
			DebugChunk(chunk,
				Compiler::Functions[c]->Name->Chars,
				Compiler::Functions[c]->MinArity,
				Compiler::Functions[c]->Arity);
			Log::PrintSimple("\n");
		}
	}

	if (output) {
		WriteBytecode(output, filename);
	}

	for (size_t c = 0; c < Compiler::Functions.size(); c++) {
		ObjFunction* func = Compiler::Functions[c];
		func->Chunk.Free();
	}

	return !parser.HadError;
}
void Compiler::Finish() {
	if (UnusedVariables || UnsetVariables) {
		WarnVariablesUnusedUnset();
		if (UnusedVariables) {
			delete UnusedVariables;
		}
		if (UnsetVariables) {
			delete UnsetVariables;
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
	Memory::Free(Rules);
}
