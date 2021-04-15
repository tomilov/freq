#include "io.hpp"
#include "timer.hpp"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_MSC_VER) || defined(__MINGW32__)
# include <immintrin.h>
#elif defined(__clang__) || defined(__GNUG__)
# include <x86intrin.h>
#else
#error "!"
#endif

#if defined(__GNUC__)
#define LIKELY(x) (__builtin_expect((x), 1))
#define UNLIKELY(x) (__builtin_expect((x), 0))
#endif

namespace
{
constexpr size_t kHardwareConstructiveInterferenceSize = 64;
alignas(sizeof(__m128i)) char input[1u << 29];

constexpr uint32_t kInitialChecksum = 8;

#pragma pack(push, 1)
struct alignas(kHardwareConstructiveInterferenceSize) Chunk
{
    uint16_t checksumHigh[10] = {
        1u << 15,
        1u << 15,
        1u << 15,
        1u << 15,
        1u << 15,
        1u << 15,
        1u << 15,
        1u << 15,
        1u << 15,
        1u << 15,
    };
    uint32_t count[10] = {};
};
#pragma pack(pop)
static_assert(sizeof(Chunk) == kHardwareConstructiveInterferenceSize);

constexpr size_t hashTableOrder = 17;
static_assert(hashTableOrder > 16);
Chunk hashTable[1u << hashTableOrder];
uint32_t words[1u << hashTableOrder][10] = {};

inline void incCounter(uint32_t checksum, uint32_t wordEnd, uint32_t len)
{
    uint32_t checksumLow = checksum & ((1u << hashTableOrder) - 1u);
    Chunk & chunk = hashTable[checksumLow];
    uint16_t checksumHigh = checksum >> hashTableOrder;
    int mask = _mm_movemask_epi8(_mm_cmpeq_epi16(*reinterpret_cast<const __m128i *>(&chunk.checksumHigh), _mm_set1_epi16(checksumHigh)));
    mask |= ((chunk.checksumHigh[8] == checksumHigh) ? (0b11u << (8 * 2)) : 0) | ((chunk.checksumHigh[9] == checksumHigh) ? (0b11u << (9 * 2)) : 0);
    int index;
    if (UNLIKELY(mask == 0)) {
        mask = _mm_movemask_epi8(_mm_cmpeq_epi16(*reinterpret_cast<const __m128i *>(&chunk.checksumHigh), _mm_set1_epi16(1u << 15)));
        mask |= ((chunk.checksumHigh[8] == 1u << 15) ? (0b11u << (8 * 2)) : 0) | ((chunk.checksumHigh[9] == 1u << 15) ? (0b11u << (9 * 2)) : 0);
        assert(mask != 0); // more then 10 collisions by checksumLow
        index = _bit_scan_forward(mask) / 2;
        chunk.checksumHigh[index] = checksumHigh;
        words[checksumLow][index] = wordEnd - len;
        for (auto i = wordEnd - len; i < wordEnd; ++i) {
            if (UNLIKELY(input[i] < 'a')) {
                input[i] += 'a' - 'A';
            }
        }
        input[wordEnd] = '\0';
    } else {
        assert(_popcnt32(mask) == 2);
        index = _bit_scan_forward(mask) / 2;
    }
    assert(index < 10);
    ++chunk.count[index];
}

} // namespace

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

    size_t size = std::fread(input, sizeof *input, sizeof input, inputFile.get());
    assert(size < std::extent_v<decltype(input)>);
    fprintf(stderr, "input size = %zu bytes\n", size);
    {
        const size_t roundedSize = ((size + kHardwareConstructiveInterferenceSize - 1) / kHardwareConstructiveInterferenceSize) * kHardwareConstructiveInterferenceSize;
        std::fill(input + size, input + roundedSize, '\0');
        size = roundedSize;
    }
    timer.report("read all the input");

    {
        uint32_t checksum = kInitialChecksum;
        uint8_t len = 0;
        for (uint32_t i = 0; i < size; i += sizeof(__m128i)) {
            __m128i str = _mm_stream_load_si128(reinterpret_cast<__m128i *>(input + i));
            static_assert('A' < 'a');
            __m128i lowercase = _mm_add_epi8(_mm_and_si128(_mm_cmplt_epi8(str, _mm_set1_epi8('a')), _mm_set1_epi8('a' - 'A')), str);
            int mask = _mm_movemask_epi8(_mm_or_si128(_mm_cmplt_epi8(lowercase, _mm_set1_epi8('a')), _mm_cmpgt_epi8(lowercase, _mm_set1_epi8('z'))));
#define BYTE(offset)                                                                    \
            if (UNLIKELY(mask & (1 << offset))) {                                       \
                if (len > 0) {                                                          \
                    incCounter(checksum, i + offset, len);                              \
                    len = 0;                                                            \
                    checksum = kInitialChecksum;                                        \
                }                                                                       \
            } else {                                                                    \
                ++len;                                                                  \
                checksum = _mm_crc32_u8(checksum, _mm_extract_epi8(lowercase, offset)); \
            }
            BYTE(0);
            BYTE(1);
            BYTE(2);
            BYTE(3);
            BYTE(4);
            BYTE(5);
            BYTE(6);
            BYTE(7);
            BYTE(8);
            BYTE(9);
            BYTE(10);
            BYTE(11);
            BYTE(12);
            BYTE(13);
            BYTE(14);
            BYTE(15);
#undef BYTE
        }
        if (len > 0) {
            incCounter(checksum, size, len);
        }
    }
    timer.report("filter all the input");

    std::vector<std::pair<uint32_t, std::string_view>> rank;
    rank.reserve(std::extent_v<decltype(words)> * std::extent_v<decltype(words), 1>);
    {
        uint32_t checksumLow = 0;
        for (const auto & w : words) {
            uint32_t index = 0;
            for (uint32_t word : w) {
                if (LIKELY(word)) {
                    rank.emplace_back(hashTable[checksumLow].count[index], input + word);
                }
                ++index;
            }
            ++checksumLow;
        }
    }
    timer.report("collect word counts");

    std::sort(std::begin(rank), std::end(rank), [] (auto && lhs, auto && rhs) { return std::tie(rhs.first, lhs.second) < std::tie(lhs.first, rhs.second); });
    timer.report("rank word counts");

    OutputStream<> outputStream{outputFile.get()};
    for (const auto & [count, word] : rank) {
        if (!outputStream.print(count)) {
            fprintf(stderr, "output failure");
            return EXIT_FAILURE;
        }
        if (!outputStream.putChar(' ')) {
            fprintf(stderr, "output failure");
            return EXIT_FAILURE;
        }
        if (!outputStream.print(word.data())) {
            fprintf(stderr, "output failure");
            return EXIT_FAILURE;
        }
        if (!outputStream.putChar('\n')) {
            fprintf(stderr, "output failure");
            return EXIT_FAILURE;
        }
    }
    timer.report("output");

    return EXIT_SUCCESS;
}
