#ifndef Z80_SIM_TEST_HPP
#define Z80_SIM_TEST_HPP

#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>

#include "../include/z80.h"
#include "../include/z80operations.h"

class Z80SimTest : public Z80operations {
  private:
    uint64_t tstates{0};
    std::unique_ptr<Z80> cpu;
    std::array<uint8_t, 0x10000> z80Ram{};
    std::array<uint8_t, 0x10000> z80Ports{};
    volatile bool finish{false};
    volatile uint16_t failed{0};
    int64_t elapsed_ms{0};
    int64_t total_ms{0};
    int64_t num_tests{0};
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point opcode_start_time;
    bool cpmMode{true};

  public:
    Z80SimTest();
    ~Z80SimTest();
    Z80SimTest(const Z80SimTest&) = delete;
    Z80SimTest& operator=(const Z80SimTest&) = delete;
    Z80SimTest(Z80SimTest&&) = delete;
    Z80SimTest& operator=(Z80SimTest&&) = delete;

    // Z80operations interface implementation
    uint8_t fetchOpcode(uint16_t address) override;
    uint8_t peek8(uint16_t address) override;
    void poke8(uint16_t address, uint8_t value) override;
    uint16_t peek16(uint16_t address) override;
    void poke16(uint16_t address, RegisterPair word) override;
    uint8_t inPort(uint16_t port) override;
    void outPort(uint16_t port, uint8_t value) override;
    void addressOnBus(uint16_t address, int32_t tstates) override;
    void interruptHandlingTime(int32_t tstates) override;
    bool isActiveINT() override;

#ifdef WITH_BREAKPOINT_SUPPORT
    uint8_t breakpoint(uint16_t address, uint8_t opcode) override;
#else
    uint8_t breakpoint(uint16_t address, uint8_t opcode);
#endif

    void handleStringOutput();

#ifdef WITH_EXEC_DONE
    void execDone(void) override;
#endif

    void runTest(std::ifstream* fileStream);
};

#endif // Z80_SIM_TEST_HPP
