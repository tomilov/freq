#pragma once

#include "helpers.hpp"
#include "timer.hpp"

#include <fmt/color.h>
#include <fmt/format.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include <cctype>
#include <cstdint>
#include <cstdlib>

template<template<typename...> typename Map, bool kIsOrdered = false,
         bool kSetEmptyKey = false>
int basic(int argc, char * argv[])
{
    Timer timer{fmt::format(fg(fmt::color::dark_green), "total")};

    if (argc < 3) {
        return EXIT_FAILURE;
    }

    std::ifstream i(argv[1]);
    if (!i.is_open()) {
        return EXIT_FAILURE;
    }

    i.seekg(0, std::ios::end);
    auto size = i.tellg();
    i.seekg(0, std::ios::beg);

    std::string input;
    input.resize(std::size_t(size));
    i.read(input.data(), std::streamsize(size));

    timer.report("read input");

    auto toLowerChar = [](char c) {
        return char(std::tolower(std::make_unsigned_t<char>(c)));
    };
    std::transform(std::cbegin(input), std::cend(input), std::begin(input),
                   toLowerChar);

    timer.report("make input lowercase");

    Map<std::string_view, uint32_t> wordCounts;
    if constexpr (kSetEmptyKey) {
        using namespace std::string_view_literals;
        wordCounts.set_empty_key(""sv);
    }

    auto isAlpha = [](char c) {
        return bool(std::isalpha(std::make_unsigned_t<char>(c)));
    };
    auto end = std::next(input.data(), input.size());
    auto beg = std::find_if(input.data(), end, isAlpha);
    while (beg != end) {
        auto it = std::find_if_not(beg, end, isAlpha);
        ++wordCounts[{beg, std::size_t(std::distance(beg, it))}];
        beg = std::find_if(it, end, isAlpha);
    }

    timer.report(fmt::format(fg(fmt::color::dark_blue), "count words"));

    std::vector<const typename decltype(wordCounts)::value_type *> output;
    output.reserve(wordCounts.size());
    for (const auto & wordCount : wordCounts) {
        output.push_back(&wordCount);
    }

    if (kIsOrdered) {
        auto isLess = [](auto lhs, auto rhs) -> bool {
            return rhs->second < lhs->second;
        };
        std::stable_sort(std::begin(output), std::end(output), isLess);
    } else {
        auto isLess = [](auto lhs, auto rhs) -> bool {
            return std::tie(rhs->second, lhs->first) <
                   std::tie(lhs->second, rhs->first);
        };
        std::sort(std::begin(output), std::end(output), isLess);
    }
    timer.report(fmt::format(fg(fmt::color::dark_orange), "sort words"));

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
