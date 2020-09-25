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
        fetch();
    }

    // non-moveable/non-copyable due to `it` and `end` points to member's data
    InputStream(const InputStream &) = delete;
    InputStream & operator = (const InputStream &) = delete;

    bool fetch()
    {
        size = std::fread(buffer, sizeof *buffer, sizeof buffer, inputFile);
        if (size == 0) {
            return false;
        }

        constexpr int flags = _SIDD_UBYTE_OPS | _SIDD_CMP_RANGES | _SIDD_MASKED_POSITIVE_POLARITY | _SIDD_UNIT_MASK;

        alignas(__m128i) uchar d[sizeof(__m128i)];
        static_assert('A' < 'a', "!");
        std::fill(std::begin(d), std::end(d), 'a' - 'A');
        __m128i delta = _mm_load_si128(reinterpret_cast<const __m128i *>(d));

        alignas(__m128i) uchar u[sizeof(__m128i)] = "AZ";
        __m128i uppercase = _mm_load_si128(reinterpret_cast<const __m128i *>(u));

        alignas(__m128i) uchar l[sizeof(__m128i)] = "az";
        __m128i lowercase = _mm_load_si128(reinterpret_cast<const __m128i *>(l));

        auto data = reinterpret_cast<__m128i *>(buffer);

        auto unalignedHead = alignof(__m128i) - reinterpret_cast<ptrdiff_t>(&buffer) % alignof(__m128i);
        if (unalignedHead != 0) {
            for (auto it = buffer; it != buffer + unalignedHead; ++it) {
                if ((*it >= 'A') && (*it <= 'Z')) {
                    *it += d[0];
                } else if ((*it < 'a') || (*it > 'z')) {
                    *it = '\0';
                }
            }

            data = reinterpret_cast<__m128i *>(buffer + unalignedHead);
            if (unalignedHead <= size) {
                size -= unalignedHead;
            } else {
                size = 0;
            }
        }

        const auto dataEnd = data + size / sizeof *data;
        for (; data != dataEnd; ++data) {
            __m128i str = _mm_stream_load_si128(data);
            __m128i mask = _mm_cmpistrm(uppercase, str, flags);
            str = _mm_adds_epu8(_mm_and_si128(mask, delta), str);
            mask = _mm_cmpistrm(lowercase, str, flags);
            _mm_stream_si128(data, _mm_and_si128(mask, str));
        }

        auto unalignedTail = size % sizeof *data;
        if (unalignedTail != 0) {
            auto it = buffer + unalignedHead + size - unalignedTail;
            do {
                if ((*it >= 'A') && (*it <= 'Z')) {
                    *it += d[0];
                } else if ((*it < 'a') || (*it > 'z')) {
                    *it = '\0';
                }
            } while (++it != buffer + unalignedHead + size);
        }
        return true;
    }

    auto begin() const
    {
        return buffer + 0;
    }

    auto end() const
    {
        return buffer + size;
    }

private :
    std::FILE * inputFile;

    uchar buffer[bufferSize];
    size_t size = 0;
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

    uchar buffer[bufferSize];
    uchar * it = buffer;
    uchar * end = it;
};
