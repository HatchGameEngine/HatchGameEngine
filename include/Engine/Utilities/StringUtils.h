#ifndef ENGINE_UTILITIES_STRINGUTILS_H
#define ENGINE_UTILITIES_STRINGUTILS_H

#include <Engine/Includes/Standard.h>
#include <Engine/Includes/Token.h>

class StringUtils {
public:
    static char* Create(void* src, size_t length);
    static char* Create(string src);
    static char* Create(Token token);
    static char* Duplicate(const char* src);
    static char* Duplicate(const char* src, size_t length);
    static char* Resize(char* src, size_t length);
    static bool WildcardMatch(const char* first, const char* second);
    static bool StartsWith(const char* string, const char* compare);
    static char* StrCaseStr(const char* haystack, const char* needle);
    static size_t Copy(char* dst, const char* src, size_t sz);
    static size_t Concat(char* dst, const char* src, size_t sz);
    static bool ToNumber(int* dst, const char* src);
    static bool ToDecimal(double* dst, const char* src);
    static bool ToNumber(int* dst, string src);
    static bool ToDecimal(double* dst, string src);
    static char* GetPath(const char* filename);
    static char* ConcatPaths(const char* pathA, const char* pathB);
    static char* ReplacePathSeparators(const char* path);
    static void ReplacePathSeparatorsInPlace(char* path);
};

#endif /* ENGINE_UTILITIES_STRINGUTILS_H */
