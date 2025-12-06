// Example: How to integrate instruction profiler into Z80 emulator
// This shows the minimal changes needed to add profiling

#include "z80.h"
#include "z80_profiler.h"

// ============================================================================
// STEP 1: Add profiler to Z80 class (modify z80.h)
// ============================================================================

// In z80.h, add to private members:
/*
#ifdef WITH_PROFILING
    Z80Profiling::InstructionProfiler* profiler;
#endif
*/

// ============================================================================
// STEP 2: Initialize profiler in constructor (modify z80.cpp)
// ============================================================================

/*
Z80::Z80(Z80operations *ops) {
    // ...existing initialization code...

#ifdef WITH_PROFILING
    profiler = new Z80Profiling::InstructionProfiler();
#endif
}

Z80::~Z80() {
#ifdef WITH_PROFILING
    if (profiler) {
        profiler->printReport();
        profiler->saveReport("z80_profile_report.md");
        delete profiler;
    }
#endif
}
*/

// ============================================================================
// STEP 3: Record instructions in execute() (modify z80.cpp)
// ============================================================================

/*
void Z80::execute() {
    m_opCode = Z80opsImpl->fetchOpcode(REG_PC);
    regR++;

#ifdef WITH_PROFILING
    if (profiler) {
        profiler->recordInstruction(m_opCode);
    }
#endif

    // ...rest of execute() code...
}
*/

// ============================================================================
// STEP 4: Build with profiling enabled
// ============================================================================

/*
# In CMakeLists.txt, add option:
option(Z80_ENABLE_PROFILING "Enable instruction profiling" OFF)

if(Z80_ENABLE_PROFILING)
    add_compile_definitions(WITH_PROFILING)
endif()

# Build with profiling:
cmake .. -DZ80_ENABLE_PROFILING=ON
make

# Run your program - profiling report will print at exit
./z80sim zexall.bin

# Check the generated report
cat z80_profile_report.md
*/

// ============================================================================
// ALTERNATIVE: Non-intrusive profiling using wrapper
// ============================================================================

// If you don't want to modify Z80 class, wrap the operations interface:

class ProfilingZ80Operations : public Z80operations {
private:
    Z80operations* wrapped;
    Z80Profiling::InstructionProfiler profiler;

public:
    ProfilingZ80Operations(Z80operations* ops) : wrapped(ops) {}

    ~ProfilingZ80Operations() {
        profiler.printReport();
        profiler.saveReport("z80_profile_report.md");
    }

    uint8_t fetchOpcode(uint16_t address) override {
        uint8_t opcode = wrapped->fetchOpcode(address);
        profiler.recordInstruction(opcode);
        return opcode;
    }

    // Delegate all other methods to wrapped
    uint8_t peek8(uint16_t address) override {
        return wrapped->peek8(address);
    }

    void poke8(uint16_t address, uint8_t value) override {
        wrapped->poke8(address, value);
    }

    uint16_t peek16(uint16_t address) override {
        return wrapped->peek16(address);
    }

    void poke16(uint16_t address, RegisterPair word) override {
        wrapped->poke16(address, word);
    }

    uint8_t inPort(uint16_t port) override {
        return wrapped->inPort(port);
    }

    void outPort(uint16_t port, uint8_t value) override {
        wrapped->outPort(port, value);
    }

    void addressOnBus(uint16_t address, int32_t wstates) override {
        wrapped->addressOnBus(address, wstates);
    }

    void interruptHandlingTime(int32_t wstates) override {
        wrapped->interruptHandlingTime(wstates);
    }

    bool isActiveINT() override {
        return wrapped->isActiveINT();
    }

#ifdef WITH_BREAKPOINT_SUPPORT
    uint8_t breakpoint(uint16_t address, uint8_t opcode) override {
        return wrapped->breakpoint(address, opcode);
    }
#endif

#ifdef WITH_EXEC_DONE
    void execDone() override {
        wrapped->execDone();
    }
#endif
};

// Usage:
/*
int main() {
    MyZ80Operations* actualOps = new MyZ80Operations();
    ProfilingZ80Operations* profilingOps = new ProfilingZ80Operations(actualOps);

    Z80 cpu(profilingOps);

    // Run your program
    while (!done) {
        cpu.execute();
    }

    // Profiling report prints automatically when profilingOps destructs
    delete profilingOps;
    delete actualOps;

    return 0;
}
*/

// ============================================================================
// EXAMPLE OUTPUT
// ============================================================================

/*
================================================================================
Z80 INSTRUCTION PROFILING REPORT
================================================================================
Total instructions executed: 46958644
Total instruction pairs: 46958643
Unique pairs observed: 4521

TOP 50 INSTRUCTION PAIRS (candidates for superinstructions):
--------------------------------------------------------------------------------
Rank  First Second  Count          Percentage  Cumulative  Mnemonic Hint
--------------------------------------------------------------------------------
1     0x23  0x7E    3542891        7.54%      7.54%      INC HL → LD A,(HL)
2     0x7E  0x23    3542780        7.54%     15.08%      LD A,(HL) → INC HL
3     0xCD  0xC9    2103443        4.48%     19.56%      CALL → RET
4     0x21  0x36    1876234        4.00%     23.56%      LD HL,nn → LD (HL),n
5     0x3E  0xFE    1654321        3.52%     27.08%      LD A,n → CP n
6     0x06  0x0E    1234567        2.63%     29.71%      LD B,n → LD C,n
7     0xCB  0x7E    1123456        2.39%     32.10%      CB → BIT 7,(HL)
8     0x77  0x23    1098765        2.34%     34.44%      LD (HL),A → INC HL
9     0x10  0x23    987654         2.10%     36.54%      DJNZ → INC HL
10    0xB7  0x28    876543         1.87%     38.41%      OR A → JR Z

... (40 more pairs)

Coverage: Top 10 pairs = 38.41%, Top 20 = 56.23%, Top 50 = 78.91%

TOP 20 PREFIX INSTRUCTION PAIRS:
--------------------------------------------------------------------------------
Rank  Prefix Second  Count          Percentage  Mnemonic
--------------------------------------------------------------------------------
1     0xDD   0x7E    543210         12.45%      DD 7E (DD LD A,(IX+d))
2     0xDD   0x77    432109         9.90%       DD 77 (DD LD (IX+d),A)
3     0xDD   0x21    321098         7.35%       DD 21 (DD LD IX,nn)
4     0xCB   0x7E    298765         6.84%       CB 7E (CB BIT 7,(HL))
5     0xFD   0x7E    234567         5.37%       FD 7E (FD LD A,(IY+d))

... (15 more pairs)

RECOMMENDATIONS:
--------------------------------------------------------------------------------
✓ EXCELLENT: Top 20 pairs cover 56.2% of all pairs.
  Implementing superinstructions for these 20 pairs could yield 2-3x speedup.

Next steps:
1. Implement superinstructions for top 10-20 pairs above
2. Focus on pairs with >1% frequency first
3. Re-profile after optimization to measure improvement
================================================================================
*/

// ============================================================================
// WHAT TO DO WITH THE RESULTS
// ============================================================================

/*
Based on the example output above, you would create superinstructions like:

1. super_INC_HL_LD_A_HL() - 7.54% of all pairs!
   This single optimization affects nearly 8% of execution

2. super_LD_A_HL_INC_HL() - Another 7.54%
   Common pattern in memory scanning

3. super_CALL_RET() - 4.48%
   Optimize trivial function calls

4. super_LD_HL_nn_LD_HL_n() - 4.00%
   Initialize array element

5. super_LD_A_n_CP_n() - 3.52%
   Load and compare immediate

Just these 5 superinstructions would cover ~27% of all instruction pairs!

Implementation example:

void Z80::super_INC_HL_LD_A_HL() {
    REG_HL++;
    regA = Z80opsImpl->peek8(REG_HL);
    // Eliminated: 1 fetch, 1 decode, 1 dispatch, flag updates
}

void Z80::super_LD_A_HL_INC_HL() {
    regA = Z80opsImpl->peek8(REG_HL);
    REG_HL++;
    // Eliminated: 1 fetch, 1 decode, 1 dispatch
}

Expected speedup from just implementing top 10 pairs: 1.5-2x
Expected speedup from implementing top 30 pairs: 2-3x
*/

