#include "helpers.hpp"
#include "io.hpp"
#include "timer.hpp"

#include <fmt/color.h>
#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <iterator>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace
{

constexpr std::size_t kAlphabetSize = 'z' - 'a' + 1;

alignas(__m128i) char input[1 << 29];
auto inputEnd = input;

struct TrieNode
{
    uint32_t count = 0;
    uint32_t children[kAlphabetSize] = {};
};

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

    timer.report("open files");

    std::size_t readSize =
        readInput(std::begin(input), std::size(input), inputFile);
    if (readSize == 0) {
        return EXIT_SUCCESS;
    }
    inputEnd += readSize;
    timer.report("read input");

    toLower(input, inputEnd);
    timer.report("make input lowercase");

    std::vector<TrieNode> trie(1);
    {
        uint32_t index = 0;
        for (auto i = input; i != inputEnd; ++i) {
            if (*i != '\0') {
                uint32_t & child = trie[index].children[*i - 'a'];
                if (child == 0) {
                    child = uint32_t(trie.size());
                    index = child;
                    trie.emplace_back();
                } else {
                    index = child;
                }
            } else if (index != 0) {
                ++trie[index].count;
                index = 0;
            }
        }
    }
    fmt::print(stderr, "trie size = {}\n", trie.size());

    timer.report(fmt::format(fg(fmt::color::dark_blue), "count words"));

    std::vector<std::pair<uint32_t, uint32_t>> rank;
    std::vector<char> words;

    std::vector<char> word;
    auto traverseTrie = [&](const auto & traverseTrie,
                            const auto & children) -> void {
        size_t c = 0;
        for (uint32_t child : children) {
            if (child != 0) {
                const TrieNode & node = trie[child];
                word.push_back('a' + c);
                if (node.count != 0) {
                    rank.emplace_back(node.count, uint32_t(words.size()));
                    words.insert(std::cend(words), std::cbegin(word),
                                 std::cend(word));
                    words.push_back('\0');
                }
                traverseTrie(traverseTrie, node.children);
                word.pop_back();
            }
            ++c;
        }
    };
    traverseTrie(traverseTrie, trie.front().children);
    assert(word.empty());
    fmt::print(stderr, "word count = {}, length = {}\n", rank.size(),
               words.size());

    timer.report("recover words from trie");

    std::stable_sort(std::begin(rank), std::end(rank),
                     [](auto && l, auto && r) { return r.first < l.first; });

    timer.report(fmt::format(fg(fmt::color::dark_orange), "sort words"));

    OutputStream<> outputStream{outputFile};
    for (const auto & [count, word] : rank) {
        if (!outputStream.print(count)) {
            fmt::print(stderr, "output failure");
            return EXIT_FAILURE;
        }
        if (!outputStream.putChar(' ')) {
            fmt::print(stderr, "output failure");
            return EXIT_FAILURE;
        }
        if (!outputStream.print(std::next(words.data(), word))) {
            fmt::print(stderr, "output failure");
            return EXIT_FAILURE;
        }
        if (!outputStream.putChar('\n')) {
            fmt::print(stderr, "output failure");
            return EXIT_FAILURE;
        }
    }
    timer.report("write output");

    return EXIT_SUCCESS;
}
