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

namespace gperf
{
namespace
{
// ./freq pg.txt out.txt && cut -d' ' -f2 out.txt | head -1000 >top.gperf && ./gperf -m50 -G -rL C++ top.gperf >gperf.hpp
#include "gperf.hpp"
} // static namespace
} // namespace gperf

static constexpr int AlphabetSize = 'z' - 'a' + 1;

struct TrieNode
{
    size_t count = 0;
    size_t children[AlphabetSize] = {};
};

static size_t topCount[MAX_HASH_VALUE + 1] = {};

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

    char str[100];
    auto ss = str;
    std::vector<TrieNode> trie(1);
    size_t index = 0;
    for (;;) {
        int c = inputStream.getChar();
        if (c > '\0') {
            *ss++ = char(c);
        } else {
            *ss = '\0';
            int hashValue = 0;
            {
                using namespace  gperf;
                if (ss <= str + MAX_WORD_LENGTH && ss >= str + MIN_WORD_LENGTH) {
                    int key = Perfect_Hash::hash(str, ss - str);
                    if (key <= MAX_HASH_VALUE && key >= MIN_HASH_VALUE) {
                        const char * s = wordlist[key];
                        if ((*str == *s) && (strcmp(str + 1, s + 1) == 0)) {
                            hashValue = key;
                        }
                    }
                }
            }
            if (hashValue < MIN_HASH_VALUE) {
                for (auto sss = str; sss != ss; ++sss) {
                    size_t & child = trie[index].children[*sss - 'a'];
                    if (child == 0) {
                        child = trie.size();
                        index = child;
                        trie.emplace_back();
                    } else {
                        index = child;
                    }
                }
            } else {
                ++topCount[hashValue];
            }
            ss = str;
            if (index != 0) {
                ++trie[index].count;
                index = 0;
            }
            if (c < 0) {
                break;
            }
        }
    }
    fprintf(stderr, "trie size = %zu\n", trie.size());

    timer.report("build counting trie from input");

    std::vector<std::pair<size_t, size_t>> rank;
    std::vector<uchar> words;

    std::vector<uchar> word;
    auto traverseTrie = [&] (const auto & traverseTrie, decltype((std::as_const(trie).front().children)) children) -> void
    {
        int c = 0;
        for (size_t index : children) {
            if (index != 0) {
                const TrieNode & node = trie[index];
                word.push_back(uchar('a' + c));
                if (node.count != 0) {
                    rank.emplace_back(node.count, words.size());
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
