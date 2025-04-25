#ifndef ENGINE_UTILITIES_STRINGUTILS_H
#define ENGINE_UTILITIES_STRINGUTILS_H

#include <Engine/Includes/Standard.h>
#include <Engine/Includes/Token.h>

namespace StringUtils {
//public:
	char* Create(void* src, size_t length);
	char* Create(std::string src);
	char* Create(Token token);
	char* Duplicate(const char* src);
	char* Duplicate(const char* src, size_t length);
	char* Resize(char* src, size_t length);
	bool WildcardMatch(const char* first, const char* second);
	bool StartsWith(const char* string, const char* compare);
	bool StartsWith(std::string string, std::string compare);
	char* StrCaseStr(const char* haystack, const char* needle);
	size_t Copy(char* dst, const char* src, size_t sz);
	size_t Copy(char* dst, std::string src, size_t sz);
	size_t Concat(char* dst, const char* src, size_t sz);
	bool ToNumber(int* dst, const char* src);
	bool ToDecimal(double* dst, const char* src);
	bool ToNumber(int* dst, string src);
	bool ToDecimal(double* dst, string src);
	bool HexToUint32(Uint32* dst, const char* hexstr);
	char* GetPath(const char* filename);
	const char* GetFilename(const char* filename);
	const char* GetExtension(const char* filename);
	char* ReplacePathSeparators(const char* path);
	char* NormalizePath(const char* path);
	void NormalizePath(const char* path, char* dest, size_t destSize);
	void ReplacePathSeparatorsInPlace(char* path);
};

#endif /* ENGINE_UTILITIES_STRINGUTILS_H */
