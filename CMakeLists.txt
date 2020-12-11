cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

project(compare_allocators CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

include(cmake/CPM.cmake)

cpmaddpackage(NAME fmt GIT_TAG 7.1.3 GITHUB_REPOSITORY fmtlib/fmt)

cpmaddpackage(NAME Hoard GITHUB_REPOSITORY emeryberger/Hoard GIT_TAG 3.13)

add_custom_target(hoard
    COMMAND make
    COMMAND echo "Use 'LD_PRELOAD=${Hoard_SOURCE_DIR}/src/libhoard.so ./main' to replace libstdc++'s malloc with Hoards malloc"
    WORKING_DIRECTORY cd ${Hoard_SOURCE_DIR}/src
    COMMENT "Building Hoard allocator"
    VERBATIM USES_TERMINAL
    )

cpmaddpackage(
  NAME
  cxxopts
  GITHUB_REPOSITORY
  jarro2783/cxxopts
  VERSION
  2.2.1
  OPTIONS
  "CXXOPTS_BUILD_EXAMPLES Off"
  "CXXOPTS_BUILD_TESTS Off")

cpmaddpackage(
  NAME
  oneTBB
  GITHUB_REPOSITORY
  oneapi-src/oneTBB
  VERSION
  2020.3
  OPTIONS
  "TBB_TEST Off")

find_package(Threads REQUIRED)

add_executable(main src/print.cpp src/main.cpp src/util.cpp include/print.h include/types.h
                    include/util.h)
target_include_directories(
  main PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
              $<INSTALL_INTERFACE:include> # <prefix>/include/mylib<
)
target_link_libraries(main fmt tbbmalloc cxxopts Threads::Threads)
