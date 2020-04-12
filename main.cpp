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

static constexpr int AlphabetSize = 'z' - 'a' + 1;

struct TrieNode
{
    int c = -1;
    size_t parent = 0;
    size_t count = 0;
    size_t children[AlphabetSize] = {};
};

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

    std::vector<TrieNode> trie(1);
    size_t index = 0;
    for (;;) {
        int c = inputStream.getChar();
        if (c > '\0') {
            size_t & child = trie[index].children[c - 'a'];
            if (child == 0) {
                child = trie.size();
                trie.push_back({c, std::exchange(index, child)});
            } else {
                index = child;
            }
        } else {
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

    std::vector<uchar> word, words;
    std::vector<std::pair<size_t, size_t>> rank;

    if ((true)) {
        auto traverseTrie = [&] (const auto & traverseTrie, decltype((std::as_const(trie).front().children)) children) -> void
        {
            for (size_t index : children) {
                if (index == 0) {
                    continue;
                }
                const TrieNode & node = trie[index];
                word.push_back(uchar(node.c));
                if (node.count != 0) {
                    rank.emplace_back(node.count, words.size());
                    words.insert(words.cend(), std::cbegin(word), std::cend(word));
                    words.push_back(uchar('\0'));
                }
                traverseTrie(traverseTrie, node.children);
                word.pop_back();
            }
        };
        traverseTrie(traverseTrie, trie.front().children);
        assert(word.empty());
        fprintf(stderr, "word count = %zu\n", rank.size());

        timer.report("recover words from trie"); // 58 10

        std::stable_sort(std::begin(rank), std::end(rank), [] (auto && l, auto && r) { return r.first < l.first; });
    } else {
        assert(index == 0);
        for (const TrieNode & leaf : trie) {
            if (leaf.count != 0) {
                word.clear();
                size_t parent = index;
                do {
                    const TrieNode & node = trie[parent];
                    word.push_back(uchar(node.c));
                    parent = node.parent;
                } while (parent != 0);
                rank.emplace_back(leaf.count, words.size());
                std::reverse(std::begin(word), std::end(word));
                words.insert(words.cend(), std::cbegin(word), std::cend(word));
                words.push_back(uchar('\0'));
            }
            ++index;
        }

        timer.report("recover words from trie"); // 37 60

        auto less = [words = words.data(), wordsEnd = &words.back() + 1] (auto && l, auto && r)
        {
            if (r.first < l.first) {
                return true;
            } else if (l.first < r.first) {
                return false;
            } else {
                return std::lexicographical_compare(words + l.second, wordsEnd, words + r.second, wordsEnd);
            }
        };
        std::stable_sort(std::begin(rank), std::end(rank), less);
    }

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
