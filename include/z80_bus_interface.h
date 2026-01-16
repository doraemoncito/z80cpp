#ifndef Z80_BUS_INTERFACE_H
#define Z80_BUS_INTERFACE_H

#include <cstdint>

#include "z80_types.h"

// CRTP bus interface: TBusInterface must implement the *Impl methods.
template <typename TBusInterface> class Z80BusInterface {
  public:
    ~Z80BusInterface() = default;
    Z80BusInterface& operator=(const Z80BusInterface&) = default;
    Z80BusInterface& operator=(Z80BusInterface&&) = default;

    /* Read opcode from RAM */
    Z80_FORCE_INLINE uint8_t fetchOpcode(uint16_t address) {
        return derived().fetchOpcodeImpl(address);
    }

    /* Read/Write byte from/to RAM */
    Z80_FORCE_INLINE uint8_t peek8(uint16_t address) {
        return derived().peek8Impl(address);
    }

    Z80_FORCE_INLINE void poke8(uint16_t address, uint8_t value) {
        derived().poke8Impl(address, value);
    }

    /* Read/Write word from/to RAM */
    Z80_FORCE_INLINE uint16_t peek16(uint16_t address) {
        return derived().peek16Impl(address);
    }

    Z80_FORCE_INLINE void poke16(uint16_t address, RegisterPair word) {
        derived().poke16Impl(address, word);
    }

    /* In/Out byte from/to IO Bus */
    Z80_FORCE_INLINE uint8_t inPort(uint16_t port) {
        return derived().inPortImpl(port);
    }

    Z80_FORCE_INLINE void outPort(uint16_t port, uint8_t value) {
        derived().outPortImpl(port, value);
    }

    /* Put an address on bus lasting 'tstates' cycles */
    Z80_FORCE_INLINE void addressOnBus(uint16_t address, int32_t wstates) {
        derived().addressOnBusImpl(address, wstates);
    }

    /* Clocks needed for processing INT and NMI */
    Z80_FORCE_INLINE void interruptHandlingTime(int32_t wstates) {
        derived().interruptHandlingTimeImpl(wstates);
    }

    /* Callback to know when the INT signal is active */
    Z80_FORCE_INLINE bool isActiveINT() {
        return derived().isActiveINTImpl();
    }

#ifdef WITH_BREAKPOINT_SUPPORT
    /* Callback for notify at PC address */
    uint8_t breakpoint(uint16_t address, uint8_t opcode) {
        return derived().breakpointImpl(address, opcode);
    }
#endif

#ifdef WITH_EXEC_DONE
    /* Callback to notify that one instruction has ended */
    void execDone(void) {
        derived().execDoneImpl();
    }
#endif

  private:
    Z80BusInterface() = default;
    Z80BusInterface(const Z80BusInterface&) = default;
    Z80BusInterface(Z80BusInterface&&) = default;

    TBusInterface& derived() {
        return static_cast<TBusInterface&>(*this);
    }

    friend TBusInterface;
};

#endif // Z80_BUS_INTERFACE_H
