#ifndef Z80SIM_H
#define Z80SIM_H

#include <fstream>
#include <iostream>

// Forward declaration for CRTP
class Z80sim;

#include "../include/z80.h"
#include "../include/z80operations.h"

class Z80sim : public Z80operations<Z80sim>
{
protected:  // Changed from private to allow benchmark tool to access
    uint64_t tstates;
    Z80<Z80sim> cpu;
    uint8_t z80Ram[0x10000];
    uint8_t z80Ports[0x10000];
    volatile bool finish;
    uint64_t instructionCount;
    bool cpmMode;  // Enable CP/M BDOS call handling

public:
    Z80sim();
    ~Z80sim();

    // Public accessors for benchmark tests
    Z80<Z80sim>& getCpu() { return cpu; }
    uint8_t* getRam() { return z80Ram; }
    uint64_t getTstates() const { return tstates; }
    uint64_t getInstructionCount() const { return instructionCount; }
    void resetInstructionCount() { instructionCount = 0; }
    void setCpmMode(bool enable) { cpmMode = enable; }
    bool isCpmMode() const { return cpmMode; }

    // Z80operations interface - these methods are called via CRTP without virtual dispatch
    uint8_t fetchOpcode(uint16_t address);
    uint8_t peek8(uint16_t address);
    void poke8(uint16_t address, uint8_t value);
    uint16_t peek16(uint16_t address);
    void poke16(uint16_t address, RegisterPair word);
    uint8_t inPort(uint16_t port);
    void outPort(uint16_t port, uint8_t value);
    void addressOnBus(uint16_t address, int32_t tstates);
    void interruptHandlingTime(int32_t tstates);
    bool isActiveINT();

#ifdef WITH_BREAKPOINT_SUPPORT
    uint8_t breakpoint(uint16_t address, uint8_t opcode);
#else
    uint8_t breakpoint(uint16_t address, uint8_t opcode);
#endif

#ifdef WITH_EXEC_DONE
    void execDone(void);
#endif

    void runTest(std::ifstream* f);
};
#endif // Z80SIM_H
