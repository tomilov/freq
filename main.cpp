#include "io.hpp"
#include "timer.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <cstdio>
#include <cstdlib>

#if defined(_MSC_VER) || defined(__MINGW32__)
# include <intrin.h>
#elif defined(__clang__) || defined(__GNUG__)
# include <x86intrin.h>
#else
# error "!"
#endif

#include <omp.h>

namespace
{
alignas(__m128i) char input[1u << 29];
auto i = input;

// perfect hash seeds: 10675, 98363, 102779, 103674, 105067, 194036, 242662, 290547, 313385, ...
constexpr uint32_t kInitialChecksum = 10675;
constexpr uint16_t kDefaultChecksumHigh = 0xFFFFu;

#pragma pack(push, 1)
struct Chunk
{
    __m128i checksumsHigh = _mm_set1_epi16(int16_t(kDefaultChecksumHigh));
    int count[sizeof(__m128i) / sizeof(uint16_t)] = {};
};
#pragma pack(pop)

constexpr auto kHashTableOrder = std::numeric_limits<uint16_t>::digits + 1; // one bit window for kDefaultChecksumHigh
Chunk hashTable[1u << kHashTableOrder];
alignas(__m128i) char output[std::extent_v<decltype(input)> + 2]; // provide space for leading unused char and trailing null
auto o = output + 1; // words[i][j] == std::distance(output, o) is 0 for unused hashes only
int words[std::extent_v<decltype(hashTable)>][std::extent_v<decltype(Chunk::count)>] = {};

#ifdef _MSC_VER
void incCounter(uint32_t checksum, const char * wordEnd, uint8_t len)
#else
void incCounter(uint32_t checksum, const char * __restrict__ wordEnd, uint8_t len)
#endif
{
    uint32_t checksumLow = checksum & ((1u << kHashTableOrder) - 1u);
    Chunk & chunk = hashTable[checksumLow];
    uint16_t checksumHigh = checksum >> kHashTableOrder;
    __m128i checksumsHigh = _mm_load_si128(&chunk.checksumsHigh);
    __m128i mask = _mm_cmpeq_epi16(checksumsHigh, _mm_set1_epi16(int16_t(checksumHigh)));
    uint16_t m = uint16_t(_mm_movemask_epi8(mask));
#if defined(_MSC_VER) || defined(__MINGW32__)
    unsigned long index;
#elif defined(__clang__) || defined(__GNUG__)
    int index;
#else
# error "!"
#endif
    if (m == 0) {
        m = uint16_t(_mm_movemask_epi8(checksumsHigh)) & 0b1010101010101010u;
#if defined(_MSC_VER) || defined(__MINGW32__)
        _BitScanForward(&index, m);
#elif defined(__clang__) || defined(__GNUG__)
        index = __bsfd(m);
#else
# error "!"
#endif
        index /= 2;
        reinterpret_cast<uint16_t *>(&chunk.checksumsHigh)[index] = checksumHigh;
        words[checksumLow][index] = int(std::distance(output, o));
        o = std::copy_n(std::prev(wordEnd, len), len, o);
        *o++ = '\0';
    } else {
#if defined(_MSC_VER) || defined(__MINGW32__)
        _BitScanForward(&index, m);
#elif defined(__clang__) || defined(__GNUG__)
        index = __bsfd(m);
#else
# error "!"
#endif
        index /= 2;
    }
    ++chunk.count[index];
}

void countWords()
{
    uint32_t checksum = kInitialChecksum;
    uint8_t len = 0;
    for (auto in = input; in < i; in += sizeof(__m128i)) {
        __m128i str = _mm_load_si128(reinterpret_cast<const __m128i *>(in));
        __m128i lowercase = _mm_add_epi8(_mm_and_si128(_mm_cmplt_epi8(str, _mm_set1_epi8('a')), _mm_set1_epi8('a' - 'A')), str);
        __m128i mask = _mm_or_si128(_mm_cmplt_epi8(lowercase, _mm_set1_epi8('a')), _mm_cmpgt_epi8(lowercase, _mm_set1_epi8('z')));
        uint16_t m = uint16_t(_mm_movemask_epi8(mask));
#define BYTE(offset)                                                                            \
        if ((m & (1u << offset)) == 0) {                                                        \
            assert(len != std::numeric_limits<decltype(len)>::max());                           \
            ++len;                                                                              \
            checksum = _mm_crc32_u8(checksum, uint8_t(_mm_extract_epi8(lowercase, offset)));    \
        } else if (len != 0) {                                                                  \
            incCounter(checksum, in + offset, len);                                             \
            len = 0;                                                                            \
            checksum = kInitialChecksum;                                                        \
        }
        BYTE(0)
        BYTE(1)
        BYTE(2)
        BYTE(3)
        BYTE(4)
        BYTE(5)
        BYTE(6)
        BYTE(7)
        BYTE(8)
        BYTE(9)
        BYTE(10)
        BYTE(11)
        BYTE(12)
        BYTE(13)
        BYTE(14)
        BYTE(15)
#undef BYTE
    }
    if (len != 0) {
        incCounter(checksum, i, len);
    }
}

} // namespace

#ifdef __linux__
#define RED(s) "\e[3;31m" s "\e[0m"
#define GREEN(s) "\e[3;32m" s "\e[0m"
#else
#define RED(s) s
#define GREEN(s) s
#endif

#include <set>
#include <unordered_set>

static void findPerfectHash()
{
    Timer timer;

    for (auto in = input; in < i; in += sizeof(__m128i)) {
        __m128i str = _mm_load_si128(reinterpret_cast<const __m128i *>(in));
        __m128i lowercase = _mm_add_epi8(str, _mm_and_si128(_mm_cmplt_epi8(str, _mm_set1_epi8('a')), _mm_set1_epi8('a' - 'A')));
        __m128i mask = _mm_or_si128(_mm_cmplt_epi8(lowercase, _mm_set1_epi8('a')), _mm_cmpgt_epi8(lowercase, _mm_set1_epi8('z')));
        _mm_store_si128(reinterpret_cast<__m128i *>(in), _mm_andnot_si128(mask, lowercase));
    }
    timer.report("tolower input");

    const auto getWords = []() -> std::vector<std::string_view>
    {
        size_t wordCount = 0;
        std::set<std::string_view> words;
        auto lo = std::find(input, i, '\0');
        while (lo != i) {
            auto hi = std::find_if(lo, i, [](char c) { return c != '\0'; });
            lo = std::find(hi, i, '\0');
#if defined(_MSC_VER) || defined(__MINGW32__)
            words.emplace(hi, size_t(std::distance(hi, lo)));
#elif defined(__clang__) || defined(__GNUG__)
            words.emplace(hi, lo);
#else
# error "!"
#endif
            ++wordCount;
        }
        fprintf(stderr, "%zu words read\n", wordCount);
        fprintf(stderr, "%zu unique words read\n", words.size());
        return {std::begin(words), std::end(words)};
    };
    auto words = getWords();
    timer.report("collect words");

    std::stable_sort(std::begin(words), std::end(words), [](auto && lhs, auto && rhs) { return lhs.size() < rhs.size(); });
    timer.report("sort words by length");

    constexpr auto hashTableOrder = kHashTableOrder;
    constexpr auto maxCollisions = std::extent_v<decltype(Chunk::count)>;
    const auto printStatusPeriod = 1000 / omp_get_num_threads();
    int iterationCount = 0;
    size_t maxPrefix = 0;
    std::unordered_set<uint32_t> hashesFull;
    std::vector<uint8_t> hashesLow;
#pragma omp parallel for firstprivate(timer, iterationCount, maxPrefix, hashesFull, hashesLow) schedule(static, 1)
    for (int32_t initialChecksum = 0; initialChecksum < std::numeric_limits<int32_t>::max(); ++initialChecksum) { // MSVC: error C3016: 'initialChecksum': index variable in OpenMP 'for' statement must have signed integral type
        bool bad = false;
        for (const auto & word : words) {
            auto hash = uint32_t(initialChecksum);
            for (char c : word) {
                hash = _mm_crc32_u8(hash, uint8_t(c));
            }
            if (!hashesFull.insert(hash).second) {
                bad = true;
                break;
            }
        }
        if (bad) {
            hashesFull.clear();
        } else {
            uint8_t collision = 0;
            hashesLow.resize(size_t(1) << hashTableOrder);
            size_t prefix = 0;
            for (uint32_t hash : hashesFull) {
                ++prefix;
                collision = std::max(collision, ++hashesLow[hash & ((1u << hashTableOrder) - 1u)]);
                if (collision > maxCollisions) {
                    bad = true;
                    break;
                }
            }
            hashesFull.clear();
            if (prefix > maxPrefix) {
                maxPrefix = prefix;
            }
            hashesLow.clear();
            if (!bad) {
                fprintf(stderr, "FOUND: iterationCount %i ; initialChecksum = %u ; collision = %hhu ; hashTableOrder %u ; time %.3lf ; tid %i\n", iterationCount, uint32_t(initialChecksum), collision, hashTableOrder, timer.dt(), omp_get_thread_num());
            }
        }
        if ((++iterationCount % printStatusPeriod) == 0) {
            fprintf(stderr, "failed at %zu of %zu\n", std::exchange(maxPrefix, 0), words.size());
            fprintf(stderr, "status: totalIterationCount %i ; tid %i ; initialChecksum %u ; time %.3lf\n", iterationCount * omp_get_num_threads(), omp_get_thread_num(), uint32_t(initialChecksum), timer.dt());
        }
    }
}

int main(int argc, char * argv[])
{
    Timer timer{GREEN("total")};

    if (argc != 3) {
        fprintf(stderr, "usage: %s in.txt out.txt\n", argv[0]);
        return EXIT_FAILURE;
    }

    using namespace std::string_view_literals;

    std::unique_ptr<std::FILE, decltype((std::fclose))> inputFile{(argv[1] == "-"sv) ? stdin : std::fopen(argv[1], "rb"), std::fclose};
    if (!inputFile) {
        fprintf(stderr, "failed to open \"%s\" file to read\n", argv[1]);
        return EXIT_FAILURE;
    }

    std::unique_ptr<std::FILE, decltype((std::fclose))> outputFile{(argv[2] == "-"sv) ? stdout : std::fopen(argv[2], "wb"), std::fclose};
    if (!outputFile) {
        fprintf(stderr, "failed to open \"%s\" file to write\n", argv[2]);
        return EXIT_FAILURE;
    }

    timer.report("open files");

    {
        size_t size = std::fread(input, sizeof *input, std::extent_v<decltype(input)>, inputFile.get());
        assert(size < std::extent_v<decltype(input)>);
        fprintf(stderr, "input size = %zu bytes\n", size);

        constexpr size_t batchSize = sizeof(__m128i);
        i += ((size + batchSize - 1) / batchSize) * batchSize;
        std::fill(input + size, i, '\0');
    }
    timer.report("read input");

    if ((false)) {
        findPerfectHash();
        return EXIT_SUCCESS;
    }

    countWords();
    timer.report(RED("count words"));

    for (auto out = output; out < o; out += sizeof(__m128i)) {
        __m128i str = _mm_load_si128(reinterpret_cast<const __m128i *>(out));
        __m128i mask = _mm_or_si128(_mm_cmplt_epi8(str, _mm_set1_epi8('A')), _mm_cmpgt_epi8(str, _mm_set1_epi8('Z')));
        _mm_store_si128(reinterpret_cast<__m128i *>(out), _mm_add_epi8(_mm_andnot_si128(mask, _mm_set1_epi8('a' - 'A')), str));
    }
    timer.report("tolower output");

    std::vector<std::pair<uint32_t, std::string_view>> rank;
    rank.reserve(std::extent_v<decltype(words)> * std::extent_v<decltype(words), 1>);
    {
        uint32_t checksumLow = 0;
        for (const auto & w : words) {
            const Chunk & chunk = hashTable[checksumLow];
            uint32_t index = 0;
            for (int word : w) {
                if (word != 0) {
                    rank.emplace_back(chunk.count[index], output + word);
                }
                ++index;
            }
            ++checksumLow;
        }
    }
    fprintf(stderr, "load factor = %.3lf\n", double(rank.size()) / double(rank.capacity()));
    timer.report("collect word counts");

    std::sort(std::begin(rank), std::end(rank), [] (auto && lhs, auto && rhs) { return std::tie(rhs.first, lhs.second) < std::tie(lhs.first, rhs.second); });
    timer.report("rank word counts");

    OutputStream<> outputStream{outputFile.get()};
    for (const auto & [count, word] : rank) {
        if (!outputStream.print(count)) {
            fprintf(stderr, "output failure\n");
            return EXIT_FAILURE;
        }
        if (!outputStream.putChar(' ')) {
            fprintf(stderr, "output failure\n");
            return EXIT_FAILURE;
        }
        if (!outputStream.print(word.data())) {
            fprintf(stderr, "output failure\n");
            return EXIT_FAILURE;
        }
        if (!outputStream.putChar('\n')) {
            fprintf(stderr, "output failure\n");
            return EXIT_FAILURE;
        }
    }
    timer.report("output");

    return EXIT_SUCCESS;
}
