#pragma once

#include "../include/z80.h"
#include "../include/z80operations.h"
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
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

class BenchmarkSim : public Z80operations {
  public:
    std::unique_ptr<Z80> cpu;
    std::array<uint8_t, 0x10000> ram{};
    uint64_t tstates{0};
    bool cpmMode{false};
    volatile bool finished{false};

    BenchmarkSim() : cpu(std::make_unique<Z80>(this)) {
    }

    // Z80operations interface implementation
    uint8_t fetchOpcode(uint16_t address) override {
        tstates += 4;
        return ram[address];
    }

    uint8_t peek8(uint16_t address) override {
        tstates += 3;
        return ram[address];
    }

    void poke8(uint16_t address, uint8_t value) override {
        tstates += 3;
        ram[address] = value;
    }

    uint16_t peek16(uint16_t address) override {
        tstates += 6;
        return ram[address] | (ram[(address + 1) & 0xFFFF] << 8);
    }

    void poke16(uint16_t address, RegisterPair word) override {
        tstates += 6;
        ram[address] = word.byte8.lo;
        ram[(address + 1) & 0xFFFF] = word.byte8.hi;
    }

    uint8_t inPort(uint16_t port) override {
        tstates += 4;
        return 0xFF;
    }

    void outPort(uint16_t port, uint8_t value) override {
        tstates += 4;
    }

    void addressOnBus(uint16_t address, int32_t wstates) override {
        tstates += wstates;
    }

    void interruptHandlingTime(int32_t wstates) override {
        tstates += wstates;
    }

    bool isActiveINT() override {
        return false;
    }

#ifdef WITH_BREAKPOINT_SUPPORT
    uint8_t breakpoint(uint16_t address, uint8_t opcode) override {
        // CP/M BDOS call at address 0x0005
        if (cpmMode && address == 0x0005) {
            switch (cpu->getRegC()) {
                case 0: // System Reset
                    finished = true;
                    break;
            }
        }
        return opcode;
    }
#endif

    // Helper methods
    std::array<uint8_t, 0x10000>& getRam() {
        return ram;
    }

    Z80* getCpu() {
        return cpu.get();
    }

    [[nodiscard]] uint64_t getTstates() const {
        return tstates;
    }

    void setCpmMode(bool mode) {
        cpmMode = mode;
    }

    void reset() {
        cpu->reset();
        tstates = 0;
        finished = false;
    }

    void execute() {
        cpu->execute();
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
            sim.getRam()[5] = 0xC9; // RET
        } else {
            // Raw Z80 program
            std::streamsize max_size = 0x10000 - config.load_address;
            size = std::min(size, max_size);
            file.read(reinterpret_cast<char*>(&sim.getRam()[config.load_address]), size);
            file.close();
        }
    } else {
        std::cerr << "  ERROR: No code or file specified" << '\n';
        return result;
    }

    // Run the benchmark
    auto start = std::chrono::high_resolution_clock::now();
    sim.reset();

    // Execute until we hit a terminating condition
    // For CP/M programs, this is a BDOS 0 call
    // For raw programs, we need a different termination method
    int max_instructions = 1000000000; // Safety limit
    int executed = 0;
    while (executed < max_instructions && !sim.finished) {
        sim.execute();
        executed++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start).count();

    result.elapsed_seconds = elapsed;
    result.tstates = static_cast<int64_t>(sim.getTstates());
    
    if (result.instructions > 0) {
        result.mips = result.instructions / (elapsed * 1e6);
    }
    if (elapsed > 0) {
        result.mts_per_sec = result.tstates / (elapsed * 1e6);
    }

    result.passed = (result.mips >= config.expected_min_mips || config.expected_min_mips == 0.0);

    std::cout << "  Time: " << std::fixed << std::setprecision(3) << elapsed << " sec\n";
    std::cout << "  T-States: " << result.tstates << " (" << result.mts_per_sec << " MT/s)\n";
    std::cout << "  Speedup: " << result.speedup << "%" << '\n';

    if (result.instructions > 0) {
        std::cout << "  MIPS: " << result.mips << "\n";
    }
    std::cout << "  Result: " << (result.passed ? "Passed" : "Failed") << "\n\n";

    return result;
}
