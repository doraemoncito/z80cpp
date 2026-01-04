#include "z80_sim_test.hpp"

#include <cassert>
#include <chrono>
#include <iomanip>
#include <sstream>

Z80SimTest::Z80SimTest() : cpu(std::make_unique<Z80>(this)) {
}

Z80SimTest::~Z80SimTest() = default;

uint8_t Z80SimTest::fetchOpcode(uint16_t address) {
    // 3 clocks to fetch opcode from RAM and 1 execution clock
    tstates += 4;

    uint8_t opcode = z80Ram.at(address);

#ifndef WITH_BREAKPOINT_SUPPORT
    if (cpmMode && address == 0x0005) {
        return breakpoint(address, opcode);
    }
#endif

    return opcode;
}

uint8_t Z80SimTest::peek8(uint16_t address) {
    // 3 clocks for read byte from RAM
    tstates += 3;
    return z80Ram.at(address);
}

void Z80SimTest::poke8(uint16_t address, uint8_t value) {
    // 3 clocks for write byte to RAM
    tstates += 3;
    z80Ram.at(address) = value;
}

uint16_t Z80SimTest::peek16(uint16_t address) {
    // Order matters, first read lsb, then read msb, don't "optimize"
    uint8_t lsb = peek8(address);
    uint8_t msb = peek8(address + 1);
    return (msb << 8) | lsb;
}

void Z80SimTest::poke16(uint16_t address, RegisterPair word) {
    // Order matters, first write lsb, then write msb, don't "optimize"
    poke8(address, word.byte8.lo);
    poke8(address + 1, word.byte8.hi);
}

uint8_t Z80SimTest::inPort(uint16_t port) {
    // 4 clocks for read byte from bus.  See Z80 User Manual and the references:
    // https://www.zilog.com/docs/z80/um0080.pdf
    // https://fizyka.umk.pl/~jacek/zx/faq/reference/48kreference.htm
    tstates += 4;
    return z80Ports.at(port);
}

void Z80SimTest::outPort(uint16_t port, uint8_t value) {
    // 4 clocks for write byte to bus
    tstates += 4;
    z80Ports.at(port) = value;
}

void Z80SimTest::addressOnBus(uint16_t address, int32_t wstates) {
    // Additional clocks to be added on some instructions
    this->tstates += wstates;
}

void Z80SimTest::interruptHandlingTime(int32_t wstates) {
    this->tstates += wstates;
}

bool Z80SimTest::isActiveINT() {
    // Put here the logic needed to trigger an INT
    return false;
}

#ifdef WITH_EXEC_DONE
void Z80SimTest::execDone(void) {
}
#endif

#ifdef WITH_BREAKPOINT_SUPPORT
uint8_t Z80SimTest::breakpoint(uint16_t address, uint8_t opcode) {
    // Only handle CP/M BDOS calls if in CP/M mode
    if (!cpmMode) {
        return opcode; // No breakpoint handling for raw Z80 programs
    }

    // Emulate CP/M Syscall at address 5
    if (address != 0x0005) {
        return opcode;
    }

    switch (cpu->getRegC()) {
        case 0: // BDOS 0 System Reset
        {
            std::cout << '\n' << "Z80 reset after " << tstates << " t-states\n";
            finish = true;
            return opcode;
        }
        case 2: // BDOS 2 console char output
        {
            std::cout << static_cast<char>(cpu->getRegE());
            return opcode;
        }
        case 9: // BDOS 9 console string output (string terminated by "$")
        {
            handleStringOutput();
            return opcode;
        }
        default: {
            std::cout << "BDOS Call " << static_cast<unsigned int>(cpu->getRegC()) << '\n';
            finish = true;
            std::cout << finish << '\n';
        }
    }
    // opcode would be modified before the decodeOpcode method
    return opcode;
}
#else
uint8_t Z80SimTest::breakpoint(uint16_t address, uint8_t opcode) {
    // Only handle CP/M BDOS calls if in CP/M mode
    if (!cpmMode) {
        return opcode; // No breakpoint handling for raw Z80 programs
    }

    // Emulate CP/M Syscall at address 5
    if (address != 0x0005) {
        return opcode;
    }

    switch (cpu->getRegC()) {
        case 0: // BDOS 0 System Reset
        {
            std::cout << '\n' << "Z80 reset after " << tstates << " t-states\n";
            finish = true;
            return opcode;
        }
        case 2: // BDOS 2 console char output
        {
            std::cout << static_cast<char>(cpu->getRegE());
            return opcode;
        }
        case 9: // BDOS 9 console string output (string terminated by "$")
        {
            handleStringOutput();
            return opcode;
        }
        default: {
            std::cout << "BDOS Call " << static_cast<unsigned int>(cpu->getRegC()) << '\n';
            finish = true;
            std::cout << finish << '\n';
        }
    }
    // opcode would be modified before the decodeOpcode method
    return opcode;
}
#endif

void Z80SimTest::handleStringOutput() {
    uint16_t strAddr = cpu->getRegDE();
    std::string output;

    while (z80Ram.at(strAddr) != '$') {
        output += static_cast<char>(z80Ram.at(strAddr++));
    }

    auto current_time = std::chrono::high_resolution_clock::now();
    elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - opcode_start_time).count();
    /*
     * Check for "  OK\n" at the end of the output to append timing info or mark the test as failed
     * if "  ERROR" is found in any part of the output.
     */
    if (output.find("  OK\n") != std::string::npos) {
        std::ostringstream oss;
        oss << "✓ Passed " << std::setw(6) << std::fixed << std::setprecision(3)
            << static_cast<float>(elapsed_ms) / 1000.0f << " sec\n";
        output.replace(output.find("OK\n"), 3, oss.str());
        total_ms += elapsed_ms;
        ++num_tests;
    } else if (output.find("  ERROR") != std::string::npos) {
        output.replace(output.find("ERROR"), 5, "✗ Failed");
        ++failed;
    }
    std::cout << output << std::flush;
    opcode_start_time = current_time;
}

void Z80SimTest::runTest(std::ifstream* fileStream) {
    if (!fileStream->is_open()) {
        std::cout << "file NOT OPEN\n";
        return;
    }

    std::cout << "file open\n";

    const std::streampos size = fileStream->tellg();
    std::cout << "Test size: " << size << '\n';
    fileStream->seekg(0, std::ios::beg);
    fileStream->read(reinterpret_cast<char*>(&z80Ram[0x100]), size);
    fileStream->close();

#ifdef WITH_BREAKPOINT_SUPPORT
    cpu->setBreakpoint(true);
#endif

    cpu->reset();
    finish = false;

    z80Ram[0] = static_cast<uint8_t>(0xC3);
    z80Ram[1] = 0x00;
    z80Ram[2] = 0x01;                       // JP 0x100 CP/M TPA
    z80Ram[5] = static_cast<uint8_t>(0xC9); // Return from BDOS call

    std::cout << "Running zexall...\n";
    std::cout.flush();
    start_time = opcode_start_time = std::chrono::high_resolution_clock::now();
    while (!finish) {
        cpu->execute();
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "\n";
    std::cout << "Elapsed T-state count:  " << tstates << "\n";
    std::cout << "Cumulative test time:   " << static_cast<float>(total_ms) / 1000.0f << " sec\n";
    if (num_tests > 0) {
        std::cout << "Average time per test: " << static_cast<float>(total_ms) / num_tests / 1000.0f << " sec\n\n";
        std::cout << "✓ Tests passed: " << num_tests - failed << "\n";
        std::cout << "✗ Tests failed: " << failed << "\n";
    }
    std::cout << "Total elapsed time:     " << static_cast<float>(elapsed_ms) / 1000.0f << " sec\n";
    std::cout.flush();
    if (failed > 0) {
        throw std::runtime_error("Test failed");
    }
}

int main() {
    try {
        Z80SimTest sim;

        std::ifstream f1("zexall.bin", std::ios::in | std::ios::binary | std::ios::ate);
        sim.runTest(&f1);
        f1.close();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return 1;
    }
}
