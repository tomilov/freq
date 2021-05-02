#pragma once

#include <algorithm>
#include <iterator>
#include <type_traits>

#include <cassert>
#include <cstdio>

#if defined(_MSC_VER) || defined(__MINGW32__)
# define FORCEINLINE __forceinline
#elif defined(__clang__) || defined(__GNUG__)
# define FORCEINLINE __attribute__((always_inline))
#else
# define FORCEINLINE
#endif

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

    FORCEINLINE bool putChar(char c)
    {
        *it++ = c;
        if (it == end) {
            if (!flush()) {
                return false;
            }
        }
        return true;
    }

    FORCEINLINE bool print(size_t value)
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

    FORCEINLINE bool print(const char * s)
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

    char buffer[bufferSize];
    char * it = buffer;
    char * end = it;
};
