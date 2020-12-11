#pragma once

#include <chrono>
#include <tuple>
#include <vector>

/// Typedefs for clock stuff, as the std names are just to long
using fsec     = std::chrono::duration<float>;
using stdclock = std::chrono::steady_clock;

using StatsVecTuple = std::tuple<std::vector<long>, std::vector<double>, std::vector<double>, std::vector<double>, std::vector<double>>;
using StatsVecOfVecTuple = std::tuple<std::vector<std::vector<long>>, std::vector<std::vector<double>>, std::vector<std::vector<double>>,
                                      std::vector<std::vector<double>>, std::vector<std::vector<double>>>;

struct Stats {
    long num_bytes{};
    fsec alloc_elapsed{};
    fsec free_elapsed{};
    fsec tbb_alloc_elapsed{};
    fsec tbb_free_elasped{};
};
