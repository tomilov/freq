#pragma once

#include <chrono>
#include <utility>

struct Timer
{
    using high_resolution_clock = std::chrono::high_resolution_clock;
    using time_point = high_resolution_clock::time_point;

    const time_point start = high_resolution_clock::now();
    time_point timePoint = start;

    double dt(bool absolute = false)
    {
        using std::chrono::duration_cast;
        using std::chrono::nanoseconds;
        auto stop = high_resolution_clock::now();
        return duration_cast<nanoseconds>(
                   stop - (absolute ? start : std::exchange(timePoint, stop)))
                   .count() *
               1E-9;
    }

    void report(const char * description, bool absolute = false)
    {
        fprintf(stderr, "time (%s) = %.3lf\n", description, dt(absolute));
    }

    ~Timer()
    {
        report("total", true);
    }
};
