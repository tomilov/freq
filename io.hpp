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

template<size_t bufferSize = sizeof(__m512i) * 8192>
class InputStream
{
    static_assert((bufferSize % sizeof(__m512i)) == 0, "!");

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

        alignas(__m512i) uchar d[sizeof(__m512i)];
        static_assert('A' < 'a', "!");
        std::fill(std::begin(d), std::end(d), 'a' - 'A');
        const __m512i delta = _mm512_load_si512(reinterpret_cast<const __m512i *>(d));

        const __m512i ll = _mm512_set1_epi8('a');
        const __m512i lh = _mm512_set1_epi8('z');
        const __m512i hl = _mm512_set1_epi8('A');
        const __m512i hh = _mm512_set1_epi8('Z');

        auto data = reinterpret_cast<__m512i *>(buffer);
        const auto dataEnd = data + (size + sizeof *data - 1) / sizeof *data;
        for (; data != dataEnd; ++data) {
            __m512i str = _mm512_stream_load_si512(data);
            auto lcase = _kand_mask64(_mm512_cmpge_epu8_mask(str, ll), _mm512_cmple_epu8_mask(str, lh));
            auto ucase = _kand_mask64(_mm512_cmpge_epu8_mask(str, hl), _mm512_cmple_epu8_mask(str, hh));
            _mm512_stream_si512(data, _mm512_mask_add_epi8(_mm512_maskz_mov_epi8(lcase, str), ucase, str, delta));
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

    alignas(__m512i) uchar buffer[bufferSize];
    uchar * it = buffer;
    uchar * end = it;
};

template<size_t bufferSize = 131072>
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

    uchar buffer[bufferSize];
    uchar * it = buffer;
    uchar * end = it;
};
