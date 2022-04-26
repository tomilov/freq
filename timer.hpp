#pragma once

#include <fmt/format.h>

#include <chrono>
#include <string>
#include <string_view>
#include <utility>

#include <cstdio>

struct Timer
{
    std::string onScopeExit = "total";
    const std::chrono::high_resolution_clock::time_point start =
        std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point timePoint = start;

    auto dt(bool absolute = false)
    {
        auto stop = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   stop - (absolute ? start : std::exchange(timePoint, stop)))
                   .count() *
               1E-9;
    }

    void report(std::string_view description, bool absolute = false)
    {
        fmt::print(stderr, "time ({}) = {:.3}\n", description, dt(absolute));
    }

    ~Timer()
    {
        report(onScopeExit, true);
    }
};
