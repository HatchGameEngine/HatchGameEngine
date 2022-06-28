#ifndef ENGINE_UTILITIES_STRINGUTILS_H
#define ENGINE_UTILITIES_STRINGUTILS_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>

class StringUtils {
public:
    static bool  WildcardMatch(const char* first, const char* second);
    static char* StrCaseStr(const char* haystack, const char* needle);
    static size_t Copy(char* dst, const char* src, size_t sz);
    static char* GetPath(const char* filename);
    static char* ConcatPaths(const char* pathA, const char* pathB);
};

#endif /* ENGINE_UTILITIES_STRINGUTILS_H */
