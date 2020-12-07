#include <cstddef>
#include <chrono>
#include <algorithm>
#include <utility>
#include <random>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>

#include <fmt/format.h>
#include <fmt/color.h>
#include <fmt/ranges.h>

#include "tbb/scalable_allocator.h"

#include "cxxopts.hpp"

/// Max power for all loops this is equal to rougthly 4 GB
static constexpr long max_size_power = 33;

/// Size of a kilobyte
static constexpr long kilobyte = 1024;

/// Size of a megabyte
static constexpr long megabyte = 1024 * 1024;

/// Size of  gigabyte
static constexpr long gigabyte = 1024 * 1024 * 1024;

///
/// options which can be set via command line
///
 
/// Varialbe if you want to print the total time
static bool print_total_time = false;
 
/// Number of times to repeat an allocation test
static long repeat = 100;

/// Variable if statistic should be printed
static bool print_statistics = false;

using fsec     = std::chrono::duration<float>;
using stdclock = std::chrono::steady_clock;

/// Taken fron Chandler Carruth's CppCon 2015 talk "Tuning C++: Benchmarks, and CPUs, and Compilers!
/// Oh My!" This function is magic! It takes any pointer, and basically turns of the optimizer for
/// it
static void escape(void* p)
{
    // This inline assemlby tells the compiler, it has some unknowable but observable sideeffect.
    // And therefore, the compiler is not allowed to look further into it, we know better please
    // compiler don't do anything with it. The inline assembly code, takes p as an input, and we say
    // potentially we touched and motified all the memory of the system. It dosen't to anything,
    // therefore dosen't create any actuall code, but the optimizer can't see through this
    asm volatile("" : : "g"(p) : "memory");
}

/// Taken fron Chandler Carruth's CppCon 2015 talk "Tuning C++: Benchmarks, and CPUs, and Compilers!
/// Oh My!" This function is magic! It mimicks a write a global write, all the memory could have
/// been written.
static void clobber()
{
    asm volatile("" : : : "memory");
}

// Just some print functions, which make everything a little bit cleaner
void print_header();
void print_difference(float diff_total, float diff_alloc, float diff_free);
void print_round(long N, fsec alloc_elapsed, fsec free_elapsed, fsec tbb_alloc_elapsed,
                 fsec tbb_free_elapsed);

template <typename Malloc, typename Free>
auto basic_alloc_free(long N, Malloc malloc, Free free) -> std::pair<fsec, fsec>
{
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
auto alloc_permuted_free(long N, Malloc malloc, Free free) -> std::pair<fsec, fsec>
{
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
auto random_alloc_permuted_free(long lb, long ub, Malloc malloc, Free free) -> std::pair<fsec, fsec>
{
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
auto random_alloc_random_permuted_free(long lb, long ub, Malloc malloc, Free free)
    -> std::pair<fsec, fsec>
{
    std::random_device rd;
    std::mt19937       alloc_gen(rd());
    std::mt19937       free_gen(rd());

    std::vector<std::byte*> buffers;
    buffers.reserve(repeat);

    fsec alloc_elapsed = fsec{};
    fsec free_elapsed  = fsec{};

    for (int i = 0; i < repeat; ++i) {
        // Create random number of how many new allocations should be performed
        std::uniform_int_distribution<> alloc_dist(200, 500);
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
        std::vector<decltype(buffers)::value_type>(buffers.begin() + num_frees, buffers.end())
            .swap(buffers);
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

template <typename T>
void print_numpy(const std::vector<T>& v)
{
    if (v.empty())
        return;

    fmt::print("[");
    fmt::print("{}", v[0]);

    if (v.size() == 1) {
        fmt::print("]");
        return;
    }

    for (int i = 1; i < v.size(); ++i) {
        fmt::print(",{}", v[i]);
    }
    fmt::print("]");
}

struct Stats {
    long num_bytes{};
    fsec alloc_elapsed{};
    fsec free_elapsed{};
    fsec tbb_alloc_elapsed{};
    fsec tbb_free_elasped{};
};

auto print_stats(const std::vector<Stats>& statistics)
{
    if (!print_statistics)
        return;

    std::vector<long> bytes;
    bytes.reserve(statistics.size());

    std::vector<double> avg_allocs;
    avg_allocs.reserve(statistics.size());

    std::vector<double> avg_frees;
    avg_frees.reserve(statistics.size());

    std::vector<double> tbb_avg_allocs;
    tbb_avg_allocs.reserve(statistics.size());

    std::vector<double> tbb_avg_frees;
    tbb_avg_frees.reserve(statistics.size());

    for (const auto s : statistics) {
        bytes.emplace_back(s.num_bytes);
        avg_allocs.emplace_back(s.alloc_elapsed.count());
        avg_frees.emplace_back(s.free_elapsed.count());
        tbb_avg_allocs.emplace_back(s.tbb_alloc_elapsed.count());
        tbb_avg_frees.emplace_back(s.tbb_free_elasped.count());
    }

    fmt::print("bytes = np.array("); 
    print_numpy(bytes); 
    fmt::print(")\n"); 
     
    fmt::print("allocs = np.array("); 
    print_numpy(avg_allocs); 
    fmt::print(")\n"); 
     
    fmt::print("frees = np.array("); 
    print_numpy(avg_frees); 
    fmt::print(")\n"); 
     
    fmt::print("tbb_allocs = np.array("); 
    print_numpy(tbb_avg_allocs); 
    fmt::print(")\n"); 
     
    fmt::print("tbb_frees = np.array("); 
    print_numpy(tbb_avg_frees); 
    fmt::print(")\n"); 
}

/// Perform linearly growing allocations and directly free the allocated memory afterwards
auto linear_growth_alloc()
{
    print_header();

    std::vector<Stats> statistics;
    statistics.reserve(max_size_power);

    // Perform allocations from 1 byte to 1 GB
    for (long n = 1; n <= max_size_power; ++n) {
        long N = std::pow(2, n);
        auto [tbb_alloc_elapsed, tbb_free_elapsed] =
            basic_alloc_free(N, scalable_malloc, scalable_free);
        auto [alloc_elapsed, free_elapsed] = basic_alloc_free(N, std::malloc, std::free);

        print_round(N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed);

        // Log this to create nice copyable and easyly plotable stuff
        Stats stats = {N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed};
        statistics.emplace_back(stats);
    }

    print_stats(statistics);
}

/// Perform linearly growing allocations, but first perform many allocations and then free them
/// afterwards in a permuted (i.e not the order of allocation)
auto linear_growth_permuted_free()
{
    print_header();

    std::vector<Stats> statistics;
    statistics.reserve(max_size_power);

    for (long n = 1; n <= max_size_power; ++n) {
        long N                             = std::pow(2, n);
        auto [alloc_elapsed, free_elapsed] = alloc_permuted_free(N, std::malloc, std::free);
        auto [tbb_alloc_elapsed, tbb_free_elapsed] =
            alloc_permuted_free(N, scalable_malloc, scalable_free);

        print_round(N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed);

        // Log this to create nice copyable and easyly plotable stuff
        Stats stats = {N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed};
        statistics.emplace_back(stats);
    }
    print_stats(statistics);
}

/// Allocate random sizes which varie to the next lower power of 2 and the next larger power of
/// two So for a given N = 2^n, allocations are in [2^(n-1), 2^(n+1)[ Then again allocate
/// everything and randomly free them
auto random_alloc_permuted_free()
{
    print_header();

    std::vector<Stats> statistics;
    statistics.reserve(max_size_power);

    // Iterate over 2^1 to 2^20
    for (long n = 1; n <= max_size_power; ++n) {
        // Calculate lower and upper bound
        auto N  = std::pow(2, n);
        auto lb = std::pow(2, n - 1);
        auto ub = std::pow(2, n + 1);

        auto [alloc_elapsed, free_elapsed] =
            random_alloc_permuted_free(lb, ub, std::malloc, std::free);
        auto [tbb_alloc_elapsed, tbb_free_elapsed] =
            random_alloc_permuted_free(lb, ub, scalable_malloc, scalable_free);

        print_round(N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed);

        // Log this to create nice copyable and easyly plotable stuff
        Stats stats = {N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed};
        statistics.emplace_back(stats);
    }
    print_stats(statistics);
}

/// Similar to random_alloc_permuted_free(), but now don't free everything, free a random number
/// of buffers and then start allocating random sizes again and do it over and over again.
/// This should mimic a programm with many different allocations and common frees
auto random_alloc_random_permuted_free()
{
    print_header();

    std::vector<Stats> statistics;
    statistics.reserve(max_size_power);

    // Iterate over 2^1 to 2^20
    for (long n = 1; n <= max_size_power; ++n) {
        // Calculate lower and upper bound
        auto N  = std::pow(2, n);
        auto lb = std::pow(2, n - 1);
        auto ub = std::pow(2, n + 1);

        auto [alloc_elapsed, free_elapsed] =
            random_alloc_random_permuted_free(lb, ub, std::malloc, std::free);
        auto [tbb_alloc_elapsed, tbb_free_elapsed] =
            random_alloc_random_permuted_free(lb, ub, scalable_malloc, scalable_free);

        print_round(N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed);

        // Log this to create nice copyable and easyly plotable stuff
        Stats stats = {N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed};
        statistics.emplace_back(stats);
    }
    print_stats(statistics);
}

auto threaded_linear_growth_alloc(int num_threads)
{
    print_header();

    std::vector<Stats> statistics;
    statistics.reserve(max_size_power);

    // for (long N = 1; N <= gigabyte; N *= 2) {
    for (long n = 1; n <= max_size_power; ++n) {
        std::vector<std::thread>           threads;
        std::vector<std::pair<fsec, fsec>> times(num_threads);
        std::mutex                         mtx;

        auto N  = std::pow(2, n);
        auto lb = std::pow(2, n - 1);
        auto ub = std::pow(2, n + 1);

        for (int i = 0; i < num_threads; ++i) {
            // Create a thread
            threads.emplace_back([&mtx, &times, i, thread_id = i, N, lb, ub]() {
                // Save pair into tmp
                // auto tmp = alloc_permuted_free(N, std::malloc, std::free);
                auto tmp = random_alloc_random_permuted_free(lb, ub, std::malloc, std::free);

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
            threads.emplace_back([&mtx, &times, i, thread_id = i, N, lb, ub]() {
                // Save pair into tmp
                // auto tmp = alloc_permuted_free(N, scalable_malloc, scalable_free);
                auto tmp = random_alloc_random_permuted_free(lb, ub, std::malloc, std::free);

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

        print_round(N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed);

        // Log this to create nice copyable and easyly plotable stuff
        Stats stats = {N, alloc_elapsed, free_elapsed, tbb_alloc_elapsed, tbb_free_elapsed};
        statistics.emplace_back(stats);
    }
    print_stats(statistics);
}

int main(int argc, char** argv)
{
    cxxopts::Options options(argv[0], "Test defaul std::malloc vs TBB scalable malloc");

    options.add_options()("h,help", "Display Help message", cxxopts::value<bool>());
    options.add_options()("v,verbose", "Verbose output (includes small explanation of each test)",
                          cxxopts::value<bool>());

    options.add_options()("p,print-numpy", "Print as nparrays for python", cxxopts::value<bool>());
    options.add_options()("P,print-total-time", "Print column with total time", cxxopts::value<bool>());
     
    options.add_options()("n,num-threads", "Number of threads",
                          cxxopts::value<int>()->default_value("4"));
     
    options.add_options()("lin-growth-direct-free", "Linearly growing chunks, direct free",
                          cxxopts::value<bool>());
    options.add_options()("lin-growth-permuted-free",
                          "Linearly growing chunks, delayed permuted free", cxxopts::value<bool>());
    options.add_options()("random-alloc-permuted-free",
                          "random sized chunks (in ranges), delayed permuted free",
                          cxxopts::value<bool>());
    options.add_options()("random-alloc-random-free",
                          "random sized chunks (in ranges), random delayed permuted free",
                          cxxopts::value<bool>());

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        fmt::print("{}", options.help());
        exit(0);
    }

    if (result.count("print-numpy")) {
        print_statistics = true;
    }
     
    if (result.count("print-total-time")) {
        print_total_time = true;
    }

    // threaded_linear_growth_alloc(3);
    bool verbose = result.count("verbose");

    if (result.count("lin-growth-direct-free")) {
        if (verbose) {
            fmt::print("{:=^50}\n", "");
            fmt::print("Allocate fixed size chunk and free chunk right away\n");
            fmt::print(
                "This is a rather syntethic benchmark. It's quite rare to just allocate and free "
                "right away\n\n");
            fmt::print("Compare speedup of TBB scalabe allocator vs libstdc++ malloc\n");
            fmt::print("{:=^50}\n\n", "");
        }
        linear_growth_alloc();
    }

    if (result.count("lin-growth-permuted-free")) {
        if (verbose) {
            fmt::print("\n\n{:=^50}\n", "");
            fmt::print(
                "Allocate fixed size chunks and free them all in one go after all allocations\n\n");
            fmt::print("Compare speedup of TBB scalabe allocator vs libstdc++malloc\n");
            fmt::print("{:=^50}\n\n", "");
        }
        linear_growth_permuted_free();
    }

    if (result.count("random-alloc-permuted-free")) {
        if (verbose) {
            fmt::print("\n\n{:=^50}\n", "");
            fmt::print("Allocate random size chunks and free them all in one go after all "
                       "allocations\n\n");
            fmt::print("Compare speedup of TBB scalabe allocator vs libstdc++malloc\n");
            fmt::print("{:=^50}\n\n", "");
        }
        random_alloc_permuted_free();
    }

    if (result.count("random-alloc-random-free")) {
        if (verbose) {
            fmt::print("\n\n{:=^50}\n", "");
            fmt::print("Allocate random size chunks and free small portion, then start allocation "
                       "again\n\n");
            fmt::print("Compare speedup of TBB scalabe allocator vs libstdc++ malloc\n");
            fmt::print("{:=^50}\n\n", "");
        }
        random_alloc_random_permuted_free();
    }

    return 0;
}

void print_header()
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
                 fsec tbb_free_elapsed)
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

    print_difference(diff_total, diff_alloc, diff_free);
}

void print_difference(float diff_total, float diff_alloc, float diff_free)
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
    fmt::print("\n");
}
