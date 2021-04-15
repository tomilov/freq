#include "io.hpp"
#include "timer.hpp"

#include <utility>
#include <memory>
#include <vector>
#include <iterator>
#include <algorithm>

#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <string_view>

#include <cstring>
#include <cstdlib>
#include <cstdio>

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
        for (uint32_t d = 0; d < size; d += sizeof(__m128i)) {
            __m128i str = _mm_stream_load_si128(reinterpret_cast<__m128i *>(input + d));
            __m128i mask = _mm_cmplt_epi8(str, _mm_set1_epi8('a'));
            static_assert('A' < 'a');
            str = _mm_add_epi8(_mm_and_si128(mask, _mm_set1_epi8('a' - 'A')), str);
            mask = _mm_or_si128(_mm_cmplt_epi8(str, _mm_set1_epi8('a')), _mm_cmpgt_epi8(str, _mm_set1_epi8('z')));

            //_mm_stream_si128(reinterpret_cast<__m128i *>(input + d), _mm_andnot_si128(mask, str));

            int m = _mm_movemask_epi8(mask);
#define BYTE(bit)                                                               \
            if (UNLIKELY(m & (1 << bit))) {                                     \
                if (len > 0) {                                                  \
                    incCounter(checksum, d + bit, len);                         \
                    len = 0;                                                    \
                    checksum = kInitialChecksum;                                \
                }                                                               \
            } else {                                                            \
                ++len;                                                          \
                checksum = _mm_crc32_u8(checksum, _mm_extract_epi8(str, bit));  \
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

    if ((false)) {
        std::unordered_map<uint32_t, std::unordered_map<std::string_view, size_t>> hashes;

        const auto end = input + size;
        auto lo = std::find(input, end, '\0');
        ptrdiff_t s = 0;
        ptrdiff_t w = std::distance(input, lo);
        while (lo != end) {
            auto hi = std::find_if_not(lo, end, [](char c) { return c == '\0'; });
            s = std::max(s, std::distance(lo, hi));
            lo = std::find(hi, end, '\0');
            uint32_t hash = kInitialChecksum;
            for (auto c = hi; c != lo; ++c) {
                hash = _mm_crc32_u8(hash, *c);
            }
            ++hashes[hash/* & ((1u << hashTableOrder) - 1u)*/][{hi, lo}];
            w = std::max(w, std::distance(hi, lo));
        }
        fprintf(stderr, "whitespaces %zi ; word length %zi\n", s, w);

        size_t wc = 0;
        size_t n = 0;
        size_t collision = 0;
        for (const auto & [hash, set] : hashes) {
            (void)hash;
            collision = std::max(collision, set.size());
            if (set.size() > 1) {
                ++n;
            }
            for (const auto & [word, count] : set) {
                if (set.size() > 1) {
                    //std::cout << word << ", ";
                }
                wc = std::max(wc, count);
            }
            if (set.size() > 1) {
                //std::cout << std::endl;
            }
        }
        fprintf(stderr, "collision = %zu %zu %zu\n", collision, n, wc);
    }

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
