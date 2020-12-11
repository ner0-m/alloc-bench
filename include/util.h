#pragma once

#include <vector>

#include "types.h"

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

StatsVecTuple split_stats(const std::vector<Stats>& stats);
 
StatsVecOfVecTuple split_2d_stats(const std::vector<std::vector<Stats>>& stats);
