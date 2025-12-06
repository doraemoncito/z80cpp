# Z80cpp - High-Performance Z80 Emulator

A complete Z80 CPU emulator in C++ with comprehensive testing framework and real ZX Spectrum game benchmarking capabilities.

**Performance:** 220-310 MIPS average (300-370x faster than real hardware)

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Quick Start](#quick-start)
- [Building](#building)
- [Performance Testing](#performance-testing)
- [Benchmark Results](#benchmark-results)
- [Game Collection](#game-collection)
- [Tools](#tools)
- [Project Structure](#project-structure)
- [Optimization Guide](#optimization-guide)
- [Contributing](#contributing)

---

## Overview

Z80cpp is a high-performance port from Java to C++ of [Z80Core](https://github.com/jsanchezv/Z80Core), featuring complete Z80 instruction set emulation with cycle-accurate timing.

### What's Included

- **Complete Z80 emulator** - All documented and undocumented instructions
- **Performance testing framework** - Benchmark tools and synthetic tests
- **6 Real ZX Spectrum games** - Extracted and ready for testing
- **Automated test suite** - Easy benchmarking and comparison
- **TAP file extraction tool** - Convert ZX Spectrum TAP files to raw binaries

---

## Features

### CPU Emulation

- ✅ Complete instruction set emulation (all 256 opcodes + prefixed)
- ✅ Undocumented bits 3 & 5 from flags register
- ✅ MEMPTR register emulation (WZ in Zilog documentation)
- ✅ Strict execution order for every instruction
- ✅ Precise cycle timing, totally decoupled from the core
- ✅ Passes ZEXALL comprehensive test suite

### Performance Tools

- ✅ **z80_benchmark** - High-precision MIPS measurement
- ✅ **tap2bin.py** - Extract machine code from TAP files
- ✅ **Synthetic test generators** - Create targeted workloads
- ✅ **Automated test suite** - Batch testing with result comparison
- ✅ **6 Real games tested** - Ant Attack, Jet Set Willy, Pyjamarama, and more

---

## Quick Start

### 1. Build the Emulator

```bash
# Clone and build
cd /path/to/z80cpp-vanilla
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make

# Build benchmark tool
cd ..
./build_benchmark.sh
```

### 2. Run ZEXALL Test

```bash
cd build
./z80sim  # Runs ZEXALL correctness test (~30 seconds)
```

### 3. Benchmark Performance

```bash
# Quick performance test
./z80_benchmark ../example/zexall.bin -i 10000000

# Real game test
./z80_benchmark ../tests/spectrum-roms/games/ant_attack.bin -i 10000000
```

### 4. Test All Games

```bash
cd ../tests
./run_game_benchmarks.sh
```

---

## Building

### Requirements

- CMake 3.25.1 or higher
- C++14 compatible compiler (GCC, Clang, MSVC)
- Python 3 (for tools)

### Standard Build

```bash
mkdir build
cd build
cmake ..
make
```

### Optimized Build

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -march=native -flto"
make
```

### Build Targets

- `z80cpp` - Shared library
- `z80cpp-static` - Static library
- `z80sim` - Test simulator (runs ZEXALL)
- `z80_benchmark` - Performance testing tool (via build_benchmark.sh)

---

## Performance Testing

### Available Tests

### 1. ZEXALL (Correctness + Performance)
```bash
cd build
./z80sim  # Run ZEXALL correctness test
```

#### 2. Integrated Test Suite (CTest)
```bash
cd build

# Run all tests (correctness + performance)
ctest

# Run only performance tests
ctest -L performance

# Run only game tests  
ctest -L game

# Verbose output
ctest --verbose
```

#### 3. Synthetic Tests (Direct Execution)
```bash
# Generate tests first (if not already done)
cd tests
python3 generate_test_programs.py spectrum-roms/synthetic

# Run comprehensive benchmark suite
cd ../build
./benchmark_tests

# Individual synthetic tests
./z80_benchmark ../tests/spectrum-roms/synthetic/instruction_mix.bin
./z80_benchmark ../tests/spectrum-roms/synthetic/memory_intensive.bin
./z80_benchmark ../tests/spectrum-roms/synthetic/arithmetic_heavy.bin
./z80_benchmark ../tests/spectrum-roms/synthetic/branch_heavy.bin
```

#### 4. Real ZX Spectrum Games
```bash
cd build

# Ant Attack (310 MIPS)
./z80_benchmark ../tests/spectrum-roms/games/ant_attack.bin -i 10000000

# Jet Set Willy (305 MIPS)
./z80_benchmark ../tests/spectrum-roms/games/jet_set_willy_code2.bin -i 10000000

# Pyjamarama (303 MIPS)
./z80_benchmark ../tests/spectrum-roms/games/pyjamarama_code2.bin -i 10000000

# And 3 more...
```

### Automated Testing

```bash
cd tests
./run_game_benchmarks.sh  # Tests all 6 games, saves results
```

### Compare Results

```bash
cd tests
./compare_benchmarks.py results/baseline.txt results/optimized.txt
```

---

## Benchmark Results

### Real ZX Spectrum Games Performance

**Test Date:** December 6, 2024  
**Total Games:** 6  
**Average Performance:** 279.68 MIPS  
**Speedup:** 300-370x faster than real hardware

| Rank | Game | MIPS | Type | Status |
|------|------|------|------|--------|
| 🥇 | **Ant Attack** | **310.33** | Isometric 3D | ✅ Tested |
| 🥈 | **Jet Set Willy** | **305.29** | Platform | ✅ Tested |
| 🥉 | **Pyjamarama** | **303.33** | Platform | ✅ Tested |
| 4 | Arkanoid | 282.45 | Arcade | ✅ Tested |
| 5 | Horace Skiing | 251.76 | Arcade | ✅ Tested |
| 6 | Manic Miner | 224.90 | Platform | ✅ Tested |

### Performance Highlights

- **Fastest Game:** Ant Attack (310.33 MIPS) - Isometric 3D
- **Slowest Game:** Manic Miner (224.90 MIPS) - Still excellent!
- **Performance Range:** 85 MIPS spread
- **All Games:** >220 MIPS minimum

**Full results:** See `ALL_GAMES_TESTED_RESULTS.md`

---

## Game Collection

### Included Games (6 Total)

All games have been downloaded, machine code extracted, and tested:

#### Platform Games (3)
1. **Manic Miner** (1983) - Classic platform with collision detection
2. **Jet Set Willy** (1984) - Room-based adventure, sequel to Manic Miner
3. **Pyjamarama** (1984) - Wally series platform adventure

#### Arcade Games (2)
4. **Arkanoid** (1987) - Breakout clone with ball physics
5. **Horace Goes Skiing** (1982) - Two-part arcade game

#### 3D Games (1)
6. **Ant Attack** (1983) - Revolutionary isometric 3D maze game

### Files Structure

```
tests/spectrum-roms/games/
├── ant_attack.tap          (Original TAP)
├── ant_attack.bin          (Extracted 32KB) ✅
├── jet_set_willy.tap
├── jet_set_willy_code2.bin (Extracted 32KB) ✅
├── pyjamarama.tap
├── pyjamarama_code2.bin    (Extracted 39KB) ✅
├── arkanoid.tap
├── arkanoid.bin            (Extracted 1.7KB) ✅
├── horace_skiing.tap
├── horace_skiing_code2.bin (Extracted 7.9KB) ✅
├── manic_miner.tap
└── manic_miner_code2.bin   (Extracted 32KB) ✅
```

---

## Tools

### 1. z80_benchmark - Performance Testing

High-precision benchmarking tool with MIPS and T-states/sec measurement.

**Usage:**
```bash
./z80_benchmark <binary_file> [options]

Options:
  -i <count>    Run for exactly <count> instructions
  -s            Silent mode (minimal output)
  -o <file>     Save results to file

Examples:
  ./z80_benchmark zexall.bin -i 100000000
  ./z80_benchmark game.bin -i 10000000 -o results.txt
```

**Output:**
```
============================================================
Benchmark: ant_attack.bin
============================================================
Elapsed time:        0.032 seconds
Total instructions:  10,000,000
Total T-states:      40,024,383

Performance:
  310.33 MIPS (million instructions/sec)
  1,242.07 million T-states/sec
  354.88x faster than real ZX Spectrum
============================================================
```

### 2. tap2bin.py - TAP File Extraction

Extract Z80 machine code from ZX Spectrum TAP files.

**Usage:**
```bash
python3 tools/tap2bin.py <tap_file> [output_dir]

Example:
  python3 tools/tap2bin.py game.tap
  python3 tools/tap2bin.py game.tap extracted/
```

**Features:**
- Parses TAP file format
- Identifies machine code blocks (Type 0x03)
- Extracts multiple code blocks
- Shows load addresses
- Saves as .bin files

### 3. generate_test_programs.py - Synthetic Tests

Create targeted Z80 workloads for performance testing.

**Usage:**
```bash
python3 tests/generate_test_programs.py [output_dir]

Example:
  python3 tests/generate_test_programs.py tests/spectrum-roms/synthetic
```

**Generates:**
- instruction_mix.bin - Balanced instruction mix (~400K instructions)
- memory_intensive.bin - Memory stress test (4K operations)
- arithmetic_heavy.bin - CPU-intensive math (~2M operations)
- branch_heavy.bin - Branch prediction test (~50K jumps)

### 4. run_game_benchmarks.sh - Automated Testing

Batch test all games and save results.

**Usage:**
```bash
cd tests
./run_game_benchmarks.sh
```

**Output:** Saves timestamped results in `tests/results/`

### 5. compare_benchmarks.py - Result Comparison

Compare benchmark results to measure improvements.

**Usage:**
```bash
python3 tests/compare_benchmarks.py baseline.txt optimized.txt
```

---

## Project Structure

```
z80cpp-vanilla/
├── README.md                           # This file
├── CMakeLists.txt                      # Build configuration
├── build_benchmark.sh                  # Benchmark tool builder
│
├── include/
│   ├── z80.h                           # Z80 CPU class
│   └── z80operations.h                 # Memory/IO interface
│
├── src/
│   └── z80.cpp                         # Z80 implementation
│
├── example/
│   ├── z80sim.cpp                      # Example simulator
│   ├── z80sim.h
│   └── zexall.bin                      # ZEXALL test suite
│
├── tests/
│   ├── README.md                       # Testing guide
│   ├── run_game_benchmarks.sh          # Automated testing
│   ├── compare_benchmarks.py           # Result comparison
│   ├── generate_test_programs.py       # Test generator
│   ├── results/                        # Benchmark outputs
│   └── spectrum-roms/
│       ├── games/                      # Real ZX Spectrum games
│       │   ├── *.tap                   # Original TAP files
│       │   └── *.bin                   # Extracted machine code
│       └── synthetic/                  # Generated tests
│
├── tools/
│   └── tap2bin.py                      # TAP extraction tool
│
└── build/
    ├── libz80cpp.a                     # Static library
    ├── libz80cpp.dylib                 # Shared library
    ├── z80sim                          # Test simulator
    └── z80_benchmark                   # Benchmark tool
```

---

## Optimization Guide

### Current Performance

- **Average:** 279.68 MIPS
- **Range:** 224-310 MIPS
- **Speedup:** 300-370x vs real hardware

### Compiler Optimizations

#### Level 1: Basic Optimizations
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -march=native"
```

#### Level 2: Aggressive Optimizations
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -march=native -flto -funroll-loops"
```

#### Level 3: Profile-Guided Optimization
```bash
# Step 1: Build with profiling
cmake .. -DCMAKE_CXX_FLAGS="-O3 -march=native -fprofile-generate"
make

# Step 2: Run representative workload
./build/z80_benchmark example/zexall.bin -i 100000000

# Step 3: Rebuild with profile data
cmake .. -DCMAKE_CXX_FLAGS="-O3 -march=native -fprofile-use"
make
```

### Code Optimizations

**See:** `PERFORMANCE_IMPROVEMENTS.md` for detailed strategies:
- Jump table dispatch
- Superinstructions
- Template-based design
- Memory access optimization
- Branch prediction hints

### Measuring Improvements

```bash
# 1. Establish baseline
cd tests
./run_game_benchmarks.sh > baseline.txt

# 2. Apply optimizations
cd ../build
cmake .. [optimization flags]
make

# 3. Re-test
cd ../tests
./run_game_benchmarks.sh > optimized.txt

# 4. Compare
./compare_benchmarks.py baseline.txt optimized.txt
```

---

## Testing Workflow

### 1. Correctness Testing

```bash
cd build
./z80sim  # Runs ZEXALL (should pass 100%)
```

### 2. Performance Baseline

```bash
cd tests
./run_game_benchmarks.sh
```

### 3. Apply Optimizations

```bash
# See Optimization Guide above
```

### 4. Verify & Compare

```bash
cd build
./z80sim  # Verify correctness still passes

cd ../tests
./run_game_benchmarks.sh > new_results.txt
./compare_benchmarks.py baseline.txt new_results.txt
```

---

## API Usage

### Basic Example

```cpp
#include "z80.h"

class MySystem : public Z80operations {
public:
    uint8_t z80Ram[0x10000];
    uint8_t z80Ports[0x10000];
    
    uint8_t peek8(uint16_t address) override {
        return z80Ram[address];
    }
    
    void poke8(uint16_t address, uint8_t value) override {
        z80Ram[address] = value;
    }
    
    uint8_t inPort(uint16_t port) override {
        return z80Ports[port & 0xFF];
    }
    
    void outPort(uint16_t port, uint8_t value) override {
        z80Ports[port & 0xFF] = value;
    }
    
    void addressOnBus(uint16_t address, int32_t tstates) override {
        // Optional: Handle memory contention
    }
    
    void interruptHandlingTime(int32_t tstates) override {
        // Optional: Handle interrupt timing
    }
    
    bool isActiveINT() override {
        return false;  // Return true when interrupt pending
    }
};

int main() {
    MySystem sys;
    Z80 cpu;
    cpu.setZ80operations(&sys);
    cpu.reset();
    
    // Load program into memory
    sys.z80Ram[0x0000] = 0x3E;  // LD A, 42
    sys.z80Ram[0x0001] = 0x2A;
    sys.z80Ram[0x0002] = 0x76;  // HALT
    
    // Execute
    while (!cpu.isHalted()) {
        cpu.execute();
    }
    
    return 0;
}
```

---

## Contributing

### Adding New Games

1. Download ZX Spectrum TAP file
2. Extract machine code:
   ```bash
   python3 tools/tap2bin.py game.tap
   ```
3. Test:
   ```bash
   ./build/z80_benchmark game.bin -i 10000000
   ```
4. Add to game collection if successful

### Reporting Issues

Please include:
- CPU: [processor model]
- OS: [operating system]
- Compiler: [gcc/clang version]
- Performance results
- Expected vs actual behavior

---

## License

Same license as the original [Z80Core](https://github.com/jsanchezv/Z80Core) project.

---

## Credits

**Original Author:** José Luis Sánchez (jspeccy at gmail dot com)  
**Original Project:** [Z80Core](https://github.com/jsanchezv/Z80Core) (Java)  
**Port:** C++ version with performance testing framework

### Special Thanks

- ZEXALL test suite creators
- ZX Spectrum community
- World of Spectrum archive
- All game developers whose games are used for testing

---

## Links

- **Original Z80Core:** https://github.com/jsanchezv/Z80Core
- **ZEXALL Test Suite:** Classic Z80 instruction test
- **World of Spectrum:** https://worldofspectrum.org/

---

## Quick Reference

### Build
```bash
mkdir build && cd build && cmake .. && make
./build_benchmark.sh
```

### Test
```bash
cd build && ./z80sim  # Correctness
cd tests && ./run_game_benchmarks.sh  # Performance
```

### Benchmark
```bash
./build/z80_benchmark <file.bin> -i 10000000
```

**Performance:** 220-310 MIPS average | 300-370x real hardware speedup | 6 real games tested ✅

---

*Last Updated: December 6, 2024*

