#include <Engine/Diagnostics/Memory.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/Utilities/StringUtils.h>

char* StringUtils::Create(void* src, size_t length) {
	char* string = (char*)Memory::Malloc(length + 1);
	memcpy(string, src, length);
	string[length] = '\0';
	return string;
}
char* StringUtils::Create(std::string src) {
	return StringUtils::Duplicate(src.c_str());
}
char* StringUtils::Create(Token token) {
	char* string = (char*)Memory::Malloc(token.Length + 1);
	memcpy(string, token.Start, token.Length);
	string[token.Length] = '\0';
	return string;
}
char* StringUtils::Duplicate(const char* src) {
	size_t length = strlen(src) + 1;
	char* string = (char*)Memory::Malloc(length);
	memcpy(string, src, length);
	return string;
}
char* StringUtils::Duplicate(const char* src, size_t length) {
	size_t srcLength = strlen(src);
	if (length > srcLength) {
		length = srcLength;
	}
	char* string = (char*)Memory::Malloc(length + 1);
	memcpy(string, src, length);
	string[length] = '\0';
	return string;
}
char* StringUtils::Resize(char* src, size_t length) {
	size_t originalSize = strlen(src);
	char* string = (char*)Memory::Realloc(src, length + 1);
	if (length > originalSize) {
		memset(&string[originalSize], 0x00, (length - originalSize) + 1);
	}
	else {
		string[length] = '\0';
	}
	return string;
}
bool StringUtils::WildcardMatch(const char* first, const char* second) {
	if (*first == 0 && *second == 0) {
		return true;
	}
	if (*first == 0 && *second == '*' && *(second + 1) != 0) {
		return false;
	}
	if (*first == *second || *second == '?') {
		return StringUtils::WildcardMatch(first + 1, second + 1);
	}
	if (*second == '*') {
		return StringUtils::WildcardMatch(first, second + 1) ||
			StringUtils::WildcardMatch(first + 1, second);
	}
	return false;
}
bool StringUtils::StartsWith(const char* string, const char* compare) {
	size_t cmpLen = strlen(compare);
	if (strlen(string) < cmpLen) {
		return false;
	}

	return memcmp(string, compare, cmpLen) == 0;
}
bool StringUtils::StartsWith(std::string string, std::string compare) {
	size_t cmpLen = compare.size();
	if (string.size() < cmpLen) {
		return false;
	}

	return memcmp(string.c_str(), compare.c_str(), cmpLen) == 0;
}
char* StringUtils::StrCaseStr(const char* haystack, const char* needle) {
	if (!needle[0]) {
		return (char*)haystack;
	}

	/* Loop over all possible start positions. */
	for (size_t i = 0; haystack[i]; i++) {
		bool matches = true;
		/* See if the string matches here. */
		for (size_t j = 0; needle[j]; j++) {
			/* If we're out of room in the haystack, give
			 * up. */
			if (!haystack[i + j]) {
				return NULL;
			}

			/* If there's a character mismatch, the needle
			 * doesn't fit here. */
			if (tolower((unsigned char)needle[j]) !=
				tolower((unsigned char)haystack[i + j])) {
				matches = false;
				break;
			}
		}
		if (matches) {
			return (char*)(haystack + i);
		}
	}
	return NULL;
}
size_t StringUtils::Copy(char* dst, const char* src, size_t sz) {
	char* d = dst;
	const char* s = src;
	size_t n = sz;

	// Copy as many bytes as will fit
	if (n != 0) {
		while (--n != 0) {
			if ((*d++ = *s++) == '\0') {
				break;
			}
		}
	}

	// Not enough room in dst, add NUL and traverse rest of src
	if (n == 0) {
		if (sz != 0) {
			*d = '\0'; // NUL-terminate dst
		}
		while (*s++)
			;
	}

	return s - src - 1; // count does not include NUL
}
size_t StringUtils::Copy(char* dst, std::string src, size_t sz) {
	return Copy(dst, src.c_str(), sz);
}
size_t StringUtils::Concat(char* dst, const char* src, size_t sz) {
	char* d = dst;
	const char* s = src;
	size_t n = sz;
	size_t dlen;

	// Find the end of dst and adjust bytes left but don't go past
	// end
	while (n-- != 0 && *d != '\0') {
		d++;
	}
	dlen = d - dst;
	n = sz - dlen;

	if (n == 0) {
		return dlen + strlen(s);
	}
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return dlen + (s - src); // count does not include NUL
}
bool StringUtils::ToNumber(int* dst, const char* src) {
	char* end;
	long num = strtol(src, &end, 10);

	if (*end != '\0') {
		return false;
	}
	else if (num > INT_MAX || (errno == ERANGE && num == LONG_MAX)) {
		return false;
	}
	else if (num < INT_MIN || (errno == ERANGE && num == LONG_MIN)) {
		return false;
	}

	(*dst) = num;
	return true;
}
bool StringUtils::ToDecimal(double* dst, const char* src) {
	char* end;
	double num = strtod(src, &end);

	if (*end != '\0') {
		return false;
	}
	if (errno == ERANGE && (num == HUGE_VAL || num == -HUGE_VAL)) {
		return false;
	}

	(*dst) = num;
	return true;
}
bool StringUtils::ToNumber(int* dst, string src) {
	return ToNumber(dst, src.c_str());
}
bool StringUtils::ToDecimal(double* dst, string src) {
	return ToDecimal(dst, src.c_str());
}
bool StringUtils::HexToUint32(Uint32* dst, const char* hexstr) {
	if (hexstr == nullptr || *hexstr == '\0') {
		return false;
	}

	Uint32 result = 0;

	do {
		char chr = *hexstr;
		unsigned val;

		if (chr >= '0' && chr <= '9') {
			val = chr - '0';
		}
		else if (chr >= 'A' && chr <= 'F') {
			val = chr - 'A' + 10;
		}
		else if (chr >= 'a' && chr <= 'f') {
			val = chr - 'a' + 10;
		}
		else {
			return false;
		}

		result = (result << 4) | (val & 0xF);
		hexstr++;
	} while (*hexstr != '\0');

	*dst = result;

	return true;
}
char* StringUtils::GetPath(const char* filename) {
	if (!filename) {
		return nullptr;
	}

	const char* sep = strrchr(filename, '/');
	if (!sep) {
		sep = strrchr(filename, '\\');
	}
	if (!sep) {
		return nullptr;
	}

	size_t len = sep - filename;
	char* path = (char*)Memory::Malloc(len + 1);
	if (!path) {
		return nullptr;
	}

	memcpy(path, filename, len);
	path[len] = '\0';

	return path;
}
const char* StringUtils::GetFilename(const char* filename) {
	if (!filename) {
		return nullptr;
	}

	const char* sep = strrchr(filename, '/');
	if (!sep) {
		sep = strrchr(filename, '\\');
	}
	if (!sep) {
		return filename;
	}

	return sep + 1;
}
const char* StringUtils::GetExtension(const char* filename) {
	if (!filename) {
		return nullptr;
	}

	const char* dot = strrchr(filename, '.');
	if (!dot) {
		return nullptr;
	}

	return dot + 1;
}
char* StringUtils::ReplacePathSeparators(const char* path) {
	if (!path) {
		return nullptr;
	}

	char* newPath = (char*)Memory::Malloc(strlen(path) + 1);
	if (!newPath) {
		return nullptr;
	}

	char* out = newPath;
	while (*path) {
		char c = *path;
		if (c == '\\') {
			*out = '/';
		}
		else {
			*out = c;
		}
		out++;
		path++;
	}
	*out = '\0';

	return newPath;
}
char* StringUtils::NormalizePath(const char* path) {
	if (path == nullptr) {
		return nullptr;
	}

	std::string normalized = Path::Normalize(path);

	char* normalizedPath = StringUtils::Create(normalized);
	if (normalizedPath != nullptr) {
		StringUtils::ReplacePathSeparatorsInPlace(normalizedPath);
	}

	return normalizedPath;
}
void StringUtils::NormalizePath(const char* path, char* dest, size_t destSize) {
	if (path == nullptr || dest == nullptr) {
		return;
	}

	std::string normalized = Path::Normalize(path);

	snprintf(dest, destSize, "%s", normalized.c_str());
}
void StringUtils::ReplacePathSeparatorsInPlace(char* path) {
	if (!path) {
		return;
	}

	while (*path) {
		if (*path == '\\') {
			*path = '/';
		}
		path++;
	}
}
