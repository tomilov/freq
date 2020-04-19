#include "io.hpp"
#include "timer.hpp"

#include <utility>
#include <memory>
#include <vector>
#include <iterator>
#include <algorithm>
#include <array>
#include <limits>

#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <cstdio>

static constexpr auto AlphabetSize = 'z' - 'a' + 1;
static constexpr uint16_t MaxWordLength = 100;

template<typename size_type>
struct TrieNode
{
    size_t count = 0;
    size_type children[AlphabetSize] = {};
};

static_assert(sizeof(TrieNode<uint16_t>) <= 64, "!");
static_assert(sizeof(TrieNode<uint32_t>) <= 2 * 64, "!");

using SmallTrie = std::array<TrieNode<uint16_t>, std::numeric_limits<uint16_t>::max()>;
using BigTrie = std::vector<TrieNode<uint32_t>>;

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

    InputStream<> inputStream{inputFile.get()}; // InputStream::buffer lies on the stack

    std::vector<SmallTrie> tries(1);
    {
        SmallTrie * trie = tries.data();
        uint16_t trieSize = 0, index = 0;
        for (;;) {
            int c = inputStream.getChar();
            if (c > '\0') {
                auto & child = (*trie)[index].children[c - 'a'];
                if (child == 0) {
                    child = ++trieSize;
                }
                index = child;
            } else {
                if (index != 0) {
                    ++(*trie)[index].count;
                    index = 0;
                    if (trieSize + MaxWordLength >= (*trie).size()) {
                        trie = &tries.emplace_back();
                        trieSize = 0;
                        index = 0;
                    }
                }
                if (c < 0) {
                    break;
                }
            }
        }
        fprintf(stderr, "tries size = %zu\n", tries.size());
    }

    timer.report("build counting trie from input");

    BigTrie trie(1);
    for (const auto & smallTrie : tries) {
        auto traverseTries = [&] (const auto & traverseTries, const auto & children, uint32_t dstIndex) -> void
        {
            int c = 0;
            for (auto srcChild : children) {
                if (srcChild != 0) {
                    auto & dstChild = trie[dstIndex].children[c];
                    if (dstChild == 0) {
                        dstChild = uint32_t(trie.size());
                        trie.emplace_back();
                    }
                    traverseTries(traverseTries, smallTrie[srcChild].children, trie[dstIndex].children[c]);
                }
                ++c;
            }
        };
        traverseTries(traverseTries, smallTrie.front().children, 0);
    }

    timer.report("merge small tries into big trie");

    std::vector<std::pair<uint32_t, uint32_t>> rank;
    std::vector<uchar> words;

    std::vector<uchar> word;
    auto traverseTrie = [&] (const auto & traverseTrie, decltype((std::as_const(trie).front().children)) children) -> void
    {
        int c = 0;
        for (auto index : children) {
            if (index != 0) {
                const auto & node = trie[index];
                word.push_back(uchar('a' + c));
                if (node.count != 0) {
                    rank.emplace_back(uint32_t(node.count), uint32_t(words.size()));
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

    return EXIT_SUCCESS;
}
