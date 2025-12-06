// Z80 Performance Benchmark Test Suite
// Integrated C++ tests for CMake/CTest

#include "z80sim.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <cmath>

// Benchmark configuration
struct BenchmarkConfig {
    std::string name;
    std::string file;
    int64_t instructions;
    double expected_min_mips;  // Minimum acceptable MIPS
    bool is_cpm_program;       // true for CP/M (like ZEXALL), false for raw Z80
};

// Benchmark result
struct BenchmarkResult {
    std::string name;
    double elapsed_seconds;
    int64_t instructions;
    int64_t tstates;
    double mips;
    double mts_per_sec;
    double speedup;
    bool passed;
};

// Run a single benchmark test
BenchmarkResult runBenchmark(const BenchmarkConfig& config) {
    BenchmarkResult result;
    result.name = config.name;
    result.instructions = config.instructions;
    result.passed = false;

    std::cout << "Testing: " << config.name << std::endl;

    // Load binary file
    std::ifstream file(config.file, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "  ERROR: Cannot open file: " << config.file << std::endl;
        return result;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Create simulator and load program
    Z80sim sim;
    sim.setCpmMode(config.is_cpm_program);

    if (config.is_cpm_program) {
        // CP/M program (like ZEXALL) - load at 0x100
        file.read(reinterpret_cast<char*>(sim.getRam() + 0x100), size);
        file.close();

        // Set up CP/M environment
        sim.getRam()[0] = 0xC3;  // JP 0x100
        sim.getRam()[1] = 0x00;
        sim.getRam()[2] = 0x01;
        sim.getRam()[5] = 0xC9;  // RET at BDOS call address
    } else {
        // Raw Z80 program (synthetic tests) - load at 0x0000
        file.read(reinterpret_cast<char*>(sim.getRam()), size);
        file.close();
    }

    // Reset CPU
    sim.getCpu().reset();
    uint64_t instructionsExecuted = 0;

    // Run benchmark
    auto start = std::chrono::high_resolution_clock::now();

    while (instructionsExecuted < config.instructions && !sim.getCpu().isHalted()) {
        sim.getCpu().execute();
        instructionsExecuted++;
    }

    auto end = std::chrono::high_resolution_clock::now();

    // Calculate metrics
    std::chrono::duration<double> elapsed = end - start;
    result.elapsed_seconds = elapsed.count();
    result.tstates = sim.getTstates();

    if (result.elapsed_seconds > 0) {
        result.mips = (config.instructions / 1000000.0) / result.elapsed_seconds;
        result.mts_per_sec = (result.tstates / 1000000.0) / result.elapsed_seconds;
        result.speedup = result.mts_per_sec / 3.5;  // ZX Spectrum = 3.5 MHz
        result.passed = result.mips >= config.expected_min_mips;
    }

    // Print results
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Time: " << result.elapsed_seconds << "s" << std::endl;
    std::cout << "  Performance: " << result.mips << " MIPS" << std::endl;
    std::cout << "  Speedup: " << result.speedup << "x" << std::endl;
    std::cout << "  Status: " << (result.passed ? "PASS" : "FAIL") << std::endl;

    return result;
}

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "Z80 Performance Benchmark Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Define benchmark tests
    // Paths are relative to build directory where tests run
    // Min MIPS thresholds are conservative to avoid intermittent failures
    // is_cpm_program: true for CP/M programs (ZEXALL), false for raw Z80 code
    std::vector<BenchmarkConfig> benchmarks = {
        {"ZEXALL", "zexall.bin", 10000000, 100.0, true},  // CP/M program
        {"Instruction Mix", "tests/spectrum-roms/synthetic/instruction_mix.bin", 5000000, 100.0, false},  // Raw Z80
        {"Memory Intensive", "tests/spectrum-roms/synthetic/memory_intensive.bin", 2000000, 80.0, false},  // Raw Z80
        {"Arithmetic Heavy", "tests/spectrum-roms/synthetic/arithmetic_heavy.bin", 5000000, 90.0, false},  // Raw Z80
        {"Branch Heavy", "tests/spectrum-roms/synthetic/branch_heavy.bin", 3000000, 90.0, false},  // Raw Z80
    };

    // Run benchmarks
    std::vector<BenchmarkResult> results;
    int passed = 0;
    int failed = 0;

    for (const auto& config : benchmarks) {
        BenchmarkResult result = runBenchmark(config);
        results.push_back(result);

        if (result.passed) {
            passed++;
        } else {
            failed++;
        }

        std::cout << std::endl;
    }

    // Summary
    std::cout << "========================================" << std::endl;
    std::cout << "Summary" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Tests run: " << results.size() << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << std::endl;

    // Calculate average performance
    double total_mips = 0;
    int valid_results = 0;
    for (const auto& r : results) {
        if (r.passed) {
            total_mips += r.mips;
            valid_results++;
        }
    }

    if (valid_results > 0) {
        double avg_mips = total_mips / valid_results;
        std::cout << "Average Performance: " << std::fixed << std::setprecision(2)
                  << avg_mips << " MIPS" << std::endl;
    }

    return (failed == 0) ? 0 : 1;
}

