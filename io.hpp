#pragma once

#include "helpers.hpp"

#include <iterator>

#include <cassert>
#include <cstdio>
#include <cstdlib>

#if defined(__linux__) || defined(__APPLE__)
#define RED(s) "\e[3;31m" s "\e[0m"
#define GREEN(s) "\e[3;32m" s "\e[0m"
#define YELLOW(s) "\e[3;33m" s "\e[0m"
#define BLUE(s) "\e[3;34m" s "\e[0m"
#else
#define RED(s) s
#define GREEN(s) s
#define YELLOW(s) s
#define BLUE(s) s
#endif

template<std::size_t bufferSize = 131072>
class OutputStream
{
    static_assert(bufferSize > 0, "!");

public:
    OutputStream(std::FILE * outputFile)
        : outputFile{outputFile}
    {
        assert(outputFile);
    }

    OutputStream(const OutputStream &) = delete;
    OutputStream & operator=(const OutputStream &) = delete;

    ~OutputStream()
    {
        if (!flush()) {
            std::exit(EXIT_FAILURE);
        }
    }

    bool flush()
    {
        auto size = std::size_t(std::distance(output, o));
        if (std::fwrite(output, 1, size, outputFile) != size) {
            return false;
        }
        o = output;
        return true;
    }

    FORCEINLINE bool putChar(char c)
    {
        *o++ = c;
        if (o == std::end(output)) {
            if (!flush()) {
                return false;
            }
        }
        return true;
    }

    FORCEINLINE bool print(std::size_t value)
    {
        if (value == 0) {
            if (!putChar('0')) {
                return false;
            }
            return true;
        }
        std::size_t rev = value;
        std::size_t n = 0;
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

private:
    std::FILE * const outputFile;

    char output[bufferSize];
    char * o = output;
};
