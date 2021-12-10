#include "helpers.hpp"
#include "io.hpp"
#include "timer.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <cctype>
#include <cstdlib>

int main(int argc, char * argv[])
{
    Timer timer{GREEN("total")};

    if (argc < 3) {
        return EXIT_FAILURE;
    }

    std::ifstream i(argv[1]);
    if (!i.is_open()) {
        return EXIT_FAILURE;
    }

    i.seekg(0, std::ios::end);
    auto size = std::size_t(i.tellg());
    i.seekg(0, std::ios::beg);

    std::string input;
    input.resize(size);
    i.read(input.data(), size);

    timer.report("read input");

    auto toLowerChar = [](char c) {
        return char(std::tolower(std::make_unsigned_t<char>(c)));
    };
    std::transform(std::cbegin(input), std::cend(input), std::begin(input),
                   toLowerChar);

    timer.report("make input lowercase");

    std::unordered_map<std::string_view, std::size_t> wordCounts;

    auto isAlpha = [](char c) {
        return std::isalpha(std::make_unsigned_t<char>(c));
    };
    auto end = std::end(input);
    auto beg = std::find_if(std::begin(input), end, isAlpha);
    while (beg != end) {
        auto it = std::find_if(beg, end, std::not_fn(isAlpha));
        ++wordCounts[std::string_view(&*beg, std::distance(beg, it))];
        beg = std::find_if(it, end, isAlpha);
    }

    timer.report(BLUE("count words"));

    std::vector<const decltype(wordCounts)::value_type *> output;
    output.reserve(wordCounts.size());
    for (const auto & wordCount : wordCounts) {
        output.push_back(&wordCount);
    }

    auto isLess = [](auto lhs, auto rhs) -> bool {
        return std::tie(rhs->second, lhs->first) <
               std::tie(lhs->second, rhs->first);
    };
    std::sort(std::begin(output), std::end(output), isLess);

    timer.report(YELLOW("sort words"));

    std::ofstream o(argv[2]);
    if (!o.is_open()) {
        return EXIT_FAILURE;
    }
    for (auto wordCount : output) {
        o << wordCount->second << ' ' << wordCount->first << '\n';
    }

    timer.report("write output");

    return EXIT_SUCCESS;
}
