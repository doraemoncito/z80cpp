# CTest Quick Reference Card

## Common Commands

### Run All Tests
```bash
cd build
ctest
```

### Run All Benchmarks
```bash
ctest -R "benchmark_"
```

### Run Quick Tests Only
```bash
ctest -L quick
```

### Run Game Benchmarks
```bash
ctest -L game
```

### Run Performance Tests
```bash
ctest -L performance
```

### Run Specific Test (Verbose)
```bash
ctest -R benchmark_instruction_mix -V
```

### Parallel Execution
```bash
ctest -j4
```

### Rerun Failed Tests
```bash
ctest --rerun-failed
```

### Stop on First Failure
```bash
ctest --stop-on-failure
```

## Test Labels

| Label | Tests | Description |
|-------|-------|-------------|
| `quick` | 4 | Fast synthetic benchmarks |
| `performance` | 11 | All performance tests |
| `benchmark` | 4 | Individual benchmarks |
| `synthetic` | 4 | Synthetic tests |
| `game` | 6 | Game code tests |

## Available Tests

### Correctness
- `correctness_zexall`

### Performance Suite
- `performance_benchmark_suite`

### Individual Benchmarks
- `benchmark_instruction_mix`
- `benchmark_memory_intensive`
- `benchmark_arithmetic_heavy`
- `benchmark_branch_heavy`

### Game Tests
- `performance_ant_attack`
- `performance_manic_miner`
- `performance_jet_set_willy`
- `performance_pyjamarama`
- `performance_arkanoid`
- `performance_horace_skiing`


## CI/CD Usage

```yaml
# GitHub Actions
- name: Run Tests
  run: |
    cd build
    ctest --output-on-failure -j4
```

## For More Information

See **CTEST_BENCHMARKS.md** for complete documentation.

