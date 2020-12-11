#pragma once

#include <cstdio>

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

/// Varialbe if you want to print results each round
static bool print_round_time = true;

/// Number of times to repeat an allocation test
static long repeat = 100;

/// Variable if statistic should be printed
static bool print_statistics = true;

/// Minimum number of allocations done randomly each iterations for certain tests
static int min_num_random_allocs = 200;

/// Maximum number of allocations done randomly each iterations for certain tests
static int max_num_random_allocs = 500;
