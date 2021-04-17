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
#include <array>

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
        __m128i mask = _mm_or_si128(_mm_cmplt_epi8(lowercase, _mm_set1_epi8('a')), _mm_cmpgt_epi8(lowercase, _mm_set1_epi8('z')));
        int m = _mm_movemask_epi8(mask);

#if 0
        alignas(__m128i) char line[sizeof(__m128i)];
        _mm_store_si128(reinterpret_cast<__m128i *>(line), _mm_andnot_si128(mask, lowercase));

#pragma GCC unroll 16
        for (size_t offset = 0; offset < sizeof(__m128i); ++offset) {
            if ((m & (1 << offset)) == 0) {
                assert(len != std::numeric_limits<decltype(len)>::max());
                ++len;
                checksum = _mm_crc32_u8(checksum, line[offset]);
            } else {
                if (len > 0) {
                    incCounter(checksum, i + offset, len);
                    len = 0;
                    checksum = kInitialChecksum;
                }
            }
        }
#else
#define BYTE(offset)                                                                \
        if ((m & (1 << offset)) == 0) {                                             \
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
#endif
    }
    if (len > 0) {
        incCounter(checksum, size, len);
    }
}

#pragma pack(push, 1)
struct uint24
{
    uint32_t value : 24;
};
#pragma pack(pop)
static_assert(sizeof(uint24) == 3);

#pragma pack(push, 1)
struct alignas(kHardwareConstructiveInterferenceSize) Trie
{
    char c[sizeof(__m128i)];
    uint24 node[sizeof(__m128i)];
};
#pragma pack(pop)
static_assert(sizeof(Trie) == kHardwareConstructiveInterferenceSize);

constexpr size_t kTrieMaxSize = (size_t(1) << (std::numeric_limits<uint8_t>::digits * sizeof(uint24))) / 8;

constexpr int kAlhpabetSize = 'z' - 'a' + 1;

uint32_t counts[2 * kTrieMaxSize][sizeof(__m128i)] = {};
uint32_t leafCounts[2 * kTrieMaxSize][sizeof(__m128i)] = {};
uint32_t firstCount[kAlhpabetSize * (kAlhpabetSize + 1) * (kAlhpabetSize + 1) * (kAlhpabetSize + 1)] = {};
uint32_t firstLeafCount[std::extent_v<decltype(firstCount)>] = {};

Trie trie[2 * kTrieMaxSize] = {};
int trieTail = int(std::extent_v<decltype(firstCount)>);
int currNode = -1;
int prevIndex = -1;

int ccc = 0;

void addChar2(char c)
{
    Trie * node = trie + currNode;
    if (prevIndex < 0) {
        __m128i C = _mm_set1_epi8(c);
        __m128i chars = _mm_load_si128(reinterpret_cast<const __m128i *>(node->c));
        __m128i zeros = _mm_set1_epi8('\0');
        int m = _mm_movemask_epi8(_mm_or_si128(_mm_cmpeq_epi8(zeros, chars), _mm_cmpeq_epi8(C, chars)));
        if (m == 0) {
            currNode += kTrieMaxSize;
            node += kTrieMaxSize;
            chars = _mm_load_si128(reinterpret_cast<const __m128i *>(node->c));
            m = _mm_movemask_epi8(_mm_or_si128(_mm_cmpeq_epi8(zeros, chars), _mm_cmpeq_epi8(C, chars)));
        }
        prevIndex = __bsfd(m);
        node->c[prevIndex] = c;
    } else {
        currNode = node->node[prevIndex].value;
        if (currNode == 0) {
            node->node[prevIndex].value = trieTail;
            currNode = trieTail;
            ++trieTail;
        }
        prevIndex = -1;
        addChar2(c);
    }
}

void countWords2(uint32_t size)
{
    uint32_t len = 0;
    for (uint32_t i = 0; i < size; i += sizeof(__m128i)) {
        __m128i str = _mm_load_si128(reinterpret_cast<__m128i *>(input + i));
        __m128i lowercase = _mm_add_epi8(_mm_and_si128(_mm_cmplt_epi8(str, _mm_set1_epi8('a')), _mm_set1_epi8('a' - 'A')), str);
        __m128i mask = _mm_or_si128(_mm_cmplt_epi8(lowercase, _mm_set1_epi8('a')), _mm_cmpgt_epi8(lowercase, _mm_set1_epi8('z')));
        //int m = _mm_movemask_epi8(mask); // ?
        alignas(__m128i) char line[sizeof(__m128i)];
        _mm_store_si128(reinterpret_cast<__m128i *>(line), _mm_andnot_si128(mask, lowercase));

        for (size_t offset = 0; offset < sizeof(__m128i); ++offset) {
            //if ((m & (1 << offset)) == 0) {
            if (char c = line[offset]; c != '\0') {
                if (len < 4) {
                    if (len == 0) {
                        currNode = c - 'a';
                    } else {
                        currNode *= kAlhpabetSize + 1;
                        currNode += c - 'a' + 2;
                    }
                } else {
                    addChar2(c);
                }
                ++len;
            } else if (len != 0) {
                if (len <= 4) {
                    ++firstCount[currNode];
                } else {
                    ++counts[currNode][prevIndex];
                    prevIndex = -1;
                }
                len = 0;
            }
        }
    }
    if (len != 0) {
        if (len <= 4) {
            ++firstCount[currNode];
        } else {
            ++counts[currNode][prevIndex];
        }
    }
}

void sortLetters()
{
    const auto traverse = [](const auto & traverse, int currNode) -> uint32_t
    {
        const auto & node = trie[currNode].node;
        const auto & count = counts[currNode];
        auto & leafCount = leafCounts[currNode];
        uint32_t result = 0;
        for (size_t i = 0; i < sizeof(__m128i); ++i) {
            result += count[i];
            const uint24 & n = node[i];
            if (n.value != 0) {
                result += (leafCount[i] = traverse(traverse, n.value));
            }
        }
        if (currNode < kTrieMaxSize) {
            result += traverse(traverse, currNode + kTrieMaxSize);
        }
        return result;
    };
    for (int currNode = 0; currNode < int(std::extent_v<decltype(firstCount)>); ++currNode) {
        firstLeafCount[currNode] = firstCount[currNode] + traverse(traverse, currNode);
    }
    for (int currNode = 0; currNode < trieTail; ++currNode) {
        struct Order
        {
            uint32_t leafCount;
            uint32_t count;
            char c;
            uint24 node;

            bool operator < (const Order & rhs) const
            {
                return leafCount < rhs.leafCount;
            }
        };
        std::array<Order, sizeof(__m128i) * 2> order;
        for (size_t i = 0; i < sizeof(__m128i); ++i) {
            order[i] = {leafCounts[currNode][i], counts[currNode][i], trie[currNode].c[i], trie[currNode].node[i]};
            order[i + sizeof(__m128i)] = {leafCounts[currNode + kTrieMaxSize][i], counts[currNode + kTrieMaxSize][i], trie[currNode + kTrieMaxSize].c[i], trie[currNode + kTrieMaxSize].node[i]};
        }
        std::sort(std::rbegin(order), std::rend(order));
        for (size_t i = 0; i < sizeof(__m128i); ++i) {
            leafCounts[currNode][i] = order[i].leafCount;
            counts[currNode][i] = order[i].count;
            trie[currNode].c[i] = order[i].c;
            trie[currNode].node[i] = order[i].node;

            leafCounts[currNode + kTrieMaxSize][i] = order[i + sizeof(__m128i)].leafCount;
            counts[currNode + kTrieMaxSize][i] = order[i + sizeof(__m128i)].count;
            trie[currNode + kTrieMaxSize].c[i] = order[i + sizeof(__m128i)].c;
            trie[currNode + kTrieMaxSize].node[i] = order[i + sizeof(__m128i)].node;
        }
    }
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
        fprintf(stderr, "failed to open \"%s\" file to read\n", argv[1]);
        return EXIT_FAILURE;
    }

    std::unique_ptr<std::FILE, decltype((std::fclose))> outputFile{(argv[2] == "-"sv) ? stdout : std::fopen(argv[2], "wb"), std::fclose};
    if (!outputFile) {
        fprintf(stderr, "failed to open \"%s\" file to write\n", argv[2]);
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

    countWords2(size);
    timer.report("count words 1");
    fprintf(stderr, "%i\n", trieTail);

    sortLetters();
    timer.report("sort letters");

    countWords2(size);
    timer.report("count words 2");
    fprintf(stderr, "%i\n", trieTail);

    for (auto out = output; out < o; out += sizeof(__m128i)) {
        __m128i str = _mm_stream_load_si128(reinterpret_cast<__m128i *>(out));
        __m128i mask = _mm_or_si128(_mm_cmplt_epi8(str, _mm_set1_epi8('A')), _mm_cmpgt_epi8(str, _mm_set1_epi8('Z')));
        _mm_stream_si128(reinterpret_cast<__m128i *>(out), _mm_add_epi8(_mm_andnot_si128(mask, _mm_set1_epi8('a' - 'A')), str));
    }
    timer.report("make output lowercase");

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
