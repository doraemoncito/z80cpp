#ifndef Z80_SIM_TEST_H
#define Z80_SIM_TEST_H

#include <array>
#include <chrono>
#include <fstream>
#include <iostream>

#include "../include/z80.h"
#include "../include/z80_bus_interface.h"

class Z80SimTest : public Z80BusInterface<Z80SimTest> {
  private:
    uint64_t tstates{0};
    Z80<Z80SimTest> cpu;
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

    uint8_t fetchOpcodeImpl(uint16_t address);
    uint8_t peek8Impl(uint16_t address);
    void poke8Impl(uint16_t address, uint8_t value);
    uint16_t peek16Impl(uint16_t address);
    void poke16Impl(uint16_t address, RegisterPair word);
    uint8_t inPortImpl(uint16_t port);
    void outPortImpl(uint16_t port, uint8_t value);
    void addressOnBusImpl(uint16_t address, int32_t tstates);
    void interruptHandlingTimeImpl(int32_t tstates);
    static bool isActiveINTImpl();

#ifdef WITH_BREAKPOINT_SUPPORT
    uint8_t breakpointImpl(uint16_t address, uint8_t opcode);
#else
    uint8_t breakpoint(uint16_t address, uint8_t opcode);
#endif // WITH_BREAKPOINT_SUPPORT

    void handleStringOutput();

#ifdef WITH_EXEC_DONE
    void execDoneImpl(void);
#endif

    void runTest(std::ifstream* fileStream);
};

#endif // Z80_SIM_TEST_H
