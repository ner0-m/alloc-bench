
# Benchmarking allocators

This is a small repo contaning some kind of benchmarks for libstdc++'s malloc compared to
TBBs scalable_malloc.

## Build

This project is build using CMake (3.15 and newer), all dependencies are pulled in during configuration time using
[CPM](https://github.com/TheLartians/CPM.cmake) (Sorry I went a little over board using the command line arguments,
cxxopt is damn slow to compile, gotta replace it).

So the usual CMake steps are required from the root directory:

```bash
mkdir build 
cd build
cmake .. -DCMAKE_BUILD_TYPE=RELEASE -GNinja
ninja 
```

### Run benchmarks 

Then from there just call `main` from the build folder. See `main --help` for all the possible configurations.
 
### Using Hoard
 
Just for fun, I also tried using [Hoard](https://github.com/emeryberger/Hoard). It can be preloaded and then replaces
libstdc++'s allocator.

I also pull the repository in using CPM, so all we have to do:

```bash
ninja hoard
LD_PRELOAD=<path/from/previous/command>/libhoard.so ./main 
```

The build step prints out the path, where CPM puts hoard and where the final libary file is. So just look at the output
and run it.
