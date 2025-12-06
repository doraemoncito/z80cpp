# Performance Testing Suite

Comprehensive performance testing tools for Z80 emulator benchmarking.

## Quick Start

```bash
# Generate synthetic tests
python3 generate_test_programs.py spectrum-roms/synthetic

# Run all game benchmarks
./run_game_benchmarks.sh

# Compare results
./compare_benchmarks.py results/baseline.txt results/optimized.txt
```

## Tools

### 1. run_game_benchmarks.sh
Automated testing of all 6 ZX Spectrum games
- Saves timestamped results
- Tests: Ant Attack, Jet Set Willy, Pyjamarama, Arkanoid, Horace Skiing, Manic Miner

### 2. generate_test_programs.py
Creates synthetic test workloads:
- instruction_mix.bin - Balanced workload
- memory_intensive.bin - Memory stress
- arithmetic_heavy.bin - CPU intensive
- branch_heavy.bin - Branch prediction

### 3. compare_benchmarks.py
Compares benchmark results to measure optimization improvements

## Directory Structure

```
tests/
├── README.md                       # This file
├── run_game_benchmarks.sh         # Automated game testing
├── compare_benchmarks.py          # Result comparison
├── generate_test_programs.py     # Test generator
├── results/                       # Benchmark outputs
└── spectrum-roms/
    ├── games/                     # Real ZX Spectrum games
    │   ├── *.tap                  # Original TAP files
    │   └── *.bin                  # Extracted machine code
    └── synthetic/                 # Generated tests
```

## Complete Documentation

For full documentation including:
- Building and optimization
- Detailed benchmark results
- API usage
- Contributing guidelines

See the main [README.md](../README.md)

