// Converted to C++ from Java at
//... https://github.com/jsanchezv/Z80Core
//... commit c4f267e3564fa89bd88fd2d1d322f4d6b0069dbd
//... GPL 3
//... v1.0.0 (13/02/2017)
//    quick & dirty conversion by dddddd (AKA deesix)

//... compile with $ g++ -m32 -std=c++17
//... put the zen*bin files in the same directory.
#ifndef Z80_HPP
#define Z80_HPP

#include <array>
#include <cstdint>

#include "z80_types.h"

struct alignas(64) FlagTables {
    alignas(64) std::array<uint8_t, 256> sz53n_add;
    alignas(64) std::array<uint8_t, 256> sz53pn_add;
    alignas(64) std::array<uint8_t, 256> sz53n_sub;
    alignas(64) std::array<uint8_t, 256> sz53pn_sub;
};

// Build flag lookup tables at compile time
constexpr FlagTables makeFlagTables() {
    constexpr uint8_t kSignMask = 0x80;
    constexpr uint8_t kZeroMask = 0x40;
    constexpr uint8_t kParityMask = 0x04;
    constexpr uint8_t kAddSubMask = 0x02;
    constexpr uint8_t kFlag53Mask = 0x28; // bit5 | bit3

    FlagTables tables{};

    for (uint32_t idx = 0; idx < 256; idx++) {
        uint8_t addFlags = 0;

        if (idx > 0x7f) {
            addFlags |= kSignMask;
        }

        bool evenBits = true;
        for (uint8_t mask = 0x01; mask != 0; mask <<= 1) {
            if ((idx & mask) != 0) {
                evenBits = !evenBits;
            }
        }

        addFlags |= static_cast<uint8_t>(idx & kFlag53Mask);

        auto subFlags = static_cast<uint8_t>(addFlags | kAddSubMask);

        tables.sz53n_add[idx] = addFlags;
        tables.sz53n_sub[idx] = subFlags;
        tables.sz53pn_add[idx] = evenBits ? static_cast<uint8_t>(addFlags | kParityMask) : addFlags;
        tables.sz53pn_sub[idx] = evenBits ? static_cast<uint8_t>(subFlags | kParityMask) : subFlags;
    }

    tables.sz53n_add[0] |= kZeroMask;
    tables.sz53pn_add[0] |= kZeroMask;
    tables.sz53n_sub[0] |= kZeroMask;
    tables.sz53pn_sub[0] |= kZeroMask;

    return tables;
}

// Pre-computed flag tables, initialized at compile time
inline constexpr FlagTables kFlagTables = makeFlagTables();

inline const FlagTables& getFlagTables() {
    return kFlagTables;
}

#include "z80_bus_interface.h"

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

#define REG_S  regSP.byte8.hi
#define REG_P  regSP.byte8.lo
#define REG_SP regSP.word

#define REG_W  memptr.byte8.hi
#define REG_Z  memptr.byte8.lo
#define REG_WZ memptr.word

template <typename TBusInterface> class Z80 {
  public:
    Z80(const Z80&) = delete;
    Z80& operator=(const Z80&) = delete;
    Z80(Z80&&) = delete;
    Z80& operator=(Z80&&) = delete;

    // Modos de interrupción
    enum class IntMode : uint8_t { IM0, IM1, IM2 };

  private:
    TBusInterface& m_busInterface;
    // Código de instrucción a ejecutar
    // Poner esta variable como local produce peor rendimiento
    // ZEXALL test: (local) 1:54 vs 1:47 (visitante)
    uint8_t m_opCode{};
    // Se está ejecutando una instrucción prefijada con DD, ED o FD
    // Los valores permitidos son [0x00, 0xDD, 0xED, 0xFD]
    // El prefijo 0xCB queda al margen porque, detrás de 0xCB, siempre
    // viene un código de instrucción válido, tanto si delante va un
    // 0xDD o 0xFD como si no.
    uint8_t prefixOpcode = {0x00};
    // Subsistema de notificaciones
    bool execDone{false};
    // Posiciones de los flags
    const static uint8_t CARRY_MASK = 0x01;
    const static uint8_t ADDSUB_MASK = 0x02;
    const static uint8_t PARITY_MASK = 0x04;
    const static uint8_t OVERFLOW_MASK = 0x04; // alias de PARITY_MASK
    const static uint8_t BIT3_MASK = 0x08;
    const static uint8_t HALFCARRY_MASK = 0x10;
    const static uint8_t BIT5_MASK = 0x20;
    const static uint8_t ZERO_MASK = 0x40;
    const static uint8_t SIGN_MASK = 0x80;
    // Máscaras de conveniencia
    const static uint8_t FLAG_53_MASK = BIT5_MASK | BIT3_MASK;
    const static uint8_t FLAG_SZ_MASK = SIGN_MASK | ZERO_MASK;
    const static uint8_t FLAG_SZHN_MASK = FLAG_SZ_MASK | HALFCARRY_MASK | ADDSUB_MASK;
    const static uint8_t FLAG_SZP_MASK = FLAG_SZ_MASK | PARITY_MASK;
    const static uint8_t FLAG_SZHP_MASK = FLAG_SZP_MASK | HALFCARRY_MASK;
    // Acumulador y resto de registros de 8 bits
    uint8_t regA{};
    // Flags sIGN, zERO, 5, hALFCARRY, 3, pARITY y ADDSUB (n)
    uint8_t sz5h3pnFlags{};
    // El flag Carry es el único que se trata aparte
    bool carryFlag{};
    // Registros principales y alternativos
    RegisterPair regBC{}, regBCx{}, regDE{}, regDEx{}, regHL{}, regHLx{};
    /* Flags para indicar la modificación del registro F en la instrucción actual
     * y en la anterior.
     * Son necesarios para emular el comportamiento de los bits 3 y 5 del
     * registro F con las instrucciones CCF/SCF.
     *
     * http://www.worldofspectrum.org/forums/showthread.php?t=41834
     * http://www.worldofspectrum.org/forums/showthread.php?t=41704
     *
     * Thanks to Patrik Rak for his tests and investigations.
     */
    bool flagQ{}, lastFlagQ{};

    // Acumulador alternativo y flags -- 8 bits
    RegisterPair regAFx{};

    // Registros de propósito específico
    // *PC -- Program Counter -- 16 bits*
    RegisterPair regPC{};
    // *IX -- Registro de índice -- 16 bits*
    RegisterPair regIX{};
    // *IY -- Registro de índice -- 16 bits*
    RegisterPair regIY{};
    // *SP -- Stack Pointer -- 16 bits*
    RegisterPair regSP{};
    // *I -- Vector de interrupción -- 8 bits*
    uint8_t regI{};
    // *R -- Refresco de memoria -- 7 bits*
    uint8_t regR{};
    // *R7 -- Refresco de memoria -- 1 bit* (bit superior de R)
    bool regRbit7{};
    // Flip-flops de interrupción
    bool ffIFF1 = false;
    bool ffIFF2 = false;
    // EI solo habilita las interrupciones DESPUES de ejecutar la
    // siguiente instrucción (excepto si la siguiente instrucción es un EI...)
    bool pendingEI = false;
    // Estado de la línea NMI
    bool activeNMI = false;
    // Modo de interrupción
    IntMode modeINT = IntMode::IM0;
    // halted == true cuando la CPU está ejecutando un HALT (28/03/2010)
    bool halted = false;
    // pinReset == true, se ha producido un reset a través de la patilla
    bool pinReset = false;
    /*
     * Registro interno que usa la CPU de la siguiente forma
     *
     * ADD HL,xx      = Valor del registro H antes de la suma
     * LD r,(IX/IY+d) = Byte superior de la suma de IX/IY+d
     * JR d           = Byte superior de la dirección de destino del salto
     *
     * 04/12/2008     No se vayan todavía, aún hay más. Con lo que se ha
     *                implementado hasta ahora parece que funciona. El resto de
     *                la historia está contada en:
     *                http://zx.pk.ru/attachment.php?attachmentid=2989
     *
     * 25/09/2009     Se ha completado la emulación de MEMPTR. A señalar que
     *                no se puede comprobar si MEMPTR se ha emulado bien hasta
     *                que no se emula el comportamiento del registro en las
     *                instrucciones CPI y CPD. Sin ello, todos los tests de
     *                z80tests.tap fallarán aunque se haya emulado bien al
     *                registro en TODAS las otras instrucciones.
     *                Shit yourself, little parrot.
     */

    RegisterPair memptr{};
    // I and R registers
    [[nodiscard]] inline RegisterPair getPairIR() const;

    /* Algunos flags se precalculan para un tratamiento más rápido
     * Concretamente, SIGN, ZERO, los bits 3, 5, PARITY y ADDSUB:
     * sz53n_addTable tiene el ADDSUB flag a 0 y paridad sin calcular
     * sz53pn_addTable tiene el ADDSUB flag a 0 y paridad calculada
     * sz53n_subTable tiene el ADDSUB flag a 1 y paridad sin calcular
     * sz53pn_subTable tiene el ADDSUB flag a 1 y paridad calculada
     * El resto de bits están a 0 en las cuatro tablas lo que es
     * importante para muchas operaciones que ponen ciertos flags a 0 por real
     * decreto. Si lo ponen a 1 por el mismo método basta con hacer un OR con
     * la máscara correspondiente.
     */
    const std::array<uint8_t, 256>& sz53n_addTable;
    const std::array<uint8_t, 256>& sz53pn_addTable;
    const std::array<uint8_t, 256>& sz53n_subTable;
    const std::array<uint8_t, 256>& sz53pn_subTable;

// Un true en una dirección indica que se debe notificar que se va a
// ejecutar la instrucción que está en esa direción.
#ifdef WITH_BREAKPOINT_SUPPORT
    bool breakpointEnabled{false};
#endif // Z80_HPP
    void copyToRegister(uint8_t opCode, uint8_t value);
    void adjustINxROUTxRFlags();

  public:
    // Constructor de la clase
    explicit Z80(TBusInterface& ops);
    ~Z80();

    // Acceso a registros de 8 bits
    // Access to 8-bit registers
    [[nodiscard]] uint8_t getRegA() const {
        return regA;
    }

    void setRegA(uint8_t value) {
        regA = value;
    }

    [[nodiscard]] uint8_t getRegB() const {
        return REG_B;
    }

    void setRegB(uint8_t value) {
        REG_B = value;
    }

    [[nodiscard]] uint8_t getRegC() const {
        return REG_C;
    }

    void setRegC(uint8_t value) {
        REG_C = value;
    }

    [[nodiscard]] uint8_t getRegD() const {
        return REG_D;
    }

    void setRegD(uint8_t value) {
        REG_D = value;
    }

    [[nodiscard]] uint8_t getRegE() const {
        return REG_E;
    }

    void setRegE(uint8_t value) {
        REG_E = value;
    }

    [[nodiscard]] uint8_t getRegH() const {
        return REG_H;
    }

    void setRegH(uint8_t value) {
        REG_H = value;
    }

    [[nodiscard]] uint8_t getRegL() const {
        return REG_L;
    }

    void setRegL(uint8_t value) {
        REG_L = value;
    }

    // Acceso a registros alternativos de 8 bits
    // Access to alternate 8-bit registers
    [[nodiscard]] uint8_t getRegAx() const {
        return REG_Ax;
    }

    void setRegAx(uint8_t value) {
        REG_Ax = value;
    }

    [[nodiscard]] uint8_t getRegFx() const {
        return REG_Fx;
    }

    void setRegFx(uint8_t value) {
        REG_Fx = value;
    }

    [[nodiscard]] uint8_t getRegBx() const {
        return REG_Bx;
    }

    void setRegBx(uint8_t value) {
        REG_Bx = value;
    }

    [[nodiscard]] uint8_t getRegCx() const {
        return REG_Cx;
    }

    void setRegCx(uint8_t value) {
        REG_Cx = value;
    }

    [[nodiscard]] uint8_t getRegDx() const {
        return REG_Dx;
    }

    void setRegDx(uint8_t value) {
        REG_Dx = value;
    }

    [[nodiscard]] uint8_t getRegEx() const {
        return REG_Ex;
    }

    void setRegEx(uint8_t value) {
        REG_Ex = value;
    }

    [[nodiscard]] uint8_t getRegHx() const {
        return REG_Hx;
    }

    void setRegHx(uint8_t value) {
        REG_Hx = value;
    }

    [[nodiscard]] uint8_t getRegLx() const {
        return REG_Lx;
    }

    void setRegLx(uint8_t value) {
        REG_Lx = value;
    }

    // Acceso a registros de 16 bits
    // Access to registers pairs
    [[nodiscard]] uint16_t getRegAF() const {
        return (regA << 8) | (carryFlag ? sz5h3pnFlags | CARRY_MASK : sz5h3pnFlags);
    }

    void setRegAF(uint16_t word) {
        regA = word >> 8;
        sz5h3pnFlags = word & 0xfe;
        carryFlag = (word & CARRY_MASK) != 0;
    }

    [[nodiscard]] uint16_t getRegAFx() const {
        return REG_AFx;
    }

    void setRegAFx(uint16_t word) {
        REG_AFx = word;
    }

    [[nodiscard]] uint16_t getRegBC() const {
        return REG_BC;
    }

    void setRegBC(uint16_t word) {
        REG_BC = word;
    }

    [[nodiscard]] uint16_t getRegBCx() const {
        return REG_BCx;
    }

    void setRegBCx(uint16_t word) {
        REG_BCx = word;
    }

    [[nodiscard]] uint16_t getRegDE() const {
        return REG_DE;
    }

    void setRegDE(uint16_t word) {
        REG_DE = word;
    }

    [[nodiscard]] uint16_t getRegDEx() const {
        return REG_DEx;
    }

    void setRegDEx(uint16_t word) {
        REG_DEx = word;
    }

    [[nodiscard]] uint16_t getRegHL() const {
        return REG_HL;
    }

    void setRegHL(uint16_t word) {
        REG_HL = word;
    }

    [[nodiscard]] uint16_t getRegHLx() const {
        return REG_HLx;
    }

    void setRegHLx(uint16_t word) {
        REG_HLx = word;
    }

    // Acceso a registros de propósito específico
    // Access to special purpose registers
    [[nodiscard]] uint16_t getRegPC() const {
        return REG_PC;
    }

    void setRegPC(uint16_t address) {
        REG_PC = address;
    }

    [[nodiscard]] uint16_t getRegSP() const {
        return REG_SP;
    }

    void setRegSP(uint16_t word) {
        REG_SP = word;
    }

    [[nodiscard]] uint16_t getRegIX() const {
        return REG_IX;
    }

    void setRegIX(uint16_t word) {
        REG_IX = word;
    }

    [[nodiscard]] uint16_t getRegIY() const {
        return REG_IY;
    }

    void setRegIY(uint16_t word) {
        REG_IY = word;
    }

    [[nodiscard]] uint8_t getRegI() const {
        return regI;
    }

    void setRegI(uint8_t value) {
        regI = value;
    }

    [[nodiscard]] uint8_t getRegR() const {
        return regRbit7 ? regR | SIGN_MASK : regR & 0x7f;
    }

    void setRegR(uint8_t value) {
        regR = value & 0x7f;
        regRbit7 = (value > 0x7f);
    }

    // Acceso al registro oculto MEMPTR
    // Hidden register MEMPTR (known as WZ at Zilog doc?)
    [[nodiscard]] uint16_t getMemPtr() const {
        return REG_WZ;
    }

    void setMemPtr(uint16_t word) {
        REG_WZ = word;
    }

    // Acceso a los flags uno a uno
    // Access to single flags from F register
    [[nodiscard]] bool isCarryFlag() const {
        return carryFlag;
    }

    void setCarryFlag(bool state) {
        carryFlag = state;
    }

    [[nodiscard]] bool isAddSubFlag() const {
        return (sz5h3pnFlags & ADDSUB_MASK) != 0;
    }

    void setAddSubFlag(bool state);

    [[nodiscard]] bool isParOverFlag() const {
        return (sz5h3pnFlags & PARITY_MASK) != 0;
    }

    void setParOverFlag(bool state);

    /* Undocumented flag */
    [[nodiscard]] bool isBit3Flag() const {
        return (sz5h3pnFlags & BIT3_MASK) != 0;
    }

    void setBit3Flag(bool state);

    [[nodiscard]] bool isHalfCarryFlag() const {
        return (sz5h3pnFlags & HALFCARRY_MASK) != 0;
    }

    void setHalfCarryFlag(bool state);

    /* Undocumented flag */
    [[nodiscard]] bool isBit5Flag() const {
        return (sz5h3pnFlags & BIT5_MASK) != 0;
    }

    void setBit5Flag(bool state);

    [[nodiscard]] bool isZeroFlag() const {
        return (sz5h3pnFlags & ZERO_MASK) != 0;
    }

    void setZeroFlag(bool state);

    [[nodiscard]] bool isSignFlag() const {
        return sz5h3pnFlags >= SIGN_MASK;
    }

    void setSignFlag(bool state);

    // Acceso a los flags F
    // Access to F register
    [[nodiscard]] uint8_t getFlags() const {
        return carryFlag ? sz5h3pnFlags | CARRY_MASK : sz5h3pnFlags;
    }

    void setFlags(uint8_t regF) {
        sz5h3pnFlags = regF & 0xfe;
        carryFlag = (regF & CARRY_MASK) != 0;
    }

    // Acceso a los flip-flops de interrupción
    // Interrupt flip-flops
    [[nodiscard]] bool isIFF1() const {
        return ffIFF1;
    }

    void setIFF1(bool state) {
        ffIFF1 = state;
    }

    [[nodiscard]] bool isIFF2() const {
        return ffIFF2;
    }

    void setIFF2(bool state) {
        ffIFF2 = state;
    }

    [[nodiscard]] bool isNMI() const {
        return activeNMI;
    }

    void setNMI(bool nmi) {
        activeNMI = nmi;
    }

    // /NMI is negative level triggered.
    void triggerNMI() {
        activeNMI = true;
    }

    // Acceso al modo de interrupción
    //  Maskable interrupt mode
    [[nodiscard]] IntMode getIM() const {
        return modeINT;
    }

    void setIM(IntMode mode) {
        modeINT = mode;
    }

    [[nodiscard]] bool isHalted() const {
        return halted;
    }

    void setHalted(bool state) {
        halted = state;
    }

    // Reset requested by /RESET signal (not power-on)
    void setPinReset() {
        pinReset = true;
    }

    [[nodiscard]] bool isPendingEI() const {
        return pendingEI;
    }

    void setPendingEI(bool state) {
        pendingEI = state;
    }

    // Reset
    void reset();

    // Execute one instruction
    void execute();

#ifdef WITH_BREAKPOINT_SUPPORT
    [[nodiscard]] bool isBreakpoint() {
        return breakpointEnabled;
    }

    void setBreakpoint(bool state) {
        breakpointEnabled = state;
    }
#endif

#ifdef WITH_EXEC_DONE
    void setExecDone(bool status) {
        execDone = status;
    }
#endif

  private:
    // Rota a la izquierda el valor del argumento
    inline void rlc(uint8_t& oper8);

    // Rota a la izquierda el valor del argumento
    inline void rl(uint8_t& oper8);

    // Rota a la izquierda el valor del argumento
    inline void sla(uint8_t& oper8);

    // Rota a la izquierda el valor del argumento (como sla salvo por el bit 0)
    inline void sll(uint8_t& oper8);

    // Rota a la derecha el valor del argumento
    inline void rrc(uint8_t& oper8);

    // Rota a la derecha el valor del argumento
    inline void rr(uint8_t& oper8);

    // Rota a la derecha 1 bit el valor del argumento
    inline void sra(uint8_t& oper8);

    // Rota a la derecha 1 bit el valor del argumento
    inline void srl(uint8_t& oper8);

    // Incrementa un valor de 8 bits modificando los flags oportunos
    inline void inc8(uint8_t& oper8);

    // Decrementa un valor de 8 bits modificando los flags oportunos
    inline void dec8(uint8_t& oper8);

    // Suma de 8 bits afectando a los flags
    inline void add(uint8_t oper8);

    // Suma con acarreo de 8 bits
    inline void adc(uint8_t oper8);

    // Suma dos operandos de 16 bits sin carry afectando a los flags
    inline void add16(RegisterPair& reg16, uint16_t oper16);

    // Suma con acarreo de 16 bits
    inline void adc16(uint16_t reg16);

    // Resta de 8 bits
    inline void sub(uint8_t oper8);

    // Resta con acarreo de 8 bits
    inline void sbc(uint8_t oper8);

    // Resta con acarreo de 16 bits
    inline void sbc16(uint16_t reg16);

    // Operación AND lógica
    // Simple 'and' is C++ reserved keyword
    inline void and_(uint8_t oper8);

    // Operación XOR lógica
    // Simple 'xor' is C++ reserved keyword
    inline void xor_(uint8_t oper8);

    // Operación OR lógica
    // Simple 'or' is C++ reserved keyword
    inline void or_(uint8_t oper8);

    // Operación de comparación con el registro A
    // es como SUB, pero solo afecta a los flags
    // Los flags SIGN y ZERO se calculan a partir del resultado
    // Los flags 3 y 5 se copian desde el operando (sigh!)
    inline void cp(uint8_t oper8);

    // DAA
    inline void daa();

    // POP
    inline uint16_t pop();

    // PUSH
    inline void push(uint16_t word);

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
    inline void bitTest(uint8_t mask, uint8_t reg);

    // Interrupción
    void interrupt();

    // Interrupción NMI
    void nmi();

    // Decode main opcodes
    void decodeOpcode(uint8_t opCode);

    // Subconjunto de instrucciones 0xCB
    // decode CBXX opcodes
    void decodeCB();

    // Subconjunto de instrucciones 0xDD / 0xFD
    //  Decode DD/FD opcodes
    void decodeDDFD(uint8_t opCode, RegisterPair& regIXY);

    // Subconjunto de instrucciones 0xDD / 0xFD 0xCB
    // Decode DD / FD CB opcodes
    void decodeDDFDCB(uint8_t opCode, uint16_t address);

    // Subconjunto de instrucciones 0xED
    //  Decode EDXX opcodes
    void decodeED(uint8_t opCode);
};

// Constructor de la clase
template <typename TBusInterface>
Z80<TBusInterface>::Z80(TBusInterface& ops)
    : m_busInterface(ops),
      sz53n_addTable(getFlagTables().sz53n_add),
      sz53pn_addTable(getFlagTables().sz53pn_add),
      sz53n_subTable(getFlagTables().sz53n_sub),
      sz53pn_subTable(getFlagTables().sz53pn_sub) {

    reset();
}

template <typename TBusInterface> Z80<TBusInterface>::~Z80() = default;

template <typename TBusInterface> RegisterPair Z80<TBusInterface>::getPairIR() const {
    RegisterPair IR;
    IR.byte8.hi = regI;
    IR.byte8.lo = regR & 0x7f;
    if (regRbit7) {
        IR.byte8.lo |= SIGN_MASK;
    }
    return IR;
}

template <typename TBusInterface> void Z80<TBusInterface>::setAddSubFlag(bool state) {
    // Branchless flag update to reduce mispredictions in hot paths
    const uint8_t mask = state ? 0xFF : 0x00;
    sz5h3pnFlags = static_cast<uint8_t>((sz5h3pnFlags & static_cast<uint8_t>(~ADDSUB_MASK)) | (mask & ADDSUB_MASK));
}

template <typename TBusInterface> void Z80<TBusInterface>::setParOverFlag(bool state) {
    const uint8_t mask = state ? 0xFF : 0x00;
    sz5h3pnFlags = static_cast<uint8_t>((sz5h3pnFlags & static_cast<uint8_t>(~PARITY_MASK)) | (mask & PARITY_MASK));
}

template <typename TBusInterface> void Z80<TBusInterface>::setBit3Flag(bool state) {
    const uint8_t mask = state ? 0xFF : 0x00;
    sz5h3pnFlags = static_cast<uint8_t>((sz5h3pnFlags & static_cast<uint8_t>(~BIT3_MASK)) | (mask & BIT3_MASK));
}

template <typename TBusInterface> void Z80<TBusInterface>::setHalfCarryFlag(bool state) {
    const uint8_t mask = state ? 0xFF : 0x00;
    sz5h3pnFlags =
        static_cast<uint8_t>((sz5h3pnFlags & static_cast<uint8_t>(~HALFCARRY_MASK)) | (mask & HALFCARRY_MASK));
}

template <typename TBusInterface> void Z80<TBusInterface>::setBit5Flag(bool state) {
    const uint8_t mask = state ? 0xFF : 0x00;
    sz5h3pnFlags = static_cast<uint8_t>((sz5h3pnFlags & static_cast<uint8_t>(~BIT5_MASK)) | (mask & BIT5_MASK));
}

template <typename TBusInterface> void Z80<TBusInterface>::setZeroFlag(bool state) {
    const uint8_t mask = state ? 0xFF : 0x00;
    sz5h3pnFlags = static_cast<uint8_t>((sz5h3pnFlags & static_cast<uint8_t>(~ZERO_MASK)) | (mask & ZERO_MASK));
}

template <typename TBusInterface> void Z80<TBusInterface>::setSignFlag(bool state) {
    const uint8_t mask = state ? 0xFF : 0x00;
    sz5h3pnFlags = static_cast<uint8_t>((sz5h3pnFlags & static_cast<uint8_t>(~SIGN_MASK)) | (mask & SIGN_MASK));
}

// Reset
/* Según el documento de Sean Young, que se encuentra en
 * [http://www.myquest.com/z80undocumented], la mejor manera de emular el
 * reset es poniendo PC, IFF1, IFF2, R e IM0 a 0 y todos los demás registros
 * a 0xFFFF.
 *
 * 29/05/2011: cuando la CPU recibe alimentación por primera vez, los
 *             registros PC e IR se inicializan a cero y el resto a 0xFF.
 *             Si se produce un reset a través de la patilla correspondiente,
 *             los registros PC e IR se inicializan a 0 y el resto se preservan.
 *             En cualquier caso, todo parece depender bastante del modelo
 *             concreto de Z80, así que se escoge el comportamiento del
 *             modelo Zilog Z8400APS. Z80A CPU.
 *             http://www.worldofspectrum.org/forums/showthread.php?t=34574
 */
template <typename TBusInterface> void Z80<TBusInterface>::reset() {
    if (pinReset) {
        pinReset = false;
    } else {
        regA = 0xff;

        setFlags(0xfd); // The only one flag reset at cold start is the add/sub flag

        REG_AFx = 0xffff;
        REG_BC = REG_BCx = 0xffff;
        REG_DE = REG_DEx = 0xffff;
        REG_HL = REG_HLx = 0xffff;

        REG_IX = REG_IY = 0xffff;

        REG_SP = 0xffff;

        REG_WZ = 0xffff;
    }

    REG_PC = 0;
    regI = regR = 0;
    regRbit7 = false;
    ffIFF1 = false;
    ffIFF2 = false;
    pendingEI = false;
    activeNMI = false;
    halted = false;
    setIM(IntMode::IM0);
    lastFlagQ = false;
    prefixOpcode = 0x00;
}

// Rota a la izquierda el valor del argumento
// El bit 0 y el flag C toman el valor del bit 7 antes de la operación
template <typename TBusInterface> void Z80<TBusInterface>::rlc(uint8_t& oper8) {
    carryFlag = (oper8 > 0x7f);
    oper8 <<= 1;
    if (carryFlag) {
        oper8 |= CARRY_MASK;
    }
    sz5h3pnFlags = sz53pn_addTable[oper8];
}

// Rota a la izquierda el valor del argumento
// El bit 7 va al carry flag
// El bit 0 toma el valor del flag C antes de la operación
template <typename TBusInterface> void Z80<TBusInterface>::rl(uint8_t& oper8) {
    bool carry = carryFlag;
    carryFlag = (oper8 > 0x7f);
    oper8 <<= 1;
    if (carry) {
        oper8 |= CARRY_MASK;
    }
    sz5h3pnFlags = sz53pn_addTable[oper8];
}

// Rota a la izquierda el valor del argumento
// El bit 7 va al carry flag
// El bit 0 toma el valor 0
template <typename TBusInterface> void Z80<TBusInterface>::sla(uint8_t& oper8) {
    carryFlag = (oper8 > 0x7f);
    oper8 <<= 1;
    sz5h3pnFlags = sz53pn_addTable[oper8];
}

// Rota a la izquierda el valor del argumento (como sla salvo por el bit 0)
// El bit 7 va al carry flag
// El bit 0 toma el valor 1
// Instrucción indocumentada
template <typename TBusInterface> void Z80<TBusInterface>::sll(uint8_t& oper8) {
    carryFlag = (oper8 > 0x7f);
    oper8 <<= 1;
    oper8 |= CARRY_MASK;
    sz5h3pnFlags = sz53pn_addTable[oper8];
}

// Rota a la derecha el valor del argumento
// El bit 7 y el flag C toman el valor del bit 0 antes de la operación
template <typename TBusInterface> void Z80<TBusInterface>::rrc(uint8_t& oper8) {
    carryFlag = (oper8 & CARRY_MASK) != 0;
    oper8 >>= 1;
    if (carryFlag) {
        oper8 |= SIGN_MASK;
    }
    sz5h3pnFlags = sz53pn_addTable[oper8];
}

// Rota a la derecha el valor del argumento
// El bit 0 va al carry flag
// El bit 7 toma el valor del flag C antes de la operación
template <typename TBusInterface> void Z80<TBusInterface>::rr(uint8_t& oper8) {
    bool carry = carryFlag;
    carryFlag = (oper8 & CARRY_MASK) != 0;
    oper8 >>= 1;
    if (carry) {
        oper8 |= SIGN_MASK;
    }
    sz5h3pnFlags = sz53pn_addTable[oper8];
}

// Rota a la derecha 1 bit el valor del argumento
// El bit 0 pasa al carry.
// El bit 7 conserva el valor que tenga
template <typename TBusInterface> void Z80<TBusInterface>::sra(uint8_t& oper8) {
    uint8_t sign = oper8 & SIGN_MASK;
    carryFlag = (oper8 & CARRY_MASK) != 0;
    oper8 = (oper8 >> 1) | sign;
    sz5h3pnFlags = sz53pn_addTable[oper8];
}

// Rota a la derecha 1 bit el valor del argumento
// El bit 0 pasa al carry.
// El bit 7 toma el valor 0
template <typename TBusInterface> void Z80<TBusInterface>::srl(uint8_t& oper8) {
    carryFlag = (oper8 & CARRY_MASK) != 0;
    oper8 >>= 1;
    sz5h3pnFlags = sz53pn_addTable[oper8];
}

/*
 * Half-carry flag:
 *
 * FLAG = (A ^ B ^ RESULT) & 0x10  for any operation
 *
 * Overflow flag:
 *
 * FLAG = ~(A ^ B) & (B ^ RESULT) & 0x80 for addition [ADD/ADC]
 * FLAG = (A ^ B) & (A ^ RESULT) &0x80 for subtraction [SUB/SBC]
 *
 * For INC/DEC, you can use following simplifications:
 *
 * INC:
 * H_FLAG = (RESULT & 0x0F) == 0x00
 * V_FLAG = RESULT == 0x80
 *
 * DEC:
 * H_FLAG = (RESULT & 0x0F) == 0x0F
 * V_FLAG = RESULT == 0x7F
 */
// Incrementa un valor de 8 bits modificando los flags oportunos
template <typename TBusInterface> void Z80<TBusInterface>::inc8(uint8_t& oper8) {
    oper8++;

    sz5h3pnFlags = sz53n_addTable[oper8];

    if ((oper8 & 0x0f) == 0) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (oper8 == 0x80) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }
}

// Decrementa un valor de 8 bits modificando los flags oportunos
template <typename TBusInterface> void Z80<TBusInterface>::dec8(uint8_t& oper8) {
    oper8--;

    sz5h3pnFlags = sz53n_subTable[oper8];

    if ((oper8 & 0x0f) == 0x0f) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (oper8 == 0x7f) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }
}

// Suma de 8 bits afectando a los flags
template <typename TBusInterface> void Z80<TBusInterface>::add(uint8_t oper8) {
    uint16_t res = regA + oper8;

    carryFlag = res > 0xff;
    res &= 0xff;
    sz5h3pnFlags = sz53n_addTable[res];

    /* El módulo 16 del resultado será menor que el módulo 16 del registro A
     * si ha habido HalfCarry. Sucede lo mismo para todos los métodos suma
     * SIN carry */
    if ((res & 0x0f) < (regA & 0x0f)) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (((regA ^ ~oper8) & (regA ^ res)) > 0x7f) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }

    regA = res;
}

// Suma con acarreo de 8 bits
template <typename TBusInterface> void Z80<TBusInterface>::adc(uint8_t oper8) {
    uint16_t res = regA + oper8;

    if (carryFlag) {
        res++;
    }

    carryFlag = res > 0xff;
    res &= 0xff;
    sz5h3pnFlags = sz53n_addTable[res];

    if (((regA ^ oper8 ^ res) & 0x10) != 0) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (((regA ^ ~oper8) & (regA ^ res)) > 0x7f) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }

    regA = res;
}

// Suma dos operandos de 16 bits sin carry afectando a los flags
template <typename TBusInterface> void Z80<TBusInterface>::add16(RegisterPair& reg16, uint16_t oper16) {
    uint32_t tmp = oper16 + reg16.word;

    REG_WZ = reg16.word + 1;
    carryFlag = tmp > 0xffff;
    reg16.word = tmp;
    sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | ((reg16.word >> 8) & FLAG_53_MASK);

    if ((reg16.word & 0x0fff) < (oper16 & 0x0fff)) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }
}

// Suma con acarreo de 16 bits
template <typename TBusInterface> void Z80<TBusInterface>::adc16(uint16_t reg16) {
    uint16_t tmpHL = REG_HL;
    REG_WZ = REG_HL + 1;

    uint32_t res = REG_HL + reg16;
    if (carryFlag) {
        res++;
    }

    carryFlag = res > 0xffff;
    res &= 0xffff;
    REG_HL = static_cast<uint16_t>(res);

    sz5h3pnFlags = sz53n_addTable[REG_H];
    if (res != 0) {
        sz5h3pnFlags &= ~ZERO_MASK;
    }

    if (((res ^ tmpHL ^ reg16) & 0x1000) != 0) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (((tmpHL ^ ~reg16) & (tmpHL ^ res)) > 0x7fff) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }
}

// Resta de 8 bits
template <typename TBusInterface> void Z80<TBusInterface>::sub(uint8_t oper8) {
    auto res = static_cast<int16_t>(regA - oper8);

    carryFlag = res < 0;
    res &= 0xff;
    sz5h3pnFlags = sz53n_subTable[res];

    /* El módulo 16 del resultado será mayor que el módulo 16 del registro A
     * si ha habido HalfCarry. Sucede lo mismo para todos los métodos resta
     * SIN carry, incluido cp */
    if ((res & 0x0f) > (regA & 0x0f)) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (((regA ^ oper8) & (regA ^ res)) > 0x7f) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }

    regA = res;
}

// Resta con acarreo de 8 bits
template <typename TBusInterface> void Z80<TBusInterface>::sbc(uint8_t oper8) {
    auto res = static_cast<int16_t>(regA - oper8);

    if (carryFlag) {
        res--;
    }

    carryFlag = res < 0;
    res &= 0xff;
    sz5h3pnFlags = sz53n_subTable[res];

    if (((regA ^ oper8 ^ res) & 0x10) != 0) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (((regA ^ oper8) & (regA ^ res)) > 0x7f) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }

    regA = res;
}

// Resta con acarreo de 16 bits
template <typename TBusInterface> void Z80<TBusInterface>::sbc16(uint16_t reg16) {
    uint16_t tmpHL = REG_HL;
    REG_WZ = REG_HL + 1;

    int32_t res = REG_HL - reg16;
    if (carryFlag) {
        res--;
    }

    carryFlag = res < 0;
    res &= 0xffff;
    REG_HL = static_cast<uint16_t>(res);

    sz5h3pnFlags = sz53n_subTable[REG_H];
    if (res != 0) {
        sz5h3pnFlags &= ~ZERO_MASK;
    }

    if (((res ^ tmpHL ^ reg16) & 0x1000) != 0) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (((tmpHL ^ reg16) & (tmpHL ^ res)) > 0x7fff) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }
}

// Operación AND lógica
template <typename TBusInterface> void Z80<TBusInterface>::and_(uint8_t oper8) {
    regA &= oper8;
    carryFlag = false;
    sz5h3pnFlags = sz53pn_addTable[regA] | HALFCARRY_MASK;
}

// Operación XOR lógica
template <typename TBusInterface> void Z80<TBusInterface>::xor_(uint8_t oper8) {
    regA ^= oper8;
    carryFlag = false;
    sz5h3pnFlags = sz53pn_addTable[regA];
}

// Operación OR lógica
template <typename TBusInterface> void Z80<TBusInterface>::or_(uint8_t oper8) {
    regA |= oper8;
    carryFlag = false;
    sz5h3pnFlags = sz53pn_addTable[regA];
}

// Operación de comparación con el registro A
// es como SUB, pero solo afecta a los flags
// Los flags SIGN y ZERO se calculan a partir del resultado
// Los flags 3 y 5 se copian desde el operando (sigh!)
template <typename TBusInterface> void Z80<TBusInterface>::cp(uint8_t oper8) {
    auto res = static_cast<int16_t>(regA - oper8);

    carryFlag = res < 0;
    res &= 0xff;

    sz5h3pnFlags = (sz53n_addTable[oper8] & FLAG_53_MASK)
                   | // No necesito preservar H, pero está a 0 en la tabla de todas formas
                   (sz53n_subTable[res] & FLAG_SZHN_MASK);

    if ((res & 0x0f) > (regA & 0x0f)) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (((regA ^ oper8) & (regA ^ res)) > 0x7f) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }
}

// DAA
template <typename TBusInterface> void Z80<TBusInterface>::daa() {
    uint8_t suma = 0;
    bool carry = carryFlag;

    if ((sz5h3pnFlags & HALFCARRY_MASK) != 0 || (regA & 0x0f) > 0x09) {
        suma = 6;
    }

    if (carry || (regA > 0x99)) {
        suma |= 0x60;
    }

    if (regA > 0x99) {
        carry = true;
    }

    if ((sz5h3pnFlags & ADDSUB_MASK) != 0) {
        sub(suma);
        sz5h3pnFlags = (sz5h3pnFlags & HALFCARRY_MASK) | sz53pn_subTable[regA];
    } else {
        add(suma);
        sz5h3pnFlags = (sz5h3pnFlags & HALFCARRY_MASK) | sz53pn_addTable[regA];
    }

    carryFlag = carry;
    // Los add/sub ya ponen el resto de los flags
}

// POP
template <typename TBusInterface> uint16_t Z80<TBusInterface>::pop() {
    uint16_t word = m_busInterface.peek16(REG_SP);
    REG_SP = REG_SP + 2;
    return word;
}

// PUSH
template <typename TBusInterface> void Z80<TBusInterface>::push(uint16_t word) {
    m_busInterface.poke8(--REG_SP, word >> 8);
    m_busInterface.poke8(--REG_SP, word);
}

// LDI
template <typename TBusInterface> void Z80<TBusInterface>::ldi() {
    uint8_t work8 = m_busInterface.peek8(REG_HL);
    m_busInterface.poke8(REG_DE, work8);
    m_busInterface.addressOnBus(REG_DE, 2);
    REG_HL++;
    REG_DE++;
    REG_BC--;
    work8 += regA;

    sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZ_MASK) | (work8 & BIT3_MASK);

    if ((work8 & ADDSUB_MASK) != 0) {
        sz5h3pnFlags |= BIT5_MASK;
    }

    if (REG_BC != 0) {
        sz5h3pnFlags |= PARITY_MASK;
    }
}

// LDD
template <typename TBusInterface> void Z80<TBusInterface>::ldd() {
    uint8_t work8 = m_busInterface.peek8(REG_HL);
    m_busInterface.poke8(REG_DE, work8);
    m_busInterface.addressOnBus(REG_DE, 2);
    REG_HL--;
    REG_DE--;
    REG_BC--;
    work8 += regA;

    sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZ_MASK) | (work8 & BIT3_MASK);

    if ((work8 & ADDSUB_MASK) != 0) {
        sz5h3pnFlags |= BIT5_MASK;
    }

    if (REG_BC != 0) {
        sz5h3pnFlags |= PARITY_MASK;
    }
}

// CPI
template <typename TBusInterface> void Z80<TBusInterface>::cpi() {
    uint8_t memHL = m_busInterface.peek8(REG_HL);
    bool carry = carryFlag; // lo guardo porque cp lo toca
    cp(memHL);
    carryFlag = carry;
    m_busInterface.addressOnBus(REG_HL, 5);
    REG_HL++;
    REG_BC--;
    memHL = regA - memHL - ((sz5h3pnFlags & HALFCARRY_MASK) != 0 ? 1 : 0);
    sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHN_MASK) | (memHL & BIT3_MASK);

    if ((memHL & ADDSUB_MASK) != 0) {
        sz5h3pnFlags |= BIT5_MASK;
    }

    if (REG_BC != 0) {
        sz5h3pnFlags |= PARITY_MASK;
    }

    REG_WZ++;
}

// CPD
template <typename TBusInterface> void Z80<TBusInterface>::cpd() {
    uint8_t memHL = m_busInterface.peek8(REG_HL);
    bool carry = carryFlag; // lo guardo porque cp lo toca
    cp(memHL);
    carryFlag = carry;
    m_busInterface.addressOnBus(REG_HL, 5);
    REG_HL--;
    REG_BC--;
    memHL = regA - memHL - ((sz5h3pnFlags & HALFCARRY_MASK) != 0 ? 1 : 0);
    sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHN_MASK) | (memHL & BIT3_MASK);

    if ((memHL & ADDSUB_MASK) != 0) {
        sz5h3pnFlags |= BIT5_MASK;
    }

    if (REG_BC != 0) {
        sz5h3pnFlags |= PARITY_MASK;
    }

    REG_WZ--;
}

// INI
template <typename TBusInterface> void Z80<TBusInterface>::ini() {
    REG_WZ = REG_BC;
    m_busInterface.addressOnBus(getPairIR().word, 1);
    uint8_t work8 = m_busInterface.inPort(REG_WZ++);
    m_busInterface.poke8(REG_HL, work8);

    REG_B--;
    REG_HL++;

    sz5h3pnFlags = sz53pn_addTable[REG_B];
    if (work8 > 0x7f) {
        sz5h3pnFlags |= ADDSUB_MASK;
    }

    carryFlag = false;
    uint16_t tmp = work8 + ((REG_C + 1) & 0xff);
    if (tmp > 0xff) {
        sz5h3pnFlags |= HALFCARRY_MASK;
        carryFlag = true;
    }

    if ((sz53pn_addTable[((tmp & 0x07) ^ REG_B)] & PARITY_MASK) == PARITY_MASK) {
        sz5h3pnFlags |= PARITY_MASK;
    } else {
        sz5h3pnFlags &= ~PARITY_MASK;
    }
}

// IND
template <typename TBusInterface> void Z80<TBusInterface>::ind() {
    REG_WZ = REG_BC;
    m_busInterface.addressOnBus(getPairIR().word, 1);
    uint8_t work8 = m_busInterface.inPort(REG_WZ--);
    m_busInterface.poke8(REG_HL, work8);

    REG_B--;
    REG_HL--;

    sz5h3pnFlags = sz53pn_addTable[REG_B];
    if (work8 > 0x7f) {
        sz5h3pnFlags |= ADDSUB_MASK;
    }

    carryFlag = false;
    uint16_t tmp = work8 + ((REG_C - 1) & 0xff);
    if (tmp > 0xff) {
        sz5h3pnFlags |= HALFCARRY_MASK;
        carryFlag = true;
    }

    if ((sz53pn_addTable[((tmp & 0x07) ^ REG_B)] & PARITY_MASK) == PARITY_MASK) {
        sz5h3pnFlags |= PARITY_MASK;
    } else {
        sz5h3pnFlags &= ~PARITY_MASK;
    }
}

// OUTI
template <typename TBusInterface> void Z80<TBusInterface>::outi() {

    m_busInterface.addressOnBus(getPairIR().word, 1);

    REG_B--;
    REG_WZ = REG_BC;

    uint8_t work8 = m_busInterface.peek8(REG_HL);
    m_busInterface.outPort(REG_WZ++, work8);

    REG_HL++;

    carryFlag = false;
    if (work8 > 0x7f) {
        sz5h3pnFlags = sz53n_subTable[REG_B];
    } else {
        sz5h3pnFlags = sz53n_addTable[REG_B];
    }

    if ((REG_L + work8) > 0xff) {
        sz5h3pnFlags |= HALFCARRY_MASK;
        carryFlag = true;
    }

    if ((sz53pn_addTable[(((REG_L + work8) & 0x07) ^ REG_B)] & PARITY_MASK) == PARITY_MASK) {
        sz5h3pnFlags |= PARITY_MASK;
    }
}

// OUTD
template <typename TBusInterface> void Z80<TBusInterface>::outd() {

    m_busInterface.addressOnBus(getPairIR().word, 1);

    REG_B--;
    REG_WZ = REG_BC;

    uint8_t work8 = m_busInterface.peek8(REG_HL);
    m_busInterface.outPort(REG_WZ--, work8);

    REG_HL--;

    carryFlag = false;
    if (work8 > 0x7f) {
        sz5h3pnFlags = sz53n_subTable[REG_B];
    } else {
        sz5h3pnFlags = sz53n_addTable[REG_B];
    }

    if ((REG_L + work8) > 0xff) {
        sz5h3pnFlags |= HALFCARRY_MASK;
        carryFlag = true;
    }

    if ((sz53pn_addTable[(((REG_L + work8) & 0x07) ^ REG_B)] & PARITY_MASK) == PARITY_MASK) {
        sz5h3pnFlags |= PARITY_MASK;
    }
}

// Pone a 1 el Flag Z si el bit b del registro
// r es igual a 0
/*
 * En contra de lo que afirma el Z80-Undocumented, los bits 3 y 5 toman
 * SIEMPRE el valor de los bits correspondientes del valor a comparar para
 * las instrucciones BIT n,r. Para BIT n,(HL) toman el valor del registro
 * escondido (REG_WZ), y para las BIT n, (IX/IY+n) toman el valor de los
 * bits superiores de la dirección indicada por IX/IY+n.
 *
 * 04/12/08 Confirmado el comentario anterior:
 *          http://scratchpad.wikia.com/wiki/Z80
 */
template <typename TBusInterface> void Z80<TBusInterface>::bitTest(uint8_t mask, uint8_t reg) {
    bool zeroFlag = (mask & reg) == 0;

    sz5h3pnFlags = (sz53n_addTable[reg] & ~FLAG_SZP_MASK) | HALFCARRY_MASK;

    if (zeroFlag) {
        sz5h3pnFlags |= (PARITY_MASK | ZERO_MASK);
    }

    if (mask == SIGN_MASK && !zeroFlag) {
        sz5h3pnFlags |= SIGN_MASK;
    }
}

// Interrupción
/* Desglose de la interrupción, según el modo:
 * IM0:
 *      M1: 7 T-Estados -> reconocer INT y decSP
 *      M2: 3 T-Estados -> escribir byte alto y decSP
 *      M3: 3 T-Estados -> escribir byte bajo y salto a N
 * IM1:
 *      M1: 7 T-Estados -> reconocer INT y decSP
 *      M2: 3 T-Estados -> escribir byte alto PC y decSP
 *      M3: 3 T-Estados -> escribir byte bajo PC y PC=0x0038
 * IM2:
 *      M1: 7 T-Estados -> reconocer INT y decSP
 *      M2: 3 T-Estados -> escribir byte alto y decSP
 *      M3: 3 T-Estados -> escribir byte bajo
 *      M4: 3 T-Estados -> leer byte bajo del vector de INT
 *      M5: 3 T-Estados -> leer byte alto y saltar a la rutina de INT
 */
template <typename TBusInterface> void Z80<TBusInterface>::interrupt() {
    // Si estaba en un HALT esperando una INT, lo saca de la espera
    halted = false;

    m_busInterface.interruptHandlingTime(7);

    regR++;
    ffIFF1 = ffIFF2 = false;
    push(REG_PC); // el push añadirá 6 t-estados (+contended si toca)
    if (modeINT == IntMode::IM2) {
        REG_PC = m_busInterface.peek16((regI << 8) | 0xff); // +6 t-estados
    } else {
        REG_PC = 0x0038;
    }
    REG_WZ = REG_PC;
}

// Interrupción NMI, no utilizado por ahora
/* Desglose de ciclos de máquina y T-Estados
 * M1: 5 T-Estados -> extraer opcode (pá ná, es tontería) y decSP
 * M2: 3 T-Estados -> escribe byte alto de PC y decSP
 * M3: 3 T-Estados -> escrib e byte bajo de PC y PC=0x0066
 */
template <typename TBusInterface> void Z80<TBusInterface>::nmi() {
    halted = false;
    // Esta lectura consigue dos cosas:
    //      1.- La lectura del opcode del M1 que se descarta
    //      2.- Si estaba en un HALT esperando una INT, lo saca de la espera
    m_busInterface.fetchOpcode(REG_PC);
    m_busInterface.interruptHandlingTime(1);
    regR++;
    ffIFF1 = false;
    push(REG_PC); // 3+3 t-estados + contended si procede
    REG_PC = REG_WZ = 0x0066;
}

template <typename TBusInterface> void Z80<TBusInterface>::execute() {
    prefixOpcode = 0;

    if (halted) {
        m_opCode = m_busInterface.fetchOpcode(REG_PC);
        regR++;
    } else {
        uint8_t currentPrefix = 0;
        bool firstByteOfInstruction = true;

        while (true) {
            m_opCode = m_busInterface.fetchOpcode(REG_PC);
            regR++;

#ifdef WITH_BREAKPOINT_SUPPORT
            if (breakpointEnabled && currentPrefix == 0) {
                m_opCode = m_busInterface.breakpoint(REG_PC, m_opCode);
            }
#endif

            REG_PC++;

            if (firstByteOfInstruction && currentPrefix == 0) {
                // Optimization: Most instructions modify flags, so set flagQ = true by default.
                // Only instructions that do NOT modify flags need to set flagQ = false.
                flagQ = true;
                pendingEI = false;
            }

            switch (currentPrefix) {
                case 0x00:
                    decodeOpcode(m_opCode);
                    break;
                case 0xDD:
                    decodeDDFD(m_opCode, regIX);
                    break;
                case 0xED:
                    decodeED(m_opCode);
                    break;
                case 0xFD:
                    decodeDDFD(m_opCode, regIY);
                    break;
                default:
                    return;
            }

            if (Z80_UNLIKELY(prefixOpcode != 0)) {
                currentPrefix = prefixOpcode;
                prefixOpcode = 0;
                firstByteOfInstruction = false;
                continue;
            }

            break;
        }

        lastFlagQ = flagQ;

#ifdef WITH_EXEC_DONE
        if (Z80_UNLIKELY(execDone)) {
            m_busInterface.execDone();
        }
#endif
    }

    // Primero se comprueba NMI
    // Si se activa NMI no se comprueba INT porque la siguiente
    // instrucción debe ser la de 0x0066.
    if (Z80_UNLIKELY(activeNMI)) {
        activeNMI = false;
        lastFlagQ = false;
        nmi();
        return;
    }

    // Ahora se comprueba si está activada la señal INT
    if (Z80_UNLIKELY(ffIFF1 && !pendingEI && m_busInterface.isActiveINT())) {
        lastFlagQ = false;
        interrupt();
    }
}

template <typename TBusInterface> void Z80<TBusInterface>::decodeOpcode(uint8_t opCode) {

    switch (opCode) {
        default:
        case 0x00: /* NOP */
            break;

        case 0x01: /* LD BC,nn */
            REG_BC = m_busInterface.peek16(REG_PC);
            REG_PC = REG_PC + 2;
            break;

        case 0x02: /* LD (BC),A */
            m_busInterface.poke8(REG_BC, regA);
            REG_W = regA;
            REG_Z = REG_C + 1;
            // REG_WZ = (regA << 8) | (REG_C + 1);
            break;

        case 0x03: /* INC BC */
            m_busInterface.addressOnBus(getPairIR().word, 2);
            REG_BC++;
            break;

        case 0x04: /* INC B */
            inc8(REG_B);
            break;

        case 0x05: /* DEC B */
            dec8(REG_B);
            break;

        case 0x06: /* LD B,n */
            REG_B = m_busInterface.peek8(REG_PC);
            REG_PC++;
            break;

        case 0x07: { /* RLCA */
            carryFlag = (regA > 0x7f);
            regA <<= 1;
            if (carryFlag) {
                regA |= CARRY_MASK;
            }
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (regA & FLAG_53_MASK);
            break;
        }
        case 0x08: /* EX AF,AF' */ {
            uint8_t work8 = regA;
            regA = REG_Ax;
            REG_Ax = work8;

            work8 = getFlags();
            setFlags(REG_Fx);
            REG_Fx = work8;
            break;
        }

        case 0x09: /* ADD HL,BC */
            m_busInterface.addressOnBus(getPairIR().word, 7);
            add16(regHL, REG_BC);
            break;

        case 0x0A: /* LD A,(BC) */
            regA = m_busInterface.peek8(REG_BC);
            REG_WZ = REG_BC + 1;
            break;

        case 0x0B: /* DEC BC */
            m_busInterface.addressOnBus(getPairIR().word, 2);
            REG_BC--;
            break;

        case 0x0C: /* INC C */
            inc8(REG_C);
            break;

        case 0x0D: /* DEC C */
            dec8(REG_C);
            break;

        case 0x0E: /* LD C,n */
            REG_C = m_busInterface.peek8(REG_PC);
            REG_PC++;
            break;

        case 0x0F: { /* RRCA */
            carryFlag = (regA & CARRY_MASK) != 0;
            regA >>= 1;
            if (carryFlag) {
                regA |= SIGN_MASK;
            }
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (regA & FLAG_53_MASK);
            break;
        }
        case 0x10: { /* DJNZ e */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            auto offset = static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            if (--REG_B != 0) {
                m_busInterface.addressOnBus(REG_PC, 5);
                REG_PC = REG_WZ = REG_PC + offset + 1;
            } else {
                REG_PC++;
            }
            break;
        }
        case 0x11: /* LD DE,nn */
            REG_DE = m_busInterface.peek16(REG_PC);
            REG_PC = REG_PC + 2;
            break;

        case 0x12: /* LD (DE),A */
            m_busInterface.poke8(REG_DE, regA);
            REG_W = regA;
            REG_Z = REG_E + 1;
            // REG_WZ = (regA << 8) | (REG_E + 1);
            break;

        case 0x13: /* INC DE */
            m_busInterface.addressOnBus(getPairIR().word, 2);
            REG_DE++;
            break;

        case 0x14: /* INC D */
            inc8(REG_D);
            break;

        case 0x15: /* DEC D */
            dec8(REG_D);
            break;

        case 0x16: /* LD D,n */
            REG_D = m_busInterface.peek8(REG_PC);
            REG_PC++;
            break;

        case 0x17: { /* RLA */
            bool oldCarry = carryFlag;
            carryFlag = regA > 0x7f;
            regA <<= 1;
            if (oldCarry) {
                regA |= CARRY_MASK;
            }
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (regA & FLAG_53_MASK);
            break;
        }
        case 0x18: { /* JR e */
            auto offset = static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC = REG_WZ = REG_PC + offset + 1;
            break;
        }
        case 0x19: /* ADD HL,DE */
            m_busInterface.addressOnBus(getPairIR().word, 7);
            add16(regHL, REG_DE);
            break;

        case 0x1A: /* LD A,(DE) */
            regA = m_busInterface.peek8(REG_DE);
            REG_WZ = REG_DE + 1;
            break;

        case 0x1B: /* DEC DE */
            m_busInterface.addressOnBus(getPairIR().word, 2);
            REG_DE--;
            break;

        case 0x1C: /* INC E */
            inc8(REG_E);
            break;

        case 0x1D: /* DEC E */
            dec8(REG_E);
            break;

        case 0x1E: /* LD E,n */
            REG_E = m_busInterface.peek8(REG_PC);
            REG_PC++;
            break;

        case 0x1F: { /* RRA */
            bool oldCarry = carryFlag;
            carryFlag = (regA & CARRY_MASK) != 0;
            regA >>= 1;
            if (oldCarry) {
                regA |= SIGN_MASK;
            }
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (regA & FLAG_53_MASK);
            break;
        }
        case 0x20: { /* JR NZ,e */
            auto offset = static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            if ((sz5h3pnFlags & ZERO_MASK) == 0) {
                m_busInterface.addressOnBus(REG_PC, 5);
                REG_PC += offset;
                REG_WZ = REG_PC + 1;
            }
            REG_PC++;
            break;
        }
        case 0x21: /* LD HL,nn */
            REG_HL = m_busInterface.peek16(REG_PC);
            REG_PC = REG_PC + 2;
            break;

        case 0x22: /* LD (nn),HL */
            REG_WZ = m_busInterface.peek16(REG_PC);
            m_busInterface.poke16(REG_WZ, regHL);
            REG_WZ++;
            REG_PC = REG_PC + 2;
            break;

        case 0x23: /* INC HL */
            m_busInterface.addressOnBus(getPairIR().word, 2);
            REG_HL++;
            break;

        case 0x24: /* INC H */
            inc8(REG_H);
            break;

        case 0x25: /* DEC H */
            dec8(REG_H);
            break;

        case 0x26: /* LD H,n */
            REG_H = m_busInterface.peek8(REG_PC);
            REG_PC++;
            break;

        case 0x27: /* DAA */
            daa();
            break;

        case 0x28: { /* JR Z,e */
            auto offset = static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            if ((sz5h3pnFlags & ZERO_MASK) != 0) {
                m_busInterface.addressOnBus(REG_PC, 5);
                REG_PC += offset;
                REG_WZ = REG_PC + 1;
            }
            REG_PC++;
            break;
        }
        case 0x29: /* ADD HL,HL */
            m_busInterface.addressOnBus(getPairIR().word, 7);
            add16(regHL, REG_HL);
            break;

        case 0x2A: /* LD HL,(nn) */
            REG_WZ = m_busInterface.peek16(REG_PC);
            REG_HL = m_busInterface.peek16(REG_WZ);
            REG_WZ++;
            REG_PC = REG_PC + 2;
            break;

        case 0x2B: /* DEC HL */
            m_busInterface.addressOnBus(getPairIR().word, 2);
            REG_HL--;
            break;

        case 0x2C: /* INC L */
            inc8(REG_L);
            break;

        case 0x2D: /* DEC L */
            dec8(REG_L);
            break;

        case 0x2E: /* LD L,n */
            REG_L = m_busInterface.peek8(REG_PC);
            REG_PC++;
            break;

        case 0x2F: /* CPL */
            regA ^= 0xff;
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | HALFCARRY_MASK | (regA & FLAG_53_MASK) | ADDSUB_MASK;
            break;

        case 0x30: { /* JR NC,e */
            auto offset = static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            if (!carryFlag) {
                m_busInterface.addressOnBus(REG_PC, 5);
                REG_PC += offset;
                REG_WZ = REG_PC + 1;
            }
            REG_PC++;
            break;
        }
        case 0x31: /* LD SP,nn */
            REG_SP = m_busInterface.peek16(REG_PC);
            REG_PC = REG_PC + 2;
            break;

        case 0x32: /* LD (nn),A */
            REG_WZ = m_busInterface.peek16(REG_PC);
            m_busInterface.poke8(REG_WZ, regA);
            REG_WZ = (regA << 8) | ((REG_WZ + 1) & 0xff);
            REG_PC = REG_PC + 2;
            break;

        case 0x33: /* INC SP */
            m_busInterface.addressOnBus(getPairIR().word, 2);
            REG_SP++;
            break;

        case 0x34: /* INC (HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL);
            inc8(work8);
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0x35: /* DEC (HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL);
            dec8(work8);
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0x36: /* LD (HL),n */
            m_busInterface.poke8(REG_HL, m_busInterface.peek8(REG_PC));
            REG_PC++;
            break;

        case 0x37: /* SCF */ {
            uint8_t regQ = lastFlagQ ? sz5h3pnFlags : 0;
            carryFlag = true;
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (((regQ ^ sz5h3pnFlags) | regA) & FLAG_53_MASK);
            break;
        }

        case 0x38: { /* JR C,e */
            auto offset = static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            if (carryFlag) {
                m_busInterface.addressOnBus(REG_PC, 5);
                REG_PC += offset;
                REG_WZ = REG_PC + 1;
            }
            REG_PC++;
            break;
        }
        case 0x39: /* ADD HL,SP */
            m_busInterface.addressOnBus(getPairIR().word, 7);
            add16(regHL, REG_SP);
            break;

        case 0x3A: /* LD A,(nn) */
            REG_WZ = m_busInterface.peek16(REG_PC);
            regA = m_busInterface.peek8(REG_WZ);
            REG_WZ++;
            REG_PC = REG_PC + 2;
            break;

        case 0x3B: /* DEC SP */
            m_busInterface.addressOnBus(getPairIR().word, 2);
            REG_SP--;
            break;

        case 0x3C: /* INC A */
            inc8(regA);
            break;

        case 0x3D: /* DEC A */
            dec8(regA);
            break;

        case 0x3E: /* LD A,n */
            regA = m_busInterface.peek8(REG_PC);
            REG_PC++;
            break;

        case 0x3F: { /* CCF */
            uint8_t regQ = lastFlagQ ? sz5h3pnFlags : 0;
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (((regQ ^ sz5h3pnFlags) | regA) & FLAG_53_MASK);
            if (carryFlag) {
                sz5h3pnFlags |= HALFCARRY_MASK;
            }
            carryFlag = !carryFlag;
            break;
        }
        case 0x40: /* LD B,B */
            break;

        case 0x41: /* LD B,C */
            REG_B = REG_C;
            break;

        case 0x42: /* LD B,D */
            REG_B = REG_D;
            break;

        case 0x43: /* LD B,E */
            REG_B = REG_E;
            break;

        case 0x44: /* LD B,H */
            REG_B = REG_H;
            break;

        case 0x45: /* LD B,L */
            REG_B = REG_L;
            break;

        case 0x46: /* LD B,(HL) */
            REG_B = m_busInterface.peek8(REG_HL);
            break;

        case 0x47: /* LD B,A */
            REG_B = regA;
            break;

        case 0x48: /* LD C,B */
            REG_C = REG_B;
            break;

        case 0x49: /* LD C,C */
            break;

        case 0x4A: /* LD C,D */
            REG_C = REG_D;
            break;

        case 0x4B: /* LD C,E */
            REG_C = REG_E;
            break;

        case 0x4C: /* LD C,H */
            REG_C = REG_H;
            break;

        case 0x4D: /* LD C,L */
            REG_C = REG_L;
            break;

        case 0x4E: /* LD C,(HL) */
            REG_C = m_busInterface.peek8(REG_HL);
            break;

        case 0x4F: /* LD C,A */
            REG_C = regA;
            break;

        case 0x50: /* LD D,B */
            REG_D = REG_B;
            break;

        case 0x51: /* LD D,C */
            REG_D = REG_C;
            break;

        case 0x52: /* LD D,D */
            break;

        case 0x53: /* LD D,E */
            REG_D = REG_E;
            break;

        case 0x54: /* LD D,H */
            REG_D = REG_H;
            break;

        case 0x55: /* LD D,L */
            REG_D = REG_L;
            break;

        case 0x56: /* LD D,(HL) */
            REG_D = m_busInterface.peek8(REG_HL);
            break;

        case 0x57: /* LD D,A */
            REG_D = regA;
            break;

        case 0x58: /* LD E,B */
            REG_E = REG_B;
            break;

        case 0x59: /* LD E,C */
            REG_E = REG_C;
            break;

        case 0x5A: /* LD E,D */
            REG_E = REG_D;
            break;

        case 0x5B: /* LD E,E */
            break;

        case 0x5C: /* LD E,H */
            REG_E = REG_H;
            break;

        case 0x5D: /* LD E,L */
            REG_E = REG_L;
            break;

        case 0x5E: /* LD E,(HL) */
            REG_E = m_busInterface.peek8(REG_HL);
            break;

        case 0x5F: /* LD E,A */
            REG_E = regA;
            break;

        case 0x60: /* LD H,B */
            REG_H = REG_B;
            break;

        case 0x61: /* LD H,C */
            REG_H = REG_C;
            break;

        case 0x62: /* LD H,D */
            REG_H = REG_D;
            break;

        case 0x63: /* LD H,E */
            REG_H = REG_E;
            break;

        case 0x64: /* LD H,H */
            break;

        case 0x65: /* LD H,L */
            REG_H = REG_L;
            break;

        case 0x66: /* LD H,(HL) */
            REG_H = m_busInterface.peek8(REG_HL);
            break;

        case 0x67: /* LD H,A */
            REG_H = regA;
            break;

        case 0x68: /* LD L,B */
            REG_L = REG_B;
            break;

        case 0x69: /* LD L,C */
            REG_L = REG_C;
            break;

        case 0x6A: /* LD L,D */
            REG_L = REG_D;
            break;

        case 0x6B: /* LD L,E */
            REG_L = REG_E;
            break;

        case 0x6C: /* LD L,H */
            REG_L = REG_H;
            break;

        case 0x6D: /* LD L,L */
            break;

        case 0x6E: /* LD L,(HL) */
            REG_L = m_busInterface.peek8(REG_HL);
            break;

        case 0x6F: /* LD L,A */
            REG_L = regA;
            break;

        case 0x70: /* LD (HL),B */
            m_busInterface.poke8(REG_HL, REG_B);
            break;

        case 0x71: /* LD (HL),C */
            m_busInterface.poke8(REG_HL, REG_C);
            break;

        case 0x72: /* LD (HL),D */
            m_busInterface.poke8(REG_HL, REG_D);
            break;

        case 0x73: /* LD (HL),E */
            m_busInterface.poke8(REG_HL, REG_E);
            break;

        case 0x74: /* LD (HL),H */
            m_busInterface.poke8(REG_HL, REG_H);
            break;

        case 0x75: /* LD (HL),L */
            m_busInterface.poke8(REG_HL, REG_L);
            break;

        case 0x76: /* HALT */
            halted = true;
            break;

        case 0x77: /* LD (HL),A */
            m_busInterface.poke8(REG_HL, regA);
            break;

        case 0x78: /* LD A,B */
            regA = REG_B;
            break;

        case 0x79: /* LD A,C */
            regA = REG_C;
            break;

        case 0x7A: /* LD A,D */
            regA = REG_D;
            break;

        case 0x7B: /* LD A,E */
            regA = REG_E;
            break;

        case 0x7C: /* LD A,H */
            regA = REG_H;
            break;

        case 0x7D: /* LD A,L */
            regA = REG_L;
            break;

        case 0x7E: /* LD A,(HL) */
            regA = m_busInterface.peek8(REG_HL);
            break;

        case 0x7F: /* LD A,A */
            break;

        case 0x80: /* ADD A,B */
            add(REG_B);
            break;

        case 0x81: /* ADD A,C */
            add(REG_C);
            break;

        case 0x82: /* ADD A,D */
            add(REG_D);
            break;

        case 0x83: /* ADD A,E */
            add(REG_E);
            break;

        case 0x84: /* ADD A,H */
            add(REG_H);
            break;

        case 0x85: /* ADD A,L */
            add(REG_L);
            break;

        case 0x86: /* ADD A,(HL) */
            add(m_busInterface.peek8(REG_HL));
            break;

        case 0x87: /* ADD A,A */
            add(regA);
            break;

        case 0x88: /* ADC A,B */
            adc(REG_B);
            break;

        case 0x89: /* ADC A,C */
            adc(REG_C);
            break;

        case 0x8A: /* ADC A,D */
            adc(REG_D);
            break;

        case 0x8B: /* ADC A,E */
            adc(REG_E);
            break;

        case 0x8C: /* ADC A,H */
            adc(REG_H);
            break;

        case 0x8D: /* ADC A,L */
            adc(REG_L);
            break;

        case 0x8E: /* ADC A,(HL) */
            adc(m_busInterface.peek8(REG_HL));
            break;

        case 0x8F: /* ADC A,A */
            adc(regA);
            break;

        case 0x90: /* SUB B */
            sub(REG_B);
            break;

        case 0x91: /* SUB C */
            sub(REG_C);
            break;

        case 0x92: /* SUB D */
            sub(REG_D);
            break;

        case 0x93: /* SUB E */
            sub(REG_E);
            break;

        case 0x94: /* SUB H */
            sub(REG_H);
            break;

        case 0x95: /* SUB L */
            sub(REG_L);
            break;

        case 0x96: /* SUB (HL) */
            sub(m_busInterface.peek8(REG_HL));
            break;

        case 0x97: /* SUB A */
            sub(regA);
            break;

        case 0x98: /* SBC A,B */
            sbc(REG_B);
            break;

        case 0x99: /* SBC A,C */
            sbc(REG_C);
            break;

        case 0x9A: /* SBC A,D */
            sbc(REG_D);
            break;

        case 0x9B: /* SBC A,E */
            sbc(REG_E);
            break;

        case 0x9C: /* SBC A,H */
            sbc(REG_H);
            break;

        case 0x9D: /* SBC A,L */
            sbc(REG_L);
            break;

        case 0x9E: /* SBC A,(HL) */
            sbc(m_busInterface.peek8(REG_HL));
            break;

        case 0x9F: /* SBC A,A */
            sbc(regA);
            break;

        case 0xA0: /* AND B */
            and_(REG_B);
            break;

        case 0xA1: /* AND C */
            and_(REG_C);
            break;

        case 0xA2: /* AND D */
            and_(REG_D);
            break;

        case 0xA3: /* AND E */
            and_(REG_E);
            break;

        case 0xA4: /* AND H */
            and_(REG_H);
            break;

        case 0xA5: /* AND L */
            and_(REG_L);
            break;

        case 0xA6: /* AND (HL) */
            and_(m_busInterface.peek8(REG_HL));
            break;

        case 0xA7: /* AND A */
            and_(regA);
            break;

        case 0xA8: /* XOR B */
            xor_(REG_B);
            break;

        case 0xA9: /* XOR C */
            xor_(REG_C);
            break;

        case 0xAA: /* XOR D */
            xor_(REG_D);
            break;

        case 0xAB: /* XOR E */
            xor_(REG_E);
            break;

        case 0xAC: /* XOR H */
            xor_(REG_H);
            break;

        case 0xAD: /* XOR L */
            xor_(REG_L);
            break;

        case 0xAE: /* XOR (HL) */
            xor_(m_busInterface.peek8(REG_HL));
            break;

        case 0xAF: /* XOR A */
            xor_(regA);
            break;

        case 0xB0: /* OR B */
            or_(REG_B);
            break;

        case 0xB1: /* OR C */
            or_(REG_C);
            break;

        case 0xB2: /* OR D */
            or_(REG_D);
            break;

        case 0xB3: /* OR E */
            or_(REG_E);
            break;

        case 0xB4: /* OR H */
            or_(REG_H);
            break;

        case 0xB5: /* OR L */
            or_(REG_L);
            break;

        case 0xB6: /* OR (HL) */
            or_(m_busInterface.peek8(REG_HL));
            break;

        case 0xB7: /* OR A */
            or_(regA);
            break;

        case 0xB8: /* CP B */
            cp(REG_B);
            break;

        case 0xB9: /* CP C */
            cp(REG_C);
            break;

        case 0xBA: /* CP D */
            cp(REG_D);
            break;

        case 0xBB: /* CP E */
            cp(REG_E);
            break;

        case 0xBC: /* CP H */
            cp(REG_H);
            break;

        case 0xBD: /* CP L */
            cp(REG_L);
            break;

        case 0xBE: /* CP (HL) */
            cp(m_busInterface.peek8(REG_HL));
            break;

        case 0xBF: /* CP A */
            cp(regA);
            break;

        case 0xC0: { /* RET NZ */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            if ((sz5h3pnFlags & ZERO_MASK) == 0) {
                REG_PC = REG_WZ = pop();
            }
            break;
        }
        case 0xC1: /* POP BC */
            REG_BC = pop();
            break;

        case 0xC2: { /* JP NZ,nn */
            REG_WZ = m_busInterface.peek16(REG_PC);
            if ((sz5h3pnFlags & ZERO_MASK) == 0) {
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        }
        case 0xC3: /* JP nn */
            REG_WZ = REG_PC = m_busInterface.peek16(REG_PC);
            break;

        case 0xC4: { /* CALL NZ,nn */
            REG_WZ = m_busInterface.peek16(REG_PC);
            if ((sz5h3pnFlags & ZERO_MASK) == 0) {
                m_busInterface.addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        }
        case 0xC5: /* PUSH BC */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            push(REG_BC);
            break;

        case 0xC6: /* ADD A,n */
            add(m_busInterface.peek8(REG_PC));
            REG_PC++;
            break;

        case 0xC7: /* RST 00H */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x00;
            break;

        case 0xC8: { /* RET Z */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            if ((sz5h3pnFlags & ZERO_MASK) != 0) {
                REG_PC = REG_WZ = pop();
            }
            break;
        }
        case 0xC9: /* RET */
            REG_PC = REG_WZ = pop();
            break;

        case 0xCA: { /* JP Z,nn */
            REG_WZ = m_busInterface.peek16(REG_PC);
            if ((sz5h3pnFlags & ZERO_MASK) != 0) {
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        }
        case 0xCB: /* Subconjunto de instrucciones */
            decodeCB();
            break;

        case 0xCC: { /* CALL Z,nn */
            REG_WZ = m_busInterface.peek16(REG_PC);
            if ((sz5h3pnFlags & ZERO_MASK) != 0) {
                m_busInterface.addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        }
        case 0xCD: /* CALL nn */
            REG_WZ = m_busInterface.peek16(REG_PC);
            m_busInterface.addressOnBus(REG_PC + 1, 1);
            push(REG_PC + 2);
            REG_PC = REG_WZ;
            break;

        case 0xCE: /* ADC A,n */
            adc(m_busInterface.peek8(REG_PC));
            REG_PC++;
            break;

        case 0xCF: /* RST 08H */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x08;
            break;

        case 0xD0: { /* RET NC */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            if (!carryFlag) {
                REG_PC = REG_WZ = pop();
            }
            break;
        }
        case 0xD1: /* POP DE */
            REG_DE = pop();
            break;

        case 0xD2: { /* JP NC,nn */
            REG_WZ = m_busInterface.peek16(REG_PC);
            if (!carryFlag) {
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        }
        case 0xD3: /* OUT (n),A */ {
            uint8_t work8 = m_busInterface.peek8(REG_PC);
            REG_PC++;
            REG_WZ = regA << 8;
            m_busInterface.outPort(REG_WZ | work8, regA);
            REG_WZ |= (work8 + 1);
            break;
        }

        case 0xD4: { /* CALL NC,nn */
            REG_WZ = m_busInterface.peek16(REG_PC);
            if (!carryFlag) {
                m_busInterface.addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        }
        case 0xD5: /* PUSH DE */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            push(REG_DE);
            break;

        case 0xD6: /* SUB n */
            sub(m_busInterface.peek8(REG_PC));
            REG_PC++;
            break;

        case 0xD7: /* RST 10H */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x10;
            break;

        case 0xD8: { /* RET C */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            if (carryFlag) {
                REG_PC = REG_WZ = pop();
            }
            break;
        }
        case 0xD9: /* EXX */ {
            uint16_t tmp = 0;
            tmp = REG_BC;
            REG_BC = REG_BCx;
            REG_BCx = tmp;

            tmp = REG_DE;
            REG_DE = REG_DEx;
            REG_DEx = tmp;

            tmp = REG_HL;
            REG_HL = REG_HLx;
            REG_HLx = tmp;
            break;
        }

        case 0xDA: { /* JP C,nn */
            REG_WZ = m_busInterface.peek16(REG_PC);
            if (carryFlag) {
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        }
        case 0xDB: /* IN A,(n) */
            REG_W = regA;
            REG_Z = m_busInterface.peek8(REG_PC);
            // REG_WZ = (regA << 8) | m_busInterface.peek8(REG_PC);
            REG_PC++;
            regA = m_busInterface.inPort(REG_WZ);
            REG_WZ++;
            break;

        case 0xDC: { /* CALL C,nn */
            REG_WZ = m_busInterface.peek16(REG_PC);
            if (carryFlag) {
                m_busInterface.addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        }
        case 0xDD: /* Subconjunto de instrucciones */
            opCode = m_busInterface.fetchOpcode(REG_PC++);
            regR++;
            decodeDDFD(opCode, regIX);
            break;

        case 0xDE: /* SBC A,n */
            sbc(m_busInterface.peek8(REG_PC));
            REG_PC++;
            break;

        case 0xDF: /* RST 18H */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x18;
            break;

        case 0xE0: /* RET PO */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            if ((sz5h3pnFlags & PARITY_MASK) == 0) {
                REG_PC = REG_WZ = pop();
            }
            break;
        case 0xE1: /* POP HL */
            REG_HL = pop();
            break;
        case 0xE2: /* JP PO,nn */
            REG_WZ = m_busInterface.peek16(REG_PC);
            if ((sz5h3pnFlags & PARITY_MASK) == 0) {
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        case 0xE3: /* EX (SP),HL */ {
            // Instrucción de ejecución sutil.
            RegisterPair work = regHL;
            REG_HL = m_busInterface.peek16(REG_SP);
            m_busInterface.addressOnBus(REG_SP + 1, 1);
            // No se usa poke16 porque el Z80 escribe los bytes AL REVES
            m_busInterface.poke8(REG_SP + 1, work.byte8.hi);
            m_busInterface.poke8(REG_SP, work.byte8.lo);
            m_busInterface.addressOnBus(REG_SP, 2);
            REG_WZ = REG_HL;
            break;
        }

        case 0xE4: /* CALL PO,nn */
            REG_WZ = m_busInterface.peek16(REG_PC);
            if ((sz5h3pnFlags & PARITY_MASK) == 0) {
                m_busInterface.addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        case 0xE5: /* PUSH HL */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            push(REG_HL);
            break;
        case 0xE6: /* AND n */
            and_(m_busInterface.peek8(REG_PC));
            REG_PC++;
            break;
        case 0xE7: /* RST 20H */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x20;
            break;
        case 0xE8: /* RET PE */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            if ((sz5h3pnFlags & PARITY_MASK) != 0) {
                REG_PC = REG_WZ = pop();
            }
            break;
        case 0xE9: /* JP (HL) */
            REG_PC = REG_HL;
            break;
        case 0xEA: /* JP PE,nn */
            REG_WZ = m_busInterface.peek16(REG_PC);
            if ((sz5h3pnFlags & PARITY_MASK) != 0) {
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        case 0xEB: /* EX DE,HL */ {
            uint16_t tmp = REG_HL;
            REG_HL = REG_DE;
            REG_DE = tmp;
            break;
        }

        case 0xEC: /* CALL PE,nn */
            REG_WZ = m_busInterface.peek16(REG_PC);
            if ((sz5h3pnFlags & PARITY_MASK) != 0) {
                m_busInterface.addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        case 0xED: /*Subconjunto de instrucciones*/
            opCode = m_busInterface.fetchOpcode(REG_PC++);
            regR++;
            decodeED(opCode);
            break;
        case 0xEE: /* XOR n */
            xor_(m_busInterface.peek8(REG_PC));
            REG_PC++;
            break;
        case 0xEF: /* RST 28H */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x28;
            break;
        case 0xF0: /* RET P */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            if (sz5h3pnFlags < SIGN_MASK) {
                REG_PC = REG_WZ = pop();
            }
            break;
        case 0xF1: /* POP AF */
            setRegAF(pop());
            break;
        case 0xF2: /* JP P,nn */
            REG_WZ = m_busInterface.peek16(REG_PC);
            if (sz5h3pnFlags < SIGN_MASK) {
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        case 0xF3: /* DI */
            ffIFF1 = ffIFF2 = false;
            break;
        case 0xF4: /* CALL P,nn */
            REG_WZ = m_busInterface.peek16(REG_PC);
            if (sz5h3pnFlags < SIGN_MASK) {
                m_busInterface.addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        case 0xF5: /* PUSH AF */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            push(getRegAF());
            break;
        case 0xF6: /* OR n */
            or_(m_busInterface.peek8(REG_PC));
            REG_PC++;
            break;
        case 0xF7: /* RST 30H */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x30;
            break;
        case 0xF8: /* RET M */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            if (sz5h3pnFlags > 0x7f) {
                REG_PC = REG_WZ = pop();
            }
            break;
        case 0xF9: /* LD SP,HL */
            m_busInterface.addressOnBus(getPairIR().word, 2);
            REG_SP = REG_HL;
            break;
        case 0xFA: /* JP M,nn */
            REG_WZ = m_busInterface.peek16(REG_PC);
            if (sz5h3pnFlags > 0x7f) {
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        case 0xFB: /* EI */
            ffIFF1 = ffIFF2 = true;
            pendingEI = true;
            break;
        case 0xFC: /* CALL M,nn */
            REG_WZ = m_busInterface.peek16(REG_PC);
            if (sz5h3pnFlags > 0x7f) {
                m_busInterface.addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        case 0xFD: /* Subconjunto de instrucciones */
            opCode = m_busInterface.fetchOpcode(REG_PC++);
            regR++;
            decodeDDFD(opCode, regIY);
            break;
        case 0xFE: /* CP n */
            cp(m_busInterface.peek8(REG_PC));
            REG_PC++;
            break;
        case 0xFF: /* RST 38H */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x38;
    } /* del switch( codigo ) */
}

// Subconjunto de instrucciones 0xCB

template <typename TBusInterface> void Z80<TBusInterface>::decodeCB() {
    uint8_t opCode = m_busInterface.fetchOpcode(REG_PC++);
    regR++;

    switch (opCode) {
        case 0x00: /* RLC B */
            rlc(REG_B);
            break;

        case 0x01: /* RLC C */
            rlc(REG_C);
            break;

        case 0x02: /* RLC D */
            rlc(REG_D);
            break;

        case 0x03: /* RLC E */
            rlc(REG_E);
            break;

        case 0x04: /* RLC H */
            rlc(REG_H);
            break;

        case 0x05: /* RLC L */
            rlc(REG_L);
            break;

        case 0x06: /* RLC (HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL);
            rlc(work8);
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0x07: /* RLC A */
            rlc(regA);
            break;

        case 0x08: /* RRC B */
            rrc(REG_B);
            break;

        case 0x09: /* RRC C */
            rrc(REG_C);
            break;

        case 0x0A: /* RRC D */
            rrc(REG_D);
            break;

        case 0x0B: /* RRC E */
            rrc(REG_E);
            break;

        case 0x0C: /* RRC H */
            rrc(REG_H);
            break;

        case 0x0D: /* RRC L */
            rrc(REG_L);
            break;

        case 0x0E: /* RRC (HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL);
            rrc(work8);
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0x0F: /* RRC A */
            rrc(regA);
            break;

        case 0x10: /* RL B */
            rl(REG_B);
            break;

        case 0x11: /* RL C */
            rl(REG_C);
            break;

        case 0x12: /* RL D */
            rl(REG_D);
            break;

        case 0x13: /* RL E */
            rl(REG_E);
            break;

        case 0x14: /* RL H */
            rl(REG_H);
            break;

        case 0x15: /* RL L */
            rl(REG_L);
            break;

        case 0x16: /* RL (HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL);
            rl(work8);
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0x17: /* RL A */
            rl(regA);
            break;

        case 0x18: /* RR B */
            rr(REG_B);
            break;

        case 0x19: /* RR C */
            rr(REG_C);
            break;

        case 0x1A: /* RR D */
            rr(REG_D);
            break;

        case 0x1B: /* RR E */
            rr(REG_E);
            break;

        case 0x1C: /*RR H*/
            rr(REG_H);
            break;

        case 0x1D: /* RR L */
            rr(REG_L);
            break;

        case 0x1E: /* RR (HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL);
            rr(work8);
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0x1F: /* RR A */
            rr(regA);
            break;

        case 0x20: /* SLA B */
            sla(REG_B);
            break;

        case 0x21: /* SLA C */
            sla(REG_C);
            break;

        case 0x22: /* SLA D */
            sla(REG_D);
            break;

        case 0x23: /* SLA E */
            sla(REG_E);
            break;

        case 0x24: /* SLA H */
            sla(REG_H);
            break;

        case 0x25: /* SLA L */
            sla(REG_L);
            break;

        case 0x26: /* SLA (HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL);
            sla(work8);
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0x27: /* SLA A */
            sla(regA);
            break;

        case 0x28: /* SRA B */
            sra(REG_B);
            break;

        case 0x29: /* SRA C */
            sra(REG_C);
            break;

        case 0x2A: /* SRA D */
            sra(REG_D);
            break;

        case 0x2B: /* SRA E */
            sra(REG_E);
            break;

        case 0x2C: /* SRA H */
            sra(REG_H);
            break;

        case 0x2D: /* SRA L */
            sra(REG_L);
            break;

        case 0x2E: /* SRA (HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL);
            sra(work8);
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0x2F: /* SRA A */
            sra(regA);
            break;

        case 0x30: /* SLL B */
            sll(REG_B);
            break;

        case 0x31: /* SLL C */
            sll(REG_C);
            break;

        case 0x32: /* SLL D */
            sll(REG_D);
            break;

        case 0x33: /* SLL E */
            sll(REG_E);
            break;

        case 0x34: /* SLL H */
            sll(REG_H);
            break;

        case 0x35: /* SLL L */
            sll(REG_L);
            break;

        case 0x36: /* SLL (HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL);
            sll(work8);
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0x37: /* SLL A */
            sll(regA);
            break;

        case 0x38: /* SRL B */
            srl(REG_B);
            break;

        case 0x39: /* SRL C */
            srl(REG_C);
            break;

        case 0x3A: /* SRL D */
            srl(REG_D);
            break;

        case 0x3B: /* SRL E */
            srl(REG_E);
            break;

        case 0x3C: /* SRL H */
            srl(REG_H);
            break;

        case 0x3D: /* SRL L */
            srl(REG_L);
            break;

        case 0x3E: /* SRL (HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL);
            srl(work8);
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0x3F: /* SRL A */
            srl(regA);
            break;

        case 0x40: /* BIT 0,B */
            bitTest(0x01, REG_B);
            break;

        case 0x41: /* BIT 0,C */
            bitTest(0x01, REG_C);
            break;

        case 0x42: /* BIT 0,D */
            bitTest(0x01, REG_D);
            break;

        case 0x43: /* BIT 0,E */
            bitTest(0x01, REG_E);
            break;

        case 0x44: /* BIT 0,H */
            bitTest(0x01, REG_H);
            break;

        case 0x45: /* BIT 0,L */
            bitTest(0x01, REG_L);
            break;

        case 0x46: /* BIT 0,(HL) */
            bitTest(0x01, m_busInterface.peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            m_busInterface.addressOnBus(REG_HL, 1);
            break;

        case 0x47: /* BIT 0,A */
            bitTest(0x01, regA);
            break;

        case 0x48: /* BIT 1,B */
            bitTest(0x02, REG_B);
            break;

        case 0x49: /* BIT 1,C */
            bitTest(0x02, REG_C);
            break;

        case 0x4A: /* BIT 1,D */
            bitTest(0x02, REG_D);
            break;

        case 0x4B: /* BIT 1,E */
            bitTest(0x02, REG_E);
            break;

        case 0x4C: /* BIT 1,H */
            bitTest(0x02, REG_H);
            break;

        case 0x4D: /* BIT 1,L */
            bitTest(0x02, REG_L);
            break;

        case 0x4E: /* BIT 1,(HL) */
            bitTest(0x02, m_busInterface.peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            m_busInterface.addressOnBus(REG_HL, 1);
            break;

        case 0x4F: /* BIT 1,A */
            bitTest(0x02, regA);
            break;

        case 0x50: /* BIT 2,B */
            bitTest(0x04, REG_B);
            break;

        case 0x51: /* BIT 2,C */
            bitTest(0x04, REG_C);
            break;

        case 0x52: /* BIT 2,D */
            bitTest(0x04, REG_D);
            break;

        case 0x53: /* BIT 2,E */
            bitTest(0x04, REG_E);
            break;

        case 0x54: /* BIT 2,H */
            bitTest(0x04, REG_H);
            break;

        case 0x55: /* BIT 2,L */
            bitTest(0x04, REG_L);
            break;

        case 0x56: /* BIT 2,(HL) */
            bitTest(0x04, m_busInterface.peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            m_busInterface.addressOnBus(REG_HL, 1);
            break;

        case 0x57: /* BIT 2,A */
            bitTest(0x04, regA);
            break;

        case 0x58: /* BIT 3,B */
            bitTest(0x08, REG_B);
            break;

        case 0x59: /* BIT 3,C */
            bitTest(0x08, REG_C);
            break;

        case 0x5A: /* BIT 3,D */
            bitTest(0x08, REG_D);
            break;

        case 0x5B: /* BIT 3,E */
            bitTest(0x08, REG_E);
            break;

        case 0x5C: /* BIT 3,H */
            bitTest(0x08, REG_H);
            break;

        case 0x5D: /* BIT 3,L */
            bitTest(0x08, REG_L);
            break;

        case 0x5E: /* BIT 3,(HL) */
            bitTest(0x08, m_busInterface.peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            m_busInterface.addressOnBus(REG_HL, 1);
            break;

        case 0x5F: /* BIT 3,A */
            bitTest(0x08, regA);
            break;

        case 0x60: /* BIT 4,B */
            bitTest(0x10, REG_B);
            break;

        case 0x61: /* BIT 4,C */
            bitTest(0x10, REG_C);
            break;

        case 0x62: /* BIT 4,D */
            bitTest(0x10, REG_D);
            break;

        case 0x63: /* BIT 4,E */
            bitTest(0x10, REG_E);
            break;

        case 0x64: /* BIT 4,H */
            bitTest(0x10, REG_H);
            break;

        case 0x65: /* BIT 4,L */
            bitTest(0x10, REG_L);
            break;

        case 0x66: /* BIT 4,(HL) */
            bitTest(0x10, m_busInterface.peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            m_busInterface.addressOnBus(REG_HL, 1);
            break;

        case 0x67: /* BIT 4,A */
            bitTest(0x10, regA);
            break;

        case 0x68: /* BIT 5,B */
            bitTest(0x20, REG_B);
            break;

        case 0x69: /* BIT 5,C */
            bitTest(0x20, REG_C);
            break;

        case 0x6A: /* BIT 5,D */
            bitTest(0x20, REG_D);
            break;

        case 0x6B: /* BIT 5,E */
            bitTest(0x20, REG_E);
            break;

        case 0x6C: /* BIT 5,H */
            bitTest(0x20, REG_H);
            break;

        case 0x6D: /* BIT 5,L */
            bitTest(0x20, REG_L);
            break;

        case 0x6E: /* BIT 5,(HL) */
            bitTest(0x20, m_busInterface.peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            m_busInterface.addressOnBus(REG_HL, 1);
            break;

        case 0x6F: /* BIT 5,A */
            bitTest(0x20, regA);
            break;

        case 0x70: /* BIT 6,B */
            bitTest(0x40, REG_B);
            break;

        case 0x71: /* BIT 6,C */
            bitTest(0x40, REG_C);
            break;

        case 0x72: /* BIT 6,D */
            bitTest(0x40, REG_D);
            break;

        case 0x73: /* BIT 6,E */
            bitTest(0x40, REG_E);
            break;

        case 0x74: /* BIT 6,H */
            bitTest(0x40, REG_H);
            break;

        case 0x75: /* BIT 6,L */
            bitTest(0x40, REG_L);
            break;

        case 0x76: /* BIT 6,(HL) */
            bitTest(0x40, m_busInterface.peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            m_busInterface.addressOnBus(REG_HL, 1);
            break;

        case 0x77: /* BIT 6,A */
            bitTest(0x40, regA);
            break;

        case 0x78: /* BIT 7,B */
            bitTest(0x80, REG_B);
            break;

        case 0x79: /* BIT 7,C */
            bitTest(0x80, REG_C);
            break;

        case 0x7A: /* BIT 7,D */
            bitTest(0x80, REG_D);
            break;

        case 0x7B: /* BIT 7,E */
            bitTest(0x80, REG_E);
            break;

        case 0x7C: /* BIT 7,H */
            bitTest(0x80, REG_H);
            break;

        case 0x7D: /* BIT 7,L */
            bitTest(0x80, REG_L);
            break;

        case 0x7E: /* BIT 7,(HL) */
            bitTest(0x80, m_busInterface.peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            m_busInterface.addressOnBus(REG_HL, 1);
            break;

        case 0x7F: /* BIT 7,A */
            bitTest(0x80, regA);
            break;

        case 0x80: /* RES 0,B */
            REG_B &= 0xFE;
            break;

        case 0x81: /* RES 0,C */
            REG_C &= 0xFE;
            break;

        case 0x82: /* RES 0,D */
            REG_D &= 0xFE;
            break;

        case 0x83: /* RES 0,E */
            REG_E &= 0xFE;
            break;

        case 0x84: /* RES 0,H */
            REG_H &= 0xFE;
            break;

        case 0x85: /* RES 0,L */
            REG_L &= 0xFE;
            break;

        case 0x86: /* RES 0,(HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL) & 0xFE;
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0x87: /* RES 0,A */
            regA &= 0xFE;
            break;

        case 0x88: /* RES 1,B */
            REG_B &= 0xFD;
            break;

        case 0x89: /* RES 1,C */
            REG_C &= 0xFD;
            break;

        case 0x8A: /* RES 1,D */
            REG_D &= 0xFD;
            break;

        case 0x8B: /* RES 1,E */
            REG_E &= 0xFD;
            break;

        case 0x8C: /* RES 1,H */
            REG_H &= 0xFD;
            break;

        case 0x8D: /* RES 1,L */
            REG_L &= 0xFD;
            break;

        case 0x8E: /* RES 1,(HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL) & 0xFD;
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0x8F: /* RES 1,A */
            regA &= 0xFD;
            break;

        case 0x90: /* RES 2,B */
            REG_B &= 0xFB;
            break;

        case 0x91: /* RES 2,C */
            REG_C &= 0xFB;
            break;

        case 0x92: /* RES 2,D */
            REG_D &= 0xFB;
            break;

        case 0x93: /* RES 2,E */
            REG_E &= 0xFB;
            break;

        case 0x94: /* RES 2,H */
            REG_H &= 0xFB;
            break;

        case 0x95: /* RES 2,L */
            REG_L &= 0xFB;
            break;

        case 0x96: /* RES 2,(HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL) & 0xFB;
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0x97: /* RES 2,A */
            regA &= 0xFB;
            break;

        case 0x98: /* RES 3,B */
            REG_B &= 0xF7;
            break;

        case 0x99: /* RES 3,C */
            REG_C &= 0xF7;
            break;

        case 0x9A: /* RES 3,D */
            REG_D &= 0xF7;
            break;

        case 0x9B: /* RES 3,E */
            REG_E &= 0xF7;
            break;

        case 0x9C: /* RES 3,H */
            REG_H &= 0xF7;
            break;

        case 0x9D: /* RES 3,L */
            REG_L &= 0xF7;
            break;

        case 0x9E: /* RES 3,(HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL) & 0xF7;
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0x9F: /* RES 3,A */
            regA &= 0xF7;
            break;

        case 0xA0: /* RES 4,B */
            REG_B &= 0xEF;
            break;

        case 0xA1: /* RES 4,C */
            REG_C &= 0xEF;
            break;

        case 0xA2: /* RES 4,D */
            REG_D &= 0xEF;
            break;

        case 0xA3: /* RES 4,E */
            REG_E &= 0xEF;
            break;

        case 0xA4: /* RES 4,H */
            REG_H &= 0xEF;
            break;

        case 0xA5: /* RES 4,L */
            REG_L &= 0xEF;
            break;

        case 0xA6: /* RES 4,(HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL) & 0xEF;
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0xA7: /* RES 4,A */
            regA &= 0xEF;
            break;

        case 0xA8: /* RES 5,B */
            REG_B &= 0xDF;
            break;

        case 0xA9: /* RES 5,C */
            REG_C &= 0xDF;
            break;

        case 0xAA: /* RES 5,D */
            REG_D &= 0xDF;
            break;

        case 0xAB: /* RES 5,E */
            REG_E &= 0xDF;
            break;

        case 0xAC: /* RES 5,H */
            REG_H &= 0xDF;
            break;

        case 0xAD: /* RES 5,L */
            REG_L &= 0xDF;
            break;

        case 0xAE: /* RES 5,(HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL) & 0xDF;
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0xAF: /* RES 5,A */
            regA &= 0xDF;
            break;

        case 0xB0: /* RES 6,B */
            REG_B &= 0xBF;
            break;

        case 0xB1: /* RES 6,C */
            REG_C &= 0xBF;
            break;

        case 0xB2: /* RES 6,D */
            REG_D &= 0xBF;
            break;

        case 0xB3: /* RES 6,E */
            REG_E &= 0xBF;
            break;

        case 0xB4: /* RES 6,H */
            REG_H &= 0xBF;
            break;

        case 0xB5: /* RES 6,L */
            REG_L &= 0xBF;
            break;

        case 0xB6: /* RES 6,(HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL) & 0xBF;
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0xB7: /* RES 6,A */
            regA &= 0xBF;
            break;

        case 0xB8: /* RES 7,B */
            REG_B &= 0x7F;
            break;

        case 0xB9: /* RES 7,C */
            REG_C &= 0x7F;
            break;

        case 0xBA: /* RES 7,D */
            REG_D &= 0x7F;
            break;

        case 0xBB: /* RES 7,E */
            REG_E &= 0x7F;
            break;

        case 0xBC: /* RES 7,H */
            REG_H &= 0x7F;
            break;

        case 0xBD: /* RES 7,L */
            REG_L &= 0x7F;
            break;

        case 0xBE: /* RES 7,(HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL) & 0x7F;
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0xBF: /* RES 7,A */
            regA &= 0x7F;
            break;

        case 0xC0: /* SET 0,B */
            REG_B |= 0x01;
            break;

        case 0xC1: /* SET 0,C */
            REG_C |= 0x01;
            break;

        case 0xC2: /* SET 0,D */
            REG_D |= 0x01;
            break;

        case 0xC3: /* SET 0,E */
            REG_E |= 0x01;
            break;

        case 0xC4: /* SET 0,H */
            REG_H |= 0x01;
            break;

        case 0xC5: /* SET 0,L */
            REG_L |= 0x01;
            break;

        case 0xC6: /* SET 0,(HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL) | 0x01;
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0xC7: /* SET 0,A */
            regA |= 0x01;
            break;

        case 0xC8: /* SET 1,B */
            REG_B |= 0x02;
            break;

        case 0xC9: /* SET 1,C */
            REG_C |= 0x02;
            break;

        case 0xCA: /* SET 1,D */
            REG_D |= 0x02;
            break;

        case 0xCB: /* SET 1,E */
            REG_E |= 0x02;
            break;

        case 0xCC: /* SET 1,H */
            REG_H |= 0x02;
            break;

        case 0xCD: /* SET 1,L */
            REG_L |= 0x02;
            break;

        case 0xCE: /* SET 1,(HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL) | 0x02;
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0xCF: /* SET 1,A */
            regA |= 0x02;
            break;

        case 0xD0: /* SET 2,B */
            REG_B |= 0x04;
            break;

        case 0xD1: /* SET 2,C */
            REG_C |= 0x04;
            break;

        case 0xD2: /* SET 2,D */
            REG_D |= 0x04;
            break;

        case 0xD3: /* SET 2,E */
            REG_E |= 0x04;
            break;

        case 0xD4: /* SET 2,H */
            REG_H |= 0x04;
            break;

        case 0xD5: /* SET 2,L */
            REG_L |= 0x04;
            break;

        case 0xD6: /* SET 2,(HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL) | 0x04;
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0xD7: /* SET 2,A */
            regA |= 0x04;
            break;

        case 0xD8: /* SET 3,B */
            REG_B |= 0x08;
            break;

        case 0xD9: /* SET 3,C */
            REG_C |= 0x08;
            break;

        case 0xDA: /* SET 3,D */
            REG_D |= 0x08;
            break;

        case 0xDB: /* SET 3,E */
            REG_E |= 0x08;
            break;

        case 0xDC: /* SET 3,H */
            REG_H |= 0x08;
            break;

        case 0xDD: /* SET 3,L */
            REG_L |= 0x08;
            break;

        case 0xDE: /* SET 3,(HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL) | 0x08;
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0xDF: /* SET 3,A */
            regA |= 0x08;
            break;

        case 0xE0: /* SET 4,B */
            REG_B |= 0x10;
            break;

        case 0xE1: /* SET 4,C */
            REG_C |= 0x10;
            break;

        case 0xE2: /* SET 4,D */
            REG_D |= 0x10;
            break;

        case 0xE3: /* SET 4,E */
            REG_E |= 0x10;
            break;

        case 0xE4: /* SET 4,H */
            REG_H |= 0x10;
            break;

        case 0xE5: /* SET 4,L */
            REG_L |= 0x10;
            break;

        case 0xE6: /* SET 4,(HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL) | 0x10;
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0xE7: /* SET 4,A */
            regA |= 0x10;
            break;

        case 0xE8: /* SET 5,B */
            REG_B |= 0x20;
            break;

        case 0xE9: /* SET 5,C */
            REG_C |= 0x20;
            break;

        case 0xEA: /* SET 5,D */
            REG_D |= 0x20;
            break;

        case 0xEB: /* SET 5,E */
            REG_E |= 0x20;
            break;

        case 0xEC: /* SET 5,H */
            REG_H |= 0x20;
            break;

        case 0xED: /* SET 5,L */
            REG_L |= 0x20;
            break;

        case 0xEE: /* SET 5,(HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL) | 0x20;
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0xEF: /* SET 5,A */
            regA |= 0x20;
            break;

        case 0xF0: /* SET 6,B */
            REG_B |= 0x40;
            break;

        case 0xF1: /* SET 6,C */
            REG_C |= 0x40;
            break;

        case 0xF2: /* SET 6,D */
            REG_D |= 0x40;
            break;

        case 0xF3: /* SET 6,E */
            REG_E |= 0x40;
            break;

        case 0xF4: /* SET 6,H */
            REG_H |= 0x40;
            break;

        case 0xF5: /* SET 6,L */
            REG_L |= 0x40;
            break;

        case 0xF6: /* SET 6,(HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL) | 0x40;
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0xF7: /* SET 6,A */
            regA |= 0x40;
            break;

        case 0xF8: /* SET 7,B */
            REG_B |= 0x80;
            break;

        case 0xF9: /* SET 7,C */
            REG_C |= 0x80;
            break;

        case 0xFA: /* SET 7,D */
            REG_D |= 0x80;
            break;

        case 0xFB: /* SET 7,E */
            REG_E |= 0x80;
            break;

        case 0xFC: /* SET 7,H */
            REG_H |= 0x80;
            break;

        case 0xFD: /* SET 7,L */
            REG_L |= 0x80;
            break;

        case 0xFE: /* SET 7,(HL) */ {
            uint8_t work8 = m_busInterface.peek8(REG_HL) | 0x80;
            m_busInterface.addressOnBus(REG_HL, 1);
            m_busInterface.poke8(REG_HL, work8);
            break;
        }

        case 0xFF: /* SET 7,A */
            regA |= 0x80;
            break;

        default:
            break;
    }
}

// Subconjunto de instrucciones 0xDD / 0xFD
/*
 * Hay que tener en cuenta el manejo de secuencias códigos DD/FD que no
 * hacen nada. Según el apartado 3.7 del documento
 * [http://www.myquest.nl/z80undocumented/z80-documented-v0.91.pdf]
 * secuencias de códigos como FD DD 00 21 00 10 NOP NOP NOP LD HL,1000h
 * activan IY con el primer FD, IX con el segundo DD y vuelven al
 * registro HL con el código NOP. Es decir, si detrás del código DD/FD no
 * viene una instrucción que maneje el registro HL, el código DD/FD
 * "se olvida" y hay que procesar la instrucción como si nunca se
 * hubiera visto el prefijo (salvo por los 4 t-estados que ha costado).
 * Naturalmente, en una serie repetida de DDFD no hay que comprobar las
 * interrupciones entre cada prefijo.
 */
template <typename TBusInterface> void Z80<TBusInterface>::decodeDDFD(uint8_t opCode, RegisterPair& regIXY) {
    switch (opCode) {
        case 0x09: /* ADD IX,BC */
            m_busInterface.addressOnBus(getPairIR().word, 7);
            add16(regIXY, REG_BC);
            break;

        case 0x19: /* ADD IX,DE */
            m_busInterface.addressOnBus(getPairIR().word, 7);
            add16(regIXY, REG_DE);
            break;

        case 0x21: /* LD IX,nn */
            regIXY.word = m_busInterface.peek16(REG_PC);
            REG_PC = REG_PC + 2;
            break;

        case 0x22: /* LD (nn),IX */
            REG_WZ = m_busInterface.peek16(REG_PC);
            m_busInterface.poke16(REG_WZ++, regIXY);
            REG_PC = REG_PC + 2;
            break;

        case 0x23: /* INC IX */
            m_busInterface.addressOnBus(getPairIR().word, 2);
            regIXY.word++;
            break;

        case 0x24: /* INC IXh */
            inc8(regIXY.byte8.hi);
            break;

        case 0x25: /* DEC IXh */
            dec8(regIXY.byte8.hi);
            break;

        case 0x26: /* LD IXh,n */
            regIXY.byte8.hi = m_busInterface.peek8(REG_PC);
            REG_PC++;
            break;

        case 0x29: /* ADD IX,IX */
            m_busInterface.addressOnBus(getPairIR().word, 7);
            add16(regIXY, regIXY.word);
            break;

        case 0x2A: /* LD IX,(nn) */
            REG_WZ = m_busInterface.peek16(REG_PC);
            regIXY.word = m_busInterface.peek16(REG_WZ++);
            REG_PC = REG_PC + 2;
            break;

        case 0x2B: /* DEC IX */
            m_busInterface.addressOnBus(getPairIR().word, 2);
            regIXY.word--;
            break;

        case 0x2C: /* INC IXl */
            inc8(regIXY.byte8.lo);
            break;

        case 0x2D: /* DEC IXl */
            dec8(regIXY.byte8.lo);
            break;

        case 0x2E: /* LD IXl,n */
            regIXY.byte8.lo = m_busInterface.peek8(REG_PC);
            REG_PC++;
            break;

        case 0x34: /* INC (IX+d) */ {
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            uint8_t work8 = m_busInterface.peek8(REG_WZ);
            m_busInterface.addressOnBus(REG_WZ, 1);
            inc8(work8);
            m_busInterface.poke8(REG_WZ, work8);
            break;
        }

        case 0x35: /* DEC (IX+d) */ {
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            uint8_t work8 = m_busInterface.peek8(REG_WZ);
            m_busInterface.addressOnBus(REG_WZ, 1);
            dec8(work8);
            m_busInterface.poke8(REG_WZ, work8);
            break;
        }

        case 0x36: /* LD (IX+d),n */ {
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            REG_PC++;
            uint8_t work8 = m_busInterface.peek8(REG_PC);
            m_busInterface.addressOnBus(REG_PC, 2);
            REG_PC++;
            m_busInterface.poke8(REG_WZ, work8);
            break;
        }

        case 0x39: /* ADD IX,SP */
            m_busInterface.addressOnBus(getPairIR().word, 7);
            add16(regIXY, REG_SP);
            break;

        case 0x44: /* LD B,IXh */
            REG_B = regIXY.byte8.hi;
            break;

        case 0x45: /* LD B,IXl */
            REG_B = regIXY.byte8.lo;
            break;

        case 0x46: /* LD B,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_B = m_busInterface.peek8(REG_WZ);
            break;

        case 0x4C: /* LD C,IXh */
            REG_C = regIXY.byte8.hi;
            break;

        case 0x4D: /* LD C,IXl */
            REG_C = regIXY.byte8.lo;
            break;

        case 0x4E: /* LD C,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_C = m_busInterface.peek8(REG_WZ);
            break;

        case 0x54: /* LD D,IXh */
            REG_D = regIXY.byte8.hi;
            break;

        case 0x55: /* LD D,IXl */
            REG_D = regIXY.byte8.lo;
            break;

        case 0x56: /* LD D,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_D = m_busInterface.peek8(REG_WZ);
            break;

        case 0x5C: /* LD E,IXh */
            REG_E = regIXY.byte8.hi;
            break;

        case 0x5D: /* LD E,IXl */
            REG_E = regIXY.byte8.lo;
            break;

        case 0x5E: /* LD E,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_E = m_busInterface.peek8(REG_WZ);
            break;

        case 0x60: /* LD IXh,B */
            regIXY.byte8.hi = REG_B;
            break;

        case 0x61: /* LD IXh,C */
            regIXY.byte8.hi = REG_C;
            break;

        case 0x62: /* LD IXh,D */
            regIXY.byte8.hi = REG_D;
            break;

        case 0x63: /* LD IXh,E */
            regIXY.byte8.hi = REG_E;
            break;

        case 0x64: /* LD IXh,IXh */
            break;

        case 0x65: /* LD IXh,IXl */
            regIXY.byte8.hi = regIXY.byte8.lo;
            break;

        case 0x66: /* LD H,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_H = m_busInterface.peek8(REG_WZ);
            break;

        case 0x67: /* LD IXh,A */
            regIXY.byte8.hi = regA;
            break;

        case 0x68: /* LD IXl,B */
            regIXY.byte8.lo = REG_B;
            break;

        case 0x69: /* LD IXl,C */
            regIXY.byte8.lo = REG_C;
            break;

        case 0x6A: /* LD IXl,D */
            regIXY.byte8.lo = REG_D;
            break;

        case 0x6B: /* LD IXl,E */
            regIXY.byte8.lo = REG_E;
            break;

        case 0x6C: /* LD IXl,IXh */
            regIXY.byte8.lo = regIXY.byte8.hi;
            break;

        case 0x6D: /* LD IXl,IXl */
            break;

        case 0x6E: /* LD L,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_L = m_busInterface.peek8(REG_WZ);
            break;

        case 0x6F: /* LD IXl,A */
            regIXY.byte8.lo = regA;
            break;

        case 0x70: /* LD (IX+d),B */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            m_busInterface.poke8(REG_WZ, REG_B);
            break;

        case 0x71: /* LD (IX+d),C */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            m_busInterface.poke8(REG_WZ, REG_C);
            break;

        case 0x72: /* LD (IX+d),D */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            m_busInterface.poke8(REG_WZ, REG_D);
            break;

        case 0x73: /* LD (IX+d),E */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            m_busInterface.poke8(REG_WZ, REG_E);
            break;

        case 0x74: /* LD (IX+d),H */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            m_busInterface.poke8(REG_WZ, REG_H);
            break;

        case 0x75: /* LD (IX+d),L */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            m_busInterface.poke8(REG_WZ, REG_L);
            break;

        case 0x77: /* LD (IX+d),A */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            m_busInterface.poke8(REG_WZ, regA);
            break;

        case 0x7C: /* LD A,IXh */
            regA = regIXY.byte8.hi;
            break;

        case 0x7D: /* LD A,IXl */
            regA = regIXY.byte8.lo;
            break;

        case 0x7E: /* LD A,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            regA = m_busInterface.peek8(REG_WZ);
            break;

        case 0x84: /* ADD A,IXh */
            add(regIXY.byte8.hi);
            break;

        case 0x85: /* ADD A,IXl */
            add(regIXY.byte8.lo);
            break;

        case 0x86: /* ADD A,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            add(m_busInterface.peek8(REG_WZ));
            break;

        case 0x8C: /* ADC A,IXh */
            adc(regIXY.byte8.hi);
            break;

        case 0x8D: /* ADC A,IXl */
            adc(regIXY.byte8.lo);
            break;

        case 0x8E: /* ADC A,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            adc(m_busInterface.peek8(REG_WZ));
            break;

        case 0x94: /* SUB IXh */
            sub(regIXY.byte8.hi);
            break;

        case 0x95: /* SUB IXl */
            sub(regIXY.byte8.lo);
            break;

        case 0x96: /* SUB (IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            sub(m_busInterface.peek8(REG_WZ));
            break;

        case 0x9C: /* SBC A,IXh */
            sbc(regIXY.byte8.hi);
            break;

        case 0x9D: /* SBC A,IXl */
            sbc(regIXY.byte8.lo);
            break;

        case 0x9E: /* SBC A,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            sbc(m_busInterface.peek8(REG_WZ));
            break;

        case 0xA4: /* AND IXh */
            and_(regIXY.byte8.hi);
            break;

        case 0xA5: /* AND IXl */
            and_(regIXY.byte8.lo);
            break;

        case 0xA6: /* AND (IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            and_(m_busInterface.peek8(REG_WZ));
            break;

        case 0xAC: /* XOR IXh */
            xor_(regIXY.byte8.hi);
            break;

        case 0xAD: /* XOR IXl */
            xor_(regIXY.byte8.lo);
            break;

        case 0xAE: /* XOR (IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            xor_(m_busInterface.peek8(REG_WZ));
            break;

        case 0xB4: /* OR IXh */
            or_(regIXY.byte8.hi);
            break;

        case 0xB5: /* OR IXl */
            or_(regIXY.byte8.lo);
            break;

        case 0xB6: /* OR (IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            or_(m_busInterface.peek8(REG_WZ));
            break;

        case 0xBC: /* CP IXh */
            cp(regIXY.byte8.hi);
            break;

        case 0xBD: /* CP IXl */
            cp(regIXY.byte8.lo);
            break;

        case 0xBE: /* CP (IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            m_busInterface.addressOnBus(REG_PC, 5);
            REG_PC++;
            cp(m_busInterface.peek8(REG_WZ));
            break;

        case 0xCB: /* Subconjunto de instrucciones */
            REG_WZ = regIXY.word + static_cast<int8_t>(m_busInterface.peek8(REG_PC));
            REG_PC++;
            opCode = m_busInterface.peek8(REG_PC);
            m_busInterface.addressOnBus(REG_PC, 2);
            REG_PC++;
            decodeDDFDCB(opCode, REG_WZ);
            break;

        case 0xDD:
            prefixOpcode = 0xDD;
            break;
        case 0xE1: /* POP IX */
            regIXY.word = pop();
            break;

        case 0xE3: /* EX (SP),IX */ {
            // Instrucción de ejecución sutil como pocas... atento al dato.
            RegisterPair work16 = regIXY;
            regIXY.word = m_busInterface.peek16(REG_SP);
            m_busInterface.addressOnBus(REG_SP + 1, 1);
            /* I can't call poke16 from here because the Z80 CPU does the writes in inverted order.
             * Same thing goes for EX (SP), HL.
             */
            m_busInterface.poke8(REG_SP + 1, work16.byte8.hi);
            m_busInterface.poke8(REG_SP, work16.byte8.lo);
            m_busInterface.addressOnBus(REG_SP, 2);
            REG_WZ = regIXY.word;
            break;
        }

        case 0xE5: /* PUSH IX */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            push(regIXY.word);
            break;

        case 0xE9: /* JP (IX) */
            REG_PC = regIXY.word;
            break;

        case 0xED:
            prefixOpcode = 0xED;
            break;

        case 0xF9: /* LD SP,IX */
            m_busInterface.addressOnBus(getPairIR().word, 2);
            REG_SP = regIXY.word;
            break;

        case 0xFD:
            prefixOpcode = 0xFD;
            break;

        default: {
            // Detrás de un DD/FD o varios en secuencia venía un código
            // que no correspondía con una instrucción que involucra a
            // IX o IY. Se trata como si fuera un código normal.
            // Sin esto, además de emular mal, falla el test
            // ld <bcdexya>,<bcdexya> de ZEXALL.
#ifdef WITH_BREAKPOINT_SUPPORT
            if (breakpointEnabled && prefixOpcode == 0) {
                opCode = m_busInterface.breakpoint(REG_PC, opCode);
            }
#endif
            decodeOpcode(opCode);
            break;
        }
    }
}

// Subconjunto de instrucciones 0xDDCB
template <typename TBusInterface> void Z80<TBusInterface>::decodeDDFDCB(uint8_t opCode, uint16_t address) {

    switch (opCode) {
        default:
        case 0x00: /* RLC (IX+d),B */
        case 0x01: /* RLC (IX+d),C */
        case 0x02: /* RLC (IX+d),D */
        case 0x03: /* RLC (IX+d),E */
        case 0x04: /* RLC (IX+d),H */
        case 0x05: /* RLC (IX+d),L */
        case 0x06: /* RLC (IX+d)   */
        case 0x07: /* RLC (IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address);
            rlc(work8);
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x08: /* RRC (IX+d),B */
        case 0x09: /* RRC (IX+d),C */
        case 0x0A: /* RRC (IX+d),D */
        case 0x0B: /* RRC (IX+d),E */
        case 0x0C: /* RRC (IX+d),H */
        case 0x0D: /* RRC (IX+d),L */
        case 0x0E: /* RRC (IX+d)   */
        case 0x0F: /* RRC (IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address);
            rrc(work8);
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x10: /* RL (IX+d),B */
        case 0x11: /* RL (IX+d),C */
        case 0x12: /* RL (IX+d),D */
        case 0x13: /* RL (IX+d),E */
        case 0x14: /* RL (IX+d),H */
        case 0x15: /* RL (IX+d),L */
        case 0x16: /* RL (IX+d)   */
        case 0x17: /* RL (IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address);
            rl(work8);
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x18: /* RR (IX+d),B */
        case 0x19: /* RR (IX+d),C */
        case 0x1A: /* RR (IX+d),D */
        case 0x1B: /* RR (IX+d),E */
        case 0x1C: /* RR (IX+d),H */
        case 0x1D: /* RR (IX+d),L */
        case 0x1E: /* RR (IX+d)   */
        case 0x1F: /* RR (IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address);
            rr(work8);
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x20: /* SLA (IX+d),B */
        case 0x21: /* SLA (IX+d),C */
        case 0x22: /* SLA (IX+d),D */
        case 0x23: /* SLA (IX+d),E */
        case 0x24: /* SLA (IX+d),H */
        case 0x25: /* SLA (IX+d),L */
        case 0x26: /* SLA (IX+d)   */
        case 0x27: /* SLA (IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address);
            sla(work8);
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x28: /* SRA (IX+d),B */
        case 0x29: /* SRA (IX+d),C */
        case 0x2A: /* SRA (IX+d),D */
        case 0x2B: /* SRA (IX+d),E */
        case 0x2C: /* SRA (IX+d),H */
        case 0x2D: /* SRA (IX+d),L */
        case 0x2E: /* SRA (IX+d)   */
        case 0x2F: /* SRA (IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address);
            sra(work8);
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x30: /* SLL (IX+d),B */
        case 0x31: /* SLL (IX+d),C */
        case 0x32: /* SLL (IX+d),D */
        case 0x33: /* SLL (IX+d),E */
        case 0x34: /* SLL (IX+d),H */
        case 0x35: /* SLL (IX+d),L */
        case 0x36: /* SLL (IX+d)   */
        case 0x37: /* SLL (IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address);
            sll(work8);
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x38: /* SRL (IX+d),B */
        case 0x39: /* SRL (IX+d),C */
        case 0x3A: /* SRL (IX+d),D */
        case 0x3B: /* SRL (IX+d),E */
        case 0x3C: /* SRL (IX+d),H */
        case 0x3D: /* SRL (IX+d),L */
        case 0x3E: /* SRL (IX+d)   */
        case 0x3F: /* SRL (IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address);
            srl(work8);
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x47: /* BIT 0,(IX+d) */
            bitTest(0x01, m_busInterface.peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | ((address >> 8) & FLAG_53_MASK);
            m_busInterface.addressOnBus(address, 1);
            break;

        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B:
        case 0x4C:
        case 0x4D:
        case 0x4E:
        case 0x4F: /* BIT 1,(IX+d) */
            bitTest(0x02, m_busInterface.peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | ((address >> 8) & FLAG_53_MASK);
            m_busInterface.addressOnBus(address, 1);
            break;

        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x54:
        case 0x55:
        case 0x56:
        case 0x57: /* BIT 2,(IX+d) */
            bitTest(0x04, m_busInterface.peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | ((address >> 8) & FLAG_53_MASK);
            m_busInterface.addressOnBus(address, 1);
            break;

        case 0x58:
        case 0x59:
        case 0x5A:
        case 0x5B:
        case 0x5C:
        case 0x5D:
        case 0x5E:
        case 0x5F: /* BIT 3,(IX+d) */
            bitTest(0x08, m_busInterface.peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | ((address >> 8) & FLAG_53_MASK);
            m_busInterface.addressOnBus(address, 1);
            break;

        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
        case 0x64:
        case 0x65:
        case 0x66:
        case 0x67: /* BIT 4,(IX+d) */
            bitTest(0x10, m_busInterface.peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | ((address >> 8) & FLAG_53_MASK);
            m_busInterface.addressOnBus(address, 1);
            break;

        case 0x68:
        case 0x69:
        case 0x6A:
        case 0x6B:
        case 0x6C:
        case 0x6D:
        case 0x6E:
        case 0x6F: /* BIT 5,(IX+d) */
            bitTest(0x20, m_busInterface.peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | ((address >> 8) & FLAG_53_MASK);
            m_busInterface.addressOnBus(address, 1);
            break;

        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x76:
        case 0x77: /* BIT 6,(IX+d) */
            bitTest(0x40, m_busInterface.peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | ((address >> 8) & FLAG_53_MASK);
            m_busInterface.addressOnBus(address, 1);
            break;

        case 0x78:
        case 0x79:
        case 0x7A:
        case 0x7B:
        case 0x7C:
        case 0x7D:
        case 0x7E:
        case 0x7F: /* BIT 7,(IX+d) */
            bitTest(0x80, m_busInterface.peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | ((address >> 8) & FLAG_53_MASK);
            m_busInterface.addressOnBus(address, 1);
            break;

        case 0x80: /* RES 0,(IX+d),B */
        case 0x81: /* RES 0,(IX+d),C */
        case 0x82: /* RES 0,(IX+d),D */
        case 0x83: /* RES 0,(IX+d),E */
        case 0x84: /* RES 0,(IX+d),H */
        case 0x85: /* RES 0,(IX+d),L */
        case 0x86: /* RES 0,(IX+d)   */
        case 0x87: /* RES 0,(IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address) & 0xFE;
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x88: /* RES 1,(IX+d),B */
        case 0x89: /* RES 1,(IX+d),C */
        case 0x8A: /* RES 1,(IX+d),D */
        case 0x8B: /* RES 1,(IX+d),E */
        case 0x8C: /* RES 1,(IX+d),H */
        case 0x8D: /* RES 1,(IX+d),L */
        case 0x8E: /* RES 1,(IX+d)   */
        case 0x8F: /* RES 1,(IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address) & 0xFD;
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x90: /* RES 2,(IX+d),B */
        case 0x91: /* RES 2,(IX+d),C */
        case 0x92: /* RES 2,(IX+d),D */
        case 0x93: /* RES 2,(IX+d),E */
        case 0x94: /* RES 2,(IX+d),H */
        case 0x95: /* RES 2,(IX+d),L */
        case 0x96: /* RES 2,(IX+d)   */
        case 0x97: /* RES 2,(IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address) & 0xFB;
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x98: /* RES 3,(IX+d),B */
        case 0x99: /* RES 3,(IX+d),C */
        case 0x9A: /* RES 3,(IX+d),D */
        case 0x9B: /* RES 3,(IX+d),E */
        case 0x9C: /* RES 3,(IX+d),H */
        case 0x9D: /* RES 3,(IX+d),L */
        case 0x9E: /* RES 3,(IX+d)   */
        case 0x9F: /* RES 3,(IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address) & 0xF7;
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xA0: /* RES 4,(IX+d),B */
        case 0xA1: /* RES 4,(IX+d),C */
        case 0xA2: /* RES 4,(IX+d),D */
        case 0xA3: /* RES 4,(IX+d),E */
        case 0xA4: /* RES 4,(IX+d),H */
        case 0xA5: /* RES 4,(IX+d),L */
        case 0xA6: /* RES 4,(IX+d)   */
        case 0xA7: /* RES 4,(IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address) & 0xEF;
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xA8: /* RES 5,(IX+d),B */
        case 0xA9: /* RES 5,(IX+d),C */
        case 0xAA: /* RES 5,(IX+d),D */
        case 0xAB: /* RES 5,(IX+d),E */
        case 0xAC: /* RES 5,(IX+d),H */
        case 0xAD: /* RES 5,(IX+d),L */
        case 0xAE: /* RES 5,(IX+d)   */
        case 0xAF: /* RES 5,(IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address) & 0xDF;
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xB0: /* RES 6,(IX+d),B */
        case 0xB1: /* RES 6,(IX+d),C */
        case 0xB2: /* RES 6,(IX+d),D */
        case 0xB3: /* RES 6,(IX+d),E */
        case 0xB4: /* RES 6,(IX+d),H */
        case 0xB5: /* RES 6,(IX+d),L */
        case 0xB6: /* RES 6,(IX+d)   */
        case 0xB7: /* RES 6,(IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address) & 0xBF;
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xB8: /* RES 7,(IX+d),B */
        case 0xB9: /* RES 7,(IX+d),C */
        case 0xBA: /* RES 7,(IX+d),D */
        case 0xBB: /* RES 7,(IX+d),E */
        case 0xBC: /* RES 7,(IX+d),H */
        case 0xBD: /* RES 7,(IX+d),L */
        case 0xBE: /* RES 7,(IX+d)   */
        case 0xBF: /* RES 7,(IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address) & 0x7F;
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xC0: /* SET 0,(IX+d),B */
        case 0xC1: /* SET 0,(IX+d),C */
        case 0xC2: /* SET 0,(IX+d),D */
        case 0xC3: /* SET 0,(IX+d),E */
        case 0xC4: /* SET 0,(IX+d),H */
        case 0xC5: /* SET 0,(IX+d),L */
        case 0xC6: /* SET 0,(IX+d)   */
        case 0xC7: /* SET 0,(IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address) | 0x01;
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xC8: /* SET 1,(IX+d),B */
        case 0xC9: /* SET 1,(IX+d),C */
        case 0xCA: /* SET 1,(IX+d),D */
        case 0xCB: /* SET 1,(IX+d),E */
        case 0xCC: /* SET 1,(IX+d),H */
        case 0xCD: /* SET 1,(IX+d),L */
        case 0xCE: /* SET 1,(IX+d)   */
        case 0xCF: /* SET 1,(IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address) | 0x02;
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xD0: /* SET 2,(IX+d),B */
        case 0xD1: /* SET 2,(IX+d),C */
        case 0xD2: /* SET 2,(IX+d),D */
        case 0xD3: /* SET 2,(IX+d),E */
        case 0xD4: /* SET 2,(IX+d),H */
        case 0xD5: /* SET 2,(IX+d),L */
        case 0xD6: /* SET 2,(IX+d)   */
        case 0xD7: /* SET 2,(IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address) | 0x04;
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xD8: /* SET 3,(IX+d),B */
        case 0xD9: /* SET 3,(IX+d),C */
        case 0xDA: /* SET 3,(IX+d),D */
        case 0xDB: /* SET 3,(IX+d),E */
        case 0xDC: /* SET 3,(IX+d),H */
        case 0xDD: /* SET 3,(IX+d),L */
        case 0xDE: /* SET 3,(IX+d)   */
        case 0xDF: /* SET 3,(IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address) | 0x08;
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xE0: /* SET 4,(IX+d),B */
        case 0xE1: /* SET 4,(IX+d),C */
        case 0xE2: /* SET 4,(IX+d),D */
        case 0xE3: /* SET 4,(IX+d),E */
        case 0xE4: /* SET 4,(IX+d),H */
        case 0xE5: /* SET 4,(IX+d),L */
        case 0xE6: /* SET 4,(IX+d)   */
        case 0xE7: /* SET 4,(IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address) | 0x10;
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xE8: /* SET 5,(IX+d),B */
        case 0xE9: /* SET 5,(IX+d),C */
        case 0xEA: /* SET 5,(IX+d),D */
        case 0xEB: /* SET 5,(IX+d),E */
        case 0xEC: /* SET 5,(IX+d),H */
        case 0xED: /* SET 5,(IX+d),L */
        case 0xEE: /* SET 5,(IX+d)   */
        case 0xEF: /* SET 5,(IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address) | 0x20;
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xF0: /* SET 6,(IX+d),B */
        case 0xF1: /* SET 6,(IX+d),C */
        case 0xF2: /* SET 6,(IX+d),D */
        case 0xF3: /* SET 6,(IX+d),E */
        case 0xF4: /* SET 6,(IX+d),H */
        case 0xF5: /* SET 6,(IX+d),L */
        case 0xF6: /* SET 6,(IX+d)   */
        case 0xF7: /* SET 6,(IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address) | 0x40;
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xF8: /* SET 7,(IX+d),B */
        case 0xF9: /* SET 7,(IX+d),C */
        case 0xFA: /* SET 7,(IX+d),D */
        case 0xFB: /* SET 7,(IX+d),E */
        case 0xFC: /* SET 7,(IX+d),H */
        case 0xFD: /* SET 7,(IX+d),L */
        case 0xFE: /* SET 7,(IX+d)   */
        case 0xFF: /* SET 7,(IX+d),A */
        {
            uint8_t work8 = m_busInterface.peek8(address) | 0x80;
            m_busInterface.addressOnBus(address, 1);
            m_busInterface.poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
    }
}

// Subconjunto de instrucciones 0xED

template <typename TBusInterface> void Z80<TBusInterface>::decodeED(uint8_t opCode) {
    switch (opCode) {
        case 0x40: /* IN B,(C) */
            REG_WZ = REG_BC;
            REG_B = m_busInterface.inPort(REG_WZ);
            REG_WZ++;
            sz5h3pnFlags = sz53pn_addTable[REG_B];
            break;

        case 0x41: /* OUT (C),B */
            REG_WZ = REG_BC;
            m_busInterface.outPort(REG_WZ, REG_B);
            REG_WZ++;
            break;

        case 0x42: /* SBC HL,BC */
            m_busInterface.addressOnBus(getPairIR().word, 7);
            sbc16(REG_BC);
            break;

        case 0x43: /* LD (nn),BC */
            REG_WZ = m_busInterface.peek16(REG_PC);
            m_busInterface.poke16(REG_WZ, regBC);
            REG_WZ++;
            REG_PC = REG_PC + 2;
            break;

        case 0x44:
        case 0x4C:
        case 0x54:
        case 0x5C:
        case 0x64:
        case 0x6C:
        case 0x74:
        case 0x7C: /* NEG */ {
            uint8_t aux = regA;
            regA = 0;
            carryFlag = false;
            sbc(aux);
            break;
        }

        case 0x45:
        case 0x55:
        case 0x5D:
        case 0x65:
        case 0x6D:
        case 0x75:
        case 0x7D: /* RETN */
            ffIFF1 = ffIFF2;
            REG_PC = REG_WZ = pop();
            break;

        case 0x4D: /* RETI */
            /* According to the Z80 documentation, RETI should not update IFF1 from
             * IFF2; only RETN does this (to restore interrupts after NMI). This affects
             * precise interrupt handling behavior.
             */
            REG_PC = REG_WZ = pop();
            break;

        case 0x46:
        case 0x4E:
        case 0x66:
        case 0x6E: /* IM 0 */
            modeINT = IntMode::IM0;
            break;

        case 0x47: /* LD I,A */
            /*
             * El par IR se pone en el bus de direcciones *antes*
             * de poner A en el registro I. Detalle importante.
             */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            regI = regA;
            break;

        case 0x48: /* IN C,(C) */
            REG_WZ = REG_BC;
            REG_C = m_busInterface.inPort(REG_WZ);
            REG_WZ++;
            sz5h3pnFlags = sz53pn_addTable[REG_C];
            break;

        case 0x49: /* OUT (C),C */
            REG_WZ = REG_BC;
            m_busInterface.outPort(REG_WZ, REG_C);
            REG_WZ++;
            break;

        case 0x4A: /* ADC HL,BC */
            m_busInterface.addressOnBus(getPairIR().word, 7);
            adc16(REG_BC);
            break;

        case 0x4B: /* LD BC,(nn) */
            REG_WZ = m_busInterface.peek16(REG_PC);
            REG_BC = m_busInterface.peek16(REG_WZ);
            REG_WZ++;
            REG_PC = REG_PC + 2;
            break;

        case 0x4F: /* LD R,A */
            /*
             * El par IR se pone en el bus de direcciones *antes*
             * de poner A en el registro R. Detalle importante.
             */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            setRegR(regA);
            break;

        case 0x50: /* IN D,(C) */
            REG_WZ = REG_BC;
            REG_D = m_busInterface.inPort(REG_WZ);
            REG_WZ++;
            sz5h3pnFlags = sz53pn_addTable[REG_D];
            break;

        case 0x51: /* OUT (C),D */
            REG_WZ = REG_BC;
            m_busInterface.outPort(REG_WZ++, REG_D);
            break;

        case 0x52: /* SBC HL,DE */
            m_busInterface.addressOnBus(getPairIR().word, 7);
            sbc16(REG_DE);
            break;

        case 0x53: /* LD (nn),DE */
            REG_WZ = m_busInterface.peek16(REG_PC);
            m_busInterface.poke16(REG_WZ++, regDE);
            REG_PC = REG_PC + 2;
            break;

        case 0x56:
        case 0x76: /* IM 1 */
            modeINT = IntMode::IM1;
            break;

        case 0x57: { /* LD A,I */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            regA = regI;
            sz5h3pnFlags = sz53n_addTable[regA];
            /*
             * The P / V flag should reflect IFF2 state regardless of whether an
             * interrupt is currently being signaled on the bus.
             */
            if (ffIFF2) {
                sz5h3pnFlags |= PARITY_MASK;
            }
            break;
        }
        case 0x58: /* IN E,(C) */
            REG_WZ = REG_BC;
            REG_E = m_busInterface.inPort(REG_WZ++);
            sz5h3pnFlags = sz53pn_addTable[REG_E];
            break;

        case 0x59: /* OUT (C),E */
            REG_WZ = REG_BC;
            m_busInterface.outPort(REG_WZ++, REG_E);
            break;

        case 0x5A: /* ADC HL,DE */
            m_busInterface.addressOnBus(getPairIR().word, 7);
            adc16(REG_DE);
            break;

        case 0x5B: /* LD DE,(nn) */
            REG_WZ = m_busInterface.peek16(REG_PC);
            REG_DE = m_busInterface.peek16(REG_WZ++);
            REG_PC = REG_PC + 2;
            break;

        case 0x5E:
        case 0x7E: /* IM 2 */
            modeINT = IntMode::IM2;
            break;

        case 0x5F: { /* LD A,R */
            m_busInterface.addressOnBus(getPairIR().word, 1);
            regA = getRegR();
            sz5h3pnFlags = sz53n_addTable[regA];
            /*
             * The P / V flag should reflect IFF2 state regardless of whether an
             * interrupt is currently being signaled on the bus.
             */
            if (ffIFF2) {
                sz5h3pnFlags |= PARITY_MASK;
            }
            break;
        }
        case 0x60: /* IN H,(C) */
            REG_WZ = REG_BC;
            REG_H = m_busInterface.inPort(REG_WZ++);
            sz5h3pnFlags = sz53pn_addTable[REG_H];
            break;

        case 0x61: /* OUT (C),H */
            REG_WZ = REG_BC;
            m_busInterface.outPort(REG_WZ++, REG_H);
            break;

        case 0x62: /* SBC HL,HL */
            m_busInterface.addressOnBus(getPairIR().word, 7);
            sbc16(REG_HL);
            break;

        case 0x63: /* LD (nn),HL */
            REG_WZ = m_busInterface.peek16(REG_PC);
            m_busInterface.poke16(REG_WZ++, regHL);
            REG_PC = REG_PC + 2;
            break;

        case 0x67: /* RRD */ {
            // A = A7 A6 A5 A4 (HL)3 (HL)2 (HL)1 (HL)0
            // (HL) = A3 A2 A1 A0 (HL)7 (HL)6 (HL)5 (HL)4
            // Los bits 3,2,1 y 0 de (HL) se copian a los bits 3,2,1 y 0 de A.
            // Los 4 bits bajos que había en A se copian a los bits 7,6,5 y 4 de (HL).
            // Los 4 bits altos que había en (HL) se copian a los 4 bits bajos de (HL)
            // Los 4 bits superiores de A no se tocan. ¡p'habernos matao!
            uint8_t aux = regA << 4;
            REG_WZ = REG_HL;
            uint16_t memHL = m_busInterface.peek8(REG_WZ);
            regA = (regA & 0xf0) | (memHL & 0x0f);
            m_busInterface.addressOnBus(REG_WZ, 4);
            m_busInterface.poke8(REG_WZ++, (memHL >> 4) | aux);
            sz5h3pnFlags = sz53pn_addTable[regA];
            break;
        }

        case 0x68: /* IN L,(C) */
            REG_WZ = REG_BC;
            REG_L = m_busInterface.inPort(REG_WZ++);
            sz5h3pnFlags = sz53pn_addTable[REG_L];
            break;

        case 0x69: /* OUT (C),L */
            REG_WZ = REG_BC;
            m_busInterface.outPort(REG_WZ++, REG_L);
            break;

        case 0x6A: /* ADC HL,HL */
            m_busInterface.addressOnBus(getPairIR().word, 7);
            adc16(REG_HL);
            break;

        case 0x6B: /* LD HL,(nn) */
            REG_WZ = m_busInterface.peek16(REG_PC);
            REG_HL = m_busInterface.peek16(REG_WZ++);
            REG_PC = REG_PC + 2;
            break;

        case 0x6F: /* RLD */ {
            // A = A7 A6 A5 A4 (HL)7 (HL)6 (HL)5 (HL)4
            // (HL) = (HL)3 (HL)2 (HL)1 (HL)0 A3 A2 A1 A0
            // Los 4 bits bajos que había en (HL) se copian a los bits altos de (HL).
            // Los 4 bits altos que había en (HL) se copian a los 4 bits bajos de A
            // Los bits 3,2,1 y 0 de A se copian a los bits 3,2,1 y 0 de (HL).
            // Los 4 bits superiores de A no se tocan. ¡p'habernos matao!
            uint8_t aux = regA & 0x0f;
            REG_WZ = REG_HL;
            uint16_t memHL = m_busInterface.peek8(REG_WZ);
            regA = (regA & 0xf0) | (memHL >> 4);
            m_busInterface.addressOnBus(REG_WZ, 4);
            m_busInterface.poke8(REG_WZ++, (memHL << 4) | aux);
            sz5h3pnFlags = sz53pn_addTable[regA];
            break;
        }

        case 0x70: /* IN (C) */ {
            REG_WZ = REG_BC;
            uint8_t inPort = m_busInterface.inPort(REG_WZ++);
            sz5h3pnFlags = sz53pn_addTable[inPort];
            break;
        }

        case 0x71: /* OUT (C),0 */
            REG_WZ = REG_BC;
            m_busInterface.outPort(REG_WZ++, 0x00);
            break;

        case 0x72: /* SBC HL,SP */
            m_busInterface.addressOnBus(getPairIR().word, 7);
            sbc16(REG_SP);
            break;

        case 0x73: /* LD (nn),SP */
            REG_WZ = m_busInterface.peek16(REG_PC);
            m_busInterface.poke16(REG_WZ++, regSP);
            REG_PC = REG_PC + 2;
            break;

        case 0x78: /* IN A,(C) */
            REG_WZ = REG_BC;
            regA = m_busInterface.inPort(REG_WZ++);
            sz5h3pnFlags = sz53pn_addTable[regA];
            break;

        case 0x79: /* OUT (C),A */
            REG_WZ = REG_BC;
            m_busInterface.outPort(REG_WZ++, regA);
            break;

        case 0x7A: /* ADC HL,SP */
            m_busInterface.addressOnBus(getPairIR().word, 7);
            adc16(REG_SP);
            break;

        case 0x7B: /* LD SP,(nn) */
            REG_WZ = m_busInterface.peek16(REG_PC);
            REG_SP = m_busInterface.peek16(REG_WZ++);
            REG_PC = REG_PC + 2;
            break;

        case 0xA0: /* LDI */
            ldi();
            break;

        case 0xA1: /* CPI */
            cpi();
            break;

        case 0xA2: /* INI */
            ini();
            break;

        case 0xA3: /* OUTI */
            outi();
            break;

        case 0xA8: /* LDD */
            ldd();
            break;

        case 0xA9: /* CPD */
            cpd();
            break;

        case 0xAA: /* IND */
            ind();
            break;

        case 0xAB: /* OUTD */
            outd();
            break;

        case 0xB0: { /* LDIR */
            ldi();
            if (REG_BC != 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                m_busInterface.addressOnBus(REG_DE - 1, 5);
                sz5h3pnFlags &= ~FLAG_53_MASK;
                sz5h3pnFlags |= (REG_PCh & FLAG_53_MASK);
            }
            break;
        }
        case 0xB1: { /* CPIR */
            cpi();
            if ((sz5h3pnFlags & PARITY_MASK) == PARITY_MASK && (sz5h3pnFlags & ZERO_MASK) == 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                m_busInterface.addressOnBus(REG_HL - 1, 5);
                sz5h3pnFlags &= ~FLAG_53_MASK;
                sz5h3pnFlags |= (REG_PCh & FLAG_53_MASK);
            }
            break;
        }
        case 0xB2: { /* INIR */
            ini();
            if (REG_B != 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                m_busInterface.addressOnBus(REG_HL - 1, 5);
                adjustINxROUTxRFlags();
            }
            break;
        }
        case 0xB3: { /* OTIR */
            outi();
            if (REG_B != 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                m_busInterface.addressOnBus(REG_BC, 5);
                adjustINxROUTxRFlags();
            }
            break;
        }
        case 0xB8: { /* LDDR */
            ldd();
            if (REG_BC != 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                m_busInterface.addressOnBus(REG_DE + 1, 5);
                sz5h3pnFlags &= ~FLAG_53_MASK;
                sz5h3pnFlags |= (REG_PCh & FLAG_53_MASK);
            }
            break;
        }
        case 0xB9: { /* CPDR */
            cpd();
            if ((sz5h3pnFlags & PARITY_MASK) == PARITY_MASK && (sz5h3pnFlags & ZERO_MASK) == 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                m_busInterface.addressOnBus(REG_HL + 1, 5);
                sz5h3pnFlags &= ~FLAG_53_MASK;
                sz5h3pnFlags |= (REG_PCh & FLAG_53_MASK);
            }
            break;
        }
        case 0xBA: { /* INDR */
            ind();
            if (REG_B != 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                m_busInterface.addressOnBus(REG_HL + 1, 5);
                adjustINxROUTxRFlags();
            }
            break;
        }
        case 0xBB: { /* OTDR */
            outd();
            if (REG_B != 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                m_busInterface.addressOnBus(REG_BC, 5);
                adjustINxROUTxRFlags();
            }
            break;
        }
        case 0xDD:
            prefixOpcode = 0xDD;
            break;
        case 0xED:
            prefixOpcode = 0xED;
            break;
        case 0xFD:
            prefixOpcode = 0xFD;
            break;
        default:
            break;
    }
}

template <typename TBusInterface> void Z80<TBusInterface>::copyToRegister(uint8_t opCode, uint8_t value) {
    switch (opCode & 0x07) {
        case 0x00:
            REG_B = value;
            break;
        case 0x01:
            REG_C = value;
            break;
        case 0x02:
            REG_D = value;
            break;
        case 0x03:
            REG_E = value;
            break;
        case 0x04:
            REG_H = value;
            break;
        case 0x05:
            REG_L = value;
            break;
        case 0x07:
            regA = value;
        default:
            break;
    }
}

template <typename TBusInterface> void Z80<TBusInterface>::adjustINxROUTxRFlags() {
    sz5h3pnFlags &= ~FLAG_53_MASK;
    sz5h3pnFlags |= (REG_PCh & FLAG_53_MASK);

    uint8_t pf = sz5h3pnFlags & PARITY_MASK;
    if (carryFlag) {
        int8_t addsub = 1 - (sz5h3pnFlags & ADDSUB_MASK);
        pf ^= sz53pn_addTable[(REG_B + addsub) & 0x07] ^ PARITY_MASK;
        if ((REG_B & 0x0F) == (addsub != 1 ? 0x00 : 0x0F)) {
            sz5h3pnFlags |= HALFCARRY_MASK;
        } else {
            sz5h3pnFlags &= ~HALFCARRY_MASK;
        }
    } else {
        pf ^= sz53pn_addTable[REG_B & 0x07] ^ PARITY_MASK;
        sz5h3pnFlags &= ~HALFCARRY_MASK;
    }

    if ((pf & PARITY_MASK) != 0) {
        sz5h3pnFlags |= PARITY_MASK;
    } else {
        sz5h3pnFlags &= ~PARITY_MASK;
    }
}

#endif // Z80_HPP
