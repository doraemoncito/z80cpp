#!/usr/bin/env python3
"""
Compare benchmark results before and after optimizations
"""

import sys
import re
from pathlib import Path
from typing import Dict, List, Tuple

class BenchmarkResult:
    def __init__(self, name: str = ""):
        self.name = name
        self.elapsed_seconds = 0.0
        self.instructions = 0
        self.tstates = 0
        self.mips = 0.0
        self.tstates_per_sec = 0.0
        self.memory_reads = 0
        self.memory_writes = 0

    def __repr__(self):
        return f"BenchmarkResult({self.name}, {self.mips:.2f} MIPS)"

def parse_results_file(filename: str) -> List[BenchmarkResult]:
    """Parse benchmark results from file"""
    results = []
    current = BenchmarkResult()

    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()

            if line.startswith('# Benchmark run:'):
                if current.name:
                    results.append(current)
                current = BenchmarkResult(line)
            elif '=' in line:
                key, value = line.split('=', 1)
                key = key.strip()
                value = value.strip()

                try:
                    if key == 'elapsed_seconds':
                        current.elapsed_seconds = float(value)
                    elif key == 'instructions':
                        current.instructions = int(value)
                    elif key == 'tstates':
                        current.tstates = int(value)
                    elif key == 'mips':
                        current.mips = float(value)
                    elif key == 'tstates_per_sec':
                        current.tstates_per_sec = float(value)
                    elif key == 'memory_reads':
                        current.memory_reads = int(value)
                    elif key == 'memory_writes':
                        current.memory_writes = int(value)
                except ValueError:
                    pass

    if current.name:
        results.append(current)

    return results

def average_results(results: List[BenchmarkResult]) -> BenchmarkResult:
    """Calculate average of multiple benchmark runs"""
    if not results:
        return BenchmarkResult()

    avg = BenchmarkResult("Average")
    n = len(results)

    for r in results:
        avg.elapsed_seconds += r.elapsed_seconds
        avg.instructions += r.instructions
        avg.tstates += r.tstates
        avg.mips += r.mips
        avg.tstates_per_sec += r.tstates_per_sec
        avg.memory_reads += r.memory_reads
        avg.memory_writes += r.memory_writes

    avg.elapsed_seconds /= n
    avg.instructions //= n
    avg.tstates //= n
    avg.mips /= n
    avg.tstates_per_sec /= n
    avg.memory_reads //= n
    avg.memory_writes //= n

    return avg

def compare_results(baseline: BenchmarkResult, optimized: BenchmarkResult, test_name: str = ""):
    """Compare two benchmark results and print comparison"""

    print(f"\n{'='*70}")
    if test_name:
        print(f"Comparison: {test_name}")
    else:
        print("Benchmark Comparison")
    print(f"{'='*70}\n")

    print(f"{'Metric':<30} {'Baseline':>15} {'Optimized':>15} {'Change':>10}")
    print(f"{'-'*70}")

    # Elapsed time
    time_change = ((baseline.elapsed_seconds - optimized.elapsed_seconds) / baseline.elapsed_seconds) * 100
    symbol = '✓' if time_change > 0 else '✗'
    print(f"{'Elapsed time (s)':<30} {baseline.elapsed_seconds:>15.3f} {optimized.elapsed_seconds:>15.3f} {time_change:>+9.1f}% {symbol}")

    # MIPS
    mips_change = ((optimized.mips - baseline.mips) / baseline.mips) * 100
    symbol = '✓' if mips_change > 0 else '✗'
    print(f"{'Performance (MIPS)':<30} {baseline.mips:>15.2f} {optimized.mips:>15.2f} {mips_change:>+9.1f}% {symbol}")

    # Speedup factor
    speedup = optimized.mips / baseline.mips if baseline.mips > 0 else 0
    print(f"{'Speedup factor':<30} {'':<15} {speedup:>15.2f}x")

    # T-states per second
    tstates_change = ((optimized.tstates_per_sec - baseline.tstates_per_sec) / baseline.tstates_per_sec) * 100
    symbol = '✓' if tstates_change > 0 else '✗'
    print(f"{'T-states/sec (M)':<30} {baseline.tstates_per_sec:>15.2f} {optimized.tstates_per_sec:>15.2f} {tstates_change:>+9.1f}% {symbol}")

    # Memory operations
    if baseline.memory_reads > 0:
        mem_read_change = ((optimized.memory_reads - baseline.memory_reads) / baseline.memory_reads) * 100
        print(f"{'Memory reads':<30} {baseline.memory_reads:>15,} {optimized.memory_reads:>15,} {mem_read_change:>+9.1f}%")

    if baseline.memory_writes > 0:
        mem_write_change = ((optimized.memory_writes - baseline.memory_writes) / baseline.memory_writes) * 100
        print(f"{'Memory writes':<30} {baseline.memory_writes:>15,} {optimized.memory_writes:>15,} {mem_write_change:>+9.1f}%")

    print(f"{'='*70}\n")

    # Summary
    if mips_change >= 100:
        grade = "EXCELLENT"
        emoji = "🚀🚀🚀"
    elif mips_change >= 50:
        grade = "VERY GOOD"
        emoji = "🚀🚀"
    elif mips_change >= 20:
        grade = "GOOD"
        emoji = "🚀"
    elif mips_change >= 5:
        grade = "MODERATE"
        emoji = "✓"
    elif mips_change >= 0:
        grade = "MINOR"
        emoji = "~"
    else:
        grade = "REGRESSION"
        emoji = "⚠️"

    print(f"Overall improvement: {mips_change:+.1f}% - {grade} {emoji}")
    print(f"The optimized version is {speedup:.2f}x faster than baseline\n")

def print_usage():
    print("Benchmark Comparison Tool")
    print("\nUsage:")
    print("  compare_benchmarks.py <baseline_file> <optimized_file>")
    print("\nExample:")
    print("  compare_benchmarks.py baseline.txt optimized.txt")
    print()

def main():
    if len(sys.argv) != 3:
        print_usage()
        sys.exit(1)

    baseline_file = sys.argv[1]
    optimized_file = sys.argv[2]

    # Check files exist
    if not Path(baseline_file).exists():
        print(f"Error: Baseline file '{baseline_file}' not found")
        sys.exit(1)

    if not Path(optimized_file).exists():
        print(f"Error: Optimized file '{optimized_file}' not found")
        sys.exit(1)

    # Parse results
    print(f"Loading baseline results from: {baseline_file}")
    baseline_results = parse_results_file(baseline_file)

    print(f"Loading optimized results from: {optimized_file}")
    optimized_results = parse_results_file(optimized_file)

    if not baseline_results:
        print("Error: No results found in baseline file")
        sys.exit(1)

    if not optimized_results:
        print("Error: No results found in optimized file")
        sys.exit(1)

    print(f"Found {len(baseline_results)} baseline result(s)")
    print(f"Found {len(optimized_results)} optimized result(s)")

    # Calculate averages
    baseline_avg = average_results(baseline_results)
    optimized_avg = average_results(optimized_results)

    # Compare
    compare_results(baseline_avg, optimized_avg)

    # Recommendation
    speedup = optimized_avg.mips / baseline_avg.mips

    print("\nRecommendations:")
    print("-" * 70)

    if speedup >= 2.0:
        print("✓ Excellent optimization! The improvements are significant.")
        print("✓ Consider documenting what optimizations were applied.")
    elif speedup >= 1.5:
        print("✓ Good optimization. Consider profiling to find more opportunities.")
    elif speedup >= 1.1:
        print("~ Moderate improvement. Profile to find remaining hotspots.")
    elif speedup >= 1.0:
        print("~ Minor improvement. More aggressive optimizations may be needed.")
    else:
        print("⚠ Performance regression detected!")
        print("  Review recent changes and verify correctness.")
        print("  Consider reverting or investigating the cause.")

    print()

if __name__ == '__main__':
    main()

