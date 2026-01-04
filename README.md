# z80cpp

## Z80 core in C++

That's a port from Java to C++ of my [Z80Core](https://github.com/jsanchezv/Z80Core).

To build:
```
mkdir build
cd build
cmake ..
make
```

Run tests with `make test` from the build directory. All test output is shown by default.

The core have the same features of [Z80Core](https://github.com/jsanchezv/Z80Core):

* Complete instruction set emulation
* Emulates the undocumented bits 3 & 5 from flags register
* Emulates the MEMPTR register (known as WZ in official Zilog documentation)
* Strict execution order for every instruction
* Precise timing for all instructions, totally decoupled from the core

*jspeccy at gmail dot com*

## Z80 Test Suite

This projects contains a set of comprehensive validation and performance tests for the z80cpp library.

### Tests

1. **z80_sim_test** - ZEXALL instruction exerciser test with detailed timing output
2. **z80_benchmark_test** - Performance benchmarks using synthetic workloads
3. **z80_game_test** - Real-world benchmarks using ZX Spectrum game ROMs

### Building and Running Tests

```bash
# Build all tests
cd build
cmake ..
make

# Run tests (all output is shown by default)
make test

# Run tests with verbose output (shows CTest framework details)
make test ARGS="-V"
# or
ctest -V
```

### Test Details

#### z80_sim_test
- Runs the ZEXALL Z80 instruction exerciser
- Tests all Z80 instructions for correctness across 67 instruction groups
- Reports timing for each instruction group
- Total execution: ~89 seconds, 46.7 billion t-states

#### z80_benchmark_test
- Synthetic benchmarks measuring different aspects of Z80 emulation
- Tests: ZEXALL, Instruction Mix, Memory Intensive, Arithmetic Heavy, Branch Heavy
- Reports MIPS (millions of instructions per second) and T-states throughput

#### z80_game_test
- Real-world benchmarks using actual ZX Spectrum games
- Games tested: Ant Attack, Arkanoid, Horace Skiing, Jet Set Willy, Manic Miner, Pyjamarama
- Loads code blocks from TAP files and executes for 5M instructions
- Reports performance metrics for each game

#### Performance Notes

- **Test execution**: Tests run sequentially, so total execution time equals the sum of individual test times.
- **Current performance (debug build)**: ~0.4-0.9 MIPS.
    > ### General performance notes
    > ---
    > **Performance metrics:**
    > the performance value (~0.4-0.9 MIPS) represents the measured instruction throughput of the Z80 CPU emulator in a debug build configuration.
    >
    > **Calculation method:** the MIPS (Million Instructions Per Second) value was determined by executing a set of Z80 CPU instructions and measuring the elapsed time, then  calculating the ratio of total instructions executed to the time taken, expressed in  millions of instructions per second.
    >
    > **Build type:** debug build performance is typically significantly lower than release/optimized builds due to disabled compiler optimizations and additional debugging information. For production use cases, refer to release build performance benchmarks.

- **To improve performance**: Build with optimizations using `cmake -DCMAKE_BUILD_TYPE=Release ..`.
- **Optimized builds**: Can achieve significantly higher MIPS; see the benchmark tests below and measure performance depending on the system and CPU characteristics.

## Performance Benchmarks

### Z80all instruction exerciser timings

This table presents detailed timing measurements from the Z80all instruction exerciser, which systematically tests individual Z80 instruction groups and their variants.

**Column descriptions:**
- **Test**: Instruction or instruction group being tested
- **Time (sec)**: Execution time in seconds for the test

| #           | Instruction group            | Time (sec) |
|-------------|------------------------------|-----------:|
| 1           | <adc,sbc> hl,<bc,de,hl,sp>   |      4.290 |
| 2           | add hl,<bc,de,hl,sp>         |      2.098 |
| 3           | add ix,<bc,de,ix,sp>         |      2.125 |
| 4           | add iy,<bc,de,iy,sp>         |      2.120 |
| 5           | aluop a,nn                   |      1.063 |
| 6           | aluop a,<b,c,d,e,h,l,(hl),a> |     37.504 |
| 7           | aluop a,<ixh,ixl,iyh,iyl>    |     19.405 |
| 8           | aluop a,(<ix,iy>+1)          |      9.732 |
| 9           | bit n,(<ix,iy>+1)            |      0.076 |
| 10          | bit n,<b,c,d,e,h,l,(hl),a>   |      2.462 |
| 11          | cpd<r>                       |      0.475 |
| 12          | cpi<r>                       |      0.475 |
| 13          | <daa,cpl,scf,ccf>            |      2.073 |
| 14          | <inc,dec> a                  |      0.111 |
| 15          | <inc,dec> b                  |      0.111 |
| 16          | <inc,dec> bc                 |      0.058 |
| 17          | <inc,dec> c                  |      0.112 |
| 18          | <inc,dec> d                  |      0.112 |
| 19          | <inc,dec> de                 |      0.056 |
| 20          | <inc,dec> e                  |      0.112 |
| 21          | <inc,dec> h                  |      0.111 |
| 22          | <inc,dec> hl                 |      0.057 |
| 23          | <inc,dec> ix                 |      0.057 |
| 24          | <inc,dec> iy                 |      0.057 |
| 25          | <inc,dec> l                  |      0.112 |
| 26          | <inc,dec> (hl)               |      0.111 |
| 27          | <inc,dec> sp                 |      0.057 |
| 28          | <inc,dec> (<ix,iy>+1)        |      0.235 |
| 29          | <inc,dec> ixh                |      0.112 |
| 30          | <inc,dec> ixl                |      0.114 |
| 31          | <inc,dec> iyh                |      0.112 |
| 32          | <inc,dec> iyl                |      0.112 |
| 33          | ld <bc,de>,(nnnn)            |      0.001 |
| 34          | ld hl,(nnnn)                 |      0.000 |
| 35          | ld sp,(nnnn)                 |      0.000 |
| 36          | ld <ix,iy>,(nnnn)            |      0.001 |
| 37          | ld (nnnn),<bc,de>            |      0.002 |
| 38          | ld (nnnn),hl                 |      0.000 |
| 39          | ld (nnnn),sp                 |      0.000 |
| 40          | ld (nnnn),<ix,iy>            |      0.002 |
| 41          | ld <bc,de,hl,sp>,nnnn        |      0.002 |
| 42          | ld <ix,iy>,nnnn              |      0.001 |
| 43          | ld a,<(bc),(de)>             |      0.001 |
| 44          | ld <b,c,d,e,h,l,(hl),a>,nn   |      0.002 |
| 45          | ld (<ix,iy>+1),nn            |      0.001 |
| 46          | ld <b,c,d,e>,(<ix,iy>+1)     |      0.019 |
| 47          | ld <h,l>,(<ix,iy>+1)         |      0.009 |
| 48          | ld a,(<ix,iy>+1)             |      0.004 |
| 49          | ld <ixh,ixl,iyh,iyl>,nn      |      0.001 |
| 50          | ld <bcdehla>,<bcdehla>       |      0.177 |
| 51          | ld <bcdexya>,<bcdexya>       |      0.355 |
| 52          | ld a,(nnnn) / ld (nnnn),a    |      0.001 |
| 53          | ldd<r> (1)                   |      0.001 |
| 54          | ldd<r> (2)                   |      0.001 |
| 55          | ldi<r> (1)                   |      0.001 |
| 56          | ldi<r> (2)                   |      0.001 |
| 57          | neg                          |      0.485 |
| 58          | <rrd,rld>                    |      0.264 |
| 59          | <rlca,rrca,rla,rra>          |      0.226 |
| 60          | shf/rot (<ix,iy>+1)          |      0.015 |
| 61          | shf/rot <b,c,d,e,h,l,(hl),a> |      0.348 |
| 62          | <set,res> n,<bcdehl(hl)a>    |      0.346 |
| 63          | <set,res> n,(<ix,iy>+1)      |      0.016 |
| 64          | ld (<ix,iy>+1),<b,c,d,e>     |      0.044 |
| 65          | ld (<ix,iy>+1),<h,l>         |      0.009 |
| 66          | ld (<ix,iy>+1),a             |      0.002 |
| 67          | ld (<bc,de>),a               |      0.003 |
| **Total**   |                              | **88.158** |
| **Average** |                              |  **1.316** |

### Z80 Performance Benchmark Suite

This test suite measures the performance of the Z80 emulator core across various instruction patterns and workloads, demonstrating both throughput (MT/s) and instruction execution rate (MIPS).

**Column descriptions:**
- **Test**: Name of the benchmark test
- **Time (sec)**: Total execution time in seconds
- **T-States**: Total CPU clock cycles executed
- **MT/s**: Million T-States per second (throughput)
- **MIPS**: Million Instructions Per Second

| Test             | Time (sec) |      T-States |    MT/s |  MIPS |
|------------------|-----------:|--------------:|--------:|------:|
| ZEXALL           |     15.113 | 8,099,612,806 | 535.954 | 0.662 |
| Instruction Mix  |     11.004 | 6,812,499,998 | 619.091 | 0.454 |
| Memory Intensive |      4.289 | 4,000,039,014 | 932.733 | 0.466 |
| Arithmetic Heavy |     10.194 | 4,781,818,189 | 469.104 | 0.491 |
| Branch Heavy     |      4.378 | 4,000,410,001 | 913.839 | 0.685 |

### Z80 Game Benchmark Test Suite

This test suite is designed to measure and evaluate the performance characteristics of the Z80cpp emulator on real ZX Spectrum games. It provides benchmarks for CPU instruction execution, memory operations, and overall emulation speed to help identify optimization opportunities and track performance improvements across different versions.

**Column descriptions:**
- **Game**: Name of the ZX Spectrum game under test
- **Time (sec)**: Total execution time in seconds
- **T-States**: Total CPU clock cycles executed
- **MT/s**: Million T-States per second (throughput)
- **MIPS**: Million Instructions Per Second

| Game          | Time (sec) |      T-States |    MT/s |  MIPS |
|---------------|-----------:|--------------:|--------:|------:|
| Ant Attack    |     10.341 | 5,222,568,303 | 505.046 | 0.484 |
| Arkanoid      |      8.831 | 4,875,000,001 | 552.034 | 0.566 |
| Horace Skiing |      6.890 | 4,000,721,887 | 580.694 | 0.726 |
| Jet Set Willy |      6.740 | 4,000,113,568 | 593.520 | 0.742 |
| Manic Miner   |      9.386 | 8,312,414,602 | 885.635 | 0.533 |
| Pyjamarama    |      5.436 | 4,000,176,409 | 735.807 | 0.920 |

