#include <cstddef>
#include <chrono>
#include <algorithm>
#include <utility>
#include <random>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <string_view>
#include <assert.h>
#include <cstdio>

#include <fmt/format.h>
#include <fmt/color.h>
#include <fmt/ranges.h>

#include "tbb/scalable_allocator.h"

#include "cxxopts.hpp"

// Some includes to just clean this file up a bit
#include "print.h"
#include "options.h"
#include "types.h"
#include "util.h"

template <typename Malloc, typename Free>
auto basic_alloc_free_impl(long ipow, Malloc malloc, Free free) -> std::pair<fsec, fsec>
{
    long N = std::pow(2, ipow);

    fsec alloc_time{};
    fsec free_time{};

    for (int i = 0; i < repeat; ++i) {
        auto       alloc_start = stdclock::now();
        std::byte* buf         = static_cast<std::byte*>(malloc(N * sizeof(std::byte)));
        auto       alloc_end   = stdclock::now();

        alloc_time += (alloc_end - alloc_start);

        // Here we just tell the compiler, we used it somehow and therefore
        // can't assume anything.  This actually matters! Without it, the
        // result times are pretty much constant and most
        // likely garbage
        escape(buf);

        auto free_start = stdclock::now();
        free(buf);
        auto free_end = stdclock::now();

        free_time += (free_end - free_start);
    }

    return {alloc_time, free_time};
}

template <typename Malloc, typename Free>
auto alloc_permuted_free_impl(long ipow, Malloc malloc, Free free) -> std::pair<fsec, fsec>
{
    long N = std::pow(2, ipow);

    std::vector<std::byte*> buffers;
    buffers.reserve(repeat);

    auto alloc_start = stdclock::now();
    for (int i = 0; i < repeat; ++i) {
        auto buf = static_cast<std::byte*>(malloc(N));
        buffers.push_back(std::move(buf));
    }
    auto alloc_end     = stdclock::now();
    fsec alloc_elapsed = (alloc_end - alloc_start);

    // Create random permutation, with default seed, else create std::random_device and use it as
    // seed
    std::shuffle(std::begin(buffers), std::end(buffers), std::mt19937{});

    auto free_start = stdclock::now();
    for (auto buf : buffers) {
        free(buf);
    }
    auto free_end     = stdclock::now();
    fsec free_elapsed = (free_end - free_start);

    return {alloc_elapsed, free_elapsed};
}

template <typename Malloc, typename Free>
auto random_alloc_permuted_free_impl(long ipow, Malloc malloc, Free free) -> std::pair<fsec, fsec>
{
    long N  = std::pow(2, ipow);
    long lb = std::pow(2, ipow - 1);
    long ub = std::pow(2, ipow + 1);

    std::vector<std::byte*> buffers;
    buffers.reserve(repeat);

    fsec alloc_elapsed = fsec{};

    for (int i = 0; i < repeat; ++i) {
        // Create random number
        std::random_device                  rd;
        std::mt19937                        gen(rd());
        std::uniform_int_distribution<long> distrib(lb, ub);
        auto                                N = distrib(gen);

        // Measure time of allocation
        auto alloc_start = stdclock::now();
        auto buf         = static_cast<std::byte*>(malloc(N));
        auto alloc_end   = stdclock::now();
        alloc_elapsed    = (alloc_end - alloc_start);

        escape(buf);

        buffers.push_back(std::move(buf));
    }

    // Create random permutation, with default seed, else create std::random_device and use it as
    // seed
    std::shuffle(std::begin(buffers), std::end(buffers), std::mt19937{});

    auto free_start = stdclock::now();
    for (auto buf : buffers) {
        escape(buf);
        free(buf);
    }
    auto free_end     = stdclock::now();
    fsec free_elapsed = (free_end - free_start);

    return {alloc_elapsed, free_elapsed};
}

template <typename Malloc, typename Free>
auto random_alloc_random_permuted_free_impl(long ipow, Malloc malloc, Free free) -> std::pair<fsec, fsec>
{
    std::random_device rd;
    std::mt19937       alloc_gen(rd());
    std::mt19937       free_gen(rd());

    long N  = std::pow(2, ipow);
    long lb = std::pow(2, ipow - 1);
    long ub = std::pow(2, ipow + 1);

    std::vector<std::byte*> buffers;
    buffers.reserve(repeat);

    fsec alloc_elapsed = fsec{};
    fsec free_elapsed  = fsec{};

    for (int i = 0; i < repeat; ++i) {
        // Create random number of how many new allocations should be performed
        std::uniform_int_distribution<> alloc_dist(min_num_random_allocs, max_num_random_allocs);
        auto                            num_allocs = alloc_dist(alloc_gen);

        // Grow buffers by it's current size + num_allocs
        buffers.reserve(buffers.size() + num_allocs);
        for (int j = 0; j < num_allocs; j++) {
            // Create number of new
            std::mt19937                        gen(rd());
            std::uniform_int_distribution<long> distrib(lb, ub);

            // Size of new allocation
            auto N = distrib(gen);

            // Measure time of allocation
            auto alloc_start = stdclock::now();
            auto buf         = static_cast<std::byte*>(malloc(N));
            auto alloc_end   = stdclock::now();
            alloc_elapsed += (alloc_end - alloc_start);

            // Escpae it, an tell the optimizer to not optimize it away
            escape(buf);

            buffers.push_back(std::move(buf));
        }

        // Create random permutation, with default seed, else create std::random_device and use it
        // as seed
        std::shuffle(std::begin(buffers), std::end(buffers), std::mt19937{});

        // Randomly choose the number of frees we're performing this round
        std::uniform_int_distribution<> free_dist(0, buffers.size());
        auto                            num_frees = free_dist(free_gen);

        for (int j = 0; j < num_frees; j++) {
            // Measure time of free
            auto free_start = stdclock::now();
            free(buffers[j]);
            auto free_end = stdclock::now();
            free_elapsed += (free_end - free_start);
        }

        // Shorten the vector by num_frees
        std::vector<decltype(buffers)::value_type>(buffers.begin() + num_frees, buffers.end()).swap(buffers);
    }

    // Clean up, free all remaining chunks
    for (auto buf : buffers) {
        auto free_start = stdclock::now();
        free(buf);
        auto free_end = stdclock::now();
        free_elapsed += (free_end - free_start);
    }

    return {alloc_elapsed, free_elapsed};
}


/// Perform linearly growing allocations and directly free the allocated memory afterwards
auto linear_growth_alloc()
{
    print_header(print_total_time);

    std::vector<Stats> statistics;
    statistics.reserve(max_size_power);

    // Perform allocations from 1 byte to 1 GB
    for (long n = 1; n <= max_size_power; ++n) {
        long N                                     = std::pow(2, n);
        auto [tbb_alloc_elapsed, tbb_free_elapsed] = basic_alloc_free_impl(n, scalable_malloc, scalable_free);
        auto [alloc_elapsed, free_elapsed]         = basic_alloc_free_impl(n, std::malloc, std::free);

        print_round(N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed, print_round_time, print_total_time);

        // Log this to create nice copyable and easyly plotable stuff
        Stats stats = {N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed};
        statistics.emplace_back(stats);
    }

    return statistics;
}

/// Perform linearly growing allocations, but first perform many allocations and then free them
/// afterwards in a permuted (i.e not the order of allocation)
auto linear_growth_permuted_free()
{
    print_header(print_total_time);

    std::vector<Stats> statistics;
    statistics.reserve(max_size_power);

    for (long n = 1; n <= max_size_power; ++n) {
        long N                                     = std::pow(2, n);
        auto [alloc_elapsed, free_elapsed]         = alloc_permuted_free_impl(n, std::malloc, std::free);
        auto [tbb_alloc_elapsed, tbb_free_elapsed] = alloc_permuted_free_impl(n, scalable_malloc, scalable_free);

        print_round(N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed, print_round_time, print_total_time);

        // Log this to create nice copyable and easyly plotable stuff
        Stats stats = {N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed};
        statistics.emplace_back(stats);
    }

    return statistics;
}

/// Allocate random sizes which varie to the next lower power of 2 and the next larger power of
/// two So for a given N = 2^n, allocations are in [2^(n-1), 2^(n+1)[ Then again allocate
/// everything and randomly free them
auto random_alloc_permuted_free()
{
    print_header(print_total_time);

    std::vector<Stats> statistics;
    statistics.reserve(max_size_power);

    // Iterate over 2^1 to 2^20
    for (long n = 1; n <= max_size_power; ++n) {
        // Calculate lower and upper bound
        long N = std::pow(2, n);

        auto [alloc_elapsed, free_elapsed]         = random_alloc_permuted_free_impl(n, std::malloc, std::free);
        auto [tbb_alloc_elapsed, tbb_free_elapsed] = random_alloc_permuted_free_impl(n, scalable_malloc, scalable_free);

        print_round(N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed, print_round_time, print_total_time);

        // Log this to create nice copyable and easyly plotable stuff
        Stats stats = {N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed};
        statistics.emplace_back(stats);
    }

    return statistics;
}

/// Similar to random_alloc_permuted_free(), but now don't free everything, free a random number
/// of buffers and then start allocating random sizes again and do it over and over again.
/// This should mimic a programm with many different allocations and common frees
auto random_alloc_random_permuted_free()
{
    print_header(print_total_time);

    std::vector<Stats> statistics;
    statistics.reserve(max_size_power);

    // Iterate over 2^1 to 2^20
    for (long n = 1; n <= max_size_power; ++n) {
        // Calculate lower and upper bound
        long N = std::pow(2, n);

        auto [alloc_elapsed, free_elapsed]         = random_alloc_random_permuted_free_impl(n, std::malloc, std::free);
        auto [tbb_alloc_elapsed, tbb_free_elapsed] = random_alloc_random_permuted_free_impl(n, scalable_malloc, scalable_free);

        print_round(N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed, print_round_time, print_total_time);

        // Log this to create nice copyable and easyly plotable stuff
        Stats stats = {N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed};
        statistics.emplace_back(stats);
    }

    return statistics;
}

using Callback = std::pair<fsec, fsec> (*)(long, void* (*) (std::size_t), void free(void*));

auto threaded_alloc(int num_threads, Callback func)
{
    print_header(print_total_time);

    std::vector<Stats> statistics;
    statistics.reserve(max_size_power);

    // for (long N = 1; N <= gigabyte; N *= 2) {
    for (long n = 1; n <= max_size_power; ++n) {
        std::vector<std::thread>           threads;
        std::vector<std::pair<fsec, fsec>> times(num_threads);
        std::mutex                         mtx;

        long N  = std::pow(2, n);
        auto lb = std::pow(2, n - 1);
        auto ub = std::pow(2, n + 1);

        for (int i = 0; i < num_threads; ++i) {
            // Create a thread
            threads.emplace_back([&func, &mtx, &times, i, thread_id = i, n]() {
                // Save pair into tmp
                // auto tmp = alloc_permuted_free(N, std::malloc, std::free);
                auto tmp = func(n, std::malloc, std::free);

                // Lock and save pair into vector, just to be save
                std::scoped_lock _(mtx);
                times[thread_id] = tmp;
            });
        }

        // Join all threads
        for (auto&& t : threads) {
            t.join();
        }

        // Sum time for the first run
        fsec alloc_elapsed{};
        fsec free_elapsed{};
        for (auto [alloc_time, free_time] : times) {
            alloc_elapsed += alloc_time;
            free_elapsed += free_time;
        }

        // Divide by the number of threads
        alloc_elapsed /= num_threads;
        free_elapsed /= num_threads;

        // Clear threads and times
        threads.clear();
        times.clear();
        threads.reserve(num_threads);
        times.resize(num_threads);

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&func, &mtx, &times, i, thread_id = i, n]() {
                // Save pair into tmp
                // auto tmp = alloc_permuted_free(N, scalable_malloc, scalable_free);
                auto tmp = func(n, scalable_malloc, scalable_free);

                // Lock and save pair into vector, just to be save
                std::scoped_lock _(mtx);
                times[thread_id] = tmp;
            });
        }

        for (auto&& t : threads) {
            t.join();
        }

        // Sum time for the first run
        fsec tbb_alloc_elapsed{};
        fsec tbb_free_elapsed{};
        for (auto [alloc_time, free_time] : times) {
            tbb_alloc_elapsed += alloc_time;
            tbb_free_elapsed += free_time;
        }

        // Divide by the number of threads
        tbb_alloc_elapsed /= num_threads;
        tbb_free_elapsed /= num_threads;

        print_round(N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed, print_round_time, print_total_time);

        // Log this to create nice copyable and easyly plotable stuff
        Stats stats = {N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed};
        statistics.emplace_back(stats);
    }

    return statistics;
}

void loop_body() {}

int main(int argc, char** argv)
{
    cxxopts::Options options(argv[0], "Test defaul std::malloc vs TBB scalable malloc");

    options.add_options()("h,help", "Display Help message", cxxopts::value<bool>());
    options.add_options()("v,verbose", "Verbose output (v: Round time output, vv: Round time with total time difference)",
                          cxxopts::value<bool>());
    options.add_options()("q,quiet", "Don't print output each round, but only numpy output", cxxopts::value<bool>());
    options.add_options()("r,report", "Report numpy arrays to further use in plotting", cxxopts::value<bool>());

    options.add_options()("n,num-threads", "Number of threads", cxxopts::value<int>()->default_value("4"));

    options.add_options()("m,min-allocs", "Minimum number of allocs done for random alloc tests",
                          cxxopts::value<int>()->default_value("200"));
    options.add_options()("M,max-allocs", "Maximum number of allocs done for random alloc tests",
                          cxxopts::value<int>()->default_value("500"));

    options.add_options()("a,all", "Run all tests", cxxopts::value<bool>()->default_value("true"));
    options.add_options()("lin-growth-direct-free", "Linearly growing chunks, direct free", cxxopts::value<bool>());
    options.add_options()("lin-growth-permuted-free", "Linearly growing chunks, delayed permuted free", cxxopts::value<bool>());
    options.add_options()("random-alloc-permuted-free", "random sized chunks (in ranges), delayed permuted free", cxxopts::value<bool>());
    options.add_options()("random-alloc-random-free", "random sized chunks (in ranges), random delayed permuted free",
                          cxxopts::value<bool>());
    options.add_options()("threaded", "Run the specified tests threaded", cxxopts::value<bool>());
    options.add_options()("scaling", "Run all tests from 1 to num-threads", cxxopts::value<bool>());

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        fmt::print("{}", options.help());
        exit(0);
    }

    if (result.count("quiet") && result.count("verbose")) {
        fmt::print("Specified both '-q/--quiet' and '-v/--verbose'\n");
    }

    if (result.count("quiet") >= 1) {
        print_round_time = false;
    }

    if (result.count("quiet") >= 2) {
        print_round_time = false;
    }

    if (result.count("verbose") >= 1) {
        print_round_time = true;
    }

    if (result.count("verbose") >= 2) {
        print_total_time = true;
    }
    std::FILE* file_handle = nullptr; 
    if (result["report"].as<bool>()) {
        print_statistics = true;
        file_handle      = std::fopen("report.py", "w");
    } else {
        print_statistics = false;
    }

    // Set some globals
    min_num_random_allocs = result["min-allocs"].as<int>();
    max_num_random_allocs = result["max-allocs"].as<int>();

    const auto threaded = result.count("threaded");

    // Get the number of threads
    const auto num_threads = threaded ? result["num-threads"].as<int>() : 1;

    if (threaded) {
        fmt::print("Running tests using {} threads\n", num_threads);
    }

    const bool run_all = result["all"].as<bool>()
                         && !(result["lin-growth-direct-free"].as<bool>() || result["lin-growth-permuted-free"].as<bool>()
                              || result["random-alloc-permuted-free"].as<bool>() || result["random-alloc-random-free"].as<bool>());

    const bool run_scaling = result["scaling"].as<bool>();

    if (run_all) {
        fmt::print("Running all tests");
    }

    if (run_scaling) {
        fmt::print("Running scaling test from {} to {} threads\n", 1, num_threads);
    }

    std::vector<int> range_threads(num_threads);
    std::iota(range_threads.begin(), range_threads.end(), 1);

    // threaded_linear_growth_alloc(3);
    const bool verbose = result["verbose"].as<bool>();

    if (result["lin-growth-direct-free"].as<bool>() || run_all) {

        fmt::print("{:=^50}\n", "");
        fmt::print("Allocate a fixed size and release it directly. Allocations grow exponentionally\n\n");
        fmt::print("Rather syntethic benchmark. No real(TM) application just allocates");
        fmt::print("sizes in power of 2 and then releasaed them right away.\n\n");
        fmt::print("{:=^50}\n\n", "");
        if (threaded) {
            if (run_scaling) {

                std::vector<std::vector<Stats>> stats;
                stats.reserve(num_threads);

                for (int nthreads = 1; nthreads <= num_threads; ++nthreads) {
                    auto tmp_stats = threaded_alloc(num_threads, basic_alloc_free_impl);
                    stats.emplace_back(std::move(tmp_stats));
                }

                print_stats(file_handle, range_threads, stats);
            } else {
                auto stats = threaded_alloc(num_threads, basic_alloc_free_impl);
                print_stats(file_handle, stats);
            }
        } else {
            auto stats = linear_growth_alloc();
            print_stats(file_handle, stats);
        }
    }

    if (result["lin-growth-permuted-free"].as<bool>() || run_all) {
        fmt::print("\n\n{:=^50}\n", "");
        fmt::print("Allocate {} fixed size chunks, randomly shuffle them, ", repeat);
        fmt::print(" and free them in the new order\n\n");

        fmt::print("Closer to real behaviour, mimicks very easy programs, that just ");
        fmt::print("allocate a bunch at the beginning and then free it at the end\n\n");
        fmt::print("{:=^50}\n\n", "");

        if (threaded) {
            if (run_scaling) {
                std::vector<std::vector<Stats>> stats;
                stats.reserve(num_threads);

                for (int nthreads = 1; nthreads <= num_threads; ++nthreads) {
                    auto tmp_stats = threaded_alloc(num_threads, alloc_permuted_free_impl);
                    stats.emplace_back(std::move(tmp_stats));
                }
                print_stats(file_handle, range_threads, stats);
            } else {
                auto stats = threaded_alloc(num_threads, alloc_permuted_free_impl);
                print_stats(file_handle, stats);
            }
        } else {
            auto stats = linear_growth_permuted_free();
            print_stats(file_handle, stats);
        }
    }

    if (result["random-alloc-permuted-free"].as<bool>() || run_all) {
        fmt::print("\n\n{:=^50}\n", "");
        fmt::print("Allocate {} random size chunks, randomly shuffle them, ", repeat);
        fmt::print(" and free them in the new order\n\n");

        fmt::print("Closer to real(TM) behaviour, mimicks very easy programs, that just ");
        fmt::print("allocate a bunch at the beginning and then free it at the end\n\n");
        fmt::print("{:=^50}\n\n", "");

        if (threaded) {
            if (run_scaling) {
                std::vector<std::vector<Stats>> stats;
                stats.reserve(num_threads);

                for (int nthreads = 1; nthreads <= num_threads; ++nthreads) {
                    auto tmp_stats = threaded_alloc(num_threads, random_alloc_permuted_free_impl);
                    stats.emplace_back(std::move(tmp_stats));
                }
                print_stats(file_handle, range_threads, stats);
            } else {
                auto stats = threaded_alloc(num_threads, random_alloc_permuted_free_impl);
                print_stats(file_handle, stats);
            }
        } else {
            auto stats = random_alloc_permuted_free();
            print_stats(file_handle, stats);
        }
    }

    if (result["random-alloc-random-free"].as<bool>() || run_all) {
        fmt::print("\n\n{:=^50}\n", "");
        fmt::print("Allocate a random number of randomly sized chunks (in range [{}, {}[)", min_num_random_allocs, max_num_random_allocs);
        fmt::print(", free a random number and then repeat.\n\n");

        fmt::print("Closest so far to real(TM) behaviour, mimicks more complex programs, that ");
        fmt::print("allocate a bunch and then free a part of it and then allocate again and so on\n\n");
        fmt::print("{:=^50}\n\n", "");

        if (threaded) {
            if (run_scaling) {

                std::vector<std::vector<Stats>> stats;
                stats.reserve(num_threads);

                for (int nthreads = 1; nthreads <= num_threads; ++nthreads) {
                    auto tmp_stats = threaded_alloc(num_threads, random_alloc_random_permuted_free_impl);
                    stats.emplace_back(std::move(tmp_stats));
                }

                print_stats(file_handle, range_threads, stats);

            } else {
                auto stats = threaded_alloc(num_threads, random_alloc_random_permuted_free_impl);
                print_stats(file_handle, stats);
            }
        } else {
            auto stats = random_alloc_random_permuted_free();
            print_stats(file_handle, stats);
        }
    }
    if (result["report"].as<bool>() && file_handle) {
        std::fclose(file_handle);
    }

    return 0;
}

