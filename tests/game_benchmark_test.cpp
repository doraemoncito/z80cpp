// Z80 Real Game Performance Test
// Tests a single game and validates performance

#include "z80sim.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <game_file> <instructions> <min_mips>" << std::endl;
        return 1;
    }

    std::string filename = argv[1];
    int64_t instructions = std::stoll(argv[2]);
    double min_mips = std::stod(argv[3]);

    // Extract game name from filename
    size_t last_slash = filename.find_last_of("/\\");
    std::string game_name = (last_slash != std::string::npos)
        ? filename.substr(last_slash + 1)
        : filename;

    std::cout << "Game: " << game_name << std::endl;
    std::cout << "Instructions: " << instructions << std::endl;

    // Load binary file
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot open file: " << filename << std::endl;
        return 1;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Create simulator and load program (raw Z80 code, not CP/M)
    Z80sim sim;
    sim.setCpmMode(false);  // Game ROMs are raw Z80 code, not CP/M programs
    file.read(reinterpret_cast<char*>(sim.getRam()), size);
    file.close();

    std::cout << "Loaded: " << size << " bytes" << std::endl;

    // Reset CPU
    sim.getCpu().reset();
    uint64_t instructionsExecuted = 0;

    // Run benchmark
    auto start = std::chrono::high_resolution_clock::now();

    while (instructionsExecuted < instructions && !sim.getCpu().isHalted()) {
        sim.getCpu().execute();
        instructionsExecuted++;
    }

    auto end = std::chrono::high_resolution_clock::now();

    // Calculate metrics
    std::chrono::duration<double> elapsed = end - start;
    double elapsed_seconds = elapsed.count();
    int64_t tstates = sim.getTstates();

    double mips = (instructions / 1000000.0) / elapsed_seconds;
    double mts_per_sec = (tstates / 1000000.0) / elapsed_seconds;
    double speedup = mts_per_sec / 3.5;  // ZX Spectrum = 3.5 MHz

    // Print results
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "========================================" << std::endl;
    std::cout << "Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Elapsed time: " << elapsed_seconds << " seconds" << std::endl;
    std::cout << "Performance: " << mips << " MIPS" << std::endl;
    std::cout << "T-states/sec: " << mts_per_sec << " million" << std::endl;
    std::cout << "Speedup: " << speedup << "x vs real hardware" << std::endl;
    std::cout << "Expected minimum: " << min_mips << " MIPS" << std::endl;

    // Pass/fail
    bool passed = mips >= min_mips;
    std::cout << "Status: " << (passed ? "PASS" : "FAIL") << std::endl;

    return passed ? 0 : 1;
}

