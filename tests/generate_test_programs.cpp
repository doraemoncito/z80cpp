#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

// Helper function to append a byte
void push_byte(std::vector<uint8_t> &code, uint8_t byte) {
  code.push_back(byte);
}

// Helper function to append a list of bytes
void push_bytes(std::vector<uint8_t> &code,
                std::initializer_list<uint8_t> bytes) {
  code.insert(code.end(), bytes);
}

// Helper function to append a 16-bit word (little-endian)
void push_word(std::vector<uint8_t> &code, uint16_t word) {
  code.push_back(word & 0xFF);
  code.push_back((word >> 8) & 0xFF);
}

void create_instruction_mix_test(const std::string &filename,
                                 int iterations = 10000) {
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
  int offset = 256 - (code.size() - loop_start + 2);
  push_bytes(code,
             {0x20, static_cast<uint8_t>(offset & 0xFF)}); // JR NZ, loop_start

  // HALT
  push_byte(code, 0x76);

  std::ofstream f(filename, std::ios::binary);
  f.write(reinterpret_cast<const char *>(code.data()), code.size());
  f.close();

  std::cout << "Created " << filename << std::endl;
  std::cout << "  Iterations: " << iterations << std::endl;
  std::cout << "  Instructions per iteration: ~40" << std::endl;
  std::cout << "  Total instructions: ~" << iterations * 40 << std::endl;
  std::cout << "  Size: " << code.size() << " bytes" << std::endl;
}

void create_memory_intensive_test(const std::string &filename,
                                  int size = 1000) {
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
  int offset = 256 - (code.size() - fill_start + 2);
  push_bytes(code,
             {0x20, static_cast<uint8_t>(offset & 0xFF)}); // JR NZ, fill_start

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
  offset = 256 - (code.size() - read_start + 2);
  push_bytes(code,
             {0x20, static_cast<uint8_t>(offset & 0xFF)}); // JR NZ, read_start

  // HALT
  push_byte(code, 0x76);

  std::ofstream f(filename, std::ios::binary);
  f.write(reinterpret_cast<const char *>(code.data()), code.size());
  f.close();

  std::cout << "Created " << filename << std::endl;
  std::cout << "  Buffer size: " << size << " bytes" << std::endl;
  std::cout << "  Memory operations: " << size * 4 << std::endl;
  std::cout << "  Size: " << code.size() << " bytes" << std::endl;
}

void create_arithmetic_test(const std::string &filename,
                            int iterations = 50000) {
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
  int offset = 256 - (code.size() - loop_start + 2);
  push_bytes(code,
             {0x20, static_cast<uint8_t>(offset & 0xFF)}); // JR NZ, loop_start

  push_byte(code, 0x76); // HALT

  std::ofstream f(filename, std::ios::binary);
  f.write(reinterpret_cast<const char *>(code.data()), code.size());
  f.close();

  std::cout << "Created " << filename << std::endl;
  std::cout << "  Iterations: " << iterations << std::endl;
  std::cout << "  Arithmetic ops per iteration: ~40" << std::endl;
  std::cout << "  Total operations: ~" << iterations * 40 << std::endl;
}

void create_jump_test(const std::string &filename, int iterations = 10000) {
  std::vector<uint8_t> code;

  // LD BC, iterations (must fit in 16-bit: max 65535)
  if (iterations > 65535) {
    iterations = 65535;
  }
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
  int offset = 256 - (code.size() - loop_start + 2);
  push_bytes(code,
             {0x20, static_cast<uint8_t>(offset & 0xFF)}); // JR NZ, loop_start

  push_byte(code, 0x76); // HALT

  std::ofstream f(filename, std::ios::binary);
  f.write(reinterpret_cast<const char *>(code.data()), code.size());
  f.close();

  std::cout << "Created " << filename << std::endl;
  std::cout << "  Iterations: " << iterations << std::endl;
  std::cout << "  Jumps per iteration: ~5" << std::endl;
  std::cout << "  Total jumps: ~" << iterations * 5 << std::endl;
}

void mkdir_p(const std::string &path) {
  if (path.empty())
    return;
  size_t pos = 0;
  do {
    pos = path.find_first_of('/', pos + 1);
    std::string subdir = path.substr(0, pos);
    if (subdir.empty())
      continue;
    mkdir(subdir.c_str(), 0755);
  } while (pos != std::string::npos);
  mkdir(path.c_str(), 0755);
}

void create_all_tests(const std::string &output_dir) {
  mkdir_p(output_dir);

  std::cout << "\nGenerating synthetic Z80 test programs...\n" << std::endl;
  std::cout << "============================================================"
            << std::endl;

  create_instruction_mix_test(output_dir + "/instruction_mix.bin", 10000);
  std::cout << std::endl;

  create_memory_intensive_test(output_dir + "/memory_intensive.bin", 1000);
  std::cout << std::endl;

  create_arithmetic_test(output_dir + "/arithmetic_heavy.bin", 50000);
  std::cout << std::endl;

  create_jump_test(output_dir + "/branch_heavy.bin", 10000);
  std::cout << std::endl;

  std::cout << "============================================================"
            << std::endl;
  std::cout << "\nAll test programs created in: " << output_dir << std::endl;
  std::cout << "\nTo benchmark:" << std::endl;
  std::cout << "  cd build" << std::endl;
  std::cout << "  ./z80_benchmark ../" << output_dir << "/instruction_mix.bin"
            << std::endl;
  std::cout << "  ./z80_benchmark ../" << output_dir << "/memory_intensive.bin"
            << std::endl;
  std::cout << "  ./z80_benchmark ../" << output_dir << "/arithmetic_heavy.bin"
            << std::endl;
  std::cout << "  ./z80_benchmark ../" << output_dir << "/branch_heavy.bin"
            << std::endl;
  std::cout << std::endl;
}

int main(int argc, char *argv[]) {
  std::string output_dir = "tests/spectrum-roms/synthetic";
  if (argc > 1) {
    output_dir = argv[1];
  }
  create_all_tests(output_dir);
  return 0;
}
