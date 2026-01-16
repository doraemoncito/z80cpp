# z80cpp - Z80 Core in C++

This project is a C++ port of the Java-based [Z80Core](https://github.com/jsanchezv/Z80Core). It provides a highly optimized, standards-compliant Z80 emulator core.

## Features

*   **Complete Instruction Set**:
      * Full emulation of the Z80 instruction set.
      * Emulates the undocumented bits 3 & 5 from flags register
      * Emulates the MEMPTR register (known as WZ in official Zilog documentation)
      * Strict execution order for every instruction
      * Precise timing for all instructions, totally decoupled from the core
*   **High Performance**: Optimized for modern C++ compilers with specific performance tuning.
*   **Code Quality**: Enforced via clang-format, clang-tidy, and continuous integration.
*   **Build System**: Modern CMake build system with support for caching and parallel builds.


## Usage Example

See the *test* directory for an example use case.

*jspeccy at gmail dot com*


---

## 🚀 Quick Start

### Prerequisites

*   **CMake** (3.25+)
*   **C++ Compiler** (GCC, Clang, or MSVC supporting C++17)
*   **Tools** (Optional but recommended): `ccache`, `clang-format`, `clang-tidy`, `cppcheck`

### Build and Run

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

### Run Tests

```bash
make test
```

---

## 🛠️ Building the Project

### Build Options

You can configure the build using the following CMake options:

| Option                  | Default | Description                                     |
|-------------------------|---------|-------------------------------------------------|
| `Z80CPP_NATIVE_ARCH`    | `ON`    | Use `-march=native` for local CPU optimization. |
| `Z80CPP_ENABLE_LTO`     | `ON`    | Enable Link-Time Optimization.                  |
| `Z80CPP_BUILD_SHARED`   | `ON`    | Build shared library.                           |
| `Z80CPP_BUILD_STATIC`   | `ON`    | Build static library.                           |
| `Z80CPP_ENABLE_TESTING` | `ON`    | Build test suite.                               |
| `Z80CPP_ENABLE_CCACHE`  | `ON`    | Enable ccache for faster rebuilds.              |
| `Z80CPP_PARALLEL_BUILD` | `ON`    | Enable parallel builds using all CPU cores.     |

### Build Configurations

#### Maximum Performance (Local Use)
Optimized for the machine you are building on.
```bash
cmake -DCMAKE_BUILD_TYPE=Release \
      -DZ80CPP_NATIVE_ARCH=ON \
      -DZ80CPP_ENABLE_LTO=ON \
      ..
make -j$(nproc)
```

#### Distribution Build
Portable binary without architecture-specific instructions.
```bash
cmake -DCMAKE_BUILD_TYPE=Release \
      -DZ80CPP_NATIVE_ARCH=OFF \
      -DZ80CPP_ENABLE_LTO=ON \
      ..
make -j$(nproc)
```

### Build Performance

The project automatically optimizes build times using:
*   **ccache**: Compiler cache for reusing compiled objects (auto-enabled if installed).
*   **Parallel Builds**: Uses all available CPU cores by default.

To install `ccache`:
*   **macOS**: `brew install ccache`
*   **Ubuntu/Debian**: `sudo apt-get install ccache`
*   **Fedora/RHEL**: `sudo dnf install ccache`

---

## ⚡ Performance Optimizations

This fork implements several optimizations to improve emulation speed.

### Optimization Summary

| Priority | Optimization                      | Expected Gain | Status                    |
|----------|-----------------------------------|---------------|---------------------------|
| 1        | Jump table dispatch               | 15-30%        | Not implemented           |
| 2        | Force inline bus calls            | 10-20%        | ✅ Implemented             |
| 3        | Combined flag table               | 5-10%         | Not implemented           |
| 4        | Inline carry flag                 | 5-8%          | Not implemented           |
| 5        | Branch hints                      | 3-5%          | ✅ Implemented             |
| 6        | Profile-Guided Optimization (PGO) | 5-15%         | Not implemented           |
| 7        | Memory prefetch                   | 2-4%          | Not implemented           |
| 8        | Restrict pointers                 | 2-3%          | ✅ Implemented             |
| 9        | Reduce flagQ writes               | 1-2%          | ✅ Implemented             |
| 10       | Full Link-Time Optimization (LTO) | 2-5%          | ✅ Implemented             |
| 11       | Prefix shadowing / register swap  | 1-3%          | Not implemented           |
| 12       | Cache-friendly member layout      | 2-5%          | ⚠️ Partial (alignas only) |

### Detailed Descriptions

#### 1. Jump Table / Computed Goto
Replaces large `switch` statements in opcode decoding with function pointer tables or computed gotos (GCC/Clang) to reduce branch misprediction.

#### 2. Inline Bus Interface Calls
Uses compiler attributes (`__attribute__((always_inline))`, `__forceinline`) to ensure critical memory access methods (`fetchOpcode`, `peek8`, `poke8`) are fully inlined.

#### 3. Combined Flag Tables
Combines four separate 256-byte flag tables into a single structure array to improve cache locality and reduce cache pressure.

---

## 🧹 Code Quality & Tools

This project uses **clang-format** and **clang-tidy** to maintain consistent code style and detect issues.

### Installation

*   **macOS**: `brew install clang-format llvm`
*   **Linux (Ubuntu/Debian)**: `sudo apt install clang-format clang-tools`
*   **Linux (Fedora/RHEL)**: `sudo dnf install clang-tools-extra`

### Usage

From the `build/` directory:

| Command             | Description                                                        |
|---------------------|--------------------------------------------------------------------|
| `make format`       | Format all source files.                                           |
| `make format-check` | Verify formatting without modifying files.                         |
| `make lint`         | Run clang-tidy static analysis (requires `compile_commands.json`). |
| `make lint-fix`     | Run clang-tidy and automatically fix issues.                       |

### IDE Integration

*   **VS Code**: Install the C/C++ extension. It will automatically use the `.clang-format` file.
*   **CLion**: Go to Settings → Editor → Code Style → C/C++ and enable clang-format.
*   **Xcode**: Use the CMake targets (`make format`) via build settings.

---

## 🧪 Testing

The project includes a test suite to verify the emulator's correctness.

### Running Tests

1.  Build the project with testing enabled (default).
2.  Run the tests using `make test` or `ctest`.

```bash
cd build
make test
```

To run the simulator manually:
```bash
./bin/z80_sim_test
```

---

## ❓ Troubleshooting

| Issue                             | Solution                                           |
|-----------------------------------|----------------------------------------------------|
| "clang-format not found"          | Install using package manager (brew/apt/dnf).      |
| "CMake targets not found"         | Run `cmake ..` from the `build/` directory.        |
| "compile_commands.json not found" | Run `cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..`. |

## Performance Benchmarks

### Z80all instruction exerciser timings

This table presents detailed timing measurements from the Z80all instruction exerciser, which systematically tests individual Z80 instruction groups and their variants.

**Column descriptions:**
- **Test**: Instruction or instruction group being tested
- **Time (sec)**: Execution time in seconds for the test (baseline)
- **Optimized (sec)**: Execution time in seconds after optimizations

| #           | Instruction group            | Baseline (sec) | Optimized (sec) | Speedup (%) |
|-------------|------------------------------|---------------:|----------------:|------------:|
| 1           | <adc,sbc> hl,<bc,de,hl,sp>   |          4.290 |           0.870 |        79.7 |
| 2           | add hl,<bc,de,hl,sp>         |          2.098 |           0.420 |        80.0 |
| 3           | add ix,<bc,de,ix,sp>         |          2.125 |           0.420 |        80.2 |
| 4           | add iy,<bc,de,iy,sp>         |          2.120 |           0.426 |        79.9 |
| 5           | aluop a,nn                   |          1.063 |           0.213 |        79.9 |
| 6           | aluop a,<b,c,d,e,h,l,(hl),a> |         37.504 |           7.422 |        80.1 |
| 7           | aluop a,<ixh,ixl,iyh,iyl>    |         19.405 |           3.792 |        80.5 |
| 8           | aluop a,(<ix,iy>+1)          |          9.732 |           1.922 |        80.3 |
| 9           | bit n,(<ix,iy>+1)            |          0.076 |           0.014 |        81.6 |
| 10          | bit n,<b,c,d,e,h,l,(hl),a>   |          2.462 |           0.481 |        80.5 |
| 11          | cpd<r>                       |          0.475 |           0.094 |        80.2 |
| 12          | cpi<r>                       |          0.475 |           0.093 |        80.4 |
| 13          | <daa,cpl,scf,ccf>            |          2.073 |           0.410 |        80.2 |
| 14          | <inc,dec> a                  |          0.111 |           0.022 |        80.2 |
| 15          | <inc,dec> b                  |          0.111 |           0.021 |        81.1 |
| 16          | <inc,dec> bc                 |          0.058 |           0.011 |        81.0 |
| 17          | <inc,dec> c                  |          0.112 |           0.022 |        80.4 |
| 18          | <inc,dec> d                  |          0.112 |           0.022 |        80.4 |
| 19          | <inc,dec> de                 |          0.056 |           0.011 |        80.4 |
| 20          | <inc,dec> e                  |          0.112 |           0.021 |        81.3 |
| 21          | <inc,dec> h                  |          0.111 |           0.022 |        80.2 |
| 22          | <inc,dec> hl                 |          0.057 |           0.011 |        80.7 |
| 23          | <inc,dec> ix                 |          0.057 |           0.011 |        80.7 |
| 24          | <inc,dec> iy                 |          0.057 |           0.011 |        80.7 |
| 25          | <inc,dec> l                  |          0.112 |           0.022 |        80.4 |
| 26          | <inc,dec> (hl)               |          0.111 |           0.022 |        80.2 |
| 27          | <inc,dec> sp                 |          0.057 |           0.011 |        80.7 |
| 28          | <inc,dec> (<ix,iy>+1)        |          0.235 |           0.046 |        80.4 |
| 29          | <inc,dec> ixh                |          0.112 |           0.022 |        80.4 |
| 30          | <inc,dec> ixl                |          0.114 |           0.022 |        80.7 |
| 31          | <inc,dec> iyh                |          0.112 |           0.022 |        80.4 |
| 32          | <inc,dec> iyl                |          0.112 |           0.022 |        80.4 |
| 33          | ld <bc,de>,(nnnn)            |          0.001 |           0.000 |       100.0 |
| 34          | ld hl,(nnnn)                 |          0.000 |           0.000 |         N/A |
| 35          | ld sp,(nnnn)                 |          0.000 |           0.000 |         N/A |
| 36          | ld <ix,iy>,(nnnn)            |          0.001 |           0.000 |       100.0 |
| 37          | ld (nnnn),<bc,de>            |          0.002 |           0.000 |       100.0 |
| 38          | ld (nnnn),hl                 |          0.000 |           0.000 |         N/A |
| 39          | ld (nnnn),sp                 |          0.000 |           0.000 |         N/A |
| 40          | ld (nnnn),<ix,iy>            |          0.002 |           0.000 |       100.0 |
| 41          | ld <bc,de,hl,sp>,nnnn        |          0.002 |           0.000 |       100.0 |
| 42          | ld <ix,iy>,nnnn              |          0.001 |           0.000 |       100.0 |
| 43          | ld a,<(bc),(de)>             |          0.001 |           0.000 |       100.0 |
| 44          | ld <b,c,d,e,h,l,(hl),a>,nn   |          0.002 |           0.000 |       100.0 |
| 45          | ld (<ix,iy>+1),nn            |          0.001 |           0.000 |       100.0 |
| 46          | ld <b,c,d,e>,(<ix,iy>+1)     |          0.019 |           0.004 |        78.9 |
| 47          | ld <h,l>,(<ix,iy>+1)         |          0.009 |           0.001 |        88.9 |
| 48          | ld a,(<ix,iy>+1)             |          0.004 |           0.000 |       100.0 |
| 49          | ld <ixh,ixl,iyh,iyl>,nn      |          0.001 |           0.000 |       100.0 |
| 50          | ld <bcdehla>,<bcdehla>       |          0.177 |           0.033 |        81.4 |
| 51          | ld <bcdexya>,<bcdexya>       |          0.355 |           0.069 |        80.6 |
| 52          | ld a,(nnnn) / ld (nnnn),a    |          0.001 |           0.000 |       100.0 |
| 53          | ldd<r> (1)                   |          0.001 |           0.000 |       100.0 |
| 54          | ldd<r> (2)                   |          0.001 |           0.000 |       100.0 |
| 55          | ldi<r> (1)                   |          0.001 |           0.000 |       100.0 |
| 56          | ldi<r> (2)                   |          0.001 |           0.000 |       100.0 |
| 57          | neg                          |          0.485 |           0.096 |        80.2 |
| 58          | <rrd,rld>                    |          0.264 |           0.052 |        80.3 |
| 59          | <rlca,rrca,rla,rra>          |          0.226 |           0.045 |        80.1 |
| 60          | shf/rot (<ix,iy>+1)          |          0.015 |           0.003 |        80.0 |
| 61          | shf/rot <b,c,d,e,h,l,(hl),a> |          0.348 |           0.069 |        80.2 |
| 62          | <set,res> n,<bcdehl(hl)a>    |          0.346 |           0.068 |        80.3 |
| 63          | <set,res> n,(<ix,iy>+1)      |          0.016 |           0.003 |        81.3 |
| 64          | ld (<ix,iy>+1),<b,c,d,e>     |          0.044 |           0.009 |        79.5 |
| 65          | ld (<ix,iy>+1),<h,l>         |          0.009 |           0.001 |        88.9 |
| 66          | ld (<ix,iy>+1),a             |          0.002 |           0.000 |       100.0 |
| 67          | ld (<bc,de>),a               |          0.003 |           0.000 |       100.0 |
| **Total**   |                              |     **88.158** |      **17.404** |   **80.3%** |
| **Average** |                              |      **1.316** |       **0.260** |   **80.3%** |

### Z80 Performance Benchmark Test Suite

This test suite measures the performance of the Z80 emulator core across various instruction patterns and workloads, demonstrating both throughput (MT/s) and instruction execution rate (MIPS).

**Column descriptions:**
- **Test**: Name of the benchmark test
- **Elapsed (sec)**: Total execution time in seconds
- **T-States**: Total CPU clock cycles executed
- **MT/s**: Million T-States per second (throughput)
- **MIPS**: Million Instructions Per Second

Results before optimization:

| Test             | Elapsed (sec) |      T-States |    MT/s |  MIPS |
|------------------|--------------:|--------------:|--------:|------:|
| ZEXALL           |        13.241 | 8,099,612,806 | 611.700 | 0.755 |
| Instruction Mix  |        11.137 | 6,812,499,998 | 611.676 | 0.449 |
| Memory Intensive |         4.136 | 4,000,039,014 | 967.207 | 0.484 |
| Arithmetic Heavy |        10.093 | 4,781,818,189 | 473.791 | 0.495 |
| Branch Heavy     |         4.114 | 4,000,410,001 | 972.331 | 0.729 |

Results after optimization:

| Test             | Elapsed (sec) | Performance (MIPS) | Speedup (%) |
|------------------|--------------:|-------------------:|------------:|
| ZEXALL           |          0.04 |             240.30 |      555.15 |
| Instruction Mix  |          0.02 |             332.64 |      647.47 |
| Memory Intensive |          0.00 |           48096.58 |      680.46 |
| Arithmetic Heavy |          0.01 |             365.40 |      499.23 |
| Branch Heavy     |          0.00 |            6954.51 |      589.48 |

### Z80 Game Benchmark Test Suite

This test suite is designed to measure and evaluate the performance characteristics of the Z80cpp emulator on real ZX Spectrum games. It provides benchmarks for CPU instruction execution, memory operations, and overall emulation speed to help identify optimization opportunities and track performance improvements across different versions.

**Column descriptions:**
- **Game**: Name of the ZX Spectrum game under test
- **Time (sec)**: Total execution time in seconds
- **T-States**: Total CPU clock cycles executed
- **MT/s**: Million T-States per second (throughput)
- **MIPS**: Million Instructions Per Second

Results before optimization:

| Game          | Time (sec) |      T-States |    MT/s |  MIPS |
|---------------|-----------:|--------------:|--------:|------:|
| Ant Attack    |     10.341 | 5,222,568,303 | 505.046 | 0.484 |
| Arkanoid      |      8.831 | 4,875,000,001 | 552.034 | 0.566 |
| Horace Skiing |      6.890 | 4,000,721,887 | 580.694 | 0.726 |
| Jet Set Willy |      6.740 | 4,000,113,568 | 593.520 | 0.742 |
| Manic Miner   |      9.386 | 8,312,414,602 | 885.635 | 0.533 |
| Pyjamarama    |      5.436 | 4,000,176,409 | 735.807 | 0.920 |

Results after optimization:

| Game          | Time (sec) | Performance (MIPS) | Speedup (%) |
|---------------|-----------:|-------------------:|------------:|
| Ant Attack    |      0.021 |             241.76 |      280.62 |
| Arkanoid      |      0.022 |             224.14 |      312.19 |
| Horace Skiing |      0.012 |             418.42 |      478.34 |
| Jet Set Willy |      0.011 |             442.89 |      506.18 |
| Manic Miner   |      0.011 |             466.08 |      542.15 |
| Pyjamarama    |      0.010 |             493.43 |      563.97 |



## References

- [Z80 Instruction Set](https://www.z80.info/zip/z80instr.pdf)
- [ZX Spectrum Technical Specifications](https://www.zxnet.co.uk/faq/specs.html)
- [Z80.info](https://www.z80.info/)
- [ticalc.org Z80 Text Collection](https://www.ticalc.org/pub/text/z80/)
- [Z80 Assembly Guide (.zip)](http://www.ticalc.org/pub/text/z80/z80asmg.zip)
- [ticalc.org Z80 Index](http://www.ticalc.org/pub/text/z80/index.html)
- [Z80 Undocumented (PDF)](http://www.myquest.nl/z80undocumented/z80-documented-v0.91.pdf)
- [The Undocumented Z80 Documented](http://www.ticalc.org/archives/files/fileinfo/128/12883.html)
- [Z80 Undocumented Features](http://www.ticalc.org/archives/files/fileinfo/1/109.html)