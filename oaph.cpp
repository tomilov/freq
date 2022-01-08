#include "helpers.hpp"
#include "io.hpp"
#include "timer.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace
{
#if defined(_OPENMP)
constexpr bool kFindPerfectHash = false;
#endif
constexpr bool kEnableOpenAddressing = false;  // requires a key comparison

alignas(__m128i) char input[1 << 29];
auto inputEnd = input;

// perfect hash seeds: 10675, 98363, 102779, 103674, 105067, 194036, 242662,
// 290547, 313385, ... seeds 8, 23, 89, 126, 181, 331, 381, 507, ... are also
// perfect hash seeds, but kHashTableOrder-bit prefix of hash values gives more
// than 8 collisions per unique one, thus requires an open addressing for
// hashtable enabled
constexpr uint32_t kInitialChecksum = 10675;
constexpr uint16_t kDefaultChecksumHigh = 0xFFFF;

struct alignas(kHardwareDestructiveInterferenceSize) Chunk
{
    __m128i hashesHigh;
    uint32_t count[sizeof(__m128i) / sizeof(uint16_t)];
};

static_assert((alignof(Chunk) % alignof(__m128i)) == 0, "!");

constexpr auto kHashTableOrder =
    std::numeric_limits<uint16_t>::digits +
    1;  // one bit window to distinct kDefaultChecksumHigh
constexpr uint32_t kHashTableMask = (uint32_t(1) << kHashTableOrder) - 1;
Chunk hashTable[1 << kHashTableOrder];

alignas(__m128i) char output[1 << 22] = {};
auto o = std::next(output);  // words[i][j] == std::distance(output, o) is 0 for
                             // unused hashes only
uint32_t words[std::extent_v<decltype(hashTable)>]
              [std::extent_v<decltype(Chunk::count)>] = {};

void incCounter(uint32_t hash, const char * __restrict wordEnd, uint32_t len)
{
    uint32_t hashLow = hash & kHashTableMask;
    uint32_t hashHigh = hash >> kHashTableOrder;
    for (;;) {
        Chunk & chunk = hashTable[hashLow];
        __m128i hashesHigh = _mm_load_si128(&chunk.hashesHigh);
        __m128i mask =
            _mm_cmpeq_epi16(hashesHigh, _mm_set1_epi16(int16_t(hashHigh)));
        uint16_t m = uint16_t(_mm_movemask_epi8(mask));
        unsigned long index;
        if LIKELY (m != 0) {
            BSF(index, m);
            index /= 2;
            if (kEnableOpenAddressing &&
                UNLIKELY(!std::equal(wordEnd - len, wordEnd + 1,
                                     output + words[hashLow][index])))
            {
                hashLow = (hashLow + 1) & kHashTableMask;  // linear probing
                continue;
            }
        } else {
            m = uint16_t(_mm_movemask_epi8(hashesHigh)) & 0b1010101010101010u;
            if (kEnableOpenAddressing && UNLIKELY(m == 0)) {
                hashLow = (hashLow + 1) & kHashTableMask;  // linear probing
                continue;
            }
            BSF(index, m);
            index /= 2;
            reinterpret_cast<uint16_t *>(&chunk.hashesHigh)[index] = hashHigh;
            words[hashLow][index] = uint32_t(std::distance(output, o));
            o = std::next(std::copy_n(std::prev(wordEnd, len), len, o));
        }
        ++chunk.count[index];
        return;
    }
}

void countWords()
{
    uint32_t hash = kInitialChecksum;
    uint32_t len = 0;
    for (auto i = input; LIKELY(i < inputEnd); i += sizeof(__m128i)) {
        __m128i str = _mm_load_si128(reinterpret_cast<const __m128i *>(i));
        __m128i mask;
        if (kEnableOpenAddressing) {
            mask = _mm_cmpeq_epi8(str, _mm_setzero_si128());
        } else {
            str = _mm_add_epi8(
                _mm_and_si128(_mm_cmplt_epi8(str, _mm_set1_epi8('a')),
                              _mm_set1_epi8('a' - 'A')),
                str);
            mask = _mm_or_si128(_mm_cmplt_epi8(str, _mm_set1_epi8('a')),
                                _mm_cmpgt_epi8(str, _mm_set1_epi8('z')));
        }
        uint16_t m = uint16_t(_mm_movemask_epi8(mask));
#define BYTE(offset)                                                       \
    if UNPREDICTABLE ((m & (uint32_t(1) << offset)) == 0) {                \
        ++len;                                                             \
        hash = _mm_crc32_u8(hash, uint8_t(_mm_extract_epi8(str, offset))); \
    } else if UNPREDICTABLE (len != 0) {                                   \
        incCounter(hash, i + offset, len);                                 \
        len = 0;                                                           \
        hash = kInitialChecksum;                                           \
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
        incCounter(hash, inputEnd, len);
    }
}

}  // namespace

#if defined(_OPENMP)

#include <omp.h>

static void findPerfectHash()
{
    Timer timer{GREEN("total")};

    toLower(input, inputEnd);
    timer.report("make input lowercase");

    auto words = []() -> std::vector<std::string_view> {
        std::size_t wordCount = 0;
        std::unordered_set<std::string_view> words;
        auto lo = std::find(input, inputEnd, '\0');
        while (lo != inputEnd) {
            auto hi =
                std::find_if(lo, inputEnd, [](char c) { return c != '\0'; });
            lo = std::find(hi, inputEnd, '\0');
            words.emplace(hi, std::size_t(std::distance(hi, lo)));
            ++wordCount;
        }
        std::fprintf(stderr, "%zu words read\n", wordCount);
        std::fprintf(stderr, "%zu unique words read\n", words.size());
        return {std::cbegin(words), std::cend(words)};
    }();
    timer.report("collect words");

    std::sort(std::begin(words), std::end(words), [](auto && lhs, auto && rhs) {
        return std::make_tuple(lhs.size(), std::cref(lhs)) <
               std::make_tuple(rhs.size(), std::cref(rhs));
    });
    timer.report(YELLOW("sort words"));

    constexpr auto hashTableOrder = kHashTableOrder;
    constexpr uint32_t hashTableMask = (uint32_t(1) << hashTableOrder) - 1;
    constexpr auto maxCollisions = std::extent_v<decltype(Chunk::count)>;
    const auto printStatusPeriod = 1000 / omp_get_num_threads();
#pragma omp parallel firstprivate(timer)
    {
        uint32_t iterationCount = 0;
        std::size_t maxPrefix = 0;
        std::unordered_set<uint32_t> hashesFull;
        std::vector<uint8_t> hashesLow;
#pragma omp for schedule(static, 1)
        for (int32_t initialChecksum = 0;
             initialChecksum < std::numeric_limits<int32_t>::max();
             ++initialChecksum)
        {  // MSVC: error C3016: 'initialChecksum': index variable in OpenMP
           // 'for' statement must have signed integral type
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
            uint8_t collision = 0;
            if (!kEnableOpenAddressing && !bad) {
                hashesLow.resize(std::size_t(1) << hashTableOrder);
                std::size_t prefix = 0;
                for (uint32_t hash : hashesFull) {
                    ++prefix;
                    collision =
                        std::max(collision, ++hashesLow[hash & hashTableMask]);
                    if (collision > maxCollisions) {
                        bad = true;
                        break;
                    }
                }
                if (prefix > maxPrefix) {
                    maxPrefix = prefix;
                }
                hashesLow.clear();
            }
            hashesFull.clear();
            if (!bad) {
                std::fprintf(
                    stderr,
                    YELLOW("FOUND: iterationCount %u ; initialChecksum = "
                           "%u ; collision = %hhu ; hashTableOrder %u "
                           "; time %.3lf ; tid %i\n"),
                    iterationCount, uint32_t(initialChecksum), collision,
                    hashTableOrder, timer.dt(), omp_get_thread_num());
            }
            if ((++iterationCount % printStatusPeriod) == 0) {
                std::fprintf(stderr, "failed at %zu of %zu\n",
                             std::exchange(maxPrefix, 0), words.size());
                std::fprintf(stderr,
                             "status: totalIterationCount ~%u ; tid %i ; "
                             "initialChecksum %u ; time %.3lf\n",
                             iterationCount * omp_get_num_threads(),
                             omp_get_thread_num(), uint32_t(initialChecksum),
                             timer.dt());
            }
        }
    }
}
#endif

int main(int argc, char * argv[])
{
    Timer timer{GREEN("total")};

    if (argc != 3) {
        std::fprintf(stderr, "usage: %s in.txt out.txt\n", argv[0]);
        return EXIT_FAILURE;
    }

    using namespace std::string_view_literals;

    std::unique_ptr<std::FILE, decltype((std::fclose))> inputFile{
        (argv[1] == "-"sv) ? stdin : std::fopen(argv[1], "rb"), std::fclose};
    if (!inputFile) {
        std::fprintf(stderr, "failed to open \"%s\" file to read\n", argv[1]);
        return EXIT_FAILURE;
    }

    std::unique_ptr<std::FILE, decltype((std::fclose))> outputFile{
        (argv[2] == "-"sv) ? stdout : std::fopen(argv[2], "wb"), std::fclose};
    if (!outputFile) {
        std::fprintf(stderr, "failed to open \"%s\" file to write\n", argv[2]);
        return EXIT_FAILURE;
    }

    std::size_t readSize =
        readInput(std::begin(input), std::size(input), inputFile.get());
    if (readSize == 0) {
        return EXIT_FAILURE;
    }
    inputEnd += readSize;
    timer.report("read input");

    for (Chunk & chunk : hashTable) {
        chunk.hashesHigh = _mm_set1_epi16(int16_t(kDefaultChecksumHigh));
    }
    timer.report("init hashTable");

#if defined(_OPENMP)
    if ((kFindPerfectHash)) {
        findPerfectHash();
        return EXIT_SUCCESS;
    }
#endif

    if (kEnableOpenAddressing) {
        toLower(input, inputEnd);
        timer.report("make input lowercase");
    }

    countWords();
    timer.report(BLUE("count words"));

    toLower(output, o);
    timer.report("make output lowercase");

    std::vector<std::pair<uint32_t, std::string_view>> rank;
    rank.reserve(std::extent_v<decltype(words)> *
                 std::extent_v<decltype(words), 1>);
    {
        uint32_t hashLow = 0;
        for (const auto & w : words) {
            const Chunk & chunk = hashTable[hashLow];
            uint32_t index = 0;
            for (uint32_t word : w) {
                if (word != 0) {
                    rank.emplace_back(chunk.count[index], output + word);
                }
                ++index;
            }
            ++hashLow;
        }
    }
    std::fprintf(stderr, "load factor = %.3lf\n",
                 double(rank.size()) / double(rank.capacity()));
    timer.report("collect word counts");

    auto less = [](auto && lhs, auto && rhs) {
        return std::tie(rhs.first, lhs.second) <
               std::tie(lhs.first, rhs.second);
    };
    std::sort(std::begin(rank), std::end(rank), less);
    timer.report(YELLOW("sort words"));

    OutputStream<> outputStream{outputFile.get()};
    for (const auto & [count, word] : rank) {
        if (!outputStream.print(count)) {
            std::fprintf(stderr, "output failure\n");
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
    timer.report("write output");

    return EXIT_SUCCESS;
}
