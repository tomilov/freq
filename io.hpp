#pragma once

#include "helpers.hpp"

#include <fmt/format.h>

#include <iterator>
#include <memory>

#include <cassert>
#include <cstdio>
#include <cstdlib>

using File = std::unique_ptr<std::FILE, decltype(&std::fclose)>;

inline File wrapFile(std::FILE * file)
{
    return {file, &std::fclose};
}

inline File openFile(const char * filename, const char * modes)
{
    return wrapFile(std::fopen(filename, modes));
}

inline std::size_t readInput(char * inputBegin, std::size_t inputSize,
                             const File & inputFile)
{
    std::size_t readSize =
        std::fread(inputBegin, 1, inputSize, inputFile.get());
    fmt::print(stderr, "input size = {} bytes\n", readSize);
    if (!(readSize < inputSize)) {
        fmt::print(stderr, "input is too large\n");
        return 0;
    }

    inputBegin += readSize;
    while ((readSize % sizeof(__m128i)) != 0) {
        *inputBegin++ = '\0';
        if (++readSize > inputSize) {
            fmt::print(stderr, "input is too large\n");
            return 0;
        }
    }
    return readSize;
}

template<std::size_t bufferSize = 131072>
class OutputStream
{
    static_assert(bufferSize > 0, "!");

public:
    OutputStream(const File & outputFile) : outputFile{outputFile.get()}
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
