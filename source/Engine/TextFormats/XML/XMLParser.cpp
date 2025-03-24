#include <Engine/TextFormats/XML/XMLParser.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/IO/TextStream.h>
#include <Engine/Includes/Token.h>
#include <Engine/Utilities/StringUtils.h>

Parser parser;
Scanner scanner;

bool IsEOF() {
	return *scanner.Current == 0;
}
bool IsDigit(int c) {
	return c >= '0' && c <= '9';
}
bool IsHexDigit(int c) {
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}
bool IsAlpha(int c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
bool IsIdentifierStart(int c) {
	return IsAlpha(c) || c == ':' || c == '_' || c >= 0x80;
}
bool IsIdentifierBody(int c) {
	return IsIdentifierStart(c) || c == '-' || c == '.' || ('0' <= c && c <= '9') ||
		c == 0xB7 || (0x0300 <= c && c <= 0x036F) || (0x203F <= c && c <= 0x2040);
}

bool MatchChar(int expected) {
	if (IsEOF()) {
		return false;
	}
	if (*scanner.Current != expected) {
		return false;
	}

	scanner.Current++;
	return true;
}
char AdvanceChar() {
	return *scanner.Current++;
	// scanner.Current++;
	// return *(scanner.Current - 1);
}
char PrevChar() {
	return *(scanner.Current - 1);
}
char PeekChar() {
	return *scanner.Current;
}
char PeekNextChar() {
	if (IsEOF()) {
		return 0;
	}
	return *(scanner.Current + 1);
}

void SkipWhitespace() {
	while (true) {
		char c = PeekChar();
		switch (c) {
		case ' ':
		case '\r':
		case '\t':
			AdvanceChar();
			break;

		case '\n':
			scanner.Line++;
			AdvanceChar();
			scanner.LinePos = scanner.Current;
			break;

		default:
			return;
		}
	}
}

// Token functions
enum {
	// Single-character tokens.
	TOKEN_LEFT_BRACE,
	TOKEN_RIGHT_BRACE,
	TOKEN_BANG,
	TOKEN_QUESTION,
	TOKEN_DASH,
	TOKEN_SLASH,
	TOKEN_EQUAL,
	// Others
	TOKEN_IDENTIFIER,
	TOKEN_STRING,
	TOKEN_NUMBER,

	TOKEN_ERROR,
	TOKEN_EOF,

	// XML Tokens
	TOKEN_STARTTAG,
	TOKEN_ENDTAG,

	TOKEN_CHARACTER,
	TOKEN_CDATA,

	TOKEN_INSTRUCTION,
	TOKEN_DOCTYPE,
	TOKEN_COMMENT,
};
Token MakeToken(int type) {
	Token token;
	token.Type = type;
	token.Start = scanner.Start;
	token.Length = (int)(scanner.Current - scanner.Start);
	token.Line = scanner.Line;
	token.Pos = (scanner.Start - scanner.LinePos) + 1;

	return token;
}
Token MakeTokenRaw(int type, const char* message) {
	Token token;
	token.Type = type;
	token.Start = (char*)message;
	token.Length = (int)strlen(message);
	token.Line = 0;
	token.Pos = scanner.Current - scanner.LinePos;

	return token;
}
Token ErrorToken(const char* message) {
	Token token;
	token.Type = TOKEN_ERROR;
	token.Start = (char*)message;
	token.Length = (int)strlen(message);
	token.Line = scanner.Line;
	token.Pos = scanner.Current - scanner.LinePos;

	return token;
}

int CheckKeyword(int start, int length, const char* rest, int type) {
	if (scanner.Current - scanner.Start == start + length &&
		memcmp(scanner.Start + start, rest, length) == 0) {
		return type;
	}

	return TOKEN_IDENTIFIER;
}
int GetKeywordType() {
	return TOKEN_IDENTIFIER;
}

Token StringToken() {
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
	Token tok = MakeToken(TOKEN_STRING);
	tok.Start += 1;
	tok.Length -= 2;
	return tok;
}
Token NumberToken() {
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

		return MakeToken(TOKEN_NUMBER);
	}

	return MakeToken(TOKEN_NUMBER);
}
Token IdentifierToken() {
	while (IsIdentifierBody(PeekChar())) {
		AdvanceChar();
	}

	return MakeToken(GetKeywordType());
}
Token ScanToken() {
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
	case '<':
		return MakeToken(TOKEN_LEFT_BRACE);
	case '>':
		return MakeToken(TOKEN_RIGHT_BRACE);
	case '!':
		return MakeToken(TOKEN_BANG);
	case '?':
		return MakeToken(TOKEN_QUESTION);
	case '-':
		return MakeToken(TOKEN_DASH);
	case '/':
		return MakeToken(TOKEN_SLASH);
	case '=':
		return MakeToken(TOKEN_EQUAL);
	// String
	case '"':
		return StringToken();
	}

	return ErrorToken("Unexpected character.");
}

void AdvanceToken() {
	parser.Previous = parser.Current;

	while (true) {
		parser.Current = ScanToken();
		if (parser.Current.Type != TOKEN_ERROR) {
			break;
		}

		// ErrorAtCurrent(parser.Current.Start);
	}
}
Token NextToken() {
	AdvanceToken();
	return parser.Previous;
}
Token PeekToken() {
	return parser.Current;
}
Token PrevToken() {
	return parser.Previous;
}
bool CheckToken(int expectedType) {
	if (parser.Current.Type == TOKEN_EOF) {
		return false;
	}
	// if (parser.Current.Type != expectedType) return false;
	// return true;
	return parser.Current.Type == expectedType;
}
bool _MatchToken(int expectedType) {
	if (!CheckToken(expectedType)) {
		return false;
	}
	AdvanceToken();
	return true;
}
void ConsumeToken(int type, const char* message) {
	if (parser.Current.Type == type) {
		AdvanceToken();
		return;
	}

	// ErrorAtCurrent(message);
	Log::Print(Log::LOG_ERROR,
		"%s\n\n%d\n\n%s\n",
		message,
		(int)(scanner.Current - scanner.SourceStart),
		scanner.SourceStart);
	exit(-1);
}
void PrintToken(Token token) {
	printf("%.*s", (int)token.Length, token.Start);
	printf("\n");
	// exit(-1);
}

void SynchronizeToken() {
	parser.PanicMode = false;

	while (PeekToken().Type != TOKEN_EOF) {
		// if (PrevToken().Type == TOKEN_SEMICOLON) return;

		switch (PeekToken().Type) {
		default:
			break;
		}

		AdvanceToken();
	}
}

XMLNode* XMLCurrent = NULL;

void GetAttributes() {
	while (PeekToken().Type == TOKEN_IDENTIFIER) {
		ConsumeToken(TOKEN_IDENTIFIER, "Invalid identifier for attribute.");

		Token name = PrevToken();
		name.Type = TOKEN_CDATA;

		bool strict_xml = true;
		if (strict_xml) {
			ConsumeToken(TOKEN_EQUAL, "Missing '=' after attribute name.");
			ConsumeToken(TOKEN_STRING, "Invalid token for attribute value.");

			Token value = PrevToken();
			XMLCurrent->attributes.Put(XMLParser::TokenToString(name), value);
		}

		if (PeekToken().Type != TOKEN_IDENTIFIER) {
			break;
		}
	}
}
void GetStart() {
	XMLNode* node = new (std::nothrow) XMLNode;
	if (!node) {
		return;
	}

	node->parent = XMLCurrent;
	node->base_stream = NULL;
	XMLCurrent->children.push_back(node);
	XMLCurrent = node;

	// Get instruction name
	Token name = NextToken();
	name.Type = TOKEN_STARTTAG;
	XMLCurrent->name = name;

	GetAttributes();

	if (_MatchToken(TOKEN_SLASH)) {
		PrevToken();
		// Token end = PrevToken();
		// end.Type = TOKEN_ENDTAG;
		XMLCurrent = XMLCurrent->parent;
	}
	ConsumeToken(TOKEN_RIGHT_BRACE, "Missing '>' after start.");
}
void GetEnd() {
	// Get instruction name
	NextToken();
	// Token name = NextToken();
	// name.Type = TOKEN_ENDTAG;
	XMLCurrent = XMLCurrent->parent;

	ConsumeToken(TOKEN_RIGHT_BRACE, "Missing '>' after end.");
}
void GetComment() {
	ConsumeToken(TOKEN_DASH, "Missing '-' at start of comment.");
	while (!_MatchToken(TOKEN_DASH) && !_MatchToken(TOKEN_EOF)) {
		AdvanceToken();
	}
	ConsumeToken(TOKEN_DASH, "Missing second '-' at end of comment.");
	ConsumeToken(TOKEN_RIGHT_BRACE, "Missing '>' at end of comment.");
}
void GetInstruction() {
	// Get instruction name
	NextToken();
	// Token name = NextToken();
	// name.Type = TOKEN_INSTRUCTION;

	GetAttributes();

	ConsumeToken(TOKEN_QUESTION, "Missing '?' after instruction tokens.");
	ConsumeToken(TOKEN_RIGHT_BRACE, "Missing '>' after instruction.");
}

bool XMLParser::MatchToken(Token tok, const char* string) {
	if (tok.Length == 0) {
		return false;
	}

	size_t len = strlen(string);
	if (tok.Length != len) {
		return false;
	}

	return memcmp(string, tok.Start, tok.Length) == 0;
}
float XMLParser::TokenToNumber(Token tok) {
	return atof(tok.ToString().c_str());
}

XMLNode* XMLParser::Parse() {
	XMLNode* XMLRoot = new (std::nothrow) XMLNode;
	if (!XMLRoot) {
		return NULL;
	}

	XMLRoot->base_stream = NULL;
	XMLRoot->parent = NULL;
	XMLCurrent = XMLRoot;

	AdvanceToken();
	if (!_MatchToken(TOKEN_EOF)) {
		ConsumeToken(TOKEN_LEFT_BRACE, "SXML_ERROR_XMLINVALID");

		// Instruction
		if (_MatchToken(TOKEN_QUESTION)) {
			// parse_instruction
			GetInstruction();
		}
		// Doctype
		else if (_MatchToken(TOKEN_BANG)) {
			// parse_doctype
		}
		// Start
		else {
			GetStart();
		}
	}

	while (!_MatchToken(TOKEN_EOF)) {
		if (!_MatchToken(TOKEN_LEFT_BRACE)) {
			SkipWhitespace();
			scanner.Start = scanner.Current;
			if (IsEOF()) {
				break;
			}

			while (PeekChar() != '<') {
				if (IsEOF()) {
					break;
				}
				AdvanceChar();
			}

			Token data = PeekToken();
			data.Length = scanner.Current - data.Start;
			data.Type = TOKEN_CDATA;

			XMLNode* node = new (std::nothrow) XMLNode;
			if (!node) {
				return NULL;
			}

			node->name = data;
			node->parent = XMLCurrent;
			node->base_stream = NULL;
			XMLCurrent->children.push_back(node);
			AdvanceToken();
		}
		else {
			// Instruction
			if (_MatchToken(TOKEN_QUESTION)) {
				// parse_instruction
				GetInstruction();
			}
			else if (_MatchToken(TOKEN_BANG)) {
				// Comment
				if (_MatchToken(TOKEN_DASH)) {
					GetComment();
				}
				else {
					// GetCData();
				}
			}
			// End
			else if (_MatchToken(TOKEN_SLASH)) {
				GetEnd();
			}
			// Start
			else {
				GetStart();
			}
		}
	}

	return XMLRoot;
}
XMLNode* XMLParser::ParseFromStream(TextStream* stream) {
	scanner.Line = 1;
	scanner.Start = (char*)stream->pointer;
	scanner.Current = (char*)stream->pointer;
	scanner.LinePos = (char*)stream->pointer;
	scanner.SourceStart = (char*)stream->pointer;

	parser.HadError = false;
	parser.PanicMode = false;

	XMLNode* xml = XMLParser::Parse();
	xml->base_stream = stream;
	return xml;
}
XMLNode* XMLParser::ParseFromStream(Stream* streamSrc) {
	TextStream* stream = TextStream::New(streamSrc);
	if (!stream) {
		return NULL;
	}
	return XMLParser::ParseFromStream(stream);
}
XMLNode* XMLParser::ParseFromResource(const char* filename) {
	ResourceStream* res = ResourceStream::New(filename);
	if (!res) {
		return NULL;
	}

	TextStream* textStream = TextStream::New(res);
	res->Close();

	if (!textStream) {
		return NULL;
	}

	return XMLParser::ParseFromStream(textStream);
}

char* XMLParser::TokenToString(Token tok) {
	char* string = StringUtils::Create(tok);
	if (!string) {
		Log::Print(Log::LOG_ERROR, "Out of memory converting XML token to string!");
		abort();
	}
	return string;
}

std::string XMLParser::TokenToStdString(Token tok) {
	return std::string(tok.Start, tok.Length);
}

void XMLParser::CopyTokenToString(Token tok, char* buffer, size_t size) {
	size_t length = tok.Length;
	if (length >= size) {
		length = size - 1;
	}
	memcpy(buffer, tok.Start, length);
	buffer[length] = '\0';
}

XMLNode* XMLParser::SearchNode(XMLNode* root, const char* search) {
	for (size_t i = 0; i < root->children.size(); i++) {
		XMLNode* node = root->children[i];
		if (XMLParser::MatchToken(node->name, search)) {
			return node;
		}
	}
	return nullptr;
}

void FreeNode(XMLNode* root) {
	root->attributes.Dispose();
	for (size_t i = 0; i < root->children.size(); i++) {
		FreeNode(root->children[i]);
	}
	delete root;
}

void XMLParser::Free(XMLNode* root) {
	if (root->base_stream) {
		root->base_stream->Close();
	}

	FreeNode(root);
}
