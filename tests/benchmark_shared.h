#pragma once

#include "../include/z80.h"
#include "../include/z80_bus_interface.h"
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// Benchmark configuration
struct BenchmarkConfig {
    std::string name;
    std::string file;          // For file-based tests (e.g. ZEXALL)
    std::vector<uint8_t> code; // For in-memory tests
    int64_t instructions = 0;
    double expected_min_mips = 0.0; // Minimum acceptable MIPS
    bool is_cpm_program = false;    // true for CP/M (like ZEXALL), false for raw Z80
    uint16_t load_address = 0;      // Load address for raw Z80 programs
};

// Benchmark result
struct BenchmarkResult {
    std::string name;
    double elapsed_seconds{};
    int64_t instructions{};
    int64_t tstates{};
    double mips{};
    double mts_per_sec{};
    double speedup{};
    bool passed{};
};

class BenchmarkSim : public Z80BusInterface<BenchmarkSim> {
  public:
    Z80<BenchmarkSim> cpu;
    std::array<uint8_t, 0x10000> ram{};
    uint64_t tstates{0};
    bool cpmMode{false};

    BenchmarkSim() : cpu(*this) {
    }

    // Bus Interface Implementation
    uint8_t fetchOpcodeImpl(uint16_t address) {
        tstates += 4;
        return ram[address];
    }

    uint8_t peek8Impl(uint16_t address) {
        tstates += 3;
        return ram[address];
    }

    void poke8Impl(uint16_t address, uint8_t value) {
        tstates += 3;
        ram[address] = value;
    }

    uint16_t peek16Impl(uint16_t address) {
        tstates += 6;
        return ram[address] | (ram[(address + 1) & 0xFFFF] << 8);
    }

    void poke16Impl(uint16_t address, RegisterPair word) {
        tstates += 6;
        ram[address] = word.byte8.lo;
        ram[(address + 1) & 0xFFFF] = word.byte8.hi;
    }

    uint8_t inPortImpl(uint16_t port) {
        tstates += 4;
        return 0xFF;
    }

    void outPortImpl(uint16_t port, uint8_t value) {
        tstates += 4;
    }

    void addressOnBusImpl(uint16_t address, int32_t wstates) {
        tstates += wstates;
    }

    void interruptHandlingTimeImpl(int32_t wstates) {
        tstates += wstates;
    }

    static bool isActiveINTImpl() {
        return false;
    }

    // Helper methods
    std::array<uint8_t, 0x10000>& getRam() {
        return ram;
    }

    Z80<BenchmarkSim>& getCpu() {
        return cpu;
    }

    [[nodiscard]] uint64_t getTstates() const {
        return tstates;
    }

    void setCpmMode(bool mode) {
        cpmMode = mode;
    }
};

// Run a single benchmark test
inline BenchmarkResult runBenchmark(const BenchmarkConfig& config) {
    BenchmarkResult result;
    result.name = config.name;
    result.instructions = config.instructions;
    result.passed = false;

    std::cout << "Testing: " << config.name << '\n';

    // Create simulator
    BenchmarkSim sim;
    sim.setCpmMode(config.is_cpm_program);

    if (!config.code.empty()) {
        // Load from memory
        if (config.code.size() + config.load_address > 0x10000) {
            std::cerr << "  ERROR: Code too large for RAM" << '\n';
            return result;
        }
        std::copy(config.code.begin(), config.code.end(), std::next(sim.getRam().begin(), config.load_address));
    } else if (!config.file.empty()) {
        // Load binary file
        std::ifstream file(config.file, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "  ERROR: Cannot open file: " << config.file << '\n';
            return result;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        if (config.is_cpm_program) {
            // CP/M program (like ZEXALL) - load at 0x100
            size = std::min<std::streamsize>(size, 0x10000 - 0x100);
            file.read(reinterpret_cast<char*>(&sim.getRam()[0x100]), size);
            file.close();

            // Set up CP/M environment
            sim.getRam()[0] = 0xC3; // JP 0x100
            sim.getRam()[1] = 0x00;
            sim.getRam()[2] = 0x01;
            sim.getRam()[5] = 0xC9; // RET at BDOS call address
        } else {
            // Raw Z80 program (synthetic tests) - load at 0x0000
            size = std::min<std::streamsize>(size, 0x10000);
            file.read(reinterpret_cast<char*>(sim.getRam().data()), size);
            file.close();
        }
    } else {
        std::cerr << "  ERROR: No code or file specified" << '\n';
        return result;
    }

    // Reset CPU
    sim.getCpu().reset();

    // If raw Z80 program has a custom load address, we might need to set PC
    // But for now, synthetic tests assume 0x0000 or handle it.
    // For game tests, we might need to set PC to the entry point.
    if (!config.is_cpm_program && config.load_address != 0) {
        sim.getCpu().setRegPC(config.load_address);
    }

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
    result.tstates = static_cast<int64_t>(sim.getTstates());

    if (result.elapsed_seconds > 0) {
        result.mips = (static_cast<double>(config.instructions) / 1000000.0) / result.elapsed_seconds;
        result.mts_per_sec = (static_cast<double>(result.tstates) / 1000000.0) / result.elapsed_seconds;
        result.speedup = result.mts_per_sec / 3.5; // ZX Spectrum = 3.5 MHz
        result.passed = result.mips >= config.expected_min_mips;
    }

    // Print results
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Time: " << std::fixed << std::setprecision(3) << result.elapsed_seconds << " sec\n";
    std::cout << "  T-States: " << result.tstates << " (" << result.mts_per_sec << " MT/s)\n";
    std::cout << "  Speedup: " << result.speedup << "%" << '\n';
    if (result.instructions > 0) {
        std::cout << "  MIPS: " << result.mips << "\n";
    }
    std::cout << "  Result: " << (result.passed ? "Passed" : "Failed") << "\n\n";

    return result;
}
