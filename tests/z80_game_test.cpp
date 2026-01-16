// Z80 Game Benchmark Test Suite
// Runs real Spectrum games (extracted from TAP files) as benchmarks

#include "benchmark_shared.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

// Helper to read a TAP block
std::vector<uint8_t> read_tap_block(std::ifstream& tap_file) {
    uint16_t length = 0;
    if (!tap_file.read(reinterpret_cast<char*>(&length), 2)) {
        return {};
    }

    std::vector<uint8_t> data(length);
    if (!tap_file.read(reinterpret_cast<char*>(data.data()), length)) {
        return {};
    }

    return data;
}

// Load the main code block from a TAP file
// Heuristic: Returns the largest code block found
bool load_game_from_tap(const std::string& tap_path, BenchmarkConfig& config) {
    std::ifstream tap_file(tap_path, std::ios::binary);
    if (!tap_file.is_open()) {
        std::cerr << "Error: File not found: " << tap_path << '\n';
        return false;
    }

    config.name = fs::path(tap_path).stem().string();
    config.code.clear();
    config.load_address = 0;
    config.is_cpm_program = false;

    size_t max_code_size = 0;
    bool found_code = false;

    while (true) {
        auto block = read_tap_block(tap_file);
        if (block.empty()) {
            break;
        }

        if (block.size() < 2) {
            continue;
        }

        uint8_t flag = block[0];

        // Header block
        if (flag == 0x00 && block.size() >= 19) {
            uint8_t block_type = block[1];
            // uint16_t data_length = *reinterpret_cast<uint16_t*>(&block[12]);
            uint16_t param1 = *reinterpret_cast<uint16_t*>(&block[14]); // Load address for code

            if (block_type == 0x03) { // Code
                // Read the data block immediately following
                auto data_block = read_tap_block(tap_file);
                if (data_block.empty()) {
                    break;
                }

                if (data_block.size() > 2 && data_block[0] == 0xFF) {
                    // Remove flag and checksum
                    size_t actual_size = data_block.size() - 2;

                    // Heuristic: Keep the largest code block
                    if (actual_size > max_code_size) {
                        max_code_size = actual_size;
                        config.code.assign(data_block.begin() + 1, data_block.end() - 1);
                        config.load_address = param1;
                        found_code = true;
                    }
                }
            }
        }
    }

    return found_code;
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "========================================" << '\n';
        std::cout << "Z80 Game Benchmark Test Suite" << '\n';
        std::cout << "========================================" << '\n';
        std::cout << '\n';

        // Dynamically find all TAP files in the roms directory
        std::vector<std::string> tap_files;
        const std::string roms_dir = "roms";

        if (fs::exists(roms_dir) && fs::is_directory(roms_dir)) {
            for (const auto& entry : fs::directory_iterator(roms_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".tap") {
                    tap_files.push_back(entry.path().string());
                }
            }
            std::sort(tap_files.begin(), tap_files.end());
        } else {
            std::cerr << "Warning: roms directory not found\n";
        }

        std::vector<BenchmarkResult> results;
        int passed = 0;
        int failed = 0;

        for (const auto& tap_file : tap_files) {
            BenchmarkConfig config;
            // Set default expectations
            config.instructions = 5000000;   // Run for 5M instructions
            config.expected_min_mips = 5.0;  // Expect at least 5 MIPS (tolerant minimum)

            if (load_game_from_tap(tap_file, config)) {
                BenchmarkResult result = runBenchmark(config);
                results.push_back(result);

                if (result.passed) {
                    passed++;
                } else {
                    failed++;
                }
            } else {
                std::cout << "Skipping " << tap_file << " (not found or no code block)\n";
            }
            std::cout << '\n';
        }

        // Summary
        std::cout << "========================================" << '\n';
        std::cout << "Summary" << '\n';
        std::cout << "========================================" << '\n';
        std::cout << "Tests run: " << results.size() << '\n';
        std::cout << "Passed: " << passed << '\n';
        std::cout << "Failed: " << failed << '\n';
        std::cout << '\n';

        // Calculate average performance
        double total_mips = 0;
        int valid_results = 0;
        for (const auto& result : results) {
            if (result.passed) {
                total_mips += result.mips;
                valid_results++;
            }
        }

        if (valid_results > 0) {
            double avg_mips = total_mips / valid_results;
            std::cout << "Average Performance: " << std::fixed << std::setprecision(2) << avg_mips << " MIPS" << '\n';
        }

        return (failed == 0 && valid_results > 0) ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
