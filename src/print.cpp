#include "print.h"
#include "options.h"
#include "util.h"

#include <cassert>

#include <fmt/format.h>
#include <fmt/color.h>
#include <fmt/ranges.h>

void print_header(bool print_total_time)
{
    if (print_total_time) {
        fmt::print("|{:-^12}|{:-^44}|{:-^44}|{:-^35}|\n", "", "libstdc++ malloc", "TBB malloc",
                   "Difference");
        fmt::print(
            "|{:^12}| {:^12} | {:^12} | {:^12} | {:^12} | {:^12} | {:^12} || {:^9} | {:^9} | "
            "{:^9} |\n",
            "Bytes", "Total Time", "Alloc Time", "Free Time", "Total Time", "Alloc Time",
            "Free Time", "Total", "Alloc", "Free");
    } else {
        fmt::print("|{:-^12}||{:-^29}||{:-^29}||{:-^23}||\n", "", "libstdc++ malloc", "TBB malloc",
                   "Difference");
        fmt::print("|{:^12}|| {:^12} | {:^12} || {:^12} | {:^12} || {:^9} | {:^9} ||\n", "Bytes",
                   "Alloc Time", "Free Time", "Alloc Time", "Free Time", "Alloc", "Free");
    }
}

void print_round(long N, fsec alloc_elapsed, fsec free_elapsed, fsec tbb_alloc_elapsed,
                 fsec tbb_free_elapsed, bool print_round_time, bool print_total_time)
{
    float avg_alloc = alloc_elapsed.count() / repeat;
    float avg_free  = free_elapsed.count() / repeat;
    float avg_total = avg_alloc + avg_free;

    float tbb_avg_alloc = tbb_alloc_elapsed.count() / repeat;
    float tbb_avg_free  = tbb_free_elapsed.count() / repeat;
    float tbb_avg_total = tbb_avg_alloc + tbb_avg_free;

    float diff_alloc = ((avg_alloc / tbb_avg_alloc) - 1) * 100;
    float diff_free  = ((avg_free / tbb_avg_free) - 1) * 100;
    float diff_total = ((avg_total / tbb_avg_total) - 1) * 100;

    if (print_total_time) {
        fmt::print("| {:>10} || {:>12.10f} | {:>12.10f} | {:>12.10f} || {:>12.10f} | {:>12.10f} | "
                   "{:>12.10f} |",
                   N, avg_total, avg_alloc, avg_free, tbb_avg_total, tbb_avg_alloc, tbb_avg_free);
    } else {
        fmt::print("| {:>10} || {:>12.10f} | {:>12.10f} || {:>12.10f} | "
                   "{:>12.10f} ||",
                   N, avg_alloc, avg_free, tbb_avg_alloc, tbb_avg_free);
    }

    print_difference(diff_total, diff_alloc, diff_free, print_total_time);
    if (print_round_time) {
        fmt::print("\n");
    } else {
        fmt::print("\r");
    }
}

void print_difference(float diff_total, float diff_alloc, float diff_free, bool print_total_time)
{
    auto diff_total_color = [&] {
        if (diff_total < 0.0) {
            return fmt::color::red;
        }
        return fmt::color::green;
    }();

    if (print_total_time) {
        fmt::print(fmt::fg(diff_total_color), " {:>+8.2f}% ", diff_total);
        fmt::print("|");
    }

    auto diff_alloc_color = [&] {
        if (diff_alloc < 0.0) {
            return fmt::color::red;
        }
        return fmt::color::green;
    }();

    fmt::print(fmt::fg(diff_alloc_color), " {:>+8.2f}% ", diff_alloc);
    fmt::print("|");

    auto diff_free_color = [&] {
        if (diff_free < 0.0) {
            return fmt::color::red;
        }
        return fmt::color::green;
    }();

    fmt::print(fmt::fg(diff_free_color), " {:>+8.2f}% ", diff_free);
    fmt::print("||");
}

void print_stats(std::FILE* handle, const std::vector<Stats>& statistics)
{
    if (!print_statistics)
        return;

    auto [bytes, avg_allocs, avg_frees, tbb_avg_allocs, tbb_avg_frees] = split_stats(statistics);

    print_numpy(handle, "bytes", bytes);
    print_numpy(handle, "allocs", avg_allocs);
    print_numpy(handle, "frees", avg_frees);
    print_numpy(handle, "tbb_allocs", tbb_avg_allocs);
    print_numpy(handle, "tbb_frees", tbb_avg_frees);
}

void print_stats(std::FILE* handle, const std::vector<int> threads, const std::vector<std::vector<Stats>>& statistics)
{
    fmt::print("Threads size {}, stats size {}\n", threads.size(), statistics.size());
    assert(threads.size() == statistics.size());

    auto [bytes, allocs, frees, tbballocs, tbbfrees] = split_2d_stats(statistics);

    print_numpy(handle, "threads", threads);
    print_numpy(handle, "bytes", bytes);
    print_numpy(handle, "allocs", allocs);
    print_numpy(handle, "frees", frees);
    print_numpy(handle, "tbballocs", tbballocs);
    print_numpy(handle, "tbbfrees", tbbfrees);
}
