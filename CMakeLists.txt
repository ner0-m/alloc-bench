cmake_minimum_required(VERSION 3.15 FATAL_ERROR) 

project(compare_allocators CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
 
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}") 
 
include(cmake/CPM.cmake)
 
CPMAddPackage(
  NAME fmt
  GIT_TAG 7.1.3 
  GITHUB_REPOSITORY fmtlib/fmt
) 

CPMAddPackage(
  NAME cxxopts
  GITHUB_REPOSITORY jarro2783/cxxopts
  VERSION 2.2.1
  OPTIONS
    "CXXOPTS_BUILD_EXAMPLES Off"
    "CXXOPTS_BUILD_TESTS Off"
)

CPMAddPackage(
    NAME oneTBB
    GITHUB_REPOSITORY oneapi-src/oneTBB  
    VERSION 2020.3 
    OPTIONS
        "TBB_TEST Off"
) 

find_package(Threads REQUIRED) 
 
add_executable(main src/main.cpp)
target_link_libraries(main fmt tbbmalloc cxxopts Threads::Threads) 
