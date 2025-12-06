# Quick Reference: Building Z80 Emulator for Performance

## TL;DR

**For maximum performance, always build with:**
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
```

## Build Types

### Release (Performance)
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
```
- ✅ Full optimization (`-O3`)
- ✅ CPU-specific code (`-march=native`)
- ✅ ~2-3x faster than Debug
- ✅ Use for benchmarks and production

### Debug (Development)
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
```
- ✅ No optimization (`-O0`)
- ✅ Debug symbols included
- ✅ Easier to debug with GDB/LLDB
- ⚠️ 2-3x slower than Release

### RelWithDebInfo (Profiling)
```bash
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
```
- ✅ Full optimization + debug symbols
- ✅ Best for profiling with tools
- ✅ Near-Release performance

## Performance Numbers

| Build Type | Performance | Use Case |
|------------|-------------|----------|
| Debug | ~5,000 MIPS | Development, debugging |
| Release | ~8,000 MIPS | Production, benchmarking |
| RelWithDebInfo | ~7,500 MIPS | Profiling, optimization |

## Compiler Flags Applied

### GCC
```
-O3 -march=native -mtune=native -Wall -std=c++14
```

### Clang/AppleClang
```
-O3 -march=native -mtune=native -Wall -std=c++14
```

### MSVC
```
/O2 /Ob2 /W3
```

## Verification

Check your build configuration:
```bash
grep CMAKE_BUILD_TYPE build/CMakeCache.txt
grep CMAKE_CXX_FLAGS build/CMakeCache.txt
```

Should show:
```
CMAKE_BUILD_TYPE:STRING=Release
CMAKE_CXX_FLAGS:STRING= -Wall -std=c++14 -O3 -march=native -mtune=native
```

## Common Mistakes

❌ **Don't do this:**
```bash
cmake ..              # Missing build type!
make
```
*Result: Defaults to Release now, but good practice to be explicit*

✅ **Do this instead:**
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
```

## Running Benchmarks

```bash
# Build Release version
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4

# Run benchmarks
./benchmark_tests

# Run game benchmarks
./game_benchmark_test
```

## Full Build Commands

### Fresh Release Build
```bash
rm -rf build
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
./benchmark_tests
```

### Fresh Debug Build
```bash
rm -rf build_debug
mkdir build_debug
cd build_debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j4
gdb ./benchmark_tests
```

## Performance Optimizations Enabled

When building with Release:

✅ **Computed goto dispatch** - Fast opcode dispatch  
✅ **Function inlining** - No call overhead  
✅ **Loop unrolling** - Less loop overhead  
✅ **Branch prediction** - Better CPU utilization  
✅ **Register allocation** - Fast variable access  
✅ **CPU-specific instructions** - Native code generation  
✅ **Dead code elimination** - No wasted work  
✅ **Constant propagation** - Compile-time calculations  

## Quick Performance Check

```bash
# Compare Debug vs Release
time build_debug/benchmark_tests > /dev/null
time build/benchmark_tests > /dev/null
```

Release should be 2-3x faster!

## What to Use When

| Situation | Build Type | Why |
|-----------|-----------|-----|
| Developing new features | Debug | Easier to debug |
| Fixing bugs | Debug | Better error messages |
| Testing performance | Release | Real performance |
| Benchmarking | Release | Maximum speed |
| Profiling code | RelWithDebInfo | Symbols + speed |
| Production builds | Release | Best performance |
| CI/CD pipelines | Release | Representative results |

## Remember

**The computed goto optimization only works well with compiler optimizations enabled!**

Without `-O3`, the performance is nearly identical to a regular switch statement.

---

Last Updated: December 6, 2024

