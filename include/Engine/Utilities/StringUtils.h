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
    static char* Duplicate(const char* src);
    static bool WildcardMatch(const char* first, const char* second);
    static char* StrCaseStr(const char* haystack, const char* needle);
    static size_t Copy(char* dst, const char* src, size_t sz);
    static bool ToNumber(int* dst, const char* src);
};

#endif /* ENGINE_UTILITIES_STRINGUTILS_H */
