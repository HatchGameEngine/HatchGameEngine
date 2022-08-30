#if INTERFACE
#include <Engine/Includes/Standard.h>

class StringUtils {
public:
};
#endif

#include <Engine/Utilities/StringUtils.h>

PUBLIC STATIC char* StringUtils::Duplicate(const char* src) {
    if (!src)
        return NULL;

    size_t sz = strlen(src) + 1;
    char *dst = (char*)malloc(sz);
    if (!dst)
        return NULL;

    memcpy(dst, src, sz);
    return dst;
}
PUBLIC STATIC bool StringUtils::WildcardMatch(const char* first, const char* second) {
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
    char* d = dst;
    const char* s = src;
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
PUBLIC STATIC bool StringUtils::ToNumber(int* dst, const char* src) {
    char* end;
    long num = strtol(src, &end, 10);

    if (*end != '\0')
        return false;
    else if (num > INT_MAX || (errno == ERANGE && num == LONG_MAX))
        return false;
    else if (num < INT_MIN || (errno == ERANGE && num == LONG_MIN))
        return false;

    (*dst) = num;
    return true;
}
PUBLIC STATIC bool StringUtils::ToDecimal(double* dst, const char* src) {
    char* end;
    double num = strtod(src, &end);

    if (*end != '\0')
        return false;
    if (errno == ERANGE && (num == HUGE_VAL || num == -HUGE_VAL))
        return false;

    (*dst) = num;
    return true;
}
