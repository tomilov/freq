#pragma once

#include <algorithm>
#include <new>

#include <cassert>
#include <cstdint>

#if defined(_MSC_VER) || defined(__MINGW32__)
#define FORCEINLINE __forceinline
#include <intrin.h>
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#define UNPREDICTABLE(x) (x)
#define BSF(index, mask) _BitScanForward(&index, mask)
#elif defined(__clang__) || defined(__GNUG__)
#include <x86intrin.h>
#define FORCEINLINE __attribute__((always_inline))
#define LIKELY(x) (__builtin_expect((x), 1))
#define UNLIKELY(x) (__builtin_expect((x), 0))
#define UNPREDICTABLE(x) (__builtin_expect_with_probability(x, 0, 0.5))
#define BSF(index, mask) index = decltype(index)(__bsfd(mask))
#else
#error "!"
#endif

#if defined(__GLIBCXX__) && __cpp_lib_hardware_interference_size >= 201603
inline constexpr std::size_t kHardwareConstructiveInterferenceSize =
    std::hardware_constructive_interference_size;
inline constexpr std::size_t kHardwareDestructiveInterferenceSize =
    std::hardware_destructive_interference_size;
#else
inline constexpr std::size_t kHardwareConstructiveInterferenceSize = 64;
inline constexpr std::size_t kHardwareDestructiveInterferenceSize = 64;
#endif

inline void toLower(char * beg, char * const end)
{
    assert(beg <= end);
    assert((reinterpret_cast<std::uintptr_t>(beg) % sizeof(__m128i)) == 0);
    assert((reinterpret_cast<std::uintptr_t>(end) % sizeof(__m128i)) == 0);
    for (; beg < end; beg += sizeof(__m128i)) {
        __m128i string = _mm_load_si128(reinterpret_cast<const __m128i *>(beg));
        __m128i lowercase = _mm_add_epi8(
            string, _mm_and_si128(_mm_cmplt_epi8(string, _mm_set1_epi8('a')),
                                  _mm_set1_epi8('a' - 'A')));
        __m128i mask =
            _mm_or_si128(_mm_cmplt_epi8(lowercase, _mm_set1_epi8('a')),
                         _mm_cmpgt_epi8(lowercase, _mm_set1_epi8('z')));
        _mm_store_si128(reinterpret_cast<__m128i *>(beg),
                        _mm_andnot_si128(mask, lowercase));
    }
}
