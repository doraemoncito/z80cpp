// Z80 Game Benchmark Test Suite
// Runs real Spectrum games (extracted from TAP files) as benchmarks

#include "benchmark_shared.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <iomanip>
#include <dirent.h>
#include <sys/stat.h>

// Helper to get filename without extension (C++14 compatible)
std::string get_stem(const std::string& path) {
    size_t last_slash = path.find_last_of("/\\");
    size_t start = (last_slash == std::string::npos) ? 0 : last_slash + 1;
    
    size_t last_dot = path.find_last_of('.');
    size_t end = (last_dot == std::string::npos || last_dot < start) ? path.length() : last_dot;
    
    return path.substr(start, end - start);
}

// Helper to check if path ends with extension (C++14 compatible)
bool has_extension(const std::string& path, const std::string& ext) {
    if (path.length() < ext.length()) {
        return false;
    }
    return path.compare(path.length() - ext.length(), ext.length(), ext) == 0;
}

// Helper to read a TAP block
std::vector<uint8_t> read_tap_block(std::ifstream& tap_file) {
    uint16_t length = 0;
    if (!tap_file.read(reinterpret_cast<char*>(&length), 2)) {
        return {};
    }

    std::vector<uint8_t> data(length);
    if (!tap_file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(length))) {
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

    config.name = get_stem(tap_path);
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
            uint16_t param1 = *reinterpret_cast<uint16_t*>(&block[14]); // Load address for code
            std::memcpy(&param1, &block[14], sizeof(param1));

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
    (void)argc; // Suppress unused parameter warning
    (void)argv;
    
    try {
        std::cout << "========================================" << '\n';
        std::cout << "Z80 Game Benchmark Test Suite" << '\n';
        std::cout << "========================================" << '\n';
        std::cout << '\n';

        // Dynamically find all TAP files in the roms directory
        std::vector<std::string> tap_files;
        const std::string roms_dir = "roms";

        // C++14 compatible directory listing using POSIX APIs
        DIR* dir = opendir(roms_dir.c_str());
        if (dir != nullptr) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string filename = entry->d_name;
                if (has_extension(filename, ".tap")) {
                    tap_files.push_back(roms_dir + "/" + filename);
                }
            }
            closedir(dir);
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
            config.expected_min_mips = 0.4; // Expect at least 0.4 MIPS (debug build)

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
        std::cout << "✓ Tests passed: " << passed << '\n';
        std::cout << "✗ Tests failed: " << failed << '\n';
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
