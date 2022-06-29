#if INTERFACE
#include <Engine/Includes/Standard.h>

class StringUtils {
public:
};
#endif

#include <Engine/Utilities/StringUtils.h>
#include <Engine/Diagnostics/Memory.h>

PUBLIC STATIC bool  StringUtils::WildcardMatch(const char* first, const char* second) {
    if (*first == 0 && *second == 0)
        return true;
    if (*first == 0 && *second == '*' && *(second + 1) != 0)
        return false;
    if (*first == *second || *second == '?')
        return StringUtils::WildcardMatch(first + 1, second + 1);
    if (*second == '*')
        return StringUtils::WildcardMatch(first, second + 1) || StringUtils::WildcardMatch(first + 1, second);
    return false;
}
PUBLIC STATIC char* StringUtils::StrCaseStr(const char* haystack, const char* needle) {
    if (!needle[0]) return (char*)haystack;

    /* Loop over all possible start positions. */
    for (size_t i = 0; haystack[i]; i++) {
        bool matches = true;
        /* See if the string matches here. */
        for (size_t j = 0; needle[j]; j++) {
            /* If we're out of room in the haystack, give up. */
            if (!haystack[i + j]) return NULL;

            /* If there's a character mismatch, the needle doesn't fit here. */
            if (tolower((unsigned char)needle[j]) !=
                tolower((unsigned char)haystack[i + j])) {
                matches = false;
                break;
            }
        }
        if (matches) return (char *)(haystack + i);
    }
    return NULL;
}
PUBLIC STATIC size_t StringUtils::Copy(char* dst, const char* src, size_t sz) {
    char *d = dst;
    const char *s = src;
    size_t n = sz;

    // Copy as many bytes as will fit
    if (n != 0) {
        while (--n != 0) {
            if ((*d++ = *s++) == '\0')
            break;
        }
    }

    // Not enough room in dst, add NUL and traverse rest of src
    if (n == 0) {
        if (sz != 0)
            *d = '\0'; // NUL-terminate dst
        while (*s++)
            ;
    }

    return s - src - 1; // count does not include NUL
}
PUBLIC STATIC char* StringUtils::Duplicate(const char* src) {
    size_t length = strlen(src) + 1;
    char* string = (char*)Memory::Malloc(length);
    memcpy(string, src, length);
    return string;
}
PUBLIC STATIC char* StringUtils::GetPath(const char* filename) {
    if (!filename)
        return nullptr;

    const char* sep = strrchr(filename, '/');
    if (!sep)
        sep = strrchr(filename, '\\');
    if (!sep)
        return nullptr;

    size_t len = sep - filename;
    char* path = (char*)Memory::Malloc(len + 1);
    if (!path)
        return nullptr;

    memcpy(path, filename, len);
    path[len] = '\0';

    return path;
}
PUBLIC STATIC char* StringUtils::ConcatPaths(const char* pathA, const char* pathB) {
    if (!pathA || !pathB)
        return nullptr;

    size_t lenA = strlen(pathA);
    size_t lenB = strlen(pathB) + 1;
    size_t totalLen = lenA + lenB;

    bool hasSep = pathA[lenA - 1] == '/' || pathA[lenA - 1] == '\\';
    if (!hasSep)
        totalLen++;

    char* newPath = (char*)Memory::Malloc(totalLen);
    char* out = newPath;
    if (!newPath)
        return nullptr;

    memcpy(newPath, pathA, lenA);
    newPath += lenA;

    if (!hasSep) {
        newPath[0] = '/';
        newPath++;
    }

    memcpy(newPath, pathB, lenB);
    return out;
}
