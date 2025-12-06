# CTest Integration for Z80 Benchmarks

## Overview

All benchmark shell scripts have been converted to CMake/CTest tests for better integration with the build system and CI/CD pipelines.

## Running Tests

### Run All Tests
```bash
cd build
ctest
```

### Run Specific Test Categories

#### Quick Benchmarks (Fast Synthetic Tests)
```bash
ctest -L quick
```

#### All Performance Tests
```bash
ctest -L performance
```

#### Synthetic Benchmarks Only
```bash
ctest -R "benchmark_"
```

#### Game Benchmarks Only
```bash
ctest -L game
```

#### Individual Test
```bash
ctest -R benchmark_instruction_mix
```

### Verbose Output
```bash
ctest -R benchmark_instruction_mix -V
```

### Show All Available Tests
```bash
ctest --show-only
```

## Test Categories

### 1. Correctness Tests
- **correctness_zexall**: ZEXALL Z80 instruction test suite
  - Must pass for correct emulation
  - Timeout: 60 seconds

### 2. Performance Test Suite
- **performance_benchmark_suite**: Complete synthetic benchmark suite
  - Runs all synthetic tests
  - Timeout: 120 seconds

### 3. Individual Synthetic Benchmarks
Using `z80_benchmark` tool:

- **benchmark_instruction_mix**: Mixed instruction workload
- **benchmark_memory_intensive**: Heavy memory access patterns
- **benchmark_arithmetic_heavy**: ALU-intensive operations
- **benchmark_branch_heavy**: Branch-heavy code

All synthetic benchmarks:
- Run 5,000,000 instructions
- Timeout: 15 seconds each
- Silent mode (`-s` flag)
- Label: `benchmark`, `synthetic`, `quick`, `performance`

### 4. Game Benchmarks
Using `game_benchmark_test` tool:

- **performance_ant_attack**: Ant Attack game code
- **performance_manic_miner**: Manic Miner game code
- **performance_jet_set_willy**: Jet Set Willy game code
- **performance_pyjamarama**: Pyjamarama game code
- **performance_arkanoid**: Arkanoid game code
- **performance_horace_skiing**: Horace Goes Skiing code

Game benchmarks:
- Run 5-10 million instructions (varies by game)
- Minimum MIPS threshold validation
- Timeout: 20-30 seconds
- Label: `performance`, `game`

## Test Labels

Tests are organized by labels for easy filtering:

| Label | Description |
|-------|-------------|
| `quick` | Fast-running synthetic benchmarks |
| `performance` | All performance/benchmark tests |
| `benchmark` | Individual benchmark tests |
| `synthetic` | Synthetic test programs |
| `game` | Real game code benchmarks |

## Example Usage

### CI/CD Pipeline
```yaml
# GitHub Actions example
- name: Run Quick Benchmarks
  run: |
    cd build
    ctest -L quick --output-on-failure

- name: Run All Performance Tests
  run: |
    cd build
    ctest -L performance --output-on-failure
```

### Development Workflow
```bash
# After making changes, run quick tests
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
ctest -L quick

# Before committing, run full test suite
ctest --output-on-failure
```

### Performance Regression Testing
```bash
# Run performance tests and save results
cd build
ctest -L performance -V > benchmark_results.txt

# Compare with baseline
diff benchmark_results.txt baseline_results.txt
```

## Advantages Over Shell Scripts

### ✅ Better Integration
- Works with `make test`
- Native CMake/CTest integration
- No shell script dependencies
- Cross-platform (works on Windows too)

### ✅ Cleaner Output
- Standardized test output format
- Pass/Fail status clear
- Timing information included
- Can redirect output easily

### ✅ Parallel Execution
```bash
# Run tests in parallel (4 jobs)
ctest -j4
```

### ✅ Test Selection
```bash
# Run only failed tests
ctest --rerun-failed

# Run tests matching regex
ctest -R "benchmark_.*"

# Exclude specific tests
ctest -E "game"
```

### ✅ CI/CD Friendly
- Standard exit codes (0 = pass, non-zero = fail)
- JUnit XML output available
- Integration with CDash dashboard
- Easy to parse results

## Migration from Shell Scripts

### Old Way (Shell Scripts)
```bash
# Run all benchmarks
./tests/run_all_benchmarks.sh

# Run game benchmarks
./tests/run_game_benchmarks.sh

# Run spectrum benchmarks
./tests/run_spectrum_benchmarks.sh
```

### New Way (CTest)
```bash
# Run all benchmarks
cd build && ctest -L performance

# Run game benchmarks
cd build && ctest -L game

# Run synthetic benchmarks
cd build && ctest -R "benchmark_"
```

## Shell Scripts Status

The original shell scripts are **deprecated** but kept for reference:
- `tests/run_all_benchmarks.sh` → Use `ctest -R "benchmark_"`
- `tests/run_game_benchmarks.sh` → Use `ctest -L game`
- `tests/run_spectrum_benchmarks.sh` → Not directly replaced (was more complex)

**Recommendation**: Use CTest commands instead of shell scripts.

## Advanced CTest Features

### Output Control
```bash
# Quiet mode
ctest -Q

# Verbose mode
ctest -V

# Extra verbose mode
ctest -VV

# Output only on failure
ctest --output-on-failure
```

### Test Scheduling
```bash
# Run tests in random order
ctest --schedule-random

# Stop on first failure
ctest --stop-on-failure

# Repeat tests until failure
ctest --repeat until-fail:10
```

### Resource Management
```bash
# Limit parallel jobs
ctest -j4

# Set test timeout
ctest --timeout 30
```

## Adding New Tests

To add a new benchmark test, edit `CMakeLists.txt`:

```cmake
# Add test
add_test( NAME benchmark_my_test
          COMMAND z80_benchmark
                  tests/my_test.bin
                  -i 5000000
                  -s )

# Set properties
set_tests_properties( benchmark_my_test PROPERTIES
    TIMEOUT 15
    LABELS "benchmark;synthetic;quick"
    PASS_REGULAR_EXPRESSION "MIPS"
)
```

Then reconfigure and build:
```bash
cd build
cmake ..
make
ctest -R benchmark_my_test -V
```

## Troubleshooting

### Test Not Found
```bash
# Reconfigure CMake
cd build
cmake ..

# Verify test exists
ctest --show-only | grep my_test
```

### Test Times Out
```bash
# Increase timeout
set_tests_properties( my_test PROPERTIES TIMEOUT 60 )

# Or run with longer timeout
ctest -R my_test --timeout 60
```

### Test Fails
```bash
# Run with verbose output
ctest -R my_test -V

# Run with extra verbose output
ctest -R my_test -VV

# Run with output on failure
ctest -R my_test --output-on-failure
```

## Summary

All benchmark functionality is now available through CTest:
- ✅ Better integration with build system
- ✅ Cross-platform support
- ✅ Parallel test execution
- ✅ Flexible test selection
- ✅ CI/CD friendly
- ✅ Standardized output format

Use `ctest` commands instead of shell scripts for all benchmark testing!

---

**Last Updated**: December 7, 2024  
**Status**: ✅ Complete

