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

alignas(__m128i) char input[1 << 29];
auto inputEnd = input;

struct TrieNode
{
    uint32_t count = 0;
    uint32_t children['z' - 'a' + 1] = {};
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

    std::unique_ptr<std::FILE, decltype((std::fclose))> inputFile{
        (argv[1] == "-"sv) ? stdin : std::fopen(argv[1], "rb"), std::fclose};
    if (!inputFile) {
        fmt::print(stderr, "failed to open '{}' file to read\n", argv[1]);
        return EXIT_FAILURE;
    }

    std::unique_ptr<std::FILE, decltype((std::fclose))> outputFile{
        (argv[2] == "-"sv) ? stdout : std::fopen(argv[2], "wb"), std::fclose};
    if (!outputFile) {
        fmt::print(stderr, "failed to open '{}' file to write\n", argv[2]);
        return EXIT_FAILURE;
    }

    timer.report("open files");

    std::size_t readSize =
        readInput(std::begin(input), std::size(input), inputFile.get());
    if (readSize == 0) {
        return EXIT_SUCCESS;
    }
    inputEnd += readSize;
    timer.report("read input");

    toLower(input, inputEnd);
    timer.report("make input lowercase");

    std::vector<TrieNode> trie(1);
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
    fmt::print(stderr, "trie size = {}\n", trie.size());

    timer.report(fmt::format(fg(fmt::color::dark_blue), "count words"));

    std::vector<std::pair<uint32_t, uint32_t>> rank;
    std::vector<char> words;

    size_t leafs = 0;
    std::vector<char> word;
    auto traverseTrie = [&](const auto & traverseTrie,
                            const auto & children) -> void {
        int c = 0;
        for (uint32_t index : children) {
            bool is_leaf = true;
            if (index != 0) {
                is_leaf = false;
                const TrieNode & node = trie[index];
                word.push_back('a' + c);
                if (node.count != 0) {
                    rank.emplace_back(node.count, uint32_t(words.size()));
                    words.insert(words.cend(), std::cbegin(word),
                                 std::cend(word));
                    words.push_back('\0');
                }
                traverseTrie(traverseTrie, node.children);
                word.pop_back();
            }
            if (is_leaf) {
                ++leafs;
            }
            ++c;
        }
    };
    traverseTrie(traverseTrie, trie.front().children);
    assert(word.empty());
    fmt::print(stderr, "word count = {}, length = {}\n", rank.size(),
               words.size());
    fmt::print(stderr, "LEAFS {}\n", leafs);

    timer.report("recover words from trie");

    std::stable_sort(std::begin(rank), std::end(rank),
                     [](auto && l, auto && r) { return r.first < l.first; });

    timer.report(fmt::format(fg(fmt::color::dark_orange), "sort words"));

    OutputStream<> outputStream{outputFile.get()};
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
