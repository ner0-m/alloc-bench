#pragma once

#include <cstdio>

#include <fmt/format.h>

#include "types.h"
#include "options.h"

// Just some print functions, which make everything a little bit cleaner
void print_header(bool);
void print_difference(float diff_total, float diff_alloc, float diff_free, bool);
void print_round(long N, fsec alloc_elapsed, fsec free_elapsed, fsec tbb_alloc_elapsed, fsec tbb_free_elapsed, bool, bool);
void print_stats(std::FILE* handle, const std::vector<Stats>& statistics);
void print_stats(std::FILE* handle, const std::vector<int> threads, const std::vector<std::vector<Stats>>& statistics);

template <typename T>
void print_sqaure_brackets(std::FILE* handle, const std::vector<T>& v)
{
    if (v.empty())
        return;

    fmt::print(handle, "[");
    if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
        fmt::print(handle, "{:.10f}", v[0]);
    } else {
        fmt::print(handle, "{}", v[0]);
    }

    if (v.size() == 1) {
        fmt::print(handle, "]");
        return;
    }

    for (int i = 1; i < v.size(); ++i) {
        if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
            fmt::print(handle, ",{:.10f}", v[i]);
        } else {
            fmt::print(handle, ",{}", v[i]);
        }
    }
    fmt::print(handle, "]");
}

template <typename T>
void print_numpy(std::FILE* handle, std::string_view name, const std::vector<T>& v)
{
    if (v.empty())
        return;

    fmt::print(handle, "{} = np.array(", name);
    print_sqaure_brackets(handle, v);
    fmt::print(handle, ")\n");
}

template <typename T>
void print_numpy(std::FILE* handle, std::string_view name, const std::vector<std::vector<T>>& v)
{
    if (v.empty())
        return;

    fmt::print(handle, "{} = np.array([\n    ", name);
    print_sqaure_brackets(handle, v[0]);

    if (v.size() == 1) {
        fmt::print(handle, "])\n");
        return;
    }

    // Once we know, we have to print more than one do the comma and newline
    fmt::print(handle, ",\n    ");

    for (int i = 1; i < v.size(); ++i) {
        print_sqaure_brackets(handle, v[i]);
        fmt::print(handle, ",\n    ");
    }
    fmt::print(handle, "])\n");
}

