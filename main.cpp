#include <vector>
#include <utility>
#include <iterator>
#include <string>
#include <algorithm>
#include <memory>

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

static constexpr int AlphabetSize = 'z' - 'a' + 1;

struct TrieNode
{
    int c = -1;
    size_t parent = 0;
    size_t count = 0;
    size_t children[AlphabetSize] = {};
};

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
    using uchar = unsigned char;

    std::FILE * inputFile;
    alignas(__m128i) uchar buffer[bufferSize];
    uchar * it = nullptr;
    uchar * end = it;
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
    std::vector<TrieNode> trie(1);

    size_t index = 0;
    for (;;) {
        int c = inputStream.getChar();
        if (c > '\0') {
            size_t & child = trie[index].children[c - 'a'];
            if (child == 0) {
                child = trie.size();
                trie.push_back({c, std::exchange(index, child)});
            } else {
                index = child;
            }
        } else {
            if (index != 0) {
                ++trie[index].count;
                index = 0;
            }
            if (c < 0) {
                break;
            }
        }
    }
    fprintf(stderr, "trie size = %zu\n", trie.size());

    timer.report("build counting trie from input");

    std::string word, words;
    std::vector<std::pair<size_t, size_t>> rank;
    auto traverseTrie = [&] (const auto & traverseTrie, decltype((std::as_const(trie).front().children)) children) -> void
    {
        for (size_t index : children) {
            if (index == 0) {
                continue;
            }
            const TrieNode & node = trie[index];
            word.push_back(typename std::string::value_type(node.c));
            if (node.count != 0) {
                rank.emplace_back(node.count, words.size());
                words.append(word);
                words.push_back('\0');
            }
            traverseTrie(traverseTrie, node.children);
            word.pop_back();
        }
    };
    traverseTrie(traverseTrie, trie.front().children);
    assert(word.empty());

    timer.report("recover words from trie");

    std::stable_sort(std::begin(rank), std::end(rank), [] (auto && l, auto && r) { return r.first < l.first; });

    timer.report("rank words");

    for (const auto & [count, word] : rank) {
        fprintf(outputFile.get(), "%zu %s\n", count, words.c_str() + word);
    }

    timer.report("output");

    return EXIT_SUCCESS;
}
