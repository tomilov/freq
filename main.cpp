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

struct alignas(sizeof(uint32_t) + sizeof(size_type)) HashNode
{
    uint32_t hashElement = 0;
    size_type count = 0;
};

static HashNode hashMap[10000000] = {};

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

    timer.report("create input stream");

    uint32_t hash = ~uint32_t(0);
    std::vector<uchar> words;
    words.resize(5000000);
    auto wordBeg = std::data(words), wordEnd = wordBeg;
    auto wordsEnd = std::data(words) + std::size(words);
    size_t counters[8] = {};
    for (;;) {
        //++counters[0];
        int c = inputStream.getChar();
        if (c > '\0') {
            //++counters[1];
            *wordEnd++ = uchar(c);
            hash = _mm_crc32_u8(hash, uchar(c));/*
            if (wordEnd == wordsEnd) {
                //++counters[2];
                auto distance = std::distance(std::data(words), wordEnd);
                auto length = std::distance(wordBeg, wordEnd);
                words.resize(words.size() * 2);
                wordEnd = std::next(std::data(words), distance);
                wordBeg = std::next(wordEnd, length);
                wordsEnd = std::data(words) + std::size(words);
            }*/
        } else {
            //++counters[3];
            auto & mapElement = hashMap[hash % std::size(hashMap)];
            if (mapElement.count == 0) {
                //++counters[4];
                mapElement.hashElement = hash;
                mapElement.count = 1;
                *wordEnd++ = '\0';/*
                if (wordEnd == wordsEnd) {
                    //++counters[5];
                    auto distance = std::distance(std::data(words), wordEnd);
                    words.resize(words.size() * 2);
                    wordEnd = std::next(std::data(words), distance);
                    wordsEnd = std::data(words) + std::size(words);
                }*/
                wordBeg = wordEnd;
            } else /*if (mapElement.hashElement == hash)*/ {
                //++counters[6];
                ++mapElement.count;
                wordEnd = wordBeg;
            }/* else {
                //++counters[7];
                ++mapElement.count;
                wordEnd = wordBeg;
            }*/
            hash = ~uint32_t(0);
            if (c < 0) {
                break;
            }
        }
    }
    size_t i = 0;
    for (size_t counter : counters) {
        fprintf(stderr, "%zu: %zu %lf\n", i++, counter, counter / double(counters[0]));
    }

    timer.report("build counting hashmap from input");

    if ((false))
    {
        std::vector<TrieNode> trie(1);
        size_type index = 0;
        for (;;) {
            int c = inputStream.getChar();
            if (c > '\0') {
                size_type & child = trie[index].children[c - 'a'];
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
                if (c < 0) {
                    break;
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
