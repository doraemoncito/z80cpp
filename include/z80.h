// Converted to C++ from Java at
//... https://github.com/jsanchezv/Z80Core
//... commit c4f267e3564fa89bd88fd2d1d322f4d6b0069dbd
//... GPL 3
//... v1.0.0 (13/02/2017)
//    quick & dirty conversion by dddddd (AKA deesix)

//... compile with $ g++ -m32 -std=c++14
//... put the zen*bin files in the same directory.
#ifndef Z80CPP_H
#define Z80CPP_H

#include <cstdint>

// ============================================================================
// Performance Optimization Macros
// ============================================================================

// Force inline for hot paths - critical for ALU functions
#if defined(__GNUC__) || defined(__clang__)
    #define Z80_FORCE_INLINE __attribute__((always_inline)) inline
    #define Z80_HOT __attribute__((hot))
    #define Z80_COLD __attribute__((cold))
    #define Z80_LIKELY(x) __builtin_expect(!!(x), 1)
    #define Z80_UNLIKELY(x) __builtin_expect(!!(x), 0)
#elif defined(_MSC_VER)
    #define Z80_FORCE_INLINE __forceinline
    #define Z80_HOT
    #define Z80_COLD
    #define Z80_LIKELY(x) (x)
    #define Z80_UNLIKELY(x) (x)
#else
    #define Z80_FORCE_INLINE inline
    #define Z80_HOT
    #define Z80_COLD
    #define Z80_LIKELY(x) (x)
    #define Z80_UNLIKELY(x) (x)
#endif

/* Union allowing a register pair to be accessed as bytes or as a word */
typedef union {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    struct {
        uint8_t hi, lo;
    } byte8;
#else
    struct {
        uint8_t lo, hi;
    } byte8;
#endif
    uint16_t word;
} RegisterPair;

#include "z80operations.h"

#define REG_B   regBC.byte8.hi
#define REG_C   regBC.byte8.lo
#define REG_BC  regBC.word
#define REG_Bx  regBCx.byte8.hi
#define REG_Cx  regBCx.byte8.lo
#define REG_BCx regBCx.word

#define REG_D   regDE.byte8.hi
#define REG_E   regDE.byte8.lo
#define REG_DE  regDE.word
#define REG_Dx  regDEx.byte8.hi
#define REG_Ex  regDEx.byte8.lo
#define REG_DEx regDEx.word

#define REG_H   regHL.byte8.hi
#define REG_L   regHL.byte8.lo
#define REG_HL  regHL.word
#define REG_Hx  regHLx.byte8.hi
#define REG_Lx  regHLx.byte8.lo
#define REG_HLx regHLx.word

#define REG_IXh regIX.byte8.hi
#define REG_IXl regIX.byte8.lo
#define REG_IX  regIX.word

#define REG_IYh regIY.byte8.hi
#define REG_IYl regIY.byte8.lo
#define REG_IY  regIY.word

#define REG_Ax  regAFx.byte8.hi
#define REG_Fx  regAFx.byte8.lo
#define REG_AFx regAFx.word

#define REG_PCh regPC.byte8.hi
#define REG_PCl regPC.byte8.lo
#define REG_PC  regPC.word

#define REG_S   regSP.byte8.hi
#define REG_P   regSP.byte8.lo
#define REG_SP  regSP.word

#define REG_W   memptr.byte8.hi
#define REG_Z   memptr.byte8.lo
#define REG_WZ  memptr.word

/**
 * Z80 CPU Core - CRTP Template Version
 *
 * Template parameter Derived is the operations implementation class
 * that inherits from Z80operations<Derived> and provides memory/IO hooks.
 *
 * Usage:
 *   class MySystem : public Z80operations<MySystem> {
 *       Z80<MySystem> cpu;
 *   public:
 *       MySystem() : cpu(this) {}
 *       // Implement required methods...
 *   };
 */
template<typename Derived>
class Z80 {
public:
    // Modos de interrupción
    enum IntMode {
        IM0, IM1, IM2
    };
private:
    // ========================================================================
    // HOT DATA - Frequently accessed members grouped for cache locality
    // ========================================================================

    // Acumulador y flags - accessed in almost every ALU operation
    uint8_t regA;
    uint8_t sz5h3pnFlags;  // Flags sIGN, zERO, 5, hALFCARRY, 3, pARITY y ADDSUB
    bool carryFlag;        // Carry is special-cased for performance

    // Registros principales - very frequently used
    RegisterPair regBC, regDE, regHL;

    // Program counter and stack pointer - accessed every instruction
    RegisterPair regPC;
    RegisterPair regSP;

    // Código de instrucción a ejecutar
    // Poner esta variable como local produce peor rendimiento
    uint8_t m_opCode;
    uint8_t prefixOpcode = { 0x00 };

    // Internal hidden register used by many instructions
    RegisterPair memptr;

    // I and R registers - accessed for refresh cycles
    uint8_t regI;
    uint8_t regR;
    bool regRbit7;

    // Flag Q tracking
    bool flagQ, lastFlagQ;

    // Index registers - used with DD/FD prefixed instructions
    RegisterPair regIX;
    RegisterPair regIY;

    // Operations implementation pointer
    Derived *Z80opsImpl;

    // ========================================================================
    // COLD DATA - Less frequently accessed members
    // ========================================================================

    // Registros alternativos - only used with EX/EXX
    RegisterPair regBCx, regDEx, regHLx;
    RegisterPair regAFx;

    // Interrupt state
    bool ffIFF1 = false;
    bool ffIFF2 = false;
    bool pendingEI = false;
    bool activeNMI = false;
    IntMode modeINT = IntMode::IM0;
    bool halted = false;
    bool pinReset = false;
    bool execDone;

    // ========================================================================
    // LOOKUP TABLES - Used for fast flag calculation
    // ========================================================================

    /* Algunos flags se precalculan para un tratamiento más rápido */
    uint8_t sz53n_addTable[256] = {};
    uint8_t sz53pn_addTable[256] = {};
    uint8_t sz53n_subTable[256] = {};
    uint8_t sz53pn_subTable[256] = {};

    // ========================================================================
    // CONSTANTS
    // ========================================================================

    // Posiciones de los flags
    static constexpr uint8_t CARRY_MASK = 0x01;
    static constexpr uint8_t ADDSUB_MASK = 0x02;
    static constexpr uint8_t PARITY_MASK = 0x04;
    static constexpr uint8_t OVERFLOW_MASK = 0x04; // alias de PARITY_MASK
    static constexpr uint8_t BIT3_MASK = 0x08;
    static constexpr uint8_t HALFCARRY_MASK = 0x10;
    static constexpr uint8_t BIT5_MASK = 0x20;
    static constexpr uint8_t ZERO_MASK = 0x40;
    static constexpr uint8_t SIGN_MASK = 0x80;
    // Máscaras de conveniencia
    static constexpr uint8_t FLAG_53_MASK = BIT5_MASK | BIT3_MASK;
    static constexpr uint8_t FLAG_SZ_MASK = SIGN_MASK | ZERO_MASK;
    static constexpr uint8_t FLAG_SZHN_MASK = FLAG_SZ_MASK | HALFCARRY_MASK | ADDSUB_MASK;
    static constexpr uint8_t FLAG_SZP_MASK = FLAG_SZ_MASK | PARITY_MASK;
    static constexpr uint8_t FLAG_SZHP_MASK = FLAG_SZP_MASK | HALFCARRY_MASK;

#ifdef WITH_BREAKPOINT_SUPPORT
    bool breakpointEnabled {false};
#endif

    // ========================================================================
    // PRIVATE HELPER FUNCTIONS
    // ========================================================================

    // Optimized IR register access - returns uint16_t directly
    Z80_FORCE_INLINE uint16_t getIRWord() const {
        return (static_cast<uint16_t>(regI) << 8) |
               ((regRbit7 ? (regR | SIGN_MASK) : regR) & 0x7f);
    }

    // Legacy interface - wrapper for compatibility
    inline RegisterPair getPairIR() const;

public:
    // Constructor de la clase
    explicit Z80(Derived *ops);
    ~Z80();

    // Acceso a registros de 8 bits
    // Access to 8-bit registers
    uint8_t getRegA() const { return regA; }
    void setRegA(uint8_t value) { regA = value; }

    uint8_t getRegB() const { return REG_B; }
    void setRegB(uint8_t value) { REG_B = value; }

    uint8_t getRegC() const { return REG_C; }
    void setRegC(uint8_t value) { REG_C = value; }

    uint8_t getRegD() const { return REG_D; }
    void setRegD(uint8_t value) { REG_D = value; }

    uint8_t getRegE() const { return REG_E; }
    void setRegE(uint8_t value) { REG_E = value; }

    uint8_t getRegH() const { return REG_H; }
    void setRegH(uint8_t value) { REG_H = value; }

    uint8_t getRegL() const { return REG_L; }
    void setRegL(uint8_t value) { REG_L = value; }

    // Acceso a registros alternativos de 8 bits
    // Access to alternate 8-bit registers
    uint8_t getRegAx() const { return REG_Ax; }
    void setRegAx(uint8_t value) { REG_Ax = value; }

    uint8_t getRegFx() const { return REG_Fx; }
    void setRegFx(uint8_t value) { REG_Fx = value; }

    uint8_t getRegBx() const { return REG_Bx; }
    void setRegBx(uint8_t value) { REG_Bx = value; }

    uint8_t getRegCx() const { return REG_Cx; }
    void setRegCx(uint8_t value) { REG_Cx = value; }

    uint8_t getRegDx() const { return REG_Dx; }
    void setRegDx(uint8_t value) { REG_Dx = value; }

    uint8_t getRegEx() const { return REG_Ex; }
    void setRegEx(uint8_t value) { REG_Ex = value; }

    uint8_t getRegHx() const { return REG_Hx; }
    void setRegHx(uint8_t value) { REG_Hx = value; }

    uint8_t getRegLx() const { return REG_Lx; }
    void setRegLx(uint8_t value) { REG_Lx = value; }

    // Acceso a registros de 16 bits
    // Access to registers pairs
    uint16_t getRegAF() const { return (regA << 8) | (carryFlag ? sz5h3pnFlags | CARRY_MASK : sz5h3pnFlags); }
    void setRegAF(uint16_t word) { regA = word >> 8; sz5h3pnFlags = word & 0xfe; carryFlag = (word & CARRY_MASK) != 0; }

    uint16_t getRegAFx() const { return REG_AFx; }
    void setRegAFx(uint16_t word) { REG_AFx = word; }

    uint16_t getRegBC() const { return REG_BC; }
    void setRegBC(uint16_t word) { REG_BC = word; }

    uint16_t getRegBCx() const { return REG_BCx; }
    void setRegBCx(uint16_t word) { REG_BCx = word; }

    uint16_t getRegDE() const { return REG_DE; }
    void setRegDE(uint16_t word) { REG_DE = word; }

    uint16_t getRegDEx() const { return REG_DEx; }
    void setRegDEx(uint16_t word) { REG_DEx = word; }

    uint16_t getRegHL() const { return REG_HL; }
    void setRegHL(uint16_t word) { REG_HL = word; }

    uint16_t getRegHLx() const { return REG_HLx; }
    void setRegHLx(uint16_t word) { REG_HLx = word; }

    // Acceso a registros de propósito específico
    // Access to special purpose registers
    uint16_t getRegPC() const { return REG_PC; }
    void setRegPC(uint16_t address) { REG_PC = address; }

    uint16_t getRegSP() const { return REG_SP; }
    void setRegSP(uint16_t word) { REG_SP = word; }

    uint16_t getRegIX() const { return REG_IX; }
    void setRegIX(uint16_t word) { REG_IX = word; }

    uint16_t getRegIY() const { return REG_IY; }
    void setRegIY(uint16_t word) { REG_IY = word; }

    uint8_t getRegI() const { return regI; }
    void setRegI(uint8_t value) { regI = value; }

    uint8_t getRegR() const { return regRbit7 ? regR | SIGN_MASK : regR & 0x7f; }
    void setRegR(uint8_t value) { regR = value & 0x7f; regRbit7 = (value > 0x7f); }

    // Acceso al registro oculto MEMPTR
    // Hidden register MEMPTR (known as WZ at Zilog doc?)
    uint16_t getMemPtr() const { return REG_WZ; }
    void setMemPtr(uint16_t word) { REG_WZ = word; }

    // Acceso a los flags uno a uno
    // Access to single flags from F register
    bool isCarryFlag() const { return carryFlag; }
    void setCarryFlag(bool state) { carryFlag = state; }

    bool isAddSubFlag() const { return (sz5h3pnFlags & ADDSUB_MASK) != 0; }
    void setAddSubFlag(bool state);

    bool isParOverFlag() const { return (sz5h3pnFlags & PARITY_MASK) != 0; }
    void setParOverFlag(bool state);

    /* Undocumented flag */
    bool isBit3Flag() const { return (sz5h3pnFlags & BIT3_MASK) != 0; }
    void setBit3Flag(bool state);

    bool isHalfCarryFlag() const { return (sz5h3pnFlags & HALFCARRY_MASK) != 0; }
    void setHalfCarryFlag(bool state);

    /* Undocumented flag */
    bool isBit5Flag() const { return (sz5h3pnFlags & BIT5_MASK) != 0; }
    void setBit5Flag(bool state);

    bool isZeroFlag() const { return (sz5h3pnFlags & ZERO_MASK) != 0; }
    void setZeroFlag(bool state);

    bool isSignFlag() const { return sz5h3pnFlags >= SIGN_MASK; }
    void setSignFlag(bool state);

    // Acceso a los flags F
    // Access to F register
    uint8_t getFlags() const { return carryFlag ? sz5h3pnFlags | CARRY_MASK : sz5h3pnFlags; }
    void setFlags(uint8_t regF) { sz5h3pnFlags = regF & 0xfe; carryFlag = (regF & CARRY_MASK) != 0; }

    // Acceso a los flip-flops de interrupción
    // Interrupt flip-flops
    bool isIFF1() const { return ffIFF1; }
    void setIFF1(bool state) { ffIFF1 = state; }

    bool isIFF2() const { return ffIFF2; }
    void setIFF2(bool state) { ffIFF2 = state; }

    bool isNMI() const { return activeNMI; }
    void setNMI(bool nmi) { activeNMI = nmi; }

    // /NMI is negative level triggered.
    void triggerNMI() { activeNMI = true; }

    //Acceso al modo de interrupción
    // Maskable interrupt mode
    IntMode getIM() const { return modeINT; }
    void setIM(IntMode mode) { modeINT = mode; }

    bool isHalted() const { return halted; }
    void setHalted(bool state) { halted = state; }

    // Reset requested by /RESET signal (not power-on)
    void setPinReset() { pinReset = true; }

    bool isPendingEI() const { return pendingEI; }
    void setPendingEI(bool state) { pendingEI = state; }

    // Reset
    void reset();

    // Execute one instruction
    void execute();

#ifdef WITH_BREAKPOINT_SUPPORT
    bool isBreakpoint() { return breakpointEnabled; }
    void setBreakpoint(bool state) { breakpointEnabled = state; }
#endif

#ifdef WITH_EXEC_DONE
    void setExecDone(bool status) { execDone = status; }
#endif

private:
    // Rota a la izquierda el valor del argumento
    Z80_FORCE_INLINE void rlc(uint8_t &oper8);

    // Rota a la izquierda el valor del argumento
    Z80_FORCE_INLINE void rl(uint8_t &oper8);

    // Rota a la izquierda el valor del argumento
    Z80_FORCE_INLINE void sla(uint8_t &oper8);

    // Rota a la izquierda el valor del argumento (como sla salvo por el bit 0)
    Z80_FORCE_INLINE void sll(uint8_t &oper8);

    // Rota a la derecha el valor del argumento
    Z80_FORCE_INLINE void rrc(uint8_t &oper8);

    // Rota a la derecha el valor del argumento
    Z80_FORCE_INLINE void rr(uint8_t &oper8);

    // Rota a la derecha 1 bit el valor del argumento
    Z80_FORCE_INLINE void sra(uint8_t &oper8);

    // Rota a la derecha 1 bit el valor del argumento
    Z80_FORCE_INLINE void srl(uint8_t &oper8);

    // Incrementa un valor de 8 bits modificando los flags oportunos
    Z80_FORCE_INLINE void inc8(uint8_t &oper8);

    // Decrementa un valor de 8 bits modificando los flags oportunos
    Z80_FORCE_INLINE void dec8(uint8_t &oper8);

    // Suma de 8 bits afectando a los flags
    Z80_FORCE_INLINE void add(uint8_t oper8);

    // Suma con acarreo de 8 bits
    Z80_FORCE_INLINE void adc(uint8_t oper8);

    // Suma dos operandos de 16 bits sin carry afectando a los flags
    Z80_FORCE_INLINE void add16(RegisterPair &reg16, uint16_t oper16);

    // Suma con acarreo de 16 bits
    Z80_FORCE_INLINE void adc16(uint16_t reg16);

    // Resta de 8 bits
    Z80_FORCE_INLINE void sub(uint8_t oper8);

    // Resta con acarreo de 8 bits
    Z80_FORCE_INLINE void sbc(uint8_t oper8);

    // Resta con acarreo de 16 bits
    Z80_FORCE_INLINE void sbc16(uint16_t reg16);

    // Operación AND lógica
    // Simple 'and' is C++ reserved keyword
    Z80_FORCE_INLINE void and_(uint8_t oper8);

    // Operación XOR lógica
    // Simple 'xor' is C++ reserved keyword
    Z80_FORCE_INLINE void xor_(uint8_t oper8);

    // Operación OR lógica
    // Simple 'or' is C++ reserved keyword
    Z80_FORCE_INLINE void or_(uint8_t oper8);

    // Operación de comparación con el registro A
    // es como SUB, pero solo afecta a los flags
    // Los flags SIGN y ZERO se calculan a partir del resultado
    // Los flags 3 y 5 se copian desde el operando (sigh!)
    Z80_FORCE_INLINE void cp(uint8_t oper8);

    // DAA
    Z80_FORCE_INLINE void daa();

    // POP
    Z80_FORCE_INLINE uint16_t pop();

    // PUSH
    Z80_FORCE_INLINE void push(uint16_t word);

    // LDI
    void ldi();

    // LDD
    void ldd();

    // CPI
    void cpi();

    // CPD
    void cpd();

    // INI
    void ini();

    // IND
    void ind();

    // OUTI
    void outi();

    // OUTD
    void outd();

    // BIT n,r
    Z80_FORCE_INLINE void bitTest(uint8_t mask, uint8_t reg);

    //Interrupción
    Z80_COLD void interrupt();

    //Interrupción NMI
    Z80_COLD void nmi();

    // Decode main opcodes
    Z80_HOT void decodeOpcode(uint8_t opCode);

    // Subconjunto de instrucciones 0xCB
    // decode CBXX opcodes
    Z80_HOT void decodeCB(uint8_t opCode);

    //Subconjunto de instrucciones 0xDD / 0xFD
    // Decode DD/FD opcodes
    Z80_HOT void decodeDDFD(uint8_t opCode, RegisterPair& regIXY);

    // Subconjunto de instrucciones 0xDD / 0xFD 0xCB
    // Decode DD / FD CB opcodes
    Z80_HOT void decodeDDFDCB(uint8_t opCode, uint16_t address);

    //Subconjunto de instrucciones 0xED
    // Decode EDXX opcodes
    Z80_HOT void decodeED(uint8_t opCode);

    // Helper functions
    void copyToRegister(uint8_t opCode, uint8_t value);
    void adjustINxROUTxRFlags();
};

// Include template implementation
// For template classes, the implementation must be available at compile time
#include "z80_impl.h"

#endif // Z80CPP_H
