#include "util.h"

#include <tuple>

StatsVecTuple split_stats(const std::vector<Stats>& stats)
{

    std::vector<long> bytes;
    bytes.reserve(stats.size());

    std::vector<double> avg_allocs;
    avg_allocs.reserve(stats.size());

    std::vector<double> avg_frees;
    avg_frees.reserve(stats.size());

    std::vector<double> tbb_avg_allocs;
    tbb_avg_allocs.reserve(stats.size());

    std::vector<double> tbb_avg_frees;
    tbb_avg_frees.reserve(stats.size());

    for (const auto s : stats) {
        bytes.emplace_back(s.num_bytes);
        avg_allocs.emplace_back(s.alloc_elapsed.count());
        avg_frees.emplace_back(s.free_elapsed.count());
        tbb_avg_allocs.emplace_back(s.tbb_alloc_elapsed.count());
        tbb_avg_frees.emplace_back(s.tbb_free_elasped.count());
    }

    return std::make_tuple(bytes, avg_allocs, avg_frees, tbb_avg_allocs, tbb_avg_frees);
}

StatsVecOfVecTuple split_2d_stats(const std::vector<std::vector<Stats>>& stats)
{
    std::vector<std::vector<long>> bytes;
    bytes.reserve(stats.size());

    std::vector<std::vector<double>> allocs;
    allocs.reserve(stats.size());

    std::vector<std::vector<double>> frees;
    frees.reserve(stats.size());

    std::vector<std::vector<double>> tbb_allocs;
    tbb_allocs.reserve(stats.size());

    std::vector<std::vector<double>> tbb_frees;
    tbb_frees.reserve(stats.size());

    for (auto& s : stats) {
        auto [byte, alloc, free, talloc, tfree] = split_stats(s);
        bytes.emplace_back(byte);
        allocs.emplace_back(alloc);
        frees.emplace_back(free);
        tbb_allocs.emplace_back(talloc);
        tbb_frees.emplace_back(tfree);
    }
    return std::make_tuple(bytes, allocs, frees, tbb_allocs, tbb_frees);
}

