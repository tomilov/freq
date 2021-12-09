#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tag_and_trait.hpp>
#include <ext/pb_ds/trie_policy.hpp>

#include <cctype>
#include <cstdlib>

using trie = __gnu_pbds::trie<
    std::string_view, std::size_t,
    __gnu_pbds::trie_string_access_traits<std::string_view, 'a', 'z'>,
    __gnu_pbds::pat_trie_tag, __gnu_pbds::null_node_update>;

int main(int argc, char * argv[])
{
    if (argc < 3) {
        return EXIT_FAILURE;
    }

    std::ifstream i(argv[1]);
    if (!i.is_open()) {
        return EXIT_FAILURE;
    }

    i.seekg(0, std::ios::end);
    auto size = size_t(i.tellg());
    i.seekg(0, std::ios::beg);

    std::string input;
    input.resize(size);
    i.read(input.data(), size);

    auto toLower = [](char c) {
        return std::tolower(std::make_unsigned_t<char>(c));
    };
    std::transform(std::cbegin(input), std::cend(input), std::begin(input),
                   toLower);

    trie t;

    auto isAlpha = [](char c) {
        return std::isalpha(std::make_unsigned_t<char>(c));
    };
    auto end = std::end(input);
    auto beg = std::find_if(std::begin(input), end, isAlpha);
    while (beg != end) {
        auto it = std::find_if(beg, end, std::not_fn(isAlpha));
        ++t[{beg, it}];
        beg = std::find_if(it, end, isAlpha);
    }

    std::vector<const decltype(t)::value_type *> output;
    output.reserve(t.size());
    for (const auto & wordCount : t) {
        output.push_back(&wordCount);
    }

    auto isLess = [](auto lhs, auto rhs) -> bool {
        return rhs->second < lhs->second;
    };
    std::stable_sort(std::begin(output), std::end(output), isLess);

    std::ofstream o(argv[2]);
    if (!o.is_open()) {
        return EXIT_FAILURE;
    }
    for (auto wordCount : output) {
        o << wordCount->second << ' ' << wordCount->first << '\n';
    }

    return EXIT_SUCCESS;
}
