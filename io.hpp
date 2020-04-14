#pragma once

#include <algorithm>
#include <iterator>
#include <type_traits>

#include <cassert>
#include <cstdio>

#if defined(_MSC_VER) || defined(__MINGW32__)
# include <immintrin.h>
#elif defined(__clang__) || defined(__GNUG__)
# include <x86intrin.h>
# ifndef __forceinline
#  define __forceinline __attribute__((always_inline))
# endif
#else
#error "!"
#endif

using uchar = unsigned char;

template<size_t bufferSize = sizeof(__m128i) * 8192>
class InputStream
{
    static_assert((bufferSize % sizeof(__m128i)) == 0, "!");

public :
    InputStream(std::FILE * inputFile)
        : inputFile{inputFile}
    {
        assert(inputFile);
    }

    // non-moveable/non-copyable due to `it` and `end` points to member's data
    InputStream(const InputStream &) = delete;
    InputStream & operator = (const InputStream &) = delete;

    bool fetch()
    {
        size_t size = std::fread(buffer, sizeof *buffer, sizeof buffer, inputFile);
        if (size == 0) {
            return false;
        }
        it = buffer;
        end = it + size;

        constexpr int flags = _SIDD_UBYTE_OPS | _SIDD_CMP_RANGES | _SIDD_MASKED_POSITIVE_POLARITY | _SIDD_UNIT_MASK;

        alignas(__m128i) uchar deltas[sizeof(__m128i)];
        static_assert('A' < 'a', "!");
        std::fill(std::begin(deltas), std::end(deltas), 'a' - 'A');
        __m128i delta = _mm_load_si128(reinterpret_cast<const __m128i *>(deltas));

        alignas(__m128i) uchar u[sizeof(__m128i)] = "AZ";
        __m128i uppercase = _mm_load_si128(reinterpret_cast<const __m128i *>(u));

        alignas(__m128i) uchar l[sizeof(__m128i)] = "az";
        __m128i lowercase = _mm_load_si128(reinterpret_cast<const __m128i *>(l));

        auto data = reinterpret_cast<__m128i *>(buffer);
        const auto dataEnd = data + (size + sizeof *data - 1) / sizeof *data;
        for (; data != dataEnd; ++data) {
#ifdef _MSC_VER
            _mm_prefetch(reinterpret_cast<const char *>(data + 24), _MM_HINT_T0);
#else
            _mm_prefetch(data + 24, _MM_HINT_T0);
#endif
            __m128i str = _mm_load_si128(data);
            __m128i mask = _mm_cmpistrm(uppercase, str, flags);
            str = _mm_adds_epu8(_mm_and_si128(mask, delta), str);
            mask = _mm_cmpistrm(lowercase, str, flags);
            _mm_store_si128(data, _mm_and_si128(mask, str));
        }
        return true;
    }

    __forceinline int getChar()
    {
        if (it == end) {
            if (!fetch()) {
                return EOF;
            }
        }
        return *it++;
    }

private :
    std::FILE * inputFile;

    alignas(__m128i) uchar buffer[bufferSize];
    uchar * it = buffer;
    uchar * end = it;
};

template<size_t bufferSize = sizeof(__m128i) * 8192>
class OutputStream
{
public :
    OutputStream(std::FILE * outputFile)
        : outputFile{outputFile}
    {
        assert(outputFile);
        flush();
    }

    // non-moveable/non-copyable due to `it` and `end` points to member's data
    OutputStream(const OutputStream &) = delete;
    OutputStream & operator = (const OutputStream &) = delete;

    ~OutputStream()
    {
        flush();
    }

    bool flush()
    {
        size_t size = size_t(it - buffer);
        if (std::fwrite(buffer, sizeof *buffer, size, outputFile) != size) {
            return false;
        }
        it = std::begin(buffer);
        end = std::end(buffer);
        return true;
    }

    __forceinline bool putChar(uchar c)
    {
        *it++ = c;
        if (it == end) {
            if (!flush()) {
                return false;
            }
        }
        return true;
    }

    __forceinline bool print(size_t value)
    {
        if (value == 0) {
            if (!putChar('0')) {
                return false;
            }
            return true;
        }
        size_t rev = value;
        size_t n = 0;
        while ((rev % 10) == 0) {
            ++n;
            rev /= 10;
        }
        rev = 0;
        while (value != 0) {
            rev = (rev * 10) + (value % 10);
            value /= 10;
        }
        while (rev != 0) {
            if (!putChar('0' + (rev % 10))) {
                return false;
            }
            rev /= 10;
        }
        while (0 != n) {
            --n;
            if (!putChar('0')) {
                return false;
            }
        }
        return true;
    }

    __forceinline bool print(const uchar * s)
    {
        while (*s != '\0') {
            if (!putChar(*s++)) {
                return false;
            }
        }
        return true;
    }

private :
    std::FILE * outputFile;

    alignas(__m128i) uchar buffer[bufferSize];
    uchar * it = buffer;
    uchar * end = it;
};
