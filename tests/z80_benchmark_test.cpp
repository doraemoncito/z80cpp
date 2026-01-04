// Z80 Performance Benchmark Test Suite
// Integrated C++ tests for CMake/CTest

#include "benchmark_shared.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

// --- Test Generation Helpers ---

// Helper function to append a byte
void push_byte(std::vector<uint8_t>& code, uint8_t byte) {
    code.push_back(byte);
}

// Helper function to append a list of bytes
void push_bytes(std::vector<uint8_t>& code, std::initializer_list<uint8_t> bytes) {
    code.insert(code.end(), bytes);
}

// Helper function to append a 16-bit word (little-endian)
void push_word(std::vector<uint8_t>& code, uint16_t word) {
    code.push_back(word & 0xFF);
    code.push_back((word >> 8) & 0xFF);
}

std::vector<uint8_t> generate_instruction_mix_test(int iterations = 10000) {
    std::vector<uint8_t> code;

    // LD BC, iterations
    push_byte(code, 0x01); // LD BC,nn
    push_word(code, static_cast<uint16_t>(iterations));

    size_t loop_start = code.size();

    // Arithmetic operations
    push_bytes(code, {0x3E, 0x42}); // LD A, 0x42
    push_bytes(code, {0xC6, 0x10}); // ADD A, 0x10
    push_bytes(code, {0xD6, 0x08}); // SUB 0x08
    push_bytes(code, {0xE6, 0x0F}); // AND 0x0F
    push_bytes(code, {0xF6, 0x20}); // OR 0x20
    push_bytes(code, {0xEE, 0x55}); // XOR 0x55

    // Register operations
    push_byte(code, 0x47); // LD B, A
    push_byte(code, 0x4F); // LD C, A
    push_byte(code, 0x57); // LD D, A
    push_byte(code, 0x5F); // LD E, A
    push_byte(code, 0x67); // LD H, A
    push_byte(code, 0x6F); // LD L, A

    // 16-bit operations
    push_bytes(code, {0x21, 0x00, 0x80}); // LD HL, 0x8000
    push_byte(code, 0x23);                // INC HL
    push_byte(code, 0x2B);                // DEC HL
    push_byte(code, 0x09);                // ADD HL, BC

    // Memory operations
    push_byte(code, 0x77); // LD (HL), A
    push_byte(code, 0x7E); // LD A, (HL)

    // Rotations
    push_byte(code, 0x07); // RLCA
    push_byte(code, 0x0F); // RRCA
    push_byte(code, 0x17); // RLA
    push_byte(code, 0x1F); // RRA

    // Stack operations
    push_byte(code, 0xC5); // PUSH BC
    push_byte(code, 0xD5); // PUSH DE
    push_byte(code, 0xE5); // PUSH HL
    push_byte(code, 0xE1); // POP HL
    push_byte(code, 0xD1); // POP DE
    push_byte(code, 0xC1); // POP BC

    // Loop control
    push_byte(code, 0x0B); // DEC BC
    push_byte(code, 0x78); // LD A, B
    push_byte(code, 0xB1); // OR C
    int offset = 256 - static_cast<int>(code.size() - loop_start + 2);
    push_bytes(code, {0x20, static_cast<uint8_t>(offset & 0xFF)}); // JR NZ, loop_start

    // HALT
    push_byte(code, 0x76);

    return code;
}

std::vector<uint8_t> generate_memory_intensive_test(int size = 1000) {
    std::vector<uint8_t> code;

    // Initialize: LD HL, buffer_start
    push_bytes(code, {0x21, 0x00, 0x80}); // LD HL, 0x8000

    // LD BC, size
    push_byte(code, 0x01);
    push_word(code, static_cast<uint16_t>(size));

    // Fill loop
    size_t fill_start = code.size();
    push_bytes(code, {0x3E, 0x55}); // LD A, 0x55
    push_byte(code, 0x77);          // LD (HL), A
    push_byte(code, 0x23);          // INC HL
    push_byte(code, 0x0B);          // DEC BC
    push_byte(code, 0x78);          // LD A, B
    push_byte(code, 0xB1);          // OR C
    int offset = 256 - static_cast<int>(code.size() - fill_start + 2);
    push_bytes(code, {0x20, static_cast<uint8_t>(offset & 0xFF)}); // JR NZ, fill_start

    // Reset pointer
    push_bytes(code, {0x21, 0x00, 0x80}); // LD HL, 0x8000
    push_byte(code, 0x01);
    push_word(code, static_cast<uint16_t>(size));

    // Read loop
    size_t read_start = code.size();
    push_byte(code, 0x7E);          // LD A, (HL)
    push_bytes(code, {0xC6, 0x01}); // ADD A, 1
    push_byte(code, 0x77);          // LD (HL), A
    push_byte(code, 0x23);          // INC HL
    push_byte(code, 0x0B);          // DEC BC
    push_byte(code, 0x78);          // LD A, B
    push_byte(code, 0xB1);          // OR C
    offset = 256 - static_cast<int>(code.size() - read_start + 2);
    push_bytes(code, {0x20, static_cast<uint8_t>(offset & 0xFF)}); // JR NZ, read_start

    // HALT
    push_byte(code, 0x76);

    return code;
}

std::vector<uint8_t> generate_arithmetic_test(int iterations = 50000) {
    std::vector<uint8_t> code;

    // LD BC, iterations
    push_byte(code, 0x01);
    push_word(code, static_cast<uint16_t>(iterations));

    size_t loop_start = code.size();

    // Complex arithmetic sequence
    push_bytes(code, {0x3E, 0x01}); // LD A, 1
    for (int i = 0; i < 10; ++i) {
        push_byte(code, 0xC6); // ADD A, n
        push_byte(code, static_cast<uint8_t>(i + 1));
        push_byte(code, 0x47); // LD B, A
        push_byte(code, 0x4F); // LD C, A
        push_byte(code, 0x80); // ADD A, B
        push_byte(code, 0x81); // ADD A, C
    }

    // Loop control
    push_byte(code, 0x0B); // DEC BC
    push_byte(code, 0x78); // LD A, B
    push_byte(code, 0xB1); // OR C
    int offset = 256 - static_cast<int>(code.size() - loop_start + 2);
    push_bytes(code, {0x20, static_cast<uint8_t>(offset & 0xFF)}); // JR NZ, loop_start

    push_byte(code, 0x76); // HALT

    return code;
}

std::vector<uint8_t> generate_jump_test(int iterations = 10000) {
    std::vector<uint8_t> code;

    // LD BC, iterations (must fit in 16-bit: max 65535)
    iterations = std::min(iterations, 65535);
    push_byte(code, 0x01);
    push_word(code, static_cast<uint16_t>(iterations));

    size_t loop_start = code.size();

    // Create multiple jump targets
    push_bytes(code, {0x3E, 0x00}); // LD A, 0

    // Conditional jumps
    push_byte(code, 0xA7);          // AND A
    push_bytes(code, {0x28, 0x02}); // JR Z, +2
    push_bytes(code, {0x3E, 0xFF}); // LD A, 0xFF

    push_bytes(code, {0x3E, 0x01}); // LD A, 1
    push_bytes(code, {0xFE, 0x01}); // CP 1
    push_bytes(code, {0x20, 0x02}); // JR NZ, +2
    push_bytes(code, {0x3E, 0x02}); // LD A, 2

    // Unconditional jump
    push_bytes(code, {0x18, 0x02}); // JR +2
    push_bytes(code, {0x00, 0x00}); // NOP NOP (skipped)

    // Loop control
    push_byte(code, 0x0B); // DEC BC
    push_byte(code, 0x78); // LD A, B
    push_byte(code, 0xB1); // OR C
    int offset = 256 - static_cast<int>(code.size() - loop_start + 2);
    push_bytes(code, {0x20, static_cast<uint8_t>(offset & 0xFF)}); // JR NZ, loop_start

    push_byte(code, 0x76); // HALT

    return code;
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "========================================" << '\n';
        std::cout << "Z80 Performance Benchmark Test Suite" << '\n';
        std::cout << "========================================" << '\n';
        std::cout << '\n';

        // Define benchmark tests
        // Paths are relative to build directory where tests run
        // Min MIPS thresholds are conservative to avoid intermittent failures
        // is_cpm_program: true for CP/M programs (ZEXALL), false for raw Z80 code
        std::vector<BenchmarkConfig> benchmarks = {
            {"ZEXALL", "zexall.bin", {}, 10000000, 0.1, true},                                  // CP/M program
            {"Instruction Mix", "", generate_instruction_mix_test(10000), 5000000, 0.1, false}, // Raw Z80
            {"Memory Intensive", "", generate_memory_intensive_test(1000), 2000000, 0.1, false}, // Raw Z80
            {"Arithmetic Heavy", "", generate_arithmetic_test(50000), 5000000, 0.1, false},      // Raw Z80
            {"Branch Heavy", "", generate_jump_test(10000), 3000000, 0.1, false},                // Raw Z80
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

        return (failed == 0) ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
