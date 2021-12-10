#pragma once

#include <chrono>
#include <utility>

#include <cstdio>

struct Timer
{
    const char * onScopeExit = "total";
    const std::chrono::high_resolution_clock::time_point start =
        std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point timePoint = start;

    double dt(bool absolute = false)
    {
        auto stop = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   stop - (absolute ? start : std::exchange(timePoint, stop)))
                   .count() *
               1E-9;
    }

    void report(const char * description, bool absolute = false)
    {
        std::fprintf(stderr, "time (%s) = %.3lf\n", description, dt(absolute));
    }

    ~Timer()
    {
        report(onScopeExit, true);
    }
};
