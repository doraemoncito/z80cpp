// ZX Spectrum Performance Benchmark Tool
// Simple wrapper around z80sim to measure performance

#include "../example/z80sim.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

// Simple benchmark results structure
struct BenchmarkResults {
    double elapsed_seconds;
    uint64_t total_instructions;
    uint64_t total_tstates;
    double instructions_per_second;
    double tstates_per_second;
    double mips;
};

// Benchmark-enabled simulator (can access protected members)
class Z80BenchSim : public Z80sim {
public:
    // Expose protected members through public methods
    Z80<Z80sim>& getCPU() { return cpu; }
    uint64_t getTStates() const { return tstates; }
    uint8_t* getRAM() { return z80Ram; }

    // Initialize for benchmark
    void setupBenchmark() {
        tstates = 0;
        finish = false;
    }
};

// Benchmark runner
class Z80BenchmarkRunner {
private:
    Z80BenchSim emulator;
    uint64_t instruction_count = 0;
    uint64_t max_instructions = 0;
    bool silent_mode = false;

public:
    void setMaxInstructions(uint64_t max) { max_instructions = max; }
    void setSilentMode(bool silent) { silent_mode = silent; }

    // Load binary into memory
    bool loadBinary(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open " << filename << "\n";
            return false;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        if (size > 65536) {
            std::cerr << "Error: File too large (max 64KB)\n";
            return false;
        }

        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            std::cerr << "Error: Could not read file\n";
            return false;
        }

        if (!silent_mode) {
            std::cout << "Loaded " << size << " bytes from " << filename << "\n";
        }

        // Get direct access to RAM
        uint8_t* ram = emulator.getRAM();

        // Copy to memory at address 0x100 (CP/M TPA start)
        for (size_t i = 0; i < buffer.size() && i < 65536 - 0x100; i++) {
            ram[0x100 + i] = buffer[i];
        }

        // Set up CP/M environment
        ram[0] = 0xC3;  // JP instruction
        ram[1] = 0x00;
        ram[2] = 0x01;  // JP 0x0100
        ram[5] = 0xC9;  // RET at BDOS entry

        return true;
    }

    // Run benchmark
    BenchmarkResults run() {
        BenchmarkResults results = {};

        if (!silent_mode) {
            std::cout << "Starting benchmark...\n";
            if (max_instructions > 0) {
                std::cout << "  Target: " << max_instructions << " instructions\n";
            }
            std::cout << std::endl;
        }

        // Setup
        emulator.setupBenchmark();
        instruction_count = 0;

        // Get CPU reference
        Z80<Z80sim>& cpu = emulator.getCPU();

        // Start timing
        auto start_time = std::chrono::high_resolution_clock::now();

        // Run instructions
        bool running = true;
        while (running && instruction_count < max_instructions) {
            cpu.execute();
            instruction_count++;

            // Progress reporting
            if (!silent_mode && instruction_count % 10000000 == 0) {
                auto now = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
                double seconds = duration.count() / 1000.0;
                double mips = (instruction_count / 1000000.0) / seconds;
                std::cout << "  Progress: " << (instruction_count / 1000000)
                          << "M instructions in " << std::fixed << std::setprecision(2)
                          << seconds << "s (" << mips << " MIPS)\r" << std::flush;
            }

            // Check for halt
            if (cpu.isHalted()) {
                running = false;
            }
        }

        // End timing
        auto end_time = std::chrono::high_resolution_clock::now();

        if (!silent_mode) {
            std::cout << std::endl;
        }

        // Calculate results
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        results.elapsed_seconds = duration.count() / 1000000.0;
        results.total_instructions = instruction_count;
        results.total_tstates = emulator.getTStates();
        results.instructions_per_second = instruction_count / results.elapsed_seconds;
        results.tstates_per_second = results.total_tstates / results.elapsed_seconds;
        results.mips = results.instructions_per_second / 1000000.0;

        return results;
    }
};

// Print results
void printResults(const BenchmarkResults& results, const std::string& test_name = "") {
    std::cout << "\n";
    std::cout << "============================================================\n";
    if (!test_name.empty()) {
        std::cout << "Benchmark: " << test_name << "\n";
        std::cout << "============================================================\n";
    } else {
        std::cout << "Benchmark Results\n";
        std::cout << "============================================================\n";
    }

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Elapsed time:        " << results.elapsed_seconds << " seconds\n";
    std::cout << "Total instructions:  " << results.total_instructions << "\n";
    std::cout << "Total T-states:      " << results.total_tstates << "\n";
    std::cout << "\n";

    std::cout << std::setprecision(2);
    std::cout << "Performance:\n";
    std::cout << "  " << results.mips << " MIPS (million instructions/sec)\n";
    std::cout << "  " << (results.tstates_per_second / 1000000.0) << " million T-states/sec\n";

    // Compare to real ZX Spectrum (3.5 MHz)
    const double ZX_SPECTRUM_MHZ = 3.5;
    double speedup = (results.tstates_per_second / 1000000.0) / ZX_SPECTRUM_MHZ;
    std::cout << "  " << speedup << "x faster than real ZX Spectrum\n";
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << std::endl;
}

// Save results to file
void saveResults(const BenchmarkResults& results, const std::string& filename) {
    std::ofstream file(filename, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open " << filename << " for writing\n";
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    file << "# Benchmark run: " << std::ctime(&time);
    file << "elapsed_seconds=" << results.elapsed_seconds << "\n";
    file << "instructions=" << results.total_instructions << "\n";
    file << "tstates=" << results.total_tstates << "\n";
    file << "mips=" << results.mips << "\n";
    file << "tstates_per_sec=" << (results.tstates_per_second / 1000000.0) << "\n";
    file << "\n";

    file.close();
}

// Main benchmark program
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "ZX Spectrum Z80 Performance Benchmark Tool\n\n";
        std::cout << "Usage: " << argv[0] << " <binary_file> [options]\n\n";
        std::cout << "Options:\n";
        std::cout << "  -i <count>    Run for exactly <count> instructions\n";
        std::cout << "  -s            Silent mode (minimal output)\n";
        std::cout << "  -o <file>     Save results to file\n";
        std::cout << "\n";
        std::cout << "Examples:\n";
        std::cout << "  " << argv[0] << " zexall.bin -i 100000000\n";
        std::cout << "  " << argv[0] << " game.z80 -i 50000000 -o results.txt\n";
        std::cout << "\n";
        return 1;
    }

    // Parse arguments
    std::string binary_file = argv[1];
    uint64_t max_instructions = 1000000000;  // Default 1 billion
    bool silent = false;
    std::string output_file;

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-i" && i + 1 < argc) {
            max_instructions = std::stoull(argv[++i]);
        } else if (arg == "-s") {
            silent = true;
        } else if (arg == "-o" && i + 1 < argc) {
            output_file = argv[++i];
        }
    }

    // Create benchmark
    Z80BenchmarkRunner benchmark;
    benchmark.setMaxInstructions(max_instructions);
    benchmark.setSilentMode(silent);

    // Load binary
    if (!benchmark.loadBinary(binary_file)) {
        return 1;
    }

    // Run benchmark
    auto results = benchmark.run();

    // Print results
    printResults(results, binary_file);

    // Save to file if requested
    if (!output_file.empty()) {
        saveResults(results, output_file);
        if (!silent) {
            std::cout << "Results appended to " << output_file << "\n";
        }
    }

    return 0;
}

