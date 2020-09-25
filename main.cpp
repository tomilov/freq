#include "io.hpp"
#include "timer.hpp"

#include <utility>
#include <memory>
#include <vector>
#include <iterator>
#include <algorithm>

#include <cstring>
#include <cstdlib>
#include <cstdio>

using size_type = uint32_t;

static constexpr auto AlphabetSize = 'z' - 'a' + 1;

struct TrieNode
{
    size_type count = 0;
    size_type children[AlphabetSize] = {};
};

struct HashNode
{
    uint32_t hash0 = 0;
    uint32_t hash1 = 0;
    size_type count = 0;
    const uchar * wordBeg = nullptr;
    const uchar * wordEnd = nullptr;
};

static HashNode hashMap[size_t(1) << 24] = {};

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

    constexpr auto WholeInputSize = size_t(512) * 1024 * 1024;
    auto inputStream = std::make_unique<InputStream<WholeInputSize>>(inputFile.get());

    timer.report("create input stream");

    const auto beg = inputStream->begin();
    const auto end = inputStream->end();
    if ((true)) {
        auto wordBeg = beg;
        while (wordBeg != end) {
            while (*wordBeg == '\0') {
                if (++wordBeg == end) {
                    break;
                }
            }
            if (wordBeg == end) {
                break;
            }
            auto hash0 = ~uint32_t(0);
            auto hash1 = ~uint32_t(1);
            auto wordEnd = wordBeg;
            while (*wordEnd != '\0') {
                hash0 = _mm_crc32_u8(hash0, *wordEnd);
                hash1 = _mm_crc32_u8(hash1, *wordEnd);
                if (++wordEnd == end) {
                    break;
                }
            }

            auto mapElement = hashMap + (hash0 % std::size(hashMap));
            if (mapElement->count == 0) {
                mapElement->hash0 = hash0;
                mapElement->hash0 = hash1;
                mapElement->wordBeg = wordBeg;
                mapElement->wordEnd = wordEnd;
            } else {
                while ((mapElement->hash0 != hash0) || (mapElement->hash1 != hash1)/* || !std::equal(mapElement->wordBeg, mapElement->wordEnd, wordBeg, wordEnd)*/) {
                    if (++mapElement == std::end(hashMap)) {
                        mapElement = std::begin(hashMap);
                    }
                    if (mapElement->count == 0) {
                        mapElement->hash0 = hash0;
                        mapElement->hash1 = hash1;
                        mapElement->wordBeg = wordBeg;
                        mapElement->wordEnd = wordEnd;
                        break;
                    }
                }
            }
            ++mapElement->count;

            wordBeg = wordEnd;
        }
        timer.report("build counting hashmap from input");
    } else {
        std::vector<TrieNode> trie(1);
        size_type index = 0;
        for (auto it = beg; it != end; ++it) {
            if (*it != '\0') {
                size_type & child = trie[index].children[*it - 'a'];
                if (child == 0) {
                    child = size_type(trie.size());
                    index = child;
                    trie.emplace_back();
                } else {
                    index = child;
                }
            } else {
                if (index != 0) {
                    ++trie[index].count;
                    index = 0;
                }
            }
        }
        fprintf(stderr, "trie size = %zu\n", trie.size());

        timer.report("build counting trie from input");

        std::vector<std::pair<size_type, size_type>> rank;
        std::vector<uchar> words;

        std::vector<uchar> word;
        auto traverseTrie = [&] (const auto & traverseTrie, decltype((std::as_const(trie).front().children)) children) -> void
        {
            int c = 0;
            for (size_type index : children) {
                if (index != 0) {
                    const TrieNode & node = trie[index];
                    word.push_back(uchar('a' + c));
                    if (node.count != 0) {
                        rank.emplace_back(size_type(node.count), size_type(words.size()));
                        words.insert(words.cend(), std::cbegin(word), std::cend(word));
                        words.push_back(uchar('\0'));
                    }
                    traverseTrie(traverseTrie, node.children);
                    word.pop_back();
                }
                ++c;
            }
        };
        traverseTrie(traverseTrie, trie.front().children);
        assert(word.empty());
        fprintf(stderr, "word count = %zu, length = %zu\n", rank.size(), words.size());

        timer.report("recover words from trie");

        std::stable_sort(std::begin(rank), std::end(rank), [] (auto && l, auto && r) { return r.first < l.first; });

        timer.report("rank words");

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
    }

    return EXIT_SUCCESS;
}
