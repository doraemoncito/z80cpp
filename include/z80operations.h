#ifndef Z80OPERATIONS_H
#define Z80OPERATIONS_H

#include <cstdint>

// RegisterPair is defined in z80.h as a typedef union
// When z80operations.h is included from z80.h, RegisterPair is already defined

/**
 * Z80operations - CRTP Base Class for Memory/IO Operations
 *
 * This uses the Curiously Recurring Template Pattern (CRTP) to eliminate
 * virtual dispatch overhead while allowing customization of memory and I/O operations.
 *
 * Usage:
 *   class MySystem : public Z80operations<MySystem> {
 *   public:
 *       uint8_t fetchOpcode(uint16_t address) { ... }
 *       uint8_t peek8(uint16_t address) { ... }
 *       // ... implement all required methods
 *   };
 *
 * The Z80 CPU will call these methods without virtual dispatch, allowing
 * the compiler to inline them for maximum performance.
 *
 * Required methods in Derived class:
 *   - uint8_t fetchOpcode(uint16_t address)
 *   - uint8_t peek8(uint16_t address)
 *   - void poke8(uint16_t address, uint8_t value)
 *   - uint16_t peek16(uint16_t address)
 *   - void poke16(uint16_t address, RegisterPair word)
 *   - uint8_t inPort(uint16_t port)
 *   - void outPort(uint16_t port, uint8_t value)
 *   - void addressOnBus(uint16_t address, int32_t wstates)
 *   - void interruptHandlingTime(int32_t wstates)
 *   - bool isActiveINT()
 *   - uint8_t breakpoint(uint16_t address, uint8_t opcode) [if WITH_BREAKPOINT_SUPPORT]
 *   - void execDone() [if WITH_EXEC_DONE]
 */
template<typename Derived>
class Z80operations {
public:
    Z80operations() = default;
    ~Z80operations() = default;

    // Non-copyable (prevents slicing)
    Z80operations(const Z80operations&) = delete;
    Z80operations& operator=(const Z80operations&) = delete;

    /* Read opcode from RAM */
    uint8_t fetchOpcode(uint16_t address) {
        return static_cast<Derived*>(this)->fetchOpcode(address);
    }

    /* Read/Write byte from/to RAM */
    uint8_t peek8(uint16_t address) {
        return static_cast<Derived*>(this)->peek8(address);
    }
    void poke8(uint16_t address, uint8_t value) {
        static_cast<Derived*>(this)->poke8(address, value);
    }

    /* Read/Write word from/to RAM */
    uint16_t peek16(uint16_t address) {
        return static_cast<Derived*>(this)->peek16(address);
    }
    void poke16(uint16_t address, RegisterPair word) {
        static_cast<Derived*>(this)->poke16(address, word);
    }

    /* In/Out byte from/to IO Bus */
    uint8_t inPort(uint16_t port) {
        return static_cast<Derived*>(this)->inPort(port);
    }
    void outPort(uint16_t port, uint8_t value) {
        static_cast<Derived*>(this)->outPort(port, value);
    }

    /* Put an address on bus lasting 'tstates' cycles */
    void addressOnBus(uint16_t address, int32_t wstates) {
        static_cast<Derived*>(this)->addressOnBus(address, wstates);
    }

    /* Clocks needed for processing INT and NMI */
    void interruptHandlingTime(int32_t wstates) {
        static_cast<Derived*>(this)->interruptHandlingTime(wstates);
    }

    /* Callback to know when the INT signal is active */
    bool isActiveINT() {
        return static_cast<Derived*>(this)->isActiveINT();
    }

#ifdef WITH_BREAKPOINT_SUPPORT
    /* Callback for notify at PC address */
    uint8_t breakpoint(uint16_t address, uint8_t opcode) {
        return static_cast<Derived*>(this)->breakpoint(address, opcode);
    }
#endif

#ifdef WITH_EXEC_DONE
    /* Callback to notify that one instruction has ended */
    void execDone(void) {
        static_cast<Derived*>(this)->execDone();
    }
#endif

protected:
    // Allow derived class to access the CRTP cast
    Derived& derived() { return *static_cast<Derived*>(this); }
    const Derived& derived() const { return *static_cast<const Derived*>(this); }
};

#endif // Z80OPERATIONS_H
