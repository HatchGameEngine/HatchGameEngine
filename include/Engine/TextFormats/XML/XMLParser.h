#ifndef ENGINE_TEXTFORMATS_XML_XMLPARSER_H
#define ENGINE_TEXTFORMATS_XML_XMLPARSER_H

#include <Engine/Bytecode/CompilerEnums.h>
#include <Engine/IO/Stream.h>
#include <Engine/IO/TextStream.h>
#include <Engine/Includes/HashMap.h>
#include <Engine/Includes/Standard.h>
#include <Engine/TextFormats/XML/XMLNode.h>

namespace XMLParser {
//public:
	bool MatchToken(Token tok, const char* string);
	float TokenToNumber(Token tok);
	XMLNode* Parse();
	XMLNode* ParseFromStream(TextStream* stream);
	XMLNode* ParseFromStream(Stream* streamSrc);
	XMLNode* ParseFromResource(const char* filename);
	char* TokenToString(Token tok);
	std::string TokenToStdString(Token tok);
	void CopyTokenToString(Token tok, char* buffer, size_t size);
	XMLNode* SearchNode(XMLNode* root, const char* search);
	void Free(XMLNode* root);
};

#endif /* ENGINE_TEXTFORMATS_XML_XMLPARSER_H */
