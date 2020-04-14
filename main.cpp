#include "io.hpp"
#include "timer.hpp"

#include <utility>
#include <memory>
#include <vector>
#include <iterator>
#include <algorithm>
#include <tuple>
#include <unordered_map>

#include <cstring>
#include <cstdlib>
#include <cstdio>

static constexpr auto AlphabetSize = 'z' - 'a' + 1;

using size_type = uint32_t;

inline uint32_t crc32c(const uchar * begin, const uchar * end)
{
    auto result = ~uint32_t(0);
#ifdef __x86_64__
    while (begin + sizeof(uint64_t) <= end) {
        result = uint32_t(_mm_crc32_u64(uint64_t(result), *reinterpret_cast<const uint64_t *>(begin)));
        begin += sizeof(uint64_t);
    }
#endif
    while (begin + sizeof(uint32_t) <= end) {
        result = _mm_crc32_u32(result, *reinterpret_cast<const uint32_t *>(begin));
        begin += sizeof(uint32_t);
    }
    while (begin < end) {
        result = _mm_crc32_u8(result, *begin);
        ++begin;
    }
    return ~result;
};

static std::tuple<uint32_t, int32_t/*, int32_t*/> hashes[100'000'000] = {};

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

    auto inputStream = std::make_unique<InputStream<400'000'000>>(inputFile.get());

    timer.report("read input");

    size_type h = 0;
    uint32_t hash = ~uint32_t(0);
    const auto first = inputStream->begin();
    const auto last = inputStream->end();
    auto it = first;
    auto start = it;
    while (it != last) {
        if (*it == '\0') {
            if (start != it) {
                hashes[h++] = {hash, int32_t(start - first)/*, int32_t(it - start)*/};
                hash = ~uint32_t(0);
                start = it;
            }
        } else {
            if (*start == '\0') {
                start = it;
            }
            //hash =_mm_crc32_u8(hash, *it);
        }
        ++it;
    }

    timer.report("1");
    //fprintf(stderr, "%zu %zu %zu %zu", n4, n8, n16, n); 37797872 17370574 3631578 1257

    auto less = [first] (auto && l, auto && r)
    {
        if (std::get<0>(l) < std::get<0>(r)) {
            return false;
        } else if (std::get<0>(r) < std::get<0>(l)) {
            return true;
        } else {
            return std::strcmp(reinterpret_cast<const char *>(first + std::get<1>(l)), reinterpret_cast<const char *>(first + std::get<1>(r))) < 0;
        }
    };
    std::sort(hashes + 0, hashes + h, less);

    timer.report("2");

    std::vector<std::pair<size_type, size_type>> rank;
    std::vector<uchar> words;
    std::stable_sort(std::begin(rank), std::end(rank), [] (auto && l, auto && r) { return r.first < l.first; });

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
        if (!outputStream.print(words.data() + word)) {
            fprintf(stderr, "output failure");
            return EXIT_FAILURE;
        }
        if (!outputStream.putChar('\n')) {
            fprintf(stderr, "output failure");
            return EXIT_FAILURE;
        }
    }

    timer.report("output");

    return EXIT_SUCCESS; // 250156885 58801281 4.254276
}
