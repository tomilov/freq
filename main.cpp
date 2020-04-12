#include <vector>
#include <utility>
#include <iterator>
#include <string>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <string_view>

#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#if defined(_MSC_VER) || defined(__MINGW32__)
#include <immintrin.h>
#elif defined(__clang__) || defined(__GNUG__)
#include <x86intrin.h>
#ifndef __forceinline
#define __forceinline __attribute__((always_inline))
#endif
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

    __forceinline bool fetch()
    {
        if (it != end) {
            return true;
        }

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
        if (!fetch()) {
            return EOF;
        }
        return *it++;
    }

private :
    std::FILE * inputFile;
    alignas(__m128i) uchar buffer[bufferSize];
    uchar * it = nullptr;
    uchar * end = it;
};

static constexpr int AlphabetSize = 'z' - 'a' + 1;

struct Trie
{
    size_t c = 0;
    size_t parent = 0;
    size_t count = 0;
    std::unordered_map<size_t, size_t> children;
};

#include <chrono>

struct Timer
{
    using high_resolution_clock = std::chrono::high_resolution_clock;
    using time_point = high_resolution_clock::time_point;

    const time_point start = high_resolution_clock::now();
    time_point timePoint = start;

    double dt(bool absolute = false)
    {
        using std::chrono::duration_cast;
        using std::chrono::nanoseconds;
        auto stop = high_resolution_clock::now();
        return duration_cast<nanoseconds>(stop - (absolute ? start : std::exchange(timePoint, stop))).count() * 1E-9;
    }

    void report(const char * description, bool absolute = false) { fprintf(stderr, "time (%s) = %lf\n", description, dt(absolute)); }

    ~Timer() { report("total", true); }
};

#include <iostream>

int main(int argc, char * argv[])
{
    Timer timer;

    if (argc != 3) {
        fprintf(stderr, "usage: %s in.txt out.txt", argv[0]);
        return EXIT_FAILURE;
    }

    std::unique_ptr<std::FILE, decltype((std::fclose))> inputFile{(std::strcmp(argv[1], "-") == 0) ? stdin : std::fopen(argv[1], "rb"), std::fclose};
    if (!inputFile) {
        fprintf(stderr, "failed to open \"%s\" file to read", argv[1]);
        return EXIT_FAILURE;
    }

    std::unique_ptr<std::FILE, decltype((std::fclose))> outputFile{(std::strcmp(argv[2], "-") == 0) ? stdout : std::fopen(argv[2], "wb"), std::fclose};
    if (!outputFile) {
        fprintf(stderr, "failed to open \"%s\" file to write", argv[2]);
        return EXIT_FAILURE;
    }

    timer.report("open files");

    InputStream<> inputStream{inputFile.get()}; // InputStream::buffer lies on the stack

    std::vector<uchar> words;
    using range_type = std::pair<size_t, size_t>;
    auto crc32cHash = [&words] (range_type range) -> size_t
    {
        auto data = words.data();

        auto result = ~uint32_t(0);
#ifdef __x86_64__
        while (range.first + sizeof(uint64_t) <= range.second) {
            result = uint32_t(_mm_crc32_u64(uint64_t(result), *reinterpret_cast<const uint64_t *>(data + range.first)));
            range.first += sizeof(uint64_t);
        }
#endif
        while (range.first + sizeof(uint32_t) <= range.second) {
            result = _mm_crc32_u32(result, *reinterpret_cast<const uint32_t *>(data + range.first));
            range.first += sizeof(uint32_t);
        }
        while (range.first < range.second) {
            result = _mm_crc32_u8(result, data[range.first]);
            ++range.first;
        }
        return ~size_t(result);
    };
    std::unordered_map<range_type, size_t, decltype((crc32cHash))> counts{0, crc32cHash};
    size_t start = 0, stop = 0;
    for (;;) {
        int c = inputStream.getChar();
        if (c > '\0') {
            words.push_back(uchar(c));
            ++stop;
        } else {
            if (start != stop) {
                auto [position, inserted] = counts.emplace(std::make_pair(start, stop), 1);
                if (!inserted) {
                    ++position->second;
                }
            }
            if (c < 0) {
                break;
            }
            start = stop;
        }
    }
    fprintf(stderr, "size = %zu\n", counts.size());

    timer.report("build counting trie from input");

#if 0
    std::vector<std::pair<size_t, size_t>> rank;
    while (!counts.empty()) {
        auto node = counts.extract(counts.cbegin());

    }
    timer.report("recover words from trie");

    std::stable_sort(std::begin(rank), std::end(rank), [] (auto && l, auto && r) { return r.first < l.first; });

    timer.report("rank words");

    for (const auto & [count, word] : rank) {
        fprintf(outputFile.get(), "%zu %s\n", count, words.c_str() + word);
    }

    timer.report("output");
#endif
    return EXIT_SUCCESS;
}
