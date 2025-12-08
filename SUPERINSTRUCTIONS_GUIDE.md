# Adding New Superinstructions - Developer Guide

This guide explains how to add new superinstruction patterns to the Z80 emulator.

## Quick Start

To add a new superinstruction for the pattern `OPCODE1 + OPCODE2`:

### Step 1: Add to Detection (z80_superinstructions_impl.h)

In `detectSuperinstruction()`, add your pattern:

```cpp
case 0xXX: // OPCODE1 comment
    if (nextOpcode == 0xYY) return true; // + OPCODE2
    break;
```

### Step 2: Add to Dispatch (z80_superinstructions_impl.h)

In `executeSuperinstruction()`, add the handler call:

```cpp
case 0xXX: // OPCODE1 + ...
    if (nextOpcode == 0xYY) {
        super_OPCODE1_OPCODE2();
    }
    break;
```

### Step 3: Implement Handler (z80_superinstructions_impl.h)

Add the fused implementation:

```cpp
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::super_OPCODE1_OPCODE2() {
    // Execute OPCODE1 inline
    // ... instruction 1 code ...
    
    // Execute OPCODE2 inline
    REG_PC++; // Skip second opcode
    // ... instruction 2 code ...
}
```

### Step 4: Add Declaration (z80.h)

In the `#if Z80_ENABLE_SUPERINSTRUCTIONS` section:

```cpp
Z80_FORCE_INLINE void super_OPCODE1_OPCODE2();
```

### Step 5: Test

```bash
cd build_opt
make
./benchmark_tests  # Verify performance
./z80sim ../example/zexall.bin  # Verify correctness
```

## Detailed Example: LD B,n + LD C,n

Let's walk through implementing `LD B,n + LD C,n` step by step.

### Analysis

**Individual Instructions:**
```
LD B,n:     opcode 0x06, reads immediate byte, stores in B
LD C,n:     opcode 0x0E, reads immediate byte, stores in C
```

**Fused Version:**
```
LD BC,nn equivalent using two immediate loads
```

### Implementation

#### 1. Detection

```cpp
template<typename Derived>
Z80_FORCE_INLINE bool Z80<Derived>::detectSuperinstruction(uint8_t opcode, uint8_t& nextOpcode) {
    nextOpcode = Z80opsImpl->peek8(REG_PC);
    
    switch(opcode) {
        // ...existing cases...
        
        case 0x06: // LD B,n
            if (nextOpcode == 0x0E) return true; // + LD C,n
            break;
```

#### 2. Dispatch

```cpp
template<typename Derived>
Z80_HOT void Z80<Derived>::executeSuperinstruction(uint8_t opcode, uint8_t nextOpcode) {
    switch(opcode) {
        // ...existing cases...
        
        case 0x06: // LD B,n + LD C,n
            if (nextOpcode == 0x0E) {
                super_LD_B_n_LD_C_n();
            }
            break;
```

#### 3. Handler

```cpp
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::super_LD_B_n_LD_C_n() {
    // First instruction: LD B,n
    REG_B = Z80opsImpl->peek8(REG_PC);
    REG_PC++;
    
    // Skip second opcode (LD C)
    REG_PC++;
    
    // Second instruction: LD C,n
    REG_C = Z80opsImpl->peek8(REG_PC);
    REG_PC++;
}
```

#### 4. Declaration

```cpp
class Z80 {
    // ...
    
#if Z80_ENABLE_SUPERINSTRUCTIONS
    Z80_FORCE_INLINE void super_LD_B_n_LD_C_n();
#endif
```

## Common Patterns

### Pattern: Two Register Loads

```cpp
// LD r1,n + LD r2,n
Z80_FORCE_INLINE void Z80<Derived>::super_LD_r1_n_LD_r2_n() {
    reg1 = Z80opsImpl->peek8(REG_PC++);
    REG_PC++; // Skip second opcode
    reg2 = Z80opsImpl->peek8(REG_PC++);
}
```

### Pattern: Operation + Conditional Jump

```cpp
// OP A + JR cc,e
Z80_FORCE_INLINE void Z80<Derived>::super_OP_A_JR_cc() {
    operation(regA);  // Execute operation
    REG_PC++; // Skip JR opcode
    
    auto offset = static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
    if (condition) {
        Z80opsImpl->addressOnBus(REG_PC, 5);
        REG_PC += offset;
        REG_WZ = REG_PC + 1;
    }
    REG_PC++;
}
```

### Pattern: Memory Op + Pointer Adjust

```cpp
// LD (HL),r + INC HL
Z80_FORCE_INLINE void Z80<Derived>::super_LD_HL_r_INC_HL() {
    Z80opsImpl->poke8(REG_HL, reg);
    Z80opsImpl->addressOnBus(getIRWord(), 2);  // INC timing
    REG_HL++;
    REG_PC++; // Skip second instruction
}
```

## Important Rules

### 1. Maintain T-State Accuracy

Every superinstruction must execute in the same T-states as the individual instructions:

```cpp
// Wrong: Missing timing
super_INC_HL_LD_A_HL() {
    REG_HL++;
    regA = Z80opsImpl->peek8(REG_HL);
}

// Correct: Proper timing
super_INC_HL_LD_A_HL() {
    Z80opsImpl->addressOnBus(getIRWord(), 2);  // INC HL takes 2 T-states
    REG_HL++;
    regA = Z80opsImpl->peek8(REG_HL);
}
```

### 2. Handle REG_PC Correctly

Always skip the second opcode:

```cpp
// Pattern: After executing both instructions
REG_PC++;  // Skip the second opcode byte
```

For multi-byte instructions:

```cpp
// LD HL,nn + LD (HL),n
super_LD_HL_nn_LD_HL_n() {
    REG_HL = Z80opsImpl->peek16(REG_PC);
    REG_PC += 2;  // Skip nn
    
    REG_PC++;  // Skip LD (HL),n opcode
    
    uint8_t value = Z80opsImpl->peek8(REG_PC);
    REG_PC++;  // Skip n
    
    Z80opsImpl->poke8(REG_HL, value);
}
```

### 3. Preserve Flags

Execute flag-affecting operations in the correct order:

```cpp
// Correct: Execute operations that may affect each other
super_INC_A_CP_n() {
    inc8(regA);  // This sets flags
    REG_PC++;
    uint8_t operand = Z80opsImpl->peek8(REG_PC);
    REG_PC++;
    cp(operand);  // This overwrites flags (correct behavior)
}
```

### 4. Memory Contention

Include proper memory contention timing:

```cpp
super_LD_HL_nn_LD_A_HL() {
    REG_HL = Z80opsImpl->peek16(REG_PC);  // Contended read
    REG_PC += 2;
    REG_PC++;
    regA = Z80opsImpl->peek8(REG_HL);  // Contended read
    REG_WZ = REG_HL + 1;
}
```

## Testing Your Superinstruction

### 1. Unit Test

Create a test program:

```assembly
ORG 0x8000
    LD B,0x12    ; Your pattern
    LD C,0x34
    HALT
```

Assemble and run:

```bash
./z80sim test.bin
```

### 2. Verify Flags

Ensure all flags are set correctly:

```cpp
// Before superinstruction:
LD A,0xFF
INC A      ; A=0x00, Z=1, S=0, P=0, H=1
CP 0x00    ; Z=1, S=0 (maintained)

// After fusing INC A + CP n:
// Flags must match exactly
```

### 3. Profile Impact

Measure before/after:

```bash
# Before adding superinstruction
./benchmark_tests > before.txt

# After adding superinstruction  
./benchmark_tests > after.txt

# Compare
diff before.txt after.txt
```

### 4. ZEXALL Verification

Always run the comprehensive test:

```bash
timeout 120 ./z80sim ../example/zexall.bin | tail -20
```

All tests must pass!

## Performance Guidelines

### High-Value Patterns

Prioritize patterns that are:
1. **Frequent** - >1% of all pairs
2. **Sequential** - No branches between them
3. **Independent** - Second doesn't depend on first's flags

### Low-Value Patterns

Avoid fusing:
1. **Rare** - <0.1% frequency
2. **Conditional** - Branches invalidate fusion
3. **Complex** - Hard to maintain, small benefit

### Measuring Frequency

Use the profiler to find candidates:

```cpp
// See PROFILER_INTEGRATION_EXAMPLE.cpp
// Top patterns will be listed by frequency
```

## Debugging

### Enable Logging

```cpp
#define Z80_SUPER_DEBUG 1

super_YOUR_PATTERN() {
    #if Z80_SUPER_DEBUG
    std::cerr << "SUPER: YOUR_PATTERN at PC=" 
              << std::hex << REG_PC << std::endl;
    #endif
    // ...
}
```

### Disable Temporarily

```cpp
// In your test code:
#define Z80_ENABLE_SUPERINSTRUCTIONS 0
#include "z80.h"
```

### Compare Execution

Run with and without superinstructions:

```bash
# With superinstructions
./z80sim test.bin > with_super.log

# Without
cmake -DZ80_ENABLE_SUPERINSTRUCTIONS=0 ..
make
./z80sim test.bin > without_super.log

# Verify identical output (except timing)
diff <(grep -v "Time:" with_super.log) <(grep -v "Time:" without_super.log)
```

## Best Practices

### 1. Document Your Pattern

```cpp
// LD B,n + LD C,n - 2.63% of all pairs
// Common in: Game initialization, sprite routines
// Benefit: Saves 1 fetch + 1 decode = ~7 T-states
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::super_LD_B_n_LD_C_n() {
```

### 2. Keep It Simple

One superinstruction = one function. Don't create mega-functions.

### 3. Profile After Adding

Every new superinstruction should improve performance. If it doesn't, remove it.

### 4. Maintain Compatibility

Test with ZEXALL after every change.

## Common Mistakes

### ❌ Wrong: Forgetting to Skip Opcode

```cpp
super_LD_B_n_LD_C_n() {
    REG_B = Z80opsImpl->peek8(REG_PC++);
    // Missing: REG_PC++; // Skip LD C opcode
    REG_C = Z80opsImpl->peek8(REG_PC++);
}
// Result: Will re-execute LD C as next instruction!
```

### ❌ Wrong: Incorrect T-States

```cpp
super_INC_HL_LD_A_HL() {
    REG_HL++;  // Missing timing!
    regA = Z80opsImpl->peek8(REG_HL);
}
// Result: Wrong timing, fails timing-sensitive code
```

### ❌ Wrong: Flag Order

```cpp
super_OR_A_JR_Z() {
    // Check condition first (WRONG!)
    if ((sz5h3pnFlags & ZERO_MASK) != 0) { ... }
    or_(regA);  // This changes the flags!
}
// Result: Uses old flags instead of new ones
```

### ✅ Correct Versions

See the implementations in `z80_superinstructions_impl.h` for correct examples.

## Performance Checklist

Before submitting a new superinstruction:

- [ ] Pattern is >0.5% frequency (verified by profiling)
- [ ] T-states match individual instructions
- [ ] All flags set correctly
- [ ] REG_PC handled properly
- [ ] ZEXALL passes
- [ ] Benchmarks show improvement
- [ ] Code documented with frequency data
- [ ] Tested with and without enabled

## Example: Complete Addition

Here's a complete example adding `DEC A + CP n`:

```cpp
// 1. z80_superinstructions_impl.h - Detection
case 0x3D: // DEC A
    if (nextOpcode == 0xFE) return true; // + CP n
    break;

// 2. z80_superinstructions_impl.h - Dispatch  
case 0x3D: // DEC A + CP n
    if (nextOpcode == 0xFE) {
        super_DEC_A_CP_n();
    }
    break;

// 3. z80_superinstructions_impl.h - Handler
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::super_DEC_A_CP_n() {
    dec8(regA);
    REG_PC++; // Skip CP opcode
    uint8_t operand = Z80opsImpl->peek8(REG_PC);
    REG_PC++;
    cp(operand);
}

// 4. z80.h - Declaration
#if Z80_ENABLE_SUPERINSTRUCTIONS
    Z80_FORCE_INLINE void super_DEC_A_CP_n();
#endif

// 5. Test
make && ./benchmark_tests && ./z80sim ../example/zexall.bin
```

## Conclusion

Adding superinstructions is straightforward:
1. Identify frequent patterns (profiling)
2. Add detection (switch case)
3. Add dispatch (switch case)
4. Implement handler (inline execution)
5. Declare method (header)
6. Test (ZEXALL + benchmarks)

Follow the examples in `z80_superinstructions_impl.h` for reference implementations.

Happy optimizing! 🚀

