#include "helpers.hpp"
#include "io.hpp"
#include "timer.hpp"

#include <fmt/color.h>
#include <fmt/format.h>

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

#include <unistd.h>

namespace
{
alignas(__m128i) char input[1 << 29];
auto inputEnd = input;

// perfect hash seeds 8, 23, 89, 126, 181, 331, 381, 507, ...
constexpr uint32_t kInitialChecksum = 23;

#pragma pack(push, 1)
struct uint24
{
    unsigned long long int value : 24 = 0;

    uint24 operator++(int) &
    {
        auto temp = *this;
        ++value;
        return temp;
    }

    operator uint32_t() const
    {
        return value;
    }
};
#pragma pack(pop)

static_assert(sizeof(uint24) == 3);

constexpr std::size_t kPageSize = 4096;

alignas(kPageSize)
    uint24 counts[std::size_t(std::numeric_limits<uint32_t>::max()) + 1];

alignas(__m128i) char output[1 << 22] = {};
auto o = output;

alignas(kHardwareDestructiveInterferenceSize)
    uint24 words[std::size_t(std::numeric_limits<uint32_t>::max()) + 1];

void incCounter(uint32_t hash, const char * __restrict wordEnd, uint32_t len)
{
    if UNLIKELY (counts[hash]++ == 0) {
        words[hash].value = uint32_t(std::distance(output, o));
        o = std::next(std::copy_n(std::prev(wordEnd, len), len, o));
    }
}

void countWords()
{
    uint32_t hash = kInitialChecksum;
    uint32_t len = 0;
    for (auto i = input; LIKELY(i < inputEnd); i += sizeof(__m128i)) {
        __m128i str = _mm_load_si128(reinterpret_cast<const __m128i *>(i));
        str =
            _mm_add_epi8(_mm_and_si128(_mm_cmplt_epi8(str, _mm_set1_epi8('a')),
                                       _mm_set1_epi8('a' - 'A')),
                         str);
        __m128i mask = _mm_or_si128(_mm_cmplt_epi8(str, _mm_set1_epi8('a')),
                                    _mm_cmpgt_epi8(str, _mm_set1_epi8('z')));
        uint16_t m = uint16_t(_mm_movemask_epi8(mask));
        // clang-format off
#define BYTE(offset)                                                           \
        if UNPREDICTABLE ((m & (uint32_t(1) << offset)) == 0) {                \
            ++len;                                                             \
            hash = _mm_crc32_u8(hash, uint8_t(_mm_extract_epi8(str, offset))); \
        } else if UNPREDICTABLE (len != 0) {                                   \
            incCounter(hash, std::next(i, offset), len);                       \
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
        // clang-format on
    }
    if (len != 0) {
        incCounter(hash, inputEnd, len);
    }
}

}  // namespace

int main(int argc, char * argv[])
{
    Timer timer{fmt::format(fg(fmt::color::dark_green), "total")};

    if (argc != 3) {
        fmt::print(stderr, "usage: {} in.txt out.txt\n", argv[0]);
        return EXIT_FAILURE;
    }

    using namespace std::string_view_literals;

    auto inputFile =
        (argv[1] == "-"sv) ? wrapFile(stdin) : openFile(argv[1], "rb");
    if (!inputFile) {
        fmt::print(stderr, "failed to open '{}' file to read\n", argv[1]);
        return EXIT_FAILURE;
    }

    auto outputFile =
        (argv[2] == "-"sv) ? wrapFile(stdout) : openFile(argv[2], "wb");
    if (!outputFile) {
        fmt::print(stderr, "failed to open '{}' file to write\n", argv[2]);
        return EXIT_FAILURE;
    }

    std::size_t readSize =
        readInput(std::begin(input), std::size(input), inputFile);
    if (readSize == 0) {
        return EXIT_SUCCESS;
    }
    inputEnd += readSize;
    timer.report("read input");

    countWords();
    timer.report(fmt::format(fg(fmt::color::dark_blue), "count words"));

    toLower(output, o);
    timer.report("make output lowercase");

    std::vector<std::pair<uint32_t, std::string_view>> rank;
    rank.reserve(213637);
    if ((false)) {
        for (std::size_t i = 0; i < std::extent_v<decltype(counts)>; ++i) {
            if (auto count = uint32_t(counts[i]); count != 0) {
                rank.emplace_back(count, std::next(output, words[i].value));
            }
        }
    } else {
        if (auto pageSize = std::size_t(getpagesize()); pageSize != kPageSize) {
            fmt::print(stderr, "change value of kPageSize to {}\n", pageSize);
            return EXIT_FAILURE;
        }

        auto pagemapFile = openFile("/proc/self/pagemap", "rb");
        if (!pagemapFile) {
            fmt::print(stderr, "failed to open pagemap file to write\n");
            return EXIT_FAILURE;
        }

        using PmEntry = uint64_t;
        constexpr std::size_t kPmPresent = 1ULL << 63;

        auto lowerAddress = reinterpret_cast<std::uintptr_t>(counts + 0);
        auto upperAddress = lowerAddress + sizeof counts;
        if (fseeko64(pagemapFile.get(),
                     sizeof(PmEntry) * (lowerAddress / kPageSize),
                     SEEK_SET) != 0)
        {
            fmt::print(stderr, "failed to seek pagemap file\n");
            return EXIT_FAILURE;
        }
        std::vector<PmEntry> pagemap((upperAddress + kPageSize - 1) /
                                         kPageSize -
                                     lowerAddress / kPageSize);
        std::size_t readSize = std::fread(pagemap.data(), sizeof pagemap.back(),
                                          pagemap.size(), pagemapFile.get());
        if (readSize != pagemap.size()) {
            fmt::print(stderr, "error during reading of pagemap file\n");
            return EXIT_FAILURE;
        }

        auto isPagePresent = [](const PmEntry & entry) {
            return (entry & kPmPresent) != 0;
        };
        auto hi = std::cbegin(pagemap);
        for (;;) {
            auto lo = std::find_if(hi, std::cend(pagemap), isPagePresent);
            if (lo == std::cend(pagemap)) {
                break;
            }
            hi = std::find_if_not(lo, std::cend(pagemap), isPagePresent);
            auto l = (std::distance(std::cbegin(pagemap), lo) * kPageSize +
                      sizeof counts[0] - 1) /
                     sizeof counts[0];
            auto r = (std::distance(std::cbegin(pagemap), hi) * kPageSize) /
                     sizeof counts[0];
            for (auto i = l; i != r; ++i) {
                if (auto count = uint32_t(counts[i]); count != 0) {
                    rank.emplace_back(count, std::next(output, words[i].value));
                }
            }
        }
    }
    fmt::print(stderr, "load factor = {:.3}\n",
               double(rank.size()) / double(rank.capacity()));
    timer.report("collect word counts");

    auto less = [](auto && lhs, auto && rhs) {
        return std::tie(rhs.first, lhs.second) <
               std::tie(lhs.first, rhs.second);
    };
    std::sort(std::begin(rank), std::end(rank), less);
    timer.report(fmt::format(fg(fmt::color::dark_orange), "sort words"));

    OutputStream<> outputStream{outputFile};
    for (const auto & [count, word] : rank) {
        if (!outputStream.print(count)) {
            fmt::print(stderr, "output failure\n");
            return EXIT_FAILURE;
        }
        if (!outputStream.putChar(' ')) {
            fmt::print(stderr, "output failure\n");
            return EXIT_FAILURE;
        }
        if (!outputStream.print(word.data())) {
            fmt::print(stderr, "output failure\n");
            return EXIT_FAILURE;
        }
        if (!outputStream.putChar('\n')) {
            fmt::print(stderr, "output failure\n");
            return EXIT_FAILURE;
        }
    }
    timer.report("write output");

    return EXIT_SUCCESS;
}
