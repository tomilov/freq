#include <vector>
#include <utility>
#include <iterator>
#include <string>
#include <algorithm>
#include <memory>
#include <unordered_map>

#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#if defined(_MSC_VER) || defined(__MINGW32__)
# include <immintrin.h>
#elif defined(__clang__) || defined(__GNUG__)
# include <x86intrin.h>
#endif

#if defined(_MSC_VER)
# ifdef _WIN64
#  define bswap(x) _byteswap_uint64(x)
# else
#  define bswap(x) _byteswap_ulong(x)
# endif
#else
# ifdef __x86_64__
#  define bswap(x) __builtin_bswap64(x)
# else
#  define bswap(x) __builtin_bswap32(x)
# endif
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

    int getChar()
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

struct TrieNode
{
    size_t c = 0;
    size_t parent = 0;
    size_t count = 0;
#if 1
    std::unordered_map<size_t, size_t> children = {};
#else
    std::vector<std::pair<size_t, size_t>> children = {};
#endif
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

    void report(const char * description, bool absolute = false) { fprintf(stderr, "time (%s) = %.3lf\n", description, dt(absolute)); }

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

    // preskip leading whitespaces
    int c = inputStream.getChar();
    while (c == '\0') {
        c = inputStream.getChar();
    }

    std::vector<TrieNode> trie(1);
    size_t index = 0;
    while (c > '\0') {
        auto chunk = size_t(c);
        c = inputStream.getChar();
        for (size_t offset = 1; (c > '\0') && (offset < sizeof(size_t)); ++offset) {
            chunk |= size_t(c) << (offset * size_t(std::numeric_limits<uchar>::digits));
            c = inputStream.getChar();
        }
        chunk = bswap(chunk);
        auto & children = trie[index].children;
#if 1
        size_t & child = children[chunk];
        if (child == 0) {
            child = trie.size();
            trie.push_back({chunk, std::exchange(index, child)});
        } else {
            index = child;
        }
#else
        auto it = std::partition_point(std::begin(children), std::end(children), [&chunk] (auto && child) { return child.first < chunk; });
        if ((it == std::end(children)) || (it->first != chunk)) {
            it = children.emplace(it, chunk, trie.size());
            trie.push_back({chunk, index});
        }
        index = it->second;
#endif
        if (!(c > '\0')) {
            if (index != 0) {
                ++trie[index].count;
                index = 0;
            }
            while (c == '\0') {
                c = inputStream.getChar();
            }
        }
    }
    fprintf(stderr, "trie size = %zu\n", trie.size());

    timer.report("build counting trie from input");

    std::vector<size_t> word, words;
    std::vector<std::pair<size_t, size_t>> rank;
    auto traverseTrie = [&] (const auto & traverseTrie, decltype((std::as_const(trie).front().children)) children) -> void
    {
#if 1
        std::vector<std::pair<size_t, size_t>> orderedChildren{std::cbegin(children), std::cend(children)};
        std::sort(std::begin(orderedChildren), std::end(orderedChildren));
        for (auto [chunk, index] : orderedChildren) {
#else
        for (auto [chunk, index] : children) {
#endif
            if (index == 0) {
                continue;
            }
            chunk = bswap(chunk);
            word.push_back(chunk);
            const TrieNode & node = trie[index];
            if (node.count != 0) {
                rank.emplace_back(node.count, words.size());
                words.insert(words.cend(), std::cbegin(word), std::cend(word));
                words.push_back(0);
            }
            if (!node.children.empty()) {
                traverseTrie(traverseTrie, node.children);
            }
            word.pop_back();
        }
    };
    traverseTrie(traverseTrie, trie.front().children);
    assert(word.empty());

    timer.report("recover words from trie");

    std::stable_sort(std::begin(rank), std::end(rank), [] (auto && l, auto && r) { return r.first < l.first; });

    timer.report("rank words");

    for (const auto & [count, word] : rank) {
        fprintf(outputFile.get(), "%zu %s\n", count, reinterpret_cast<const char *>(words.data() + word));
    }

    timer.report("output");
    return EXIT_SUCCESS;
}
