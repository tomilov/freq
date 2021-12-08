#include <algorithm>
#include <chrono>
#include <iterator>
#include <limits>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <cassert>
#include <cstdio>
#include <cstdlib>

#if defined(_MSC_VER) || defined(__MINGW32__)
#define FORCEINLINE __forceinline
#include <intrin.h>
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#define UNPREDICTABLE(x) (x)
#define BSF(index, mask) _BitScanForward(&index, mask)
#elif defined(__clang__) || defined(__GNUG__)
#include <x86intrin.h>
#define FORCEINLINE __attribute__((always_inline))
#define LIKELY(x) (__builtin_expect((x), 1))
#define UNLIKELY(x) (__builtin_expect((x), 0))
#define UNPREDICTABLE(x) (__builtin_unpredictable(x))
#define BSF(index, mask) index = decltype(index)(__bsfd(mask))
#else
#error "!"
#endif

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

namespace
{
#if defined(_OPENMP)
constexpr bool kFindPerfectHash = false;
#endif
constexpr bool kEnableOpenAddressing = false;  // requires a key comparison

alignas(__m128i) char input[1u << 29];
auto i = input;

// perfect hash seeds: 10675, 98363, 102779, 103674, 105067, 194036, 242662,
// 290547, 313385, ... seeds 8, 23, 89, 126, 181, 331, 381, 507, ... are also
// perfect hash seeds, but kHashTableOrder-bit prefix of hash values gives more
// than 8 collisions per unique one, thus requires an open addressing for
// hashtable enabled
constexpr uint32_t kInitialChecksum = 10675;
constexpr uint16_t kDefaultChecksumHigh = 0xFFFFu;

struct alignas(64) Chunk
{
    __m128i hashesHigh;
    uint32_t count[sizeof(__m128i) / sizeof(uint16_t)];
};
static_assert((alignof(Chunk) % alignof(__m128i)) == 0, "!");

constexpr auto kHashTableOrder =
    std::numeric_limits<uint16_t>::digits +
    1;  // one bit window to distinct kDefaultChecksumHigh
constexpr uint32_t kHashTableMask = (1u << kHashTableOrder) - 1u;
Chunk hashTable[1u << kHashTableOrder];

alignas(__m128i) char output[1u << 22] = {};
auto o = std::next(output);  // words[i][j] == std::distance(output, o) is 0 for
                             // unused hashes only
uint32_t words[std::extent_v<decltype(hashTable)>]
              [std::extent_v<decltype(Chunk::count)>] = {};

struct Timer
{
    using high_resolution_clock = std::chrono::high_resolution_clock;
    using time_point = high_resolution_clock::time_point;

    const char * onScopeExit = "total";
    const time_point start = high_resolution_clock::now();
    time_point timePoint = start;

    double dt(bool absolute = false)
    {
        using std::chrono::duration_cast;
        using std::chrono::nanoseconds;
        auto stop = high_resolution_clock::now();
        return duration_cast<nanoseconds>(
                   stop - (absolute ? start : std::exchange(timePoint, stop)))
                   .count() *
               1E-9;
    }

    void report(const char * description, bool absolute = false)
    {
        fprintf(stderr, "time (%s) = %.3lf\n", description, dt(absolute));
    }

    ~Timer()
    {
        report(onScopeExit, true);
    }
};

template<size_t bufferSize = 131072>
class OutputStream
{
public:
    OutputStream(std::FILE * outputFile)
        : outputFile{outputFile}
    {
        assert(outputFile);
        flush();
    }

    OutputStream(const OutputStream &) = delete;
    OutputStream & operator=(const OutputStream &) = delete;

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

private:
    std::FILE * outputFile;

    char buffer[bufferSize];
    char * it = buffer;
    char * end = it;
};

void toLowerInput()
{
    for (auto in = input; in < i; in += sizeof(__m128i)) {
        __m128i str = _mm_load_si128(reinterpret_cast<const __m128i *>(in));
        __m128i lowercase = _mm_add_epi8(
            str, _mm_and_si128(_mm_cmplt_epi8(str, _mm_set1_epi8('a')),
                               _mm_set1_epi8('a' - 'A')));
        __m128i mask =
            _mm_or_si128(_mm_cmplt_epi8(lowercase, _mm_set1_epi8('a')),
                         _mm_cmpgt_epi8(lowercase, _mm_set1_epi8('z')));
        _mm_store_si128(reinterpret_cast<__m128i *>(in),
                        _mm_andnot_si128(mask, lowercase));
    }
}

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
    for (auto in = input; LIKELY(in < i); in += sizeof(__m128i)) {
        __m128i str = _mm_load_si128(reinterpret_cast<const __m128i *>(in));
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
    if UNPREDICTABLE ((m & (1u << offset)) == 0) {                         \
        ++len;                                                             \
        hash = _mm_crc32_u8(hash, uint8_t(_mm_extract_epi8(str, offset))); \
    } else if UNPREDICTABLE (len != 0) {                                   \
        incCounter(hash, in + offset, len);                                \
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
        incCounter(hash, i, len);
    }
}

}  // namespace

#if defined(_OPENMP)
#include <tuple>
#include <unordered_set>

#include <omp.h>

static void findPerfectHash()
{
    Timer timer{GREEN("total")};

    toLowerInput();
    timer.report("tolower input");

    const auto getWords = []() -> std::vector<std::string_view> {
        size_t wordCount = 0;
        std::unordered_set<std::string_view> words;
        auto lo = std::find(input, i, '\0');
        while (lo != i) {
            auto hi = std::find_if(lo, i, [](char c) { return c != '\0'; });
            lo = std::find(hi, i, '\0');
            words.emplace(hi, size_t(std::distance(hi, lo)));
            ++wordCount;
        }
        fprintf(stderr, "%zu words read\n", wordCount);
        fprintf(stderr, "%zu unique words read\n", words.size());
        return {std::begin(words), std::end(words)};
    };
    auto words = getWords();
    timer.report("collect words");

    std::sort(std::begin(words), std::end(words), [](auto && lhs, auto && rhs) {
        return std::forward_as_tuple(lhs.size(), lhs) <
               std::forward_as_tuple(rhs.size(), rhs);
    });
    timer.report("sort words");

    constexpr auto hashTableOrder = kHashTableOrder;
    constexpr uint32_t hashTableMask = (1u << hashTableOrder) - 1u;
    constexpr auto maxCollisions = std::extent_v<decltype(Chunk::count)>;
    const auto printStatusPeriod = 1000 / omp_get_num_threads();
#pragma omp parallel firstprivate(timer)
    {
        uint32_t iterationCount = 0;
        size_t maxPrefix = 0;
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
                hashesLow.resize(size_t(1) << hashTableOrder);
                size_t prefix = 0;
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
                fprintf(stderr,
                        YELLOW("FOUND: iterationCount %u ; initialChecksum = "
                               "%u ; collision = %hhu ; hashTableOrder %u "
                               "; time %.3lf ; tid %i\n"),
                        iterationCount, uint32_t(initialChecksum), collision,
                        hashTableOrder, timer.dt(), omp_get_thread_num());
            }
            if ((++iterationCount % printStatusPeriod) == 0) {
                fprintf(stderr, "failed at %zu of %zu\n",
                        std::exchange(maxPrefix, 0), words.size());
                fprintf(stderr,
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
        fprintf(stderr, "usage: %s in.txt out.txt\n", argv[0]);
        return EXIT_FAILURE;
    }

    using namespace std::string_view_literals;

    std::unique_ptr<std::FILE, decltype((std::fclose))> inputFile{
        (argv[1] == "-"sv) ? stdin : std::fopen(argv[1], "rb"), std::fclose};
    if (!inputFile) {
        fprintf(stderr, "failed to open \"%s\" file to read\n", argv[1]);
        return EXIT_FAILURE;
    }

    std::unique_ptr<std::FILE, decltype((std::fclose))> outputFile{
        (argv[2] == "-"sv) ? stdout : std::fopen(argv[2], "wb"), std::fclose};
    if (!outputFile) {
        fprintf(stderr, "failed to open \"%s\" file to write\n", argv[2]);
        return EXIT_FAILURE;
    }

    timer.report("open files");

    for (Chunk & chunk : hashTable) {
        chunk.hashesHigh = _mm_set1_epi16(int16_t(kDefaultChecksumHigh));
    }
    timer.report("init hashTable");

    {
        size_t size =
            std::fread(input, sizeof *input, std::extent_v<decltype(input)>,
                       inputFile.get());
        assert(size < std::extent_v<decltype(input)>);
        fprintf(stderr, "input size = %zu bytes\n", size);

        constexpr size_t batchSize = sizeof(__m128i);
        i += ((size + batchSize - 1) / batchSize) * batchSize;
        std::fill(input + size, i, '\0');
    }
    timer.report("read input");

#if defined(_OPENMP)
    if ((kFindPerfectHash)) {
        findPerfectHash();
        return EXIT_SUCCESS;
    }
#endif

    if (kEnableOpenAddressing) {
        toLowerInput();
        timer.report("tolower input");
    }

    countWords();
    timer.report(BLUE("count words"));

    for (auto out = output; out < o; out += sizeof(__m128i)) {
        __m128i str = _mm_load_si128(reinterpret_cast<const __m128i *>(out));
        __m128i mask = _mm_or_si128(_mm_cmplt_epi8(str, _mm_set1_epi8('A')),
                                    _mm_cmpgt_epi8(str, _mm_set1_epi8('Z')));
        _mm_store_si128(
            reinterpret_cast<__m128i *>(out),
            _mm_add_epi8(_mm_andnot_si128(mask, _mm_set1_epi8('a' - 'A')),
                         str));
    }
    timer.report("tolower output");

    std::vector<std::pair<uint32_t, std::string_view>> rank;
    rank.reserve(std::extent_v<decltype(words)> * std::extent_v<decltype(words), 1>);
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
    fprintf(stderr, "load factor = %.3lf\n",
            double(rank.size()) / double(rank.capacity()));
    timer.report("collect word counts");

    auto less = [](auto && lhs, auto && rhs) {
        return std::tie(rhs.first, lhs.second) <
               std::tie(lhs.first, rhs.second);
    };
    std::sort(std::begin(rank), std::end(rank), less);
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
