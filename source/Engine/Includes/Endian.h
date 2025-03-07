#ifndef ENDIAN_H
#define ENDIAN_H

#include <cstdint>
#include <type_traits>

// TODO: All of this could be simplified using C++20's std::endian

// Assume Windows/MSVC is always little-endian, and use GCC/Clang's
// __BYTE_ORDER__ for the other platforms
#if defined(_MSC_VER) || (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
#define HATCH_LITTLE_ENDIAN 1
#define HATCH_BIG_ENDIAN 0
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define HATCH_LITTLE_ENDIAN 0
#define HATCH_BIG_ENDIAN 1
#else
#error "Unknown endianness"
#endif

// GCC/Clang first so that clang-cl can work on Windows
#if defined(__GNUC__) || defined(__clang__) || defined(__llvm__)
#define bswap16(x) __builtin_bswap16(x)
#define bswap32(x) __builtin_bswap32(x)
#define bswap64(x) __builtin_bswap64(x)
#elif defined(_MSC_VER)
#include <stdlib.h>
#define bswap16(x) _byteswap_ushort(x)
#define bswap32(x) _byteswap_ulong(x)
#define bswap64(x) _byteswap_uint64(x)
#else
#error "Unknown compiler"
#endif

// C++17 equivalent to std::bit_cast for float conversion
template<class To, class From>
constexpr std::enable_if_t<sizeof(To) == sizeof(From) && std::is_trivially_copyable_v<From> &&
		std::is_trivially_copyable_v<To>,
	To>
bit_cast(const From& src) noexcept {
	static_assert(std::is_trivially_constructible_v<To>);
	To dst;
	memcpy(&dst, &src, sizeof(To));
	return dst;
}

// Endian conversion macros
#if HATCH_LITTLE_ENDIAN

#define TO_LE16(x) (x)
#define TO_LE32(x) (x)
#define TO_LE32F(x) (x)
#define TO_LE64(x) (x)
#define FROM_LE16(x) (x)
#define FROM_LE32(x) (x)
#define FROM_LE32F(x) (x)
#define FROM_LE64(x) (x)

#define TO_BE16(x) bswap16(x)
#define TO_BE32(x) bswap32(x)
#define TO_BE32F(x) bit_cast<float>(bswap32(bit_cast<uint32_t>(x)))
#define TO_BE64(x) bswap64(x)
#define FROM_BE16(x) bswap16(x)
#define FROM_BE32(x) bswap32(x)
#define FROM_BE32F(x) bit_cast<float>(bswap32(bit_cast<uint32_t>(x)))
#define FROM_BE64(x) bswap64(x)

#elif HATCH_BIG_ENDIAN

#define TO_LE16(x) bswap16(x)
#define TO_LE32(x) bswap32(x)
#define TO_LE32F(x) bit_cast<float>(bswap32(bit_cast<uint32_t>(x)))
#define TO_LE64(x) bswap64(x)
#define FROM_LE16(x) bswap16(x)
#define FROM_LE32(x) bswap32(x)
#define FROM_LE32F(x) bit_cast<float>(bswap32(bit_cast<uint32_t>(x)))
#define FROM_LE64(x) bswap64(x)

#define TO_BE16(x) (x)
#define TO_BE32(x) (x)
#define TO_BE32F(x) (x)
#define TO_BE64(x) (x)
#define FROM_BE16(x) (x)
#define FROM_BE32(x) (x)
#define FROM_BE32F(x) (x)
#define FROM_BE64(x) (x)

#endif

// Constexpr magic string to uint32 little-endian
constexpr uint32_t MAGIC_LE32(const char* s) {
	return ((s[3] << 24) | (s[2] << 16) | (s[1] << 8) | s[0]);
}

// Constexpr magic string to uint32 big-endian
constexpr uint32_t MAGIC_BE32(const char* s) {
	return ((s[0] << 24) | (s[1] << 16) | (s[2] << 8) | s[3]);
}

#endif // ENDIAN_H
