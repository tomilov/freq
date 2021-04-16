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

#include <map>
#include <unordered_map>
#include <unordered_set>

#include <cstdio>
#include <cstdlib>

#if defined(_MSC_VER) || defined(__MINGW32__)
# include <intrin.h>
#elif defined(__clang__) || defined(__GNUG__)
# include <x86intrin.h>
#else
# error "!"
#endif

namespace
{
alignas(__m128i) char input[1u << 29];

constexpr uint32_t kHardwareConstructiveInterferenceSize = 64;

constexpr uint32_t kInitialChecksum = 8;
constexpr uint16_t kDefaultChecksumHigh = 1u << 15;

#pragma pack(push, 1)
struct alignas(kHardwareConstructiveInterferenceSize) Chunk
{
    uint16_t checksumHigh[10] = {
        kDefaultChecksumHigh,
        kDefaultChecksumHigh,
        kDefaultChecksumHigh,
        kDefaultChecksumHigh,
        kDefaultChecksumHigh,
        kDefaultChecksumHigh,
        kDefaultChecksumHigh,
        kDefaultChecksumHigh,
        kDefaultChecksumHigh,
        kDefaultChecksumHigh,
    };
    uint32_t count[std::extent_v<decltype(checksumHigh)>] = {};
    uint32_t next = 0;
};
#pragma pack(pop)
static_assert(sizeof(Chunk) == kHardwareConstructiveInterferenceSize);

constexpr auto hashTableOrder = std::numeric_limits<uint16_t>::digits + 1; // one bit for kDefaultChecksumHigh
Chunk hashTable[1u << hashTableOrder];
alignas(__m128i) char output[std::extent_v<decltype(input)> + 2]; // provide space for leading unused char and trailing null
auto o = output + 1; // words[i][j] == std::distance(output, o) == 0 only for unused hashes
uint32_t words[std::extent_v<decltype(hashTable)>][std::extent_v<decltype(Chunk::checksumHigh)>] = {};

void incCounter(uint32_t checksum, uint32_t wordEnd, uint8_t len)
{
    uint32_t checksumLow = checksum & ((1u << hashTableOrder) - 1u);
    Chunk & chunk = hashTable[checksumLow];
    uint16_t checksumHigh = checksum >> hashTableOrder;
    int mask = _mm_movemask_epi8(_mm_cmpeq_epi16(*reinterpret_cast<const __m128i *>(&chunk.checksumHigh), _mm_set1_epi16(checksumHigh)));
    mask |= ((chunk.checksumHigh[8] == checksumHigh) ? (0b11 << (8 * 2)) : 0) | ((chunk.checksumHigh[9] == checksumHigh) ? (0b11 << (9 * 2)) : 0);

#if defined(_MSC_VER) || defined(__MINGW32__)
    unsigned long index;
#elif defined(__clang__) || defined(__GNUG__)
    int index;
#else
# error "!"
#endif
    if (mask != 0) {
        assert(_mm_popcnt_u32(mask) == 2);
#if defined(_MSC_VER) || defined(__MINGW32__)
        _BitScanForward(&index, mask);
#elif defined(__clang__) || defined(__GNUG__)
        index = __bsfd(mask);
#else
# error "!"
#endif
        index /= 2;
    } else {
        mask = _mm_movemask_epi8(_mm_cmpeq_epi16(*reinterpret_cast<const __m128i *>(&chunk.checksumHigh), _mm_set1_epi16(kDefaultChecksumHigh)));
        mask |= ((chunk.checksumHigh[8] == kDefaultChecksumHigh) ? (0b11 << (8 * 2)) : 0) | ((chunk.checksumHigh[9] == kDefaultChecksumHigh) ? (0b11 << (9 * 2)) : 0);
        assert(mask != 0); // fire if there is more then 10 collisions by checksumLow
#if defined(_MSC_VER) || defined(__MINGW32__)
        _BitScanForward(&index, mask);
#elif defined(__clang__) || defined(__GNUG__)
        index = __bsfd(mask);
#else
# error "!"
#endif
        index /= 2;
        chunk.checksumHigh[index] = checksumHigh;
        words[checksumLow][index] = uint32_t(std::distance(output, o));
        o = std::copy_n(std::next(input, wordEnd - len), len, o);
        *o++ = '\0';
    }
    assert(index < std::extent_v<decltype(chunk.count)>);
    ++chunk.count[index];
}

void countWords(uint32_t size)
{
    uint32_t checksum = kInitialChecksum;
    uint8_t len = 0;
    for (uint32_t i = 0; i < size; i += sizeof(__m128i)) {
        __m128i str = _mm_stream_load_si128(reinterpret_cast<__m128i *>(input + i));
        __m128i lowercase = _mm_add_epi8(_mm_and_si128(_mm_cmplt_epi8(str, _mm_set1_epi8('a')), _mm_set1_epi8('a' - 'A')), str);
        int mask = _mm_movemask_epi8(_mm_or_si128(_mm_cmplt_epi8(lowercase, _mm_set1_epi8('a')), _mm_cmpgt_epi8(lowercase, _mm_set1_epi8('z'))));
#define BYTE(offset)                                                                \
        if ((mask & (1 << offset)) == 0) {                                          \
            assert(len != std::numeric_limits<decltype(len)>::max());               \
            ++len;                                                                  \
            checksum = _mm_crc32_u8(checksum, _mm_extract_epi8(lowercase, offset)); \
        } else {                                                                    \
            if (len > 0) {                                                          \
                incCounter(checksum, i + offset, len);                              \
                len = 0;                                                            \
                checksum = kInitialChecksum;                                        \
            }                                                                       \
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

void addDyad(uint16_t dyad)
{
    volatile int i = 0;
    ++i;
}

void incCounter()
{
    volatile int i = 0;
    ++i;

}

} // namespace

int main(int argc, char * argv[])
{
    Timer timer;

    if (argc != 3) {
        fprintf(stderr, "usage: %s in.txt out.txt", argv[0]);
        return EXIT_FAILURE;
    }

    using namespace std::string_view_literals;

    std::unique_ptr<std::FILE, decltype((std::fclose))> inputFile{(argv[1] == "-"sv) ? stdin : std::fopen(argv[1], "rb"), std::fclose};
    if (!inputFile) {
        fprintf(stderr, "failed to open \"%s\" file to read", argv[1]);
        return EXIT_FAILURE;
    }

    std::unique_ptr<std::FILE, decltype((std::fclose))> outputFile{(argv[2] == "-"sv) ? stdout : std::fopen(argv[2], "wb"), std::fclose};
    if (!outputFile) {
        fprintf(stderr, "failed to open \"%s\" file to write", argv[2]);
        return EXIT_FAILURE;
    }

    timer.report("open files");

    auto size = uint32_t(std::fread(input, sizeof *input, std::extent_v<decltype(input)>, inputFile.get()));
    assert(size < std::extent_v<decltype(input)>);
    fprintf(stderr, "input size = %u bytes\n", size);
    {
        const uint32_t roundedUpSize = ((size + sizeof(__m128i) - 1) / sizeof(__m128i)) * sizeof(__m128i);
        std::fill(input + size, input + roundedUpSize, '\0');
        size = roundedUpSize;
    }
    timer.report("read input");

    {
        const auto inputEnd = input + size;
        for (auto in = input; in < inputEnd; in += sizeof(__m128i)) {
            __m128i str = _mm_stream_load_si128(reinterpret_cast<__m128i *>(in));
            __m128i lowercase = _mm_add_epi8(_mm_and_si128(_mm_cmplt_epi8(str, _mm_set1_epi8('a')), _mm_set1_epi8('a' - 'A')), str);
            __m128i mask = _mm_or_si128(_mm_cmplt_epi8(lowercase, _mm_set1_epi8('a')), _mm_cmpgt_epi8(lowercase, _mm_set1_epi8('z')));
            _mm_stream_si128(reinterpret_cast<__m128i *>(in), _mm_andnot_si128(mask, lowercase));
        }
        timer.report("lowercase input");

        if ((false)) {
            std::vector<std::string_view> words;
            auto lo = std::find_if_not(input, inputEnd, [](char c) { return c == '\0'; });
            assert(lo != inputEnd);
            do {
                auto hi = std::find(lo, inputEnd, '\0');
                std::reverse(lo, hi);
                words.emplace_back(lo, hi);
                lo = std::find_if_not(hi, inputEnd, [](char c) { return c == '\0'; });
            } while (lo != inputEnd);

            //std::sort(std::begin(words), std::end(words));
            //timer.report("sort words");

            std::unordered_map<std::string_view, size_t> wordCounts;
            for (const auto & word : words) {
                ++wordCounts[word];
            }
            timer.report("count words");

            size_t counter[4] = {};
            std::unordered_map<std::string_view, std::unordered_map<std::string_view, std::unordered_map<std::string_view, std::unordered_map<std::string_view, std::unordered_set<std::string_view>>>>> chain;
            for (const auto & [word, count] : wordCounts) {
                (void)count;
                size_t offset = 0;

                auto k0 = word.substr(offset, 2);
                offset += k0.size();
                auto & c0 = chain[k0];
                if (word.size() <= offset) {
                    continue;
                }

                auto k1 = word.substr(offset, 2);
                offset += k1.size();
                auto & c1 = c0[k1];
                counter[0] = std::max(counter[0], c0.size());
                if (word.size() <= offset) {
                    continue;
                }

                auto k2 = word.substr(offset, 2);
                offset += k2.size();
                auto & c2 = c1[k2];
                counter[1] = std::max(counter[1], c1.size());
                if (word.size() <= offset) {
                    continue;
                }

                auto k3 = word.substr(offset, 2);
                offset += k3.size();
                auto & c3 = c2[k3];
                counter[2] = std::max(counter[2], c2.size());
                if (word.size() <= offset) {
                    continue;
                }

                auto k4 = word.substr(offset);
                //offset += k4.size();
                //auto & c4 = c3[k4];
                c3.insert(k4);
                counter[3] = std::max(counter[3], c3.size());
            }
            timer.report("calc chain");

            fprintf(stderr, "counters = %zu %zu %zu %zu %zu\n", chain.size(), counter[0], counter[1], counter[2], counter[3]);

            std::map<size_t, size_t> counters[4];
            for (const auto & [w0, c0] : chain) {
                (void)w0;
                ++counters[0][c0.size()];
                for (const auto & [w1, c1] : c0) {
                    (void)w1;
                    ++counters[1][c1.size()];
                    for (const auto & [w2, c2] : c1) {
                        (void)w2;
                        ++counters[2][c2.size()];
                        for (const auto & [w3, c3] : c2) {
                            (void)w3;
                            ++counters[3][c3.size()];
                        }
                    }
                }
            }
            timer.report("calc counters");
            for (const auto & counter : counters) {
                for (const auto & [size, count] : counter) {
                    fprintf(stderr, "\t%zu %zu\n", size, count);
                }
                fprintf(stderr, "\n");
            }
            timer.report("print counters");
        }

        {
            uint32_t checksum = kInitialChecksum;
            uint32_t len = 0;
            uint16_t dyad = 0;
            for (uint32_t i = 0; i < size; i += sizeof(__m128i)) {
                __m128i str = _mm_stream_load_si128(reinterpret_cast<__m128i *>(input + i));
                __m128i lowercase = _mm_add_epi8(_mm_and_si128(_mm_cmplt_epi8(str, _mm_set1_epi8('a')), _mm_set1_epi8('a' - 'A')), str);
                int mask0 = _mm_movemask_epi8(_mm_or_si128(_mm_cmplt_epi8(lowercase, _mm_set1_epi8('a')), _mm_cmpgt_epi8(lowercase, _mm_set1_epi8('z'))));
                int mask1 = ~mask0 & 0xFFFFu;
                while (mask0 || mask1) {
                    if ((mask0 & 1) != 0) {
                        if (dyad) {
                            addDyad(dyad);
                            dyad = 0;
                        }
                        incCounter();

                        int lzcnt = __bsfd(~mask0);
                        mask0 >>= lzcnt;
                        mask1 >>= lzcnt;
                        if ((mask0 == 0) && (mask1 == 0)) {
                            break;
                        }
                        switch (lzcnt) {
#define CASE(offset) case offset : lowercase = _mm_srli_si128(lowercase, offset); break
                        CASE(1);
                        CASE(2);
                        CASE(3);
                        CASE(4);
                        CASE(5);
                        CASE(6);
                        CASE(7);
                        CASE(8);
                        CASE(9);
                        CASE(10);
                        CASE(11);
                        CASE(12);
                        CASE(13);
                        CASE(14);
                        CASE(15);
#undef CASE
                        default : {
                            assert(false);
                        }
                        }
                    }
                    if (dyad) {
                        addDyad((dyad << 8) | uint8_t(_mm_extract_epi8(lowercase, 0)));
                        dyad = 0;
                        mask0 >>= 1;
                        mask1 >>= 1;
                        lowercase = _mm_srli_si128(lowercase, 1);
                        continue;
                    }
                    int lzcnt = __bsfd(~mask1);
                    mask0 >>= lzcnt;
                    mask1 >>= lzcnt;
                    assert(lzcnt != 0);
                    switch (lzcnt) {
                    case 1 : {
                        dyad = uint8_t(_mm_extract_epi8(lowercase, 0));
                        lowercase = _mm_srli_si128(lowercase, 1);
                        break;
                    }
                    case 2 : {
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 0)));
                        lowercase = _mm_srli_si128(lowercase, 2);
                        break;
                    }
                    case 3 : {
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 0)));
                        dyad = uint8_t(_mm_extract_epi8(lowercase, 2));
                        lowercase = _mm_srli_si128(lowercase, 3);
                        break;
                    }
                    case 4 : {
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 0)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 1)));
                        lowercase = _mm_srli_si128(lowercase, 4);
                        break;
                    }
                    case 5 : {
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 0)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 1)));
                        dyad = uint8_t(_mm_extract_epi8(lowercase, 4));
                        lowercase = _mm_srli_si128(lowercase, 5);
                        break;
                    }
                    case 6 : {
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 0)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 1)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 2)));
                        lowercase = _mm_srli_si128(lowercase, 6);
                        break;
                    }
                    case 7 : {
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 0)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 1)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 2)));
                        dyad = uint8_t(_mm_extract_epi8(lowercase, 6));
                        lowercase = _mm_srli_si128(lowercase, 7);
                        break;
                    }
                    case 8 : {
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 0)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 1)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 2)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 3)));
                        lowercase = _mm_srli_si128(lowercase, 8);
                        break;
                    }
                    case 9 : {
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 0)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 1)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 2)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 3)));
                        dyad = uint8_t(_mm_extract_epi8(lowercase, 8));
                        lowercase = _mm_srli_si128(lowercase, 9);
                        break;
                    }
                    case 10 : {
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 0)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 1)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 2)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 3)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 4)));
                        lowercase = _mm_srli_si128(lowercase, 10);
                        break;
                    }
                    case 11 : {
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 0)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 1)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 2)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 3)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 4)));
                        dyad = uint8_t(_mm_extract_epi8(lowercase, 10));
                        lowercase = _mm_srli_si128(lowercase, 11);
                        break;
                    }
                    case 12 : {
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 0)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 1)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 2)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 3)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 4)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 5)));
                        lowercase = _mm_srli_si128(lowercase, 12);
                        break;
                    }
                    case 13 : {
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 0)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 1)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 2)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 3)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 4)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 5)));
                        dyad = uint8_t(_mm_extract_epi8(lowercase, 12));
                        lowercase = _mm_srli_si128(lowercase, 13);
                        break;
                    }
                    case 14 : {
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 0)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 1)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 2)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 3)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 4)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 5)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 6)));
                        lowercase = _mm_srli_si128(lowercase, 14);
                        break;
                    }
                    case 15 : {
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 0)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 1)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 2)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 3)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 4)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 5)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 6)));
                        dyad = uint8_t(_mm_extract_epi8(lowercase, 14));
                        lowercase = _mm_srli_si128(lowercase, 15);
                        break;
                    }
                    case 16 : {
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 0)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 1)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 2)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 3)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 4)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 5)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 6)));
                        addDyad(uint16_t(_mm_extract_epi16(lowercase, 7)));
                        //lowercase = _mm_srli_si128(lowercase, 16);
                        break;
                    }
                    }
                }
            }
            if (dyad) {
                addDyad(dyad);
            }
            incCounter();
        }
        timer.report("count words");
    }
    return 0;

    countWords(size);
    timer.report("count words");

    for (auto out = output; out < o; out += sizeof(__m128i)) {
        __m128i str = _mm_stream_load_si128(reinterpret_cast<__m128i *>(out));
        __m128i mask = _mm_or_si128(_mm_cmplt_epi8(str, _mm_set1_epi8('A')), _mm_cmpgt_epi8(str, _mm_set1_epi8('Z')));
        _mm_stream_si128(reinterpret_cast<__m128i *>(out), _mm_add_epi8(_mm_andnot_si128(mask, _mm_set1_epi8('a' - 'A')), str));
    }
    timer.report("lowercase output");

    std::vector<std::pair<uint32_t, std::string_view>> rank;
    rank.reserve(std::extent_v<decltype(words)> * std::extent_v<decltype(words), 1>);
    {
        uint32_t checksumLow = 0;
        for (const auto & w : words) {
            const Chunk & chunk = hashTable[checksumLow];
            uint32_t index = 0;
            for (uint32_t word : w) {
                if (word) {
                    rank.emplace_back(chunk.count[index], output + word);
                }
                ++index;
            }
            ++checksumLow;
        }
    }
    fprintf(stderr, "load factor = %.3lf\n", rank.size() / double(rank.capacity()));
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
