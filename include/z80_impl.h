// Z80 CPU Core - Template Implementation
// This file is automatically included by z80.h
// Do not include directly

#ifndef Z80_IMPL_H
#define Z80_IMPL_H

// ============================================================================
// CASE_OPCODE Macro - Switchable Dispatch Mechanism
// ============================================================================
#if defined(__GNUC__) || defined(__clang__)
    #define CASE_OPCODE(hex, desc) label_##hex /* desc */
#else
    #define CASE_OPCODE(hex, desc) case hex /* desc */
#endif

// Converted to C++ from Java at
//... https://github.com/jsanchezv/Z80Core
//... commit c4f267e3564fa89bd88fd2d1d322f4d6b0069dbd
//... GPL 3
//... v1.0.0 (13/02/2017)
//    quick & dirty conversion by dddddd (AKA deesix)


// Constructor de la clase
template<typename Derived>
Z80<Derived>::Z80(Derived *ops) {
    for (uint32_t idx = 0; idx < 256; idx++) {

        if (idx > 0x7f) {
            sz53n_addTable[idx] |= SIGN_MASK;
        }

        bool evenBits = true;
        for (uint8_t mask = 0x01; mask != 0; mask <<= 1) {
            if ((idx & mask) != 0) {
                evenBits = !evenBits;
            }
        }

        sz53n_addTable[idx] |= (idx & FLAG_53_MASK);
        sz53n_subTable[idx] = sz53n_addTable[idx] | ADDSUB_MASK;

        if (evenBits) {
            sz53pn_addTable[idx] = sz53n_addTable[idx] | PARITY_MASK;
            sz53pn_subTable[idx] = sz53n_subTable[idx] | PARITY_MASK;
        } else {
            sz53pn_addTable[idx] = sz53n_addTable[idx];
            sz53pn_subTable[idx] = sz53n_subTable[idx];
        }
    }

    sz53n_addTable[0] |= ZERO_MASK;
    sz53pn_addTable[0] |= ZERO_MASK;
    sz53n_subTable[0] |= ZERO_MASK;
    sz53pn_subTable[0] |= ZERO_MASK;

    Z80opsImpl = ops;
    execDone = false;
    reset();
}

template<typename Derived>
Z80<Derived>::~Z80() = default;

// Optimized getPairIR - still returns RegisterPair for compatibility
// but uses branchless implementation
template<typename Derived>
RegisterPair Z80<Derived>::getPairIR() const {
    RegisterPair IR;
    IR.word = getIRWord();
    return IR;
}

template<typename Derived>
void Z80<Derived>::setAddSubFlag(bool state) {
    // Branchless flag update to reduce mispredictions in hot paths
    const uint8_t m = state ? 0xFF : 0x00;
    sz5h3pnFlags = static_cast<uint8_t>((sz5h3pnFlags & static_cast<uint8_t>(~ADDSUB_MASK)) | (m & ADDSUB_MASK));
}

template<typename Derived>
void Z80<Derived>::setParOverFlag(bool state) {
    const uint8_t m = state ? 0xFF : 0x00;
    sz5h3pnFlags = static_cast<uint8_t>((sz5h3pnFlags & static_cast<uint8_t>(~PARITY_MASK)) | (m & PARITY_MASK));
}

template<typename Derived>
void Z80<Derived>::setBit3Flag(bool state) {
    const uint8_t m = state ? 0xFF : 0x00;
    sz5h3pnFlags = static_cast<uint8_t>((sz5h3pnFlags & static_cast<uint8_t>(~BIT3_MASK)) | (m & BIT3_MASK));
}

template<typename Derived>
void Z80<Derived>::setHalfCarryFlag(bool state) {
    const uint8_t m = state ? 0xFF : 0x00;
    sz5h3pnFlags = static_cast<uint8_t>((sz5h3pnFlags & static_cast<uint8_t>(~HALFCARRY_MASK)) | (m & HALFCARRY_MASK));
}

template<typename Derived>
void Z80<Derived>::setBit5Flag(bool state) {
    const uint8_t m = state ? 0xFF : 0x00;
    sz5h3pnFlags = static_cast<uint8_t>((sz5h3pnFlags & static_cast<uint8_t>(~BIT5_MASK)) | (m & BIT5_MASK));
}

template<typename Derived>
void Z80<Derived>::setZeroFlag(bool state) {
    const uint8_t m = state ? 0xFF : 0x00;
    sz5h3pnFlags = static_cast<uint8_t>((sz5h3pnFlags & static_cast<uint8_t>(~ZERO_MASK)) | (m & ZERO_MASK));
}

template<typename Derived>
void Z80<Derived>::setSignFlag(bool state) {
    const uint8_t m = state ? 0xFF : 0x00;
    sz5h3pnFlags = static_cast<uint8_t>((sz5h3pnFlags & static_cast<uint8_t>(~SIGN_MASK)) | (m & SIGN_MASK));
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
template<typename Derived>
void Z80<Derived>::reset() {
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
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::rlc(uint8_t &oper8) {
    carryFlag = (oper8 > 0x7f);
    oper8 <<= 1;
    oper8 |= carryFlag;  // Branchless: add carry bit
    sz5h3pnFlags = sz53pn_addTable[oper8];
    flagQ = true;
}
// Rota a la izquierda el valor del argumento
// El bit 7 va al carry flag
// El bit 0 toma el valor del flag C antes de la operación
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::rl(uint8_t &oper8) {
    bool oldCarry = carryFlag;
    carryFlag = (oper8 > 0x7f);
    oper8 = (oper8 << 1) | oldCarry;  // Branchless
    sz5h3pnFlags = sz53pn_addTable[oper8];
    flagQ = true;
}

// Rota a la izquierda el valor del argumento
// El bit 7 va al carry flag
// El bit 0 toma el valor 0
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::sla(uint8_t &oper8) {
    carryFlag = (oper8 > 0x7f);
    oper8 <<= 1;
    sz5h3pnFlags = sz53pn_addTable[oper8];
    flagQ = true;
}

// Rota a la izquierda el valor del argumento (como sla salvo por el bit 0)
// El bit 7 va al carry flag
// El bit 0 toma el valor 1
// Instrucción indocumentada
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::sll(uint8_t &oper8) {
    carryFlag = (oper8 > 0x7f);
    oper8 = (oper8 << 1) | CARRY_MASK;
    sz5h3pnFlags = sz53pn_addTable[oper8];
    flagQ = true;
}

// Rota a la derecha el valor del argumento
// El bit 7 y el flag C toman el valor del bit 0 antes de la operación
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::rrc(uint8_t &oper8) {
    carryFlag = (oper8 & CARRY_MASK) != 0;
    oper8 = (oper8 >> 1) | (carryFlag << 7);  // Branchless
    sz5h3pnFlags = sz53pn_addTable[oper8];
    flagQ = true;
}

// Rota a la derecha el valor del argumento
// El bit 0 va al carry flag
// El bit 7 toma el valor del flag C antes de la operación
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::rr(uint8_t &oper8) {
    bool oldCarry = carryFlag;
    carryFlag = (oper8 & CARRY_MASK) != 0;
    oper8 = (oper8 >> 1) | (oldCarry << 7);  // Branchless
    sz5h3pnFlags = sz53pn_addTable[oper8];
    flagQ = true;
}

// Rota a la derecha 1 bit el valor del argumento
// El bit 0 pasa al carry.
// El bit 7 conserva el valor que tenga
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::sra(uint8_t &oper8) {
    carryFlag = (oper8 & CARRY_MASK) != 0;
    oper8 = (oper8 >> 1) | (oper8 & SIGN_MASK);  // Preserve sign bit
    sz5h3pnFlags = sz53pn_addTable[oper8];
    flagQ = true;
}

// Rota a la derecha 1 bit el valor del argumento
// El bit 0 pasa al carry.
// El bit 7 toma el valor 0
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::srl(uint8_t &oper8) {
    carryFlag = (oper8 & CARRY_MASK) != 0;
    oper8 >>= 1;
    sz5h3pnFlags = sz53pn_addTable[oper8];
    flagQ = true;
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
// Branchless version for better pipeline performance
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::inc8(uint8_t &oper8) {
    oper8++;

    sz5h3pnFlags = sz53n_addTable[oper8];

    // Branchless half-carry: set if low nibble wrapped to 0
    sz5h3pnFlags |= (((oper8 & 0x0f) == 0) * HALFCARRY_MASK);

    // Branchless overflow: set if result is exactly 0x80
    sz5h3pnFlags |= ((oper8 == 0x80) * OVERFLOW_MASK);

    flagQ = true;
}

// Decrementa un valor de 8 bits modificando los flags oportunos
// Branchless version for better pipeline performance
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::dec8(uint8_t &oper8) {
    oper8--;

    sz5h3pnFlags = sz53n_subTable[oper8];

    // Branchless half-carry: set if low nibble wrapped to 0x0f
    sz5h3pnFlags |= (((oper8 & 0x0f) == 0x0f) * HALFCARRY_MASK);

    // Branchless overflow: set if result is exactly 0x7f
    sz5h3pnFlags |= ((oper8 == 0x7f) * OVERFLOW_MASK);

    flagQ = true;
}

// Suma de 8 bits afectando a los flags
// Branchless version for better pipeline performance
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::add(uint8_t oper8) {
    uint16_t res = regA + oper8;

    carryFlag = res > 0xff;
    res &= 0xff;
    sz5h3pnFlags = sz53n_addTable[res];

    // Branchless half-carry check
    sz5h3pnFlags |= (((res & 0x0f) < (regA & 0x0f)) * HALFCARRY_MASK);

    // Branchless overflow check
    sz5h3pnFlags |= ((((regA ^ ~oper8) & (regA ^ res)) > 0x7f) * OVERFLOW_MASK);

    regA = res;
    flagQ = true;
}

// Suma con acarreo de 8 bits
// Optimized with branchless operations
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::adc(uint8_t oper8) {
    uint16_t res = regA + oper8 + carryFlag;

    carryFlag = res > 0xff;
    res &= 0xff;
    sz5h3pnFlags = sz53n_addTable[res];

    // Branchless half-carry
    sz5h3pnFlags |= ((((regA ^ oper8 ^ res) & 0x10) != 0) * HALFCARRY_MASK);

    // Branchless overflow
    sz5h3pnFlags |= ((((regA ^ ~oper8) & (regA ^ res)) > 0x7f) * OVERFLOW_MASK);

    regA = res;
    flagQ = true;
}

// Suma dos operandos de 16 bits sin carry afectando a los flags
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::add16(RegisterPair &reg16, uint16_t oper16) {
    uint32_t tmp = oper16 + reg16.word;

    REG_WZ = reg16.word + 1;
    carryFlag = tmp > 0xffff;
    reg16.word = tmp;
    sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | ((reg16.word >> 8) & FLAG_53_MASK);

    // Branchless half-carry
    sz5h3pnFlags |= (((reg16.word & 0x0fff) < (oper16 & 0x0fff)) * HALFCARRY_MASK);

    flagQ = true;
}

// Suma con acarreo de 16 bits
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::adc16(uint16_t reg16) {
    uint16_t tmpHL = REG_HL;
    REG_WZ = REG_HL + 1;

    uint32_t res = REG_HL + reg16 + carryFlag;

    carryFlag = res > 0xffff;
    res &= 0xffff;
    REG_HL = static_cast<uint16_t>(res);

    sz5h3pnFlags = sz53n_addTable[REG_H];
    // Branchless zero flag clear
    sz5h3pnFlags &= ~((res != 0) * ZERO_MASK);

    // Branchless half-carry
    sz5h3pnFlags |= ((((res ^ tmpHL ^ reg16) & 0x1000) != 0) * HALFCARRY_MASK);

    // Branchless overflow
    sz5h3pnFlags |= ((((tmpHL ^ ~reg16) & (tmpHL ^ res)) > 0x7fff) * OVERFLOW_MASK);

    flagQ = true;
}

// Resta de 8 bits
// Branchless version for better pipeline performance
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::sub(uint8_t oper8) {
    auto res = static_cast<int16_t>(regA - oper8);

    carryFlag = res < 0;
    res &= 0xff;
    sz5h3pnFlags = sz53n_subTable[res];

    // Branchless half-carry
    sz5h3pnFlags |= (((res & 0x0f) > (regA & 0x0f)) * HALFCARRY_MASK);

    // Branchless overflow
    sz5h3pnFlags |= ((((regA ^ oper8) & (regA ^ res)) > 0x7f) * OVERFLOW_MASK);

    regA = res;
    flagQ = true;
}

// Resta con acarreo de 8 bits
// Optimized with branchless operations
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::sbc(uint8_t oper8) {
    auto res = static_cast<int16_t>(regA - oper8 - carryFlag);

    carryFlag = res < 0;
    res &= 0xff;
    sz5h3pnFlags = sz53n_subTable[res];

    // Branchless half-carry
    sz5h3pnFlags |= ((((regA ^ oper8 ^ res) & 0x10) != 0) * HALFCARRY_MASK);

    // Branchless overflow
    sz5h3pnFlags |= ((((regA ^ oper8) & (regA ^ res)) > 0x7f) * OVERFLOW_MASK);

    regA = res;
    flagQ = true;
}

// Resta con acarreo de 16 bits
// Optimized with branchless operations
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::sbc16(uint16_t reg16) {
    uint16_t tmpHL = REG_HL;
    REG_WZ = REG_HL + 1;

    int32_t res = REG_HL - reg16 - carryFlag;

    carryFlag = res < 0;
    res &= 0xffff;
    REG_HL = static_cast<uint16_t>(res);

    sz5h3pnFlags = sz53n_subTable[REG_H];
    // Branchless zero flag clear
    sz5h3pnFlags &= ~((res != 0) * ZERO_MASK);

    // Branchless half-carry
    sz5h3pnFlags |= ((((res ^ tmpHL ^ reg16) & 0x1000) != 0) * HALFCARRY_MASK);

    // Branchless overflow
    sz5h3pnFlags |= ((((tmpHL ^ reg16) & (tmpHL ^ res)) > 0x7fff) * OVERFLOW_MASK);
    flagQ = true;
}

// Operación AND lógica
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::and_(uint8_t oper8) {
    regA &= oper8;
    carryFlag = false;
    sz5h3pnFlags = sz53pn_addTable[regA] | HALFCARRY_MASK;
    flagQ = true;
}

// Operación XOR lógica
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::xor_(uint8_t oper8) {
    regA ^= oper8;
    carryFlag = false;
    sz5h3pnFlags = sz53pn_addTable[regA];
    flagQ = true;
}

// Operación OR lógica
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::or_(uint8_t oper8) {
    regA |= oper8;
    carryFlag = false;
    sz5h3pnFlags = sz53pn_addTable[regA];
    flagQ = true;
}

// Operación de comparación con el registro A
// es como SUB, pero solo afecta a los flags
// Los flags SIGN y ZERO se calculan a partir del resultado
// Los flags 3 y 5 se copian desde el operando (sigh!)
// Branchless version
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::cp(uint8_t oper8) {
    auto res = static_cast<int16_t>(regA - oper8);

    carryFlag = res < 0;
    res &= 0xff;

    sz5h3pnFlags = (sz53n_addTable[oper8] & FLAG_53_MASK)
            | // No necesito preservar H, pero está a 0 en la tabla de todas formas
            (sz53n_subTable[res] & FLAG_SZHN_MASK);

    // Branchless half-carry
    sz5h3pnFlags |= (((res & 0x0f) > (regA & 0x0f)) * HALFCARRY_MASK);

    // Branchless overflow
    sz5h3pnFlags |= ((((regA ^ oper8) & (regA ^ res)) > 0x7f) * OVERFLOW_MASK);

    flagQ = true;
}

// DAA
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::daa() {
    uint8_t suma = 0;
    bool carry = carryFlag;

    // Branchless low nibble adjustment
    suma = (((sz5h3pnFlags & HALFCARRY_MASK) != 0) | ((regA & 0x0f) > 0x09)) * 6;

    // Branchless high nibble adjustment
    suma |= ((carry | (regA > 0x99)) * 0x60);

    // Branchless carry determination
    carry |= (regA > 0x99);

    if ((sz5h3pnFlags & ADDSUB_MASK) != 0) {
        sub(suma);
        sz5h3pnFlags = (sz5h3pnFlags & HALFCARRY_MASK) | sz53pn_subTable[regA];
    } else {
        add(suma);
        sz5h3pnFlags = (sz5h3pnFlags & HALFCARRY_MASK) | sz53pn_addTable[regA];
    }

    carryFlag = carry;
    flagQ = true;
}

// POP
template<typename Derived>
Z80_FORCE_INLINE uint16_t Z80<Derived>::pop() {
    uint16_t word = Z80opsImpl->peek16(REG_SP);
    REG_SP += 2;
    return word;
}

// PUSH
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::push(uint16_t word) {
    Z80opsImpl->poke8(--REG_SP, word >> 8);
    Z80opsImpl->poke8(--REG_SP, word);
}

// LDI
template<typename Derived>
void Z80<Derived>::ldi() {
    uint8_t work8 = Z80opsImpl->peek8(REG_HL);
    Z80opsImpl->poke8(REG_DE, work8);
    Z80opsImpl->addressOnBus(REG_DE, 2);
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
    flagQ = true;
}

// LDD
template<typename Derived>
void Z80<Derived>::ldd() {
    uint8_t work8 = Z80opsImpl->peek8(REG_HL);
    Z80opsImpl->poke8(REG_DE, work8);
    Z80opsImpl->addressOnBus(REG_DE, 2);
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
    flagQ = true;
}

// CPI
template<typename Derived>
void Z80<Derived>::cpi() {
    uint8_t memHL = Z80opsImpl->peek8(REG_HL);
    bool carry = carryFlag; // lo guardo porque cp lo toca
    cp(memHL);
    carryFlag = carry;
    Z80opsImpl->addressOnBus(REG_HL, 5);
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
    flagQ = true;
}

// CPD
template<typename Derived>
void Z80<Derived>::cpd() {
    uint8_t memHL = Z80opsImpl->peek8(REG_HL);
    bool carry = carryFlag; // lo guardo porque cp lo toca
    cp(memHL);
    carryFlag = carry;
    Z80opsImpl->addressOnBus(REG_HL, 5);
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
    flagQ = true;
}

// INI
template<typename Derived>
void Z80<Derived>::ini() {
    REG_WZ = REG_BC;
    Z80opsImpl->addressOnBus(getIRWord(), 1);
    uint8_t work8 = Z80opsImpl->inPort(REG_WZ++);
    Z80opsImpl->poke8(REG_HL, work8);

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

    if ((sz53pn_addTable[((tmp & 0x07) ^ REG_B)]
            & PARITY_MASK) == PARITY_MASK) {
        sz5h3pnFlags |= PARITY_MASK;
    } else {
        sz5h3pnFlags &= ~PARITY_MASK;
    }
    flagQ = true;
}

// IND
template<typename Derived>
void Z80<Derived>::ind() {
    REG_WZ = REG_BC;
    Z80opsImpl->addressOnBus(getIRWord(), 1);
    uint8_t work8 = Z80opsImpl->inPort(REG_WZ--);
    Z80opsImpl->poke8(REG_HL, work8);

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

    if ((sz53pn_addTable[((tmp & 0x07) ^ REG_B)]
            & PARITY_MASK) == PARITY_MASK) {
        sz5h3pnFlags |= PARITY_MASK;
    } else {
        sz5h3pnFlags &= ~PARITY_MASK;
    }
    flagQ = true;
}

// OUTI
template<typename Derived>
void Z80<Derived>::outi() {

    Z80opsImpl->addressOnBus(getIRWord(), 1);

    REG_B--;
    REG_WZ = REG_BC;

    uint8_t work8 = Z80opsImpl->peek8(REG_HL);
    Z80opsImpl->outPort(REG_WZ++, work8);

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

    if ((sz53pn_addTable[(((REG_L + work8) & 0x07) ^ REG_B)]
            & PARITY_MASK) == PARITY_MASK) {
        sz5h3pnFlags |= PARITY_MASK;
    }
    flagQ = true;
}

// OUTD
template<typename Derived>
void Z80<Derived>::outd() {

    Z80opsImpl->addressOnBus(getIRWord(), 1);

    REG_B--;
    REG_WZ = REG_BC;

    uint8_t work8 = Z80opsImpl->peek8(REG_HL);
    Z80opsImpl->outPort(REG_WZ--, work8);

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

    if ((sz53pn_addTable[(((REG_L + work8) & 0x07) ^ REG_B)]
            & PARITY_MASK) == PARITY_MASK) {
        sz5h3pnFlags |= PARITY_MASK;
    }
    flagQ = true;
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
template<typename Derived>
Z80_FORCE_INLINE void Z80<Derived>::bitTest(uint8_t mask, uint8_t reg) {
    bool zeroFlag = (mask & reg) == 0;

    sz5h3pnFlags = (sz53n_addTable[reg] & ~FLAG_SZP_MASK) | HALFCARRY_MASK;

    // Branchless: set parity and zero if bit is zero
    sz5h3pnFlags |= (zeroFlag * (PARITY_MASK | ZERO_MASK));

    // Branchless: set sign flag if testing bit 7 and it's set
    sz5h3pnFlags |= (((mask == SIGN_MASK) & !zeroFlag) * SIGN_MASK);
    flagQ = true;
}

//Interrupción
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
template<typename Derived>
void Z80<Derived>::interrupt() {
    // Si estaba en un HALT esperando una INT, lo saca de la espera
    halted = false;

    Z80opsImpl->interruptHandlingTime(7);

    regR++;
    ffIFF1 = ffIFF2 = false;
    push(REG_PC); // el push añadirá 6 t-estados (+contended si toca)
    if (modeINT == IntMode::IM2) {
        REG_PC = Z80opsImpl->peek16((regI << 8) | 0xff); // +6 t-estados
    } else {
        REG_PC = 0x0038;
    }
    REG_WZ = REG_PC;
}

//Interrupción NMI, no utilizado por ahora
/* Desglose de ciclos de máquina y T-Estados
 * M1: 5 T-Estados -> extraer opcode (pá ná, es tontería) y decSP
 * M2: 3 T-Estados -> escribe byte alto de PC y decSP
 * M3: 3 T-Estados -> escrib e byte bajo de PC y PC=0x0066
 */
template<typename Derived>
void Z80<Derived>::nmi() {
    halted = false;
    // Esta lectura consigue dos cosas:
    //      1.- La lectura del opcode del M1 que se descarta
    //      2.- Si estaba en un HALT esperando una INT, lo saca de la espera
    Z80opsImpl->fetchOpcode(REG_PC);
    Z80opsImpl->interruptHandlingTime(1);
    regR++;
    ffIFF1 = false;
    push(REG_PC); // 3+3 t-estados + contended si procede
    REG_PC = REG_WZ = 0x0066;
}

template<typename Derived>
void Z80<Derived>::execute() {

    m_opCode = Z80opsImpl->fetchOpcode(REG_PC);
    regR++;

#ifdef WITH_BREAKPOINT_SUPPORT
    if (breakpointEnabled && prefixOpcode == 0) {
        m_opCode = Z80opsImpl->breakpoint(REG_PC, m_opCode);
    }
#endif
    if (!halted) {
        REG_PC++;

#if Z80_ENABLE_SUPERINSTRUCTIONS
        // ====================================================================
        // SUPERINSTRUCTION DETECTION
        // ====================================================================
        // Check if current opcode + next opcode form a common pattern
        // This is done before prefix checking for maximum performance
        if (Z80_LIKELY(prefixOpcode == 0x00)) {
            uint8_t nextOpcode;
            if (detectSuperinstruction(m_opCode, nextOpcode)) {
                // Execute fused operation
                flagQ = pendingEI = false;
                executeSuperinstruction(m_opCode, nextOpcode);

                // Check for deferred execution (unlikely)
                if (Z80_UNLIKELY(prefixOpcode != 0))
                    return;

                lastFlagQ = flagQ;

#ifdef WITH_EXEC_DONE
                if (execDone) {
                    Z80opsImpl->execDone();
                }
#endif
                // Skip interrupt checking after superinstructions for now
                // They'll be checked on next execute() call
                return;
            }
        }
#endif

        // Fast prefix processing - handle prefixed instructions in a single pass
        // This eliminates the need to return and re-enter execute() for prefix opcodes
        // Most instructions are NOT prefixed, so we optimize for that common case
        if (__builtin_expect(prefixOpcode == 0x00, 1)) {
            // Fast prefix detection using a compact switch for common prefixes
            // This is faster than multiple if-else chains due to compiler optimization
            switch (m_opCode) {
                case 0xDD: {
                    // Fast path for DD prefix (IX instructions)
                    m_opCode = Z80opsImpl->fetchOpcode(REG_PC);
                    regR++;
                    REG_PC++;
                    flagQ = pendingEI = false;
                    decodeDDFD(m_opCode, regIX);
                    break;
                }
                case 0xFD: {
                    // Fast path for FD prefix (IY instructions)
                    m_opCode = Z80opsImpl->fetchOpcode(REG_PC);
                    regR++;
                    REG_PC++;
                    flagQ = pendingEI = false;
                    decodeDDFD(m_opCode, regIY);
                    break;
                }
                case 0xED: {
                    // Fast path for ED prefix (extended instructions)
                    m_opCode = Z80opsImpl->fetchOpcode(REG_PC);
                    regR++;
                    REG_PC++;
                    flagQ = pendingEI = false;
                    decodeED(m_opCode);
                    break;
                }
                case 0xCB: {
                    // Fast path for CB prefix (bit instructions)
                    m_opCode = Z80opsImpl->fetchOpcode(REG_PC);
                    regR++;
                    REG_PC++;
                    flagQ = pendingEI = false;
                    decodeCB(m_opCode);
                    break;
                }
                default: {
                    // Regular instruction (most common case)
                    flagQ = pendingEI = false;
                    decodeOpcode(m_opCode);
                    break;
                }
            }
        } else {
            // Handle deferred prefix from previous execute() call (legacy path)
            // This maintains backward compatibility but should rarely execute
            const uint8_t currentPrefix = prefixOpcode;
            prefixOpcode = 0;

            if (currentPrefix == 0xDD) {
                decodeDDFD(m_opCode, regIX);
            } else if (currentPrefix == 0xFD) {
                decodeDDFD(m_opCode, regIY);
            } else if (currentPrefix == 0xED) {
                decodeED(m_opCode);
            } else {
                return;
            }
        }

        if (__builtin_expect(prefixOpcode != 0, 0))
            return;

        lastFlagQ = flagQ;

#ifdef WITH_EXEC_DONE
        if (execDone) {
            Z80opsImpl->execDone();
        }
#endif
    }

    // Primero se comprueba NMI
    // Si se activa NMI no se comprueba INT porque la siguiente
    // instrucción debe ser la de 0x0066.
    if (__builtin_expect(activeNMI, 0)) {
        activeNMI = false;
        lastFlagQ = false;
        nmi();
        return;
    }

    // Ahora se comprueba si está activada la señal INT
    if (__builtin_expect(ffIFF1 && !pendingEI && Z80opsImpl->isActiveINT(), 0)) {
        lastFlagQ = false;
        interrupt();
    }
}

// ============================================================================
// CASE_OPCODE Macro - Switchable Dispatch Mechanism
// ============================================================================
//
// This macro provides a unified way to write opcode handlers that work with
// both computed goto (GCC/Clang) and switch statement (MSVC/others) dispatch.
//
// USAGE:
//   CASE_OPCODE(0x01, "LD BC,nn"):
//   {
//       REG_BC = Z80opsImpl->peek16(REG_PC);
//       REG_PC += 2;
//       return;
//   }
//
// EXPANSION:
//   GCC/Clang:  label_0x01: /* LD BC,nn */
//   Others:     case 0x01: /* LD BC,nn */
//
// BENEFITS:
//   - Single codebase works on all compilers
//   - No manual #if/#else blocks needed for each opcode
//   - Cleaner, more maintainable code
//   - Automatic optimization selection based on compiler
//   - Instruction documentation inline with code
//
#if defined(__GNUC__) || defined(__clang__)
    #define CASE_OPCODE(hex, desc) label_##hex /* desc */
#else
    #define CASE_OPCODE(hex, desc) case hex /* desc */
#endif

template<typename Derived>
void Z80<Derived>::decodeOpcode(uint8_t opCode) {

#if defined(__GNUC__) || defined(__clang__)
    // Using computed goto dispatch table for optimal performance
    static const void* dispatch_table[256] = {
        &&label_0x00, &&label_0x01, &&label_0x02, &&label_0x03, &&label_0x04, &&label_0x05, &&label_0x06, &&label_0x07,
        &&label_0x08, &&label_0x09, &&label_0x0A, &&label_0x0B, &&label_0x0C, &&label_0x0D, &&label_0x0E, &&label_0x0F,
        &&label_0x10, &&label_0x11, &&label_0x12, &&label_0x13, &&label_0x14, &&label_0x15, &&label_0x16, &&label_0x17,
        &&label_0x18, &&label_0x19, &&label_0x1A, &&label_0x1B, &&label_0x1C, &&label_0x1D, &&label_0x1E, &&label_0x1F,
        &&label_0x20, &&label_0x21, &&label_0x22, &&label_0x23, &&label_0x24, &&label_0x25, &&label_0x26, &&label_0x27,
        &&label_0x28, &&label_0x29, &&label_0x2A, &&label_0x2B, &&label_0x2C, &&label_0x2D, &&label_0x2E, &&label_0x2F,
        &&label_0x30, &&label_0x31, &&label_0x32, &&label_0x33, &&label_0x34, &&label_0x35, &&label_0x36, &&label_0x37,
        &&label_0x38, &&label_0x39, &&label_0x3A, &&label_0x3B, &&label_0x3C, &&label_0x3D, &&label_0x3E, &&label_0x3F,
        &&label_0x40, &&label_0x41, &&label_0x42, &&label_0x43, &&label_0x44, &&label_0x45, &&label_0x46, &&label_0x47,
        &&label_0x48, &&label_0x49, &&label_0x4A, &&label_0x4B, &&label_0x4C, &&label_0x4D, &&label_0x4E, &&label_0x4F,
        &&label_0x50, &&label_0x51, &&label_0x52, &&label_0x53, &&label_0x54, &&label_0x55, &&label_0x56, &&label_0x57,
        &&label_0x58, &&label_0x59, &&label_0x5A, &&label_0x5B, &&label_0x5C, &&label_0x5D, &&label_0x5E, &&label_0x5F,
        &&label_0x60, &&label_0x61, &&label_0x62, &&label_0x63, &&label_0x64, &&label_0x65, &&label_0x66, &&label_0x67,
        &&label_0x68, &&label_0x69, &&label_0x6A, &&label_0x6B, &&label_0x6C, &&label_0x6D, &&label_0x6E, &&label_0x6F,
        &&label_0x70, &&label_0x71, &&label_0x72, &&label_0x73, &&label_0x74, &&label_0x75, &&label_0x76, &&label_0x77,
        &&label_0x78, &&label_0x79, &&label_0x7A, &&label_0x7B, &&label_0x7C, &&label_0x7D, &&label_0x7E, &&label_0x7F,
        &&label_0x80, &&label_0x81, &&label_0x82, &&label_0x83, &&label_0x84, &&label_0x85, &&label_0x86, &&label_0x87,
        &&label_0x88, &&label_0x89, &&label_0x8A, &&label_0x8B, &&label_0x8C, &&label_0x8D, &&label_0x8E, &&label_0x8F,
        &&label_0x90, &&label_0x91, &&label_0x92, &&label_0x93, &&label_0x94, &&label_0x95, &&label_0x96, &&label_0x97,
        &&label_0x98, &&label_0x99, &&label_0x9A, &&label_0x9B, &&label_0x9C, &&label_0x9D, &&label_0x9E, &&label_0x9F,
        &&label_0xA0, &&label_0xA1, &&label_0xA2, &&label_0xA3, &&label_0xA4, &&label_0xA5, &&label_0xA6, &&label_0xA7,
        &&label_0xA8, &&label_0xA9, &&label_0xAA, &&label_0xAB, &&label_0xAC, &&label_0xAD, &&label_0xAE, &&label_0xAF,
        &&label_0xB0, &&label_0xB1, &&label_0xB2, &&label_0xB3, &&label_0xB4, &&label_0xB5, &&label_0xB6, &&label_0xB7,
        &&label_0xB8, &&label_0xB9, &&label_0xBA, &&label_0xBB, &&label_0xBC, &&label_0xBD, &&label_0xBE, &&label_0xBF,
        &&label_0xC0, &&label_0xC1, &&label_0xC2, &&label_0xC3, &&label_0xC4, &&label_0xC5, &&label_0xC6, &&label_0xC7,
        &&label_0xC8, &&label_0xC9, &&label_0xCA, &&label_0xCB, &&label_0xCC, &&label_0xCD, &&label_0xCE, &&label_0xCF,
        &&label_0xD0, &&label_0xD1, &&label_0xD2, &&label_0xD3, &&label_0xD4, &&label_0xD5, &&label_0xD6, &&label_0xD7,
        &&label_0xD8, &&label_0xD9, &&label_0xDA, &&label_0xDB, &&label_0xDC, &&label_0xDD, &&label_0xDE, &&label_0xDF,
        &&label_0xE0, &&label_0xE1, &&label_0xE2, &&label_0xE3, &&label_0xE4, &&label_0xE5, &&label_0xE6, &&label_0xE7,
        &&label_0xE8, &&label_0xE9, &&label_0xEA, &&label_0xEB, &&label_0xEC, &&label_0xED, &&label_0xEE, &&label_0xEF,
        &&label_0xF0, &&label_0xF1, &&label_0xF2, &&label_0xF3, &&label_0xF4, &&label_0xF5, &&label_0xF6, &&label_0xF7,
        &&label_0xF8, &&label_0xF9, &&label_0xFA, &&label_0xFB, &&label_0xFC, &&label_0xFD, &&label_0xFE, &&label_0xFF
    };
    goto *dispatch_table[opCode];
#else
    // Using standard switch dispatch (MSVC/other compilers)
    switch (opCode) {
#endif
        CASE_OPCODE(0x00, "NOP"):
        {
            return;
        }
        CASE_OPCODE(0x01, "LD BC,nn"):
        {
            REG_BC = Z80opsImpl->peek16(REG_PC);
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0x02, "LD (BC),A"):
        {
            Z80opsImpl->poke8(REG_BC, regA);
            REG_W = regA;
            REG_Z = REG_C + 1;
            //REG_WZ = (regA << 8) | (REG_C + 1);
            return;
        }
        CASE_OPCODE(0x03, "INC BC"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 2);
            REG_BC++;
            return;
        }
        CASE_OPCODE(0x04, "INC B"):
        {
            inc8(REG_B);
            return;
        }
        CASE_OPCODE(0x05, "DEC B"):
        {
            dec8(REG_B);
            return;
        }
        CASE_OPCODE(0x06, "LD B,n"):
        {
            REG_B = Z80opsImpl->peek8(REG_PC);
            REG_PC++;
            return;
        }
        CASE_OPCODE(0x07, "RLCA"):
        {
            carryFlag = (regA > 0x7f);
            regA <<= 1;
            if (carryFlag) {
                regA |= CARRY_MASK;
            }
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (regA & FLAG_53_MASK);
            flagQ = true;
            return;
        }
        CASE_OPCODE(0x08, "EX AF,AF'"):
        {
            uint8_t work8 = regA;
            regA = REG_Ax;
            REG_Ax = work8;

            work8 = getFlags();
            setFlags(REG_Fx);
            REG_Fx = work8;
            return;
        }
        CASE_OPCODE(0x09, "ADD HL,BC"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 7);
            add16(regHL, REG_BC);
            return;
        }
        CASE_OPCODE(0x0A, "LD A,(BC)"):
        {
            regA = Z80opsImpl->peek8(REG_BC);
            REG_WZ = REG_BC + 1;
            return;
        }
        CASE_OPCODE(0x0B, "DEC BC"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 2);
            REG_BC--;
            return;
        }
        CASE_OPCODE(0x0C, "INC C"):
        {
            inc8(REG_C);
            return;
        }
        CASE_OPCODE(0x0D, "DEC C"):
        {
            dec8(REG_C);
            return;
        }
        CASE_OPCODE(0x0E, "LD C,n"):
        {
            REG_C = Z80opsImpl->peek8(REG_PC);
            REG_PC++;
            return;
        }
        CASE_OPCODE(0x0F, "RRCA"):
        {
            carryFlag = (regA & CARRY_MASK) != 0;
            regA >>= 1;
            if (carryFlag) {
                regA |= SIGN_MASK;
            }
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (regA & FLAG_53_MASK);
            flagQ = true;
            return;
        }
        CASE_OPCODE(0x10, "DJNZ e"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            auto offset = static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            if (--REG_B != 0) {
                Z80opsImpl->addressOnBus(REG_PC, 5);
                REG_PC = REG_WZ = REG_PC + offset + 1;
            } else {
                REG_PC++;
            }
            return;
        }
        CASE_OPCODE(0x11, "LD DE,nn"):
        {
            REG_DE = Z80opsImpl->peek16(REG_PC);
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0x12, "LD (DE),A"):
        {
            Z80opsImpl->poke8(REG_DE, regA);
            REG_W = regA;
            REG_Z = REG_E + 1;
            //REG_WZ = (regA << 8) | (REG_E + 1);
            return;
        }
        CASE_OPCODE(0x13, "INC DE"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 2);
            REG_DE++;
            return;
        }
        CASE_OPCODE(0x14, "INC D"):
        {
            inc8(REG_D);
            return;
        }
        CASE_OPCODE(0x15, "DEC D"):
        {
            dec8(REG_D);
            return;
        }
        CASE_OPCODE(0x16, "LD D,n"):
        {
            REG_D = Z80opsImpl->peek8(REG_PC);
            REG_PC++;
            return;
        }
        CASE_OPCODE(0x17, "RLA"):
        {
            bool oldCarry = carryFlag;
            carryFlag = (regA > 0x7f);
            regA <<= 1;
            if (oldCarry) {
                regA |= CARRY_MASK;
            }
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (regA & FLAG_53_MASK);
            flagQ = true;
            return;
        }
        CASE_OPCODE(0x18, "JR e"):
        {
            auto offset = static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC = REG_WZ = REG_PC + offset + 1;
            return;
        }
        CASE_OPCODE(0x19, "ADD HL,DE"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 7);
            add16(regHL, REG_DE);
            return;
        }
        CASE_OPCODE(0x1A, "LD A,(DE)"):
        {
            regA = Z80opsImpl->peek8(REG_DE);
            REG_WZ = REG_DE + 1;
            return;
        }
        CASE_OPCODE(0x1B, "DEC DE"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 2);
            REG_DE--;
            return;
        }
        CASE_OPCODE(0x1C, "INC E"):
        {
            inc8(REG_E);
            return;
        }
        CASE_OPCODE(0x1D, "DEC E"):
        {
            dec8(REG_E);
            return;
        }
        CASE_OPCODE(0x1E, "LD E,n"):
        {
            REG_E = Z80opsImpl->peek8(REG_PC);
            REG_PC++;
            return;
        }
        CASE_OPCODE(0x1F, "RRA"):
        {
            bool oldCarry = carryFlag;
            carryFlag = (regA & CARRY_MASK) != 0;
            regA = (regA >> 1) | (oldCarry << 7);  // Branchless
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (regA & FLAG_53_MASK);
            flagQ = true;
            return;
        }
        CASE_OPCODE(0x20, "JR NZ,e"):
        {
            auto offset = static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            if ((sz5h3pnFlags & ZERO_MASK) == 0) {
                Z80opsImpl->addressOnBus(REG_PC, 5);
                REG_PC += offset;
                REG_WZ = REG_PC + 1;
            }
            REG_PC++;
            return;
        }
        CASE_OPCODE(0x21, "LD HL,nn"):
        {
            REG_HL = Z80opsImpl->peek16(REG_PC);
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0x22, "LD (nn),HL"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            Z80opsImpl->poke16(REG_WZ, regHL);
            REG_WZ++;
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0x23, "INC HL"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 2);
            REG_HL++;
            return;
        }
        CASE_OPCODE(0x24, "INC H"):
        {
            inc8(REG_H);
            return;
        }
        CASE_OPCODE(0x25, "DEC H"):
        {
            dec8(REG_H);
            return;
        }
        CASE_OPCODE(0x26, "LD H,n"):
        {
            REG_H = Z80opsImpl->peek8(REG_PC);
            REG_PC++;
            return;
        }
        CASE_OPCODE(0x27, "DAA"):
        {
            daa();
            return;
        }
        CASE_OPCODE(0x28, "JR Z,e"):
        {
            auto offset = static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            if ((sz5h3pnFlags & ZERO_MASK) != 0) {
                Z80opsImpl->addressOnBus(REG_PC, 5);
                REG_PC += offset;
                REG_WZ = REG_PC + 1;
            }
            REG_PC++;
            return;
        }
        CASE_OPCODE(0x29, "ADD HL,HL"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 7);
            add16(regHL, REG_HL);
            return;
        }
        CASE_OPCODE(0x2A, "LD HL,(nn)"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            REG_HL = Z80opsImpl->peek16(REG_WZ);
            REG_WZ++;
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0x2B, "DEC HL"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 2);
            REG_HL--;
            return;
        }
        CASE_OPCODE(0x2C, "INC L"):
        {
            inc8(REG_L);
            return;
        }
        CASE_OPCODE(0x2D, "DEC L"):
        {
            dec8(REG_L);
            return;
        }
        CASE_OPCODE(0x2E, "LD L,n"):
        {
            REG_L = Z80opsImpl->peek8(REG_PC);
            REG_PC++;
            return;
        }
        CASE_OPCODE(0x2F, "CPL"):
        {
            regA ^= 0xff;
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | HALFCARRY_MASK
                    | (regA & FLAG_53_MASK) | ADDSUB_MASK;
            flagQ = true;
            return;
        }
        CASE_OPCODE(0x30, "JR NC,e"):
        {
            auto offset = static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            if (!carryFlag) {
                Z80opsImpl->addressOnBus(REG_PC, 5);
                REG_PC += offset;
                REG_WZ = REG_PC + 1;
            }
            REG_PC++;
            return;
        }
        CASE_OPCODE(0x31, "LD SP,nn"):
        {
            REG_SP = Z80opsImpl->peek16(REG_PC);
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0x32, "LD (nn),A"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            Z80opsImpl->poke8(REG_WZ, regA);
            REG_WZ = (regA << 8) | ((REG_WZ + 1) & 0xff);
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0x33, "INC SP"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 2);
            REG_SP++;
            return;
        }
        CASE_OPCODE(0x34, "INC (HL)"):
        {
            uint8_t work8 = Z80opsImpl->peek8(REG_HL);
            inc8(work8);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            return;
        }
        CASE_OPCODE(0x35, "DEC (HL)"):
        {
            uint8_t work8 = Z80opsImpl->peek8(REG_HL);
            dec8(work8);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            return;
        }
        CASE_OPCODE(0x36, "LD (HL),n"):
        {
            Z80opsImpl->poke8(REG_HL, Z80opsImpl->peek8(REG_PC));
            REG_PC++;
            return;
        }
        CASE_OPCODE(0x37, "SCF"):
        {
            uint8_t regQ = lastFlagQ ? sz5h3pnFlags : 0;
            carryFlag = true;
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (((regQ ^ sz5h3pnFlags) | regA) & FLAG_53_MASK);
            flagQ = true;
            return;
        }
        CASE_OPCODE(0x38, "JR C,e"):
        {
            auto offset = static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            if (carryFlag) {
                Z80opsImpl->addressOnBus(REG_PC, 5);
                REG_PC += offset;
                REG_WZ = REG_PC + 1;
            }
            REG_PC++;
            return;
        }
        CASE_OPCODE(0x39, "ADD HL,SP"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 7);
            add16(regHL, REG_SP);
            return;
        }
        CASE_OPCODE(0x3A, "LD A,(nn)"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            regA = Z80opsImpl->peek8(REG_WZ);
            REG_WZ++;
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0x3B, "DEC SP"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 2);
            REG_SP--;
            return;
        }
        CASE_OPCODE(0x3C, "INC A"):
        {
            inc8(regA);
            return;
        }
        CASE_OPCODE(0x3D, "DEC A"):
        {
            dec8(regA);
            return;
        }
        CASE_OPCODE(0x3E, "LD A,n"):
        {
            regA = Z80opsImpl->peek8(REG_PC);
            REG_PC++;
            return;
        }
        CASE_OPCODE(0x3F, "CCF"):
        {
            uint8_t regQ = lastFlagQ ? sz5h3pnFlags : 0;
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (((regQ ^ sz5h3pnFlags) | regA) & FLAG_53_MASK);
            if (carryFlag) {
                sz5h3pnFlags |= HALFCARRY_MASK;
            }
            carryFlag = !carryFlag;
            flagQ = true;
            return;
        }
        CASE_OPCODE(0x40, "LD B,B"):
        {
           return;
        }
        CASE_OPCODE(0x41, "LD B,C"):
        {
            REG_B = REG_C;
            return;
        }
        CASE_OPCODE(0x42, "LD B,D"):
        {
            REG_B = REG_D;
            return;
        }
        CASE_OPCODE(0x43, "LD B,E"):
        {
            REG_B = REG_E;
            return;
        }
        CASE_OPCODE(0x44, "LD B,H"):
        {
            REG_B = REG_H;
            return;
        }
        CASE_OPCODE(0x45, "LD B,L"):
        {
            REG_B = REG_L;
            return;
        }
        CASE_OPCODE(0x46, "LD B,(HL)"):
        {
            REG_B = Z80opsImpl->peek8(REG_HL);
            return;
        }
        CASE_OPCODE(0x47, "LD B,A"):
        {
            REG_B = regA;
            return;
        }
        CASE_OPCODE(0x48, "LD C,B"):
        {
            REG_C = REG_B;
            return;
        }
        CASE_OPCODE(0x49, "LD C,C"):
        {
           return;
        }
        CASE_OPCODE(0x4A, "LD C,D"):
        {
            REG_C = REG_D;
            return;
        }
        CASE_OPCODE(0x4B, "LD C,E"):
        {
            REG_C = REG_E;
            return;
        }
        CASE_OPCODE(0x4C, "LD C,H"):
        {
            REG_C = REG_H;
            return;
        }
        CASE_OPCODE(0x4D, "LD C,L"):
        {
            REG_C = REG_L;
            return;
        }
        CASE_OPCODE(0x4E, "LD C,(HL)"):
        {
            REG_C = Z80opsImpl->peek8(REG_HL);
            return;
        }
        CASE_OPCODE(0x4F, "LD C,A"):
        {
            REG_C = regA;
            return;
        }
        CASE_OPCODE(0x50, "LD D,B"):
        {
            REG_D = REG_B;
            return;
        }
        CASE_OPCODE(0x51, "LD D,C"):
        {
            REG_D = REG_C;
            return;
        }
        CASE_OPCODE(0x52, "LD D,D"):
        {
           return;
        }
        CASE_OPCODE(0x53, "LD D,E"):
        {
            REG_D = REG_E;
            return;
        }
        CASE_OPCODE(0x54, "LD D,H"):
        {
            REG_D = REG_H;
            return;
        }
        CASE_OPCODE(0x55, "LD D,L"):
        {
            REG_D = REG_L;
            return;
        }
        CASE_OPCODE(0x56, "LD D,(HL)"):
        {
            REG_D = Z80opsImpl->peek8(REG_HL);
            return;
        }
        CASE_OPCODE(0x57, "LD D,A"):
        {
            REG_D = regA;
            return;
        }
        CASE_OPCODE(0x58, "LD E,B"):
        {
            REG_E = REG_B;
            return;
        }
        CASE_OPCODE(0x59, "LD E,C"):
        {
            REG_E = REG_C;
            return;
        }
        CASE_OPCODE(0x5A, "LD E,D"):
        {
            REG_E = REG_D;
            return;
        }
        CASE_OPCODE(0x5B, "LD E,E"):
        {
           return;
        }
        CASE_OPCODE(0x5C, "LD E,H"):
        {
            REG_E = REG_H;
            return;
        }
        CASE_OPCODE(0x5D, "LD E,L"):
        {
            REG_E = REG_L;
            return;
        }
        CASE_OPCODE(0x5E, "LD E,(HL)"):
        {
            REG_E = Z80opsImpl->peek8(REG_HL);
            return;
        }
        CASE_OPCODE(0x5F, "LD E,A"):
        {
            REG_E = regA;
            return;
        }
        CASE_OPCODE(0x60, "LD H,B"):
        {
            REG_H = REG_B;
            return;
        }
        CASE_OPCODE(0x61, "LD H,C"):
        {
            REG_H = REG_C;
            return;
        }
        CASE_OPCODE(0x62, "LD H,D"):
        {
            REG_H = REG_D;
            return;
        }
        CASE_OPCODE(0x63, "LD H,E"):
        {
            REG_H = REG_E;
            return;
        }
        CASE_OPCODE(0x64, "LD H,H"):
        {
           return;
        }
        CASE_OPCODE(0x65, "LD H,L"):
        {
            REG_H = REG_L;
            return;
        }
        CASE_OPCODE(0x66, "LD H,(HL)"):
        {
            REG_H = Z80opsImpl->peek8(REG_HL);
            return;
        }
        CASE_OPCODE(0x67, "LD H,A"):
        {
            REG_H = regA;
            return;
        }
        CASE_OPCODE(0x68, "LD L,B"):
        {
            REG_L = REG_B;
            return;
        }
        CASE_OPCODE(0x69, "LD L,C"):
        {
            REG_L = REG_C;
            return;
        }
        CASE_OPCODE(0x6A, "LD L,D"):
        {
            REG_L = REG_D;
            return;
        }
        CASE_OPCODE(0x6B, "LD L,E"):
        {
            REG_L = REG_E;
            return;
        }
        CASE_OPCODE(0x6C, "LD L,H"):
        {
            REG_L = REG_H;
            return;
        }
        CASE_OPCODE(0x6D, "LD L,L"):
        {
           return;
        }
        CASE_OPCODE(0x6E, "LD L,(HL)"):
        {
            REG_L = Z80opsImpl->peek8(REG_HL);
            return;
        }
        CASE_OPCODE(0x6F, "LD L,A"):
        {
            REG_L = regA;
            return;
        }
        CASE_OPCODE(0x70, "LD (HL),B"):
        {
            Z80opsImpl->poke8(REG_HL, REG_B);
            return;
        }
        CASE_OPCODE(0x71, "LD (HL),C"):
        {
            Z80opsImpl->poke8(REG_HL, REG_C);
            return;
        }
        CASE_OPCODE(0x72, "LD (HL),D"):
        {
            Z80opsImpl->poke8(REG_HL, REG_D);
            return;
        }
        CASE_OPCODE(0x73, "LD (HL),E"):
        {
            Z80opsImpl->poke8(REG_HL, REG_E);
            return;
        }
        CASE_OPCODE(0x74, "LD (HL),H"):
        {
            Z80opsImpl->poke8(REG_HL, REG_H);
            return;
        }
        CASE_OPCODE(0x75, "LD (HL),L"):
        {
            Z80opsImpl->poke8(REG_HL, REG_L);
            return;
        }
        CASE_OPCODE(0x76, "HALT"):
        {
            halted = true;
            return;
        }
        CASE_OPCODE(0x77, "LD (HL),A"):
        {
            Z80opsImpl->poke8(REG_HL, regA);
            return;
        }
        CASE_OPCODE(0x78, "LD A,B"):
        {
            regA = REG_B;
            return;
        }
        CASE_OPCODE(0x79, "LD A,C"):
        {
            regA = REG_C;
            return;
        }
        CASE_OPCODE(0x7A, "LD A,D"):
        {
            regA = REG_D;
            return;
        }
        CASE_OPCODE(0x7B, "LD A,E"):
        {
            regA = REG_E;
            return;
        }
        CASE_OPCODE(0x7C, "LD A,H"):
        {
            regA = REG_H;
            return;
        }
        CASE_OPCODE(0x7D, "LD A,L"):
        {
            regA = REG_L;
            return;
        }
        CASE_OPCODE(0x7E, "LD A,(HL)"):
        {
            regA = Z80opsImpl->peek8(REG_HL);
            return;
        }
        CASE_OPCODE(0x7F, "LD A,A"):
        {
           return;
        }
        CASE_OPCODE(0x80, "ADD A,B"):
        {
            add(REG_B);
            return;
        }
        CASE_OPCODE(0x81, "ADD A,C"):
        {
            add(REG_C);
            return;
        }
        CASE_OPCODE(0x82, "ADD A,D"):
        {
            add(REG_D);
            return;
        }
        CASE_OPCODE(0x83, "ADD A,E"):
        {
            add(REG_E);
            return;
        }
        CASE_OPCODE(0x84, "ADD A,H"):
        {
            add(REG_H);
            return;
        }
        CASE_OPCODE(0x85, "ADD A,L"):
        {
            add(REG_L);
            return;
        }
        CASE_OPCODE(0x86, "ADD A,(HL)"):
        {
            add(Z80opsImpl->peek8(REG_HL));
            return;
        }
        CASE_OPCODE(0x87, "ADD A,A"):
        {
            add(regA);
            return;
        }
        CASE_OPCODE(0x88, "ADC A,B"):
        {
            adc(REG_B);
            return;
        }
        CASE_OPCODE(0x89, "ADC A,C"):
        {
            adc(REG_C);
            return;
        }
        CASE_OPCODE(0x8A, "ADC A,D"):
        {
            adc(REG_D);
            return;
        }
        CASE_OPCODE(0x8B, "ADC A,E"):
        {
            adc(REG_E);
            return;
        }
        CASE_OPCODE(0x8C, "ADC A,H"):
        {
            adc(REG_H);
            return;
        }
        CASE_OPCODE(0x8D, "ADC A,L"):
        {
            adc(REG_L);
            return;
        }
        CASE_OPCODE(0x8E, "ADC A,(HL)"):
        {
            adc(Z80opsImpl->peek8(REG_HL));
            return;
        }
        CASE_OPCODE(0x8F, "ADC A,A"):
        {
            adc(regA);
            return;
        }
        CASE_OPCODE(0x90, "SUB B"):
        {
            sub(REG_B);
            return;
        }
        CASE_OPCODE(0x91, "SUB C"):
        {
            sub(REG_C);
            return;
        }
        CASE_OPCODE(0x92, "SUB D"):
        {
            sub(REG_D);
            return;
        }
        CASE_OPCODE(0x93, "SUB E"):
        {
            sub(REG_E);
            return;
        }
        CASE_OPCODE(0x94, "SUB H"):
        {
            sub(REG_H);
            return;
        }
        CASE_OPCODE(0x95, "SUB L"):
        {
            sub(REG_L);
            return;
        }
        CASE_OPCODE(0x96, "SUB (HL)"):
        {
            sub(Z80opsImpl->peek8(REG_HL));
            return;
        }
        CASE_OPCODE(0x97, "SUB A"):
        {
            sub(regA);
            return;
        }
        CASE_OPCODE(0x98, "SBC A,B"):
        {
            sbc(REG_B);
            return;
        }
        CASE_OPCODE(0x99, "SBC A,C"):
        {
            sbc(REG_C);
            return;
        }
        CASE_OPCODE(0x9A, "SBC A,D"):
        {
            sbc(REG_D);
            return;
        }
        CASE_OPCODE(0x9B, "SBC A,E"):
        {
            sbc(REG_E);
            return;
        }
        CASE_OPCODE(0x9C, "SBC A,H"):
        {
            sbc(REG_H);
            return;
        }
        CASE_OPCODE(0x9D, "SBC A,L"):
        {
            sbc(REG_L);
            return;
        }
        CASE_OPCODE(0x9E, "SBC A,(HL)"):
        {
            sbc(Z80opsImpl->peek8(REG_HL));
            return;
        }
        CASE_OPCODE(0x9F, "SBC A,A"):
        {
            sbc(regA);
            return;
        }
        CASE_OPCODE(0xA0, "AND B"):
        {
            and_(REG_B);
            return;
        }
        CASE_OPCODE(0xA1, "AND C"):
        {
            and_(REG_C);
            return;
        }
        CASE_OPCODE(0xA2, "AND D"):
        {
            and_(REG_D);
            return;
        }
        CASE_OPCODE(0xA3, "AND E"):
        {
            and_(REG_E);
            return;
        }
        CASE_OPCODE(0xA4, "AND H"):
        {
            and_(REG_H);
            return;
        }
        CASE_OPCODE(0xA5, "AND L"):
        {
            and_(REG_L);
            return;
        }
        CASE_OPCODE(0xA6, "AND (HL)"):
        {
            and_(Z80opsImpl->peek8(REG_HL));
            return;
        }
        CASE_OPCODE(0xA7, "AND A"):
        {
            and_(regA);
            return;
        }
        CASE_OPCODE(0xA8, "XOR B"):
        {
            xor_(REG_B);
            return;
        }
        CASE_OPCODE(0xA9, "XOR C"):
        {
            xor_(REG_C);
            return;
        }
        CASE_OPCODE(0xAA, "XOR D"):
        {
            xor_(REG_D);
            return;
        }
        CASE_OPCODE(0xAB, "XOR E"):
        {
            xor_(REG_E);
            return;
        }
        CASE_OPCODE(0xAC, "XOR H"):
        {
            xor_(REG_H);
            return;
        }
        CASE_OPCODE(0xAD, "XOR L"):
        {
            xor_(REG_L);
            return;
        }
        CASE_OPCODE(0xAE, "XOR (HL)"):
        {
            xor_(Z80opsImpl->peek8(REG_HL));
            return;
        }
        CASE_OPCODE(0xAF, "XOR A"):
        {
            xor_(regA);
            return;
        }
        CASE_OPCODE(0xB0, "OR B"):
        {
            or_(REG_B);
            return;
        }
        CASE_OPCODE(0xB1, "OR C"):
        {
            or_(REG_C);
            return;
        }
        CASE_OPCODE(0xB2, "OR D"):
        {
            or_(REG_D);
            return;
        }
        CASE_OPCODE(0xB3, "OR E"):
        {
            or_(REG_E);
            return;
        }
        CASE_OPCODE(0xB4, "OR H"):
        {
            or_(REG_H);
            return;
        }
        CASE_OPCODE(0xB5, "OR L"):
        {
            or_(REG_L);
            return;
        }
        CASE_OPCODE(0xB6, "OR (HL)"):
        {
            or_(Z80opsImpl->peek8(REG_HL));
            return;
        }
        CASE_OPCODE(0xB7, "OR A"):
        {
            or_(regA);
            return;
        }
        CASE_OPCODE(0xB8, "CP B"):
        {
            cp(REG_B);
            return;
        }
        CASE_OPCODE(0xB9, "CP C"):
        {
            cp(REG_C);
            return;
        }
        CASE_OPCODE(0xBA, "CP D"):
        {
            cp(REG_D);
            return;
        }
        CASE_OPCODE(0xBB, "CP E"):
        {
            cp(REG_E);
            return;
        }
        CASE_OPCODE(0xBC, "CP H"):
        {
            cp(REG_H);
            return;
        }
        CASE_OPCODE(0xBD, "CP L"):
        {
            cp(REG_L);
            return;
        }
        CASE_OPCODE(0xBE, "CP (HL)"):
        {
            cp(Z80opsImpl->peek8(REG_HL));
            return;
        }
        CASE_OPCODE(0xBF, "CP A"):
        {
            cp(regA);
            return;
        }
        CASE_OPCODE(0xC0, "RET NZ"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            if ((sz5h3pnFlags & ZERO_MASK) == 0) {
                REG_PC = REG_WZ = pop();
            }
            return;
        }
        CASE_OPCODE(0xC1, "POP BC"):
        {
            REG_BC = pop();
            return;
        }
        CASE_OPCODE(0xC2, "JP NZ,nn"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            if ((sz5h3pnFlags & ZERO_MASK) == 0) {
                REG_PC = REG_WZ;
                return;
            }
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0xC3, "JP nn"):
        {
            REG_WZ = REG_PC = Z80opsImpl->peek16(REG_PC);
            return;
        }
        CASE_OPCODE(0xC4, "CALL NZ,nn"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            if ((sz5h3pnFlags & ZERO_MASK) == 0) {
                Z80opsImpl->addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                return;
            }
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0xC5, "PUSH BC"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            push(REG_BC);
            return;
        }
        CASE_OPCODE(0xC6, "ADD A,n"):
        {
            add(Z80opsImpl->peek8(REG_PC));
            REG_PC++;
            return;
        }
        CASE_OPCODE(0xC7, "RST 00H"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x00;
            return;
        }
        CASE_OPCODE(0xC8, "RET Z"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            if ((sz5h3pnFlags & ZERO_MASK) != 0) {
                REG_PC = REG_WZ = pop();
            }
            return;
        }
        CASE_OPCODE(0xC9, "RET"):
        {
            REG_PC = REG_WZ = pop();
            return;
        }
        CASE_OPCODE(0xCA, "JP Z,nn"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            if ((sz5h3pnFlags & ZERO_MASK) != 0) {
                REG_PC = REG_WZ;
                return;
            }
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0xCB, "Subconjunto de instrucciones"):
        {
            uint8_t cbOpcode = Z80opsImpl->fetchOpcode(REG_PC++);
            regR++;
            decodeCB(cbOpcode);
            return;
        }
        CASE_OPCODE(0xCC, "CALL Z,nn"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            if ((sz5h3pnFlags & ZERO_MASK) != 0) {
                Z80opsImpl->addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                return;
            }
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0xCD, "CALL nn"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            Z80opsImpl->addressOnBus(REG_PC + 1, 1);
            push(REG_PC + 2);
            REG_PC = REG_WZ;
            return;
        }
        CASE_OPCODE(0xCE, "ADC A,n"):
        {
            adc(Z80opsImpl->peek8(REG_PC));
            REG_PC++;
            return;
        }
        CASE_OPCODE(0xCF, "RST 08H"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x08;
            return;
        }
        CASE_OPCODE(0xD0, "RET NC"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            if (!carryFlag) {
                REG_PC = REG_WZ = pop();
            }
            return;
        }
        CASE_OPCODE(0xD1, "POP DE"):
        {
            REG_DE = pop();
            return;
        }
        CASE_OPCODE(0xD2, "JP NC,nn"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            if (!carryFlag) {
                REG_PC = REG_WZ;
                return;
            }
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0xD3, "OUT (n),A"):
        {
            uint8_t work8 = Z80opsImpl->peek8(REG_PC);
            REG_PC++;
            REG_WZ = regA << 8;
            Z80opsImpl->outPort(REG_WZ | work8, regA);
            REG_WZ |= (work8 + 1);
            return;
        }
        CASE_OPCODE(0xD4, "CALL NC,nn"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            if (!carryFlag) {
                Z80opsImpl->addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                return;
            }
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0xD5, "PUSH DE"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            push(REG_DE);
            return;
        }
        CASE_OPCODE(0xD6, "SUB n"):
        {
            sub(Z80opsImpl->peek8(REG_PC));
            REG_PC++;
            return;
        }
        CASE_OPCODE(0xD7, "RST 10H"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x10;
            return;
        }
        CASE_OPCODE(0xD8, "RET C"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            if (carryFlag) {
                REG_PC = REG_WZ = pop();
            }
            return;
        }
        CASE_OPCODE(0xD9, "EXX"):
        {
            uint16_t tmp;
            tmp = REG_BC;
            REG_BC = REG_BCx;
            REG_BCx = tmp;

            tmp = REG_DE;
            REG_DE = REG_DEx;
            REG_DEx = tmp;

            tmp = REG_HL;
            REG_HL = REG_HLx;
            REG_HLx = tmp;
            return;
        }
        CASE_OPCODE(0xDA, "JP C,nn"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            if (carryFlag) {
                REG_PC = REG_WZ;
                return;
            }
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0xDB, "IN A,(n)"):
        {
            REG_W = regA;
            REG_Z = Z80opsImpl->peek8(REG_PC);
            //REG_WZ = (regA << 8) | Z80opsImpl->peek8(REG_PC);
            REG_PC++;
            regA = Z80opsImpl->inPort(REG_WZ);
            REG_WZ++;
            return;
        }
        CASE_OPCODE(0xDC, "CALL C,nn"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            if (carryFlag) {
                Z80opsImpl->addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                return;
            }
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0xDD, "Subconjunto de instrucciones"):
        {
            opCode = Z80opsImpl->fetchOpcode(REG_PC++);
            regR++;
            decodeDDFD(opCode, regIX);
            return;
        }
        CASE_OPCODE(0xDE, "SBC A,n"):
        {
            sbc(Z80opsImpl->peek8(REG_PC));
            REG_PC++;
            return;
        }
        CASE_OPCODE(0xDF, "RST 18H"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x18;
            return;
        }
        CASE_OPCODE(0xE0, "RET PO"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            if ((sz5h3pnFlags & PARITY_MASK) == 0) {
                REG_PC = REG_WZ = pop();
            }
            return;
        }
        CASE_OPCODE(0xE1, "POP HL"):
        {
            REG_HL = pop();
            return;
        }
        CASE_OPCODE(0xE2, "JP PO,nn"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            if ((sz5h3pnFlags & PARITY_MASK) == 0) {
                REG_PC = REG_WZ;
                return;
            }
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0xE3, "EX (SP),HL"):
        {
            // Instrucción de ejecución sutil.
            RegisterPair work = regHL;
            REG_HL = Z80opsImpl->peek16(REG_SP);
            Z80opsImpl->addressOnBus(REG_SP + 1, 1);
            // No se usa poke16 porque el Z80 escribe los bytes AL REVES
            Z80opsImpl->poke8(REG_SP + 1, work.byte8.hi);
            Z80opsImpl->poke8(REG_SP, work.byte8.lo);
            Z80opsImpl->addressOnBus(REG_SP, 2);
            REG_WZ = REG_HL;
            return;
        }
        CASE_OPCODE(0xE4, "CALL PO,nn"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            if ((sz5h3pnFlags & PARITY_MASK) == 0) {
                Z80opsImpl->addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                return;
            }
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0xE5, "PUSH HL"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            push(REG_HL);
            return;
        }
        CASE_OPCODE(0xE6, "AND n"):
        {
            and_(Z80opsImpl->peek8(REG_PC));
            REG_PC++;
            return;
        }
        CASE_OPCODE(0xE7, "RST 20H"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x20;
            return;
        }
        CASE_OPCODE(0xE8, "RET PE"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            if ((sz5h3pnFlags & PARITY_MASK) != 0) {
                REG_PC = REG_WZ = pop();
            }
            return;
        }
        CASE_OPCODE(0xE9, "JP (HL)"):
        {
            REG_PC = REG_HL;
            return;
        }
        CASE_OPCODE(0xEA, "JP PE,nn"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            if ((sz5h3pnFlags & PARITY_MASK) != 0) {
                REG_PC = REG_WZ;
                return;
            }
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0xEB, "EX DE,HL"):
        {
            uint16_t tmp = REG_HL;
            REG_HL = REG_DE;
            REG_DE = tmp;
            return;
        }
        CASE_OPCODE(0xEC, "CALL PE,nn"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            if ((sz5h3pnFlags & PARITY_MASK) != 0) {
                Z80opsImpl->addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                return;
            }
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0xED, "Subconjunto de instrucciones"):
        {
            opCode = Z80opsImpl->fetchOpcode(REG_PC++);
            regR++;
            decodeED(opCode);
            return;
        }
        CASE_OPCODE(0xEE, "XOR n"):
        {
            xor_(Z80opsImpl->peek8(REG_PC));
            REG_PC++;
            return;
        }
        CASE_OPCODE(0xEF, "RST 28H"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x28;
            return;
        }
        CASE_OPCODE(0xF0, "RET P"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            if (sz5h3pnFlags < SIGN_MASK) {
                REG_PC = REG_WZ = pop();
            }
            return;
        }
        CASE_OPCODE(0xF1, "POP AF"):
        {
            setRegAF(pop());
            return;
        }
        CASE_OPCODE(0xF2, "JP P,nn"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            if (sz5h3pnFlags < SIGN_MASK) {
                REG_PC = REG_WZ;
                return;
            }
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0xF3, "DI"):
        {
            ffIFF1 = ffIFF2 = false;
            return;
        }
        CASE_OPCODE(0xF4, "CALL P,nn"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            if (sz5h3pnFlags < SIGN_MASK) {
                Z80opsImpl->addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                return;
            }
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0xF5, "PUSH AF"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            push(getRegAF());
            return;
        }
        CASE_OPCODE(0xF6, "OR n"):
        {
            or_(Z80opsImpl->peek8(REG_PC));
            REG_PC++;
            return;
        }
        CASE_OPCODE(0xF7, "RST 30H"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x30;
            return;
        }
        CASE_OPCODE(0xF8, "RET M"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            if (sz5h3pnFlags > 0x7f) {
                REG_PC = REG_WZ = pop();
            }
            return;
        }
        CASE_OPCODE(0xF9, "LD SP,HL"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 2);
            REG_SP = REG_HL;
            return;
        }
        CASE_OPCODE(0xFA, "JP M,nn"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            if (sz5h3pnFlags > 0x7f) {
                REG_PC = REG_WZ;
                return;
            }
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0xFB, "EI"):
        {
            ffIFF1 = ffIFF2 = true;
            pendingEI = true;
            return;
        }
        CASE_OPCODE(0xFC, "CALL M,nn"):
        {
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            if (sz5h3pnFlags > 0x7f) {
                Z80opsImpl->addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                return;
            }
            REG_PC += 2;
            return;
        }
        CASE_OPCODE(0xFD, "Subconjunto de instrucciones"):
        {
            opCode = Z80opsImpl->fetchOpcode(REG_PC++);
            regR++;
            decodeDDFD(opCode, regIY);
            return;
        }
        CASE_OPCODE(0xFE, "CP n"):
        {
            cp(Z80opsImpl->peek8(REG_PC));
            REG_PC++;
            return;
        }
        CASE_OPCODE(0xFF, "RST 38H"):
        {
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x38;
    } /* del switch( codigo ) */
}


//Subconjunto de instrucciones 0xCB

template<typename Derived>
void Z80<Derived>::decodeCB(uint8_t opCode) {
    switch (opCode) {
        case 0x00:
        { /* RLC B */
            rlc(REG_B);
            break;
        }
        case 0x01:
        { /* RLC C */
            rlc(REG_C);
            break;
        }
        case 0x02:
        { /* RLC D */
            rlc(REG_D);
            break;
        }
        case 0x03:
        { /* RLC E */
            rlc(REG_E);
            break;
        }
        case 0x04:
        { /* RLC H */
            rlc(REG_H);
            break;
        }
        case 0x05:
        { /* RLC L */
            rlc(REG_L);
            break;
        }
        case 0x06:
        { /* RLC (HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL);
            rlc(work8);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0x07:
        { /* RLC A */
            rlc(regA);
            break;
        }
        case 0x08:
        { /* RRC B */
            rrc(REG_B);
            break;
        }
        case 0x09:
        { /* RRC C */
            rrc(REG_C);
            break;
        }
        case 0x0A:
        { /* RRC D */
            rrc(REG_D);
            break;
        }
        case 0x0B:
        { /* RRC E */
            rrc(REG_E);
            break;
        }
        case 0x0C:
        { /* RRC H */
            rrc(REG_H);
            break;
        }
        case 0x0D:
        { /* RRC L */
            rrc(REG_L);
            break;
        }
        case 0x0E:
        { /* RRC (HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL);
            rrc(work8);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0x0F:
        { /* RRC A */
            rrc(regA);
            break;
        }
        case 0x10:
        { /* RL B */
            rl(REG_B);
            break;
        }
        case 0x11:
        { /* RL C */
            rl(REG_C);
            break;
        }
        case 0x12:
        { /* RL D */
            rl(REG_D);
            break;
        }
        case 0x13:
        { /* RL E */
            rl(REG_E);
            break;
        }
        case 0x14:
        { /* RL H */
            rl(REG_H);
            break;
        }
        case 0x15:
        { /* RL L */
            rl(REG_L);
            break;
        }
        case 0x16:
        { /* RL (HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL);
            rl(work8);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0x17:
        { /* RL A */
            rl(regA);
            break;
        }
        case 0x18:
        { /* RR B */
            rr(REG_B);
            break;
        }
        case 0x19:
        { /* RR C */
            rr(REG_C);
            break;
        }
        case 0x1A:
        { /* RR D */
            rr(REG_D);
            break;
        }
        case 0x1B:
        { /* RR E */
            rr(REG_E);
            break;
        }
        case 0x1C:
        { /*RR H*/
            rr(REG_H);
            break;
        }
        case 0x1D:
        { /* RR L */
            rr(REG_L);
            break;
        }
        case 0x1E:
        { /* RR (HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL);
            rr(work8);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0x1F:
        { /* RR A */
            rr(regA);
            break;
        }
        case 0x20:
        { /* SLA B */
            sla(REG_B);
            break;
        }
        case 0x21:
        { /* SLA C */
            sla(REG_C);
            break;
        }
        case 0x22:
        { /* SLA D */
            sla(REG_D);
            break;
        }
        case 0x23:
        { /* SLA E */
            sla(REG_E);
            break;
        }
        case 0x24:
        { /* SLA H */
            sla(REG_H);
            break;
        }
        case 0x25:
        { /* SLA L */
            sla(REG_L);
            break;
        }
        case 0x26:
        { /* SLA (HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL);
            sla(work8);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0x27:
        { /* SLA A */
            sla(regA);
            break;
        }
        case 0x28:
        { /* SRA B */
            sra(REG_B);
            break;
        }
        case 0x29:
        { /* SRA C */
            sra(REG_C);
            break;
        }
        case 0x2A:
        { /* SRA D */
            sra(REG_D);
            break;
        }
        case 0x2B:
        { /* SRA E */
            sra(REG_E);
            break;
        }
        case 0x2C:
        { /* SRA H */
            sra(REG_H);
            break;
        }
        case 0x2D:
        { /* SRA L */
            sra(REG_L);
            break;
        }
        case 0x2E:
        { /* SRA (HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL);
            sra(work8);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0x2F:
        { /* SRA A */
            sra(regA);
            break;
        }
        case 0x30:
        { /* SLL B */
            sll(REG_B);
            break;
        }
        case 0x31:
        { /* SLL C */
            sll(REG_C);
            break;
        }
        case 0x32:
        { /* SLL D */
            sll(REG_D);
            break;
        }
        case 0x33:
        { /* SLL E */
            sll(REG_E);
            break;
        }
        case 0x34:
        { /* SLL H */
            sll(REG_H);
            break;
        }
        case 0x35:
        { /* SLL L */
            sll(REG_L);
            break;
        }
        case 0x36:
        { /* SLL (HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL);
            sll(work8);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0x37:
        { /* SLL A */
            sll(regA);
            break;
        }
        case 0x38:
        { /* SRL B */
            srl(REG_B);
            break;
        }
        case 0x39:
        { /* SRL C */
            srl(REG_C);
            break;
        }
        case 0x3A:
        { /* SRL D */
            srl(REG_D);
            break;
        }
        case 0x3B:
        { /* SRL E */
            srl(REG_E);
            break;
        }
        case 0x3C:
        { /* SRL H */
            srl(REG_H);
            break;
        }
        case 0x3D:
        { /* SRL L */
            srl(REG_L);
            break;
        }
        case 0x3E:
        { /* SRL (HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL);
            srl(work8);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0x3F:
        { /* SRL A */
            srl(regA);
            break;
        }
        case 0x40:
        { /* BIT 0,B */
            bitTest(0x01, REG_B);
            break;
        }
        case 0x41:
        { /* BIT 0,C */
            bitTest(0x01, REG_C);
            break;
        }
        case 0x42:
        { /* BIT 0,D */
            bitTest(0x01, REG_D);
            break;
        }
        case 0x43:
        { /* BIT 0,E */
            bitTest(0x01, REG_E);
            break;
        }
        case 0x44:
        { /* BIT 0,H */
            bitTest(0x01, REG_H);
            break;
        }
        case 0x45:
        { /* BIT 0,L */
            bitTest(0x01, REG_L);
            break;
        }
        case 0x46:
        { /* BIT 0,(HL) */
            bitTest(0x01, Z80opsImpl->peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            break;
        }
        case 0x47:
        { /* BIT 0,A */
            bitTest(0x01, regA);
            break;
        }
        case 0x48:
        { /* BIT 1,B */
            bitTest(0x02, REG_B);
            break;
        }
        case 0x49:
        { /* BIT 1,C */
            bitTest(0x02, REG_C);
            break;
        }
        case 0x4A:
        { /* BIT 1,D */
            bitTest(0x02, REG_D);
            break;
        }
        case 0x4B:
        { /* BIT 1,E */
            bitTest(0x02, REG_E);
            break;
        }
        case 0x4C:
        { /* BIT 1,H */
            bitTest(0x02, REG_H);
            break;
        }
        case 0x4D:
        { /* BIT 1,L */
            bitTest(0x02, REG_L);
            break;
        }
        case 0x4E:
        { /* BIT 1,(HL) */
            bitTest(0x02, Z80opsImpl->peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            break;
        }
        case 0x4F:
        { /* BIT 1,A */
            bitTest(0x02, regA);
            break;
        }
        case 0x50:
        { /* BIT 2,B */
            bitTest(0x04, REG_B);
            break;
        }
        case 0x51:
        { /* BIT 2,C */
            bitTest(0x04, REG_C);
            break;
        }
        case 0x52:
        { /* BIT 2,D */
            bitTest(0x04, REG_D);
            break;
        }
        case 0x53:
        { /* BIT 2,E */
            bitTest(0x04, REG_E);
            break;
        }
        case 0x54:
        { /* BIT 2,H */
            bitTest(0x04, REG_H);
            break;
        }
        case 0x55:
        { /* BIT 2,L */
            bitTest(0x04, REG_L);
            break;
        }
        case 0x56:
        { /* BIT 2,(HL) */
            bitTest(0x04, Z80opsImpl->peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            break;
        }
        case 0x57:
        { /* BIT 2,A */
            bitTest(0x04, regA);
            break;
        }
        case 0x58:
        { /* BIT 3,B */
            bitTest(0x08, REG_B);
            break;
        }
        case 0x59:
        { /* BIT 3,C */
            bitTest(0x08, REG_C);
            break;
        }
        case 0x5A:
        { /* BIT 3,D */
            bitTest(0x08, REG_D);
            break;
        }
        case 0x5B:
        { /* BIT 3,E */
            bitTest(0x08, REG_E);
            break;
        }
        case 0x5C:
        { /* BIT 3,H */
            bitTest(0x08, REG_H);
            break;
        }
        case 0x5D:
        { /* BIT 3,L */
            bitTest(0x08, REG_L);
            break;
        }
        case 0x5E:
        { /* BIT 3,(HL) */
            bitTest(0x08, Z80opsImpl->peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            break;
        }
        case 0x5F:
        { /* BIT 3,A */
            bitTest(0x08, regA);
            break;
        }
        case 0x60:
        { /* BIT 4,B */
            bitTest(0x10, REG_B);
            break;
        }
        case 0x61:
        { /* BIT 4,C */
            bitTest(0x10, REG_C);
            break;
        }
        case 0x62:
        { /* BIT 4,D */
            bitTest(0x10, REG_D);
            break;
        }
        case 0x63:
        { /* BIT 4,E */
            bitTest(0x10, REG_E);
            break;
        }
        case 0x64:
        { /* BIT 4,H */
            bitTest(0x10, REG_H);
            break;
        }
        case 0x65:
        { /* BIT 4,L */
            bitTest(0x10, REG_L);
            break;
        }
        case 0x66:
        { /* BIT 4,(HL) */
            bitTest(0x10, Z80opsImpl->peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            break;
        }
        case 0x67:
        { /* BIT 4,A */
            bitTest(0x10, regA);
            break;
        }
        case 0x68:
        { /* BIT 5,B */
            bitTest(0x20, REG_B);
            break;
        }
        case 0x69:
        { /* BIT 5,C */
            bitTest(0x20, REG_C);
            break;
        }
        case 0x6A:
        { /* BIT 5,D */
            bitTest(0x20, REG_D);
            break;
        }
        case 0x6B:
        { /* BIT 5,E */
            bitTest(0x20, REG_E);
            break;
        }
        case 0x6C:
        { /* BIT 5,H */
            bitTest(0x20, REG_H);
            break;
        }
        case 0x6D:
        { /* BIT 5,L */
            bitTest(0x20, REG_L);
            break;
        }
        case 0x6E:
        { /* BIT 5,(HL) */
            bitTest(0x20, Z80opsImpl->peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            break;
        }
        case 0x6F:
        { /* BIT 5,A */
            bitTest(0x20, regA);
            break;
        }
        case 0x70:
        { /* BIT 6,B */
            bitTest(0x40, REG_B);
            break;
        }
        case 0x71:
        { /* BIT 6,C */
            bitTest(0x40, REG_C);
            break;
        }
        case 0x72:
        { /* BIT 6,D */
            bitTest(0x40, REG_D);
            break;
        }
        case 0x73:
        { /* BIT 6,E */
            bitTest(0x40, REG_E);
            break;
        }
        case 0x74:
        { /* BIT 6,H */
            bitTest(0x40, REG_H);
            break;
        }
        case 0x75:
        { /* BIT 6,L */
            bitTest(0x40, REG_L);
            break;
        }
        case 0x76:
        { /* BIT 6,(HL) */
            bitTest(0x40, Z80opsImpl->peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            break;
        }
        case 0x77:
        { /* BIT 6,A */
            bitTest(0x40, regA);
            break;
        }
        case 0x78:
        { /* BIT 7,B */
            bitTest(0x80, REG_B);
            break;
        }
        case 0x79:
        { /* BIT 7,C */
            bitTest(0x80, REG_C);
            break;
        }
        case 0x7A:
        { /* BIT 7,D */
            bitTest(0x80, REG_D);
            break;
        }
        case 0x7B:
        { /* BIT 7,E */
            bitTest(0x80, REG_E);
            break;
        }
        case 0x7C:
        { /* BIT 7,H */
            bitTest(0x80, REG_H);
            break;
        }
        case 0x7D:
        { /* BIT 7,L */
            bitTest(0x80, REG_L);
            break;
        }
        case 0x7E:
        { /* BIT 7,(HL) */
            bitTest(0x80, Z80opsImpl->peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            Z80opsImpl->addressOnBus(REG_HL, 1);
            break;
        }
        case 0x7F:
        { /* BIT 7,A */
            bitTest(0x80, regA);
            break;
        }
        case 0x80:
        { /* RES 0,B */
            REG_B &= 0xFE;
            break;
        }
        case 0x81:
        { /* RES 0,C */
            REG_C &= 0xFE;
            break;
        }
        case 0x82:
        { /* RES 0,D */
            REG_D &= 0xFE;
            break;
        }
        case 0x83:
        { /* RES 0,E */
            REG_E &= 0xFE;
            break;
        }
        case 0x84:
        { /* RES 0,H */
            REG_H &= 0xFE;
            break;
        }
        case 0x85:
        { /* RES 0,L */
            REG_L &= 0xFE;
            break;
        }
        case 0x86:
        { /* RES 0,(HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL) & 0xFE;
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0x87:
        { /* RES 0,A */
            regA &= 0xFE;
            break;
        }
        case 0x88:
        { /* RES 1,B */
            REG_B &= 0xFD;
            break;
        }
        case 0x89:
        { /* RES 1,C */
            REG_C &= 0xFD;
            break;
        }
        case 0x8A:
        { /* RES 1,D */
            REG_D &= 0xFD;
            break;
        }
        case 0x8B:
        { /* RES 1,E */
            REG_E &= 0xFD;
            break;
        }
        case 0x8C:
        { /* RES 1,H */
            REG_H &= 0xFD;
            break;
        }
        case 0x8D:
        { /* RES 1,L */
            REG_L &= 0xFD;
            break;
        }
        case 0x8E:
        { /* RES 1,(HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL) & 0xFD;
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0x8F:
        { /* RES 1,A */
            regA &= 0xFD;
            break;
        }
        case 0x90:
        { /* RES 2,B */
            REG_B &= 0xFB;
            break;
        }
        case 0x91:
        { /* RES 2,C */
            REG_C &= 0xFB;
            break;
        }
        case 0x92:
        { /* RES 2,D */
            REG_D &= 0xFB;
            break;
        }
        case 0x93:
        { /* RES 2,E */
            REG_E &= 0xFB;
            break;
        }
        case 0x94:
        { /* RES 2,H */
            REG_H &= 0xFB;
            break;
        }
        case 0x95:
        { /* RES 2,L */
            REG_L &= 0xFB;
            break;
        }
        case 0x96:
        { /* RES 2,(HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL) & 0xFB;
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0x97:
        { /* RES 2,A */
            regA &= 0xFB;
            break;
        }
        case 0x98:
        { /* RES 3,B */
            REG_B &= 0xF7;
            break;
        }
        case 0x99:
        { /* RES 3,C */
            REG_C &= 0xF7;
            break;
        }
        case 0x9A:
        { /* RES 3,D */
            REG_D &= 0xF7;
            break;
        }
        case 0x9B:
        { /* RES 3,E */
            REG_E &= 0xF7;
            break;
        }
        case 0x9C:
        { /* RES 3,H */
            REG_H &= 0xF7;
            break;
        }
        case 0x9D:
        { /* RES 3,L */
            REG_L &= 0xF7;
            break;
        }
        case 0x9E:
        { /* RES 3,(HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL) & 0xF7;
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0x9F:
        { /* RES 3,A */
            regA &= 0xF7;
            break;
        }
        case 0xA0:
        { /* RES 4,B */
            REG_B &= 0xEF;
            break;
        }
        case 0xA1:
        { /* RES 4,C */
            REG_C &= 0xEF;
            break;
        }
        case 0xA2:
        { /* RES 4,D */
            REG_D &= 0xEF;
            break;
        }
        case 0xA3:
        { /* RES 4,E */
            REG_E &= 0xEF;
            break;
        }
        case 0xA4:
        { /* RES 4,H */
            REG_H &= 0xEF;
            break;
        }
        case 0xA5:
        { /* RES 4,L */
            REG_L &= 0xEF;
            break;
        }
        case 0xA6:
        { /* RES 4,(HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL) & 0xEF;
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0xA7:
        { /* RES 4,A */
            regA &= 0xEF;
            break;
        }
        case 0xA8:
        { /* RES 5,B */
            REG_B &= 0xDF;
            break;
        }
        case 0xA9:
        { /* RES 5,C */
            REG_C &= 0xDF;
            break;
        }
        case 0xAA:
        { /* RES 5,D */
            REG_D &= 0xDF;
            break;
        }
        case 0xAB:
        { /* RES 5,E */
            REG_E &= 0xDF;
            break;
        }
        case 0xAC:
        { /* RES 5,H */
            REG_H &= 0xDF;
            break;
        }
        case 0xAD:
        { /* RES 5,L */
            REG_L &= 0xDF;
            break;
        }
        case 0xAE:
        { /* RES 5,(HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL) & 0xDF;
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0xAF:
        { /* RES 5,A */
            regA &= 0xDF;
            break;
        }
        case 0xB0:
        { /* RES 6,B */
            REG_B &= 0xBF;
            break;
        }
        case 0xB1:
        { /* RES 6,C */
            REG_C &= 0xBF;
            break;
        }
        case 0xB2:
        { /* RES 6,D */
            REG_D &= 0xBF;
            break;
        }
        case 0xB3:
        { /* RES 6,E */
            REG_E &= 0xBF;
            break;
        }
        case 0xB4:
        { /* RES 6,H */
            REG_H &= 0xBF;
            break;
        }
        case 0xB5:
        { /* RES 6,L */
            REG_L &= 0xBF;
            break;
        }
        case 0xB6:
        { /* RES 6,(HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL) & 0xBF;
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0xB7:
        { /* RES 6,A */
            regA &= 0xBF;
            break;
        }
        case 0xB8:
        { /* RES 7,B */
            REG_B &= 0x7F;
            break;
        }
        case 0xB9:
        { /* RES 7,C */
            REG_C &= 0x7F;
            break;
        }
        case 0xBA:
        { /* RES 7,D */
            REG_D &= 0x7F;
            break;
        }
        case 0xBB:
        { /* RES 7,E */
            REG_E &= 0x7F;
            break;
        }
        case 0xBC:
        { /* RES 7,H */
            REG_H &= 0x7F;
            break;
        }
        case 0xBD:
        { /* RES 7,L */
            REG_L &= 0x7F;
            break;
        }
        case 0xBE:
        { /* RES 7,(HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL) & 0x7F;
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0xBF:
        { /* RES 7,A */
            regA &= 0x7F;
            break;
        }
        case 0xC0:
        { /* SET 0,B */
            REG_B |= 0x01;
            break;
        }
        case 0xC1:
        { /* SET 0,C */
            REG_C |= 0x01;
            break;
        }
        case 0xC2:
        { /* SET 0,D */
            REG_D |= 0x01;
            break;
        }
        case 0xC3:
        { /* SET 0,E */
            REG_E |= 0x01;
            break;
        }
        case 0xC4:
        { /* SET 0,H */
            REG_H |= 0x01;
            break;
        }
        case 0xC5:
        { /* SET 0,L */
            REG_L |= 0x01;
            break;
        }
        case 0xC6:
        { /* SET 0,(HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL) | 0x01;
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0xC7:
        { /* SET 0,A */
            regA |= 0x01;
            break;
        }
        case 0xC8:
        { /* SET 1,B */
            REG_B |= 0x02;
            break;
        }
        case 0xC9:
        { /* SET 1,C */
            REG_C |= 0x02;
            break;
        }
        case 0xCA:
        { /* SET 1,D */
            REG_D |= 0x02;
            break;
        }
        case 0xCB:
        { /* SET 1,E */
            REG_E |= 0x02;
            break;
        }
        case 0xCC:
        { /* SET 1,H */
            REG_H |= 0x02;
            break;
        }
        case 0xCD:
        { /* SET 1,L */
            REG_L |= 0x02;
            break;
        }
        case 0xCE:
        { /* SET 1,(HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL) | 0x02;
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0xCF:
        { /* SET 1,A */
            regA |= 0x02;
            break;
        }
        case 0xD0:
        { /* SET 2,B */
            REG_B |= 0x04;
            break;
        }
        case 0xD1:
        { /* SET 2,C */
            REG_C |= 0x04;
            break;
        }
        case 0xD2:
        { /* SET 2,D */
            REG_D |= 0x04;
            break;
        }
        case 0xD3:
        { /* SET 2,E */
            REG_E |= 0x04;
            break;
        }
        case 0xD4:
        { /* SET 2,H */
            REG_H |= 0x04;
            break;
        }
        case 0xD5:
        { /* SET 2,L */
            REG_L |= 0x04;
            break;
        }
        case 0xD6:
        { /* SET 2,(HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL) | 0x04;
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0xD7:
        { /* SET 2,A */
            regA |= 0x04;
            break;
        }
        case 0xD8:
        { /* SET 3,B */
            REG_B |= 0x08;
            break;
        }
        case 0xD9:
        { /* SET 3,C */
            REG_C |= 0x08;
            break;
        }
        case 0xDA:
        { /* SET 3,D */
            REG_D |= 0x08;
            break;
        }
        case 0xDB:
        { /* SET 3,E */
            REG_E |= 0x08;
            break;
        }
        case 0xDC:
        { /* SET 3,H */
            REG_H |= 0x08;
            break;
        }
        case 0xDD:
        { /* SET 3,L */
            REG_L |= 0x08;
            break;
        }
        case 0xDE:
        { /* SET 3,(HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL) | 0x08;
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0xDF:
        { /* SET 3,A */
            regA |= 0x08;
            break;
        }
        case 0xE0:
        { /* SET 4,B */
            REG_B |= 0x10;
            break;
        }
        case 0xE1:
        { /* SET 4,C */
            REG_C |= 0x10;
            break;
        }
        case 0xE2:
        { /* SET 4,D */
            REG_D |= 0x10;
            break;
        }
        case 0xE3:
        { /* SET 4,E */
            REG_E |= 0x10;
            break;
        }
        case 0xE4:
        { /* SET 4,H */
            REG_H |= 0x10;
            break;
        }
        case 0xE5:
        { /* SET 4,L */
            REG_L |= 0x10;
            break;
        }
        case 0xE6:
        { /* SET 4,(HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL) | 0x10;
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0xE7:
        { /* SET 4,A */
            regA |= 0x10;
            break;
        }
        case 0xE8:
        { /* SET 5,B */
            REG_B |= 0x20;
            break;
        }
        case 0xE9:
        { /* SET 5,C */
            REG_C |= 0x20;
            break;
        }
        case 0xEA:
        { /* SET 5,D */
            REG_D |= 0x20;
            break;
        }
        case 0xEB:
        { /* SET 5,E */
            REG_E |= 0x20;
            break;
        }
        case 0xEC:
        { /* SET 5,H */
            REG_H |= 0x20;
            break;
        }
        case 0xED:
        { /* SET 5,L */
            REG_L |= 0x20;
            break;
        }
        case 0xEE:
        { /* SET 5,(HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL) | 0x20;
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0xEF:
        { /* SET 5,A */
            regA |= 0x20;
            break;
        }
        case 0xF0:
        { /* SET 6,B */
            REG_B |= 0x40;
            break;
        }
        case 0xF1:
        { /* SET 6,C */
            REG_C |= 0x40;
            break;
        }
        case 0xF2:
        { /* SET 6,D */
            REG_D |= 0x40;
            break;
        }
        case 0xF3:
        { /* SET 6,E */
            REG_E |= 0x40;
            break;
        }
        case 0xF4:
        { /* SET 6,H */
            REG_H |= 0x40;
            break;
        }
        case 0xF5:
        { /* SET 6,L */
            REG_L |= 0x40;
            break;
        }
        case 0xF6:
        { /* SET 6,(HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL) | 0x40;
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0xF7:
        { /* SET 6,A */
            regA |= 0x40;
            break;
        }
        case 0xF8:
        { /* SET 7,B */
            REG_B |= 0x80;
            break;
        }
        case 0xF9:
        { /* SET 7,C */
            REG_C |= 0x80;
            break;
        }
        case 0xFA:
        { /* SET 7,D */
            REG_D |= 0x80;
            break;
        }
        case 0xFB:
        { /* SET 7,E */
            REG_E |= 0x80;
            break;
        }
        case 0xFC:
        { /* SET 7,H */
            REG_H |= 0x80;
            break;
        }
        case 0xFD:
        { /* SET 7,L */
            REG_L |= 0x80;
            break;
        }
        case 0xFE:
        { /* SET 7,(HL) */
            uint8_t work8 = Z80opsImpl->peek8(REG_HL) | 0x80;
            Z80opsImpl->addressOnBus(REG_HL, 1);
            Z80opsImpl->poke8(REG_HL, work8);
            break;
        }
        case 0xFF:
        { /* SET 7,A */
            regA |= 0x80;
            break;
        }
        default:
        {
            break;
        }
    }
}

//Subconjunto de instrucciones 0xDD / 0xFD
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
template<typename Derived>
void Z80<Derived>::decodeDDFD(uint8_t opCode, RegisterPair& regIXY) {
    switch (opCode) {
        case 0x09:
        { /* ADD IX,BC */
            Z80opsImpl->addressOnBus(getIRWord(), 7);
            add16(regIXY, REG_BC);
            break;
        }
        case 0x19:
        { /* ADD IX,DE */
            Z80opsImpl->addressOnBus(getIRWord(), 7);
            add16(regIXY, REG_DE);
            break;
        }
        case 0x21:
        { /* LD IX,nn */
            regIXY.word = Z80opsImpl->peek16(REG_PC);
            REG_PC += 2;
            break;
        }
        case 0x22:
        { /* LD (nn),IX */
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            Z80opsImpl->poke16(REG_WZ++, regIXY);
            REG_PC += 2;
            break;
        }
        case 0x23:
        { /* INC IX */
            Z80opsImpl->addressOnBus(getIRWord(), 2);
            regIXY.word++;
            break;
        }
        case 0x24:
        { /* INC IXh */
            inc8(regIXY.byte8.hi);
            break;
        }
        case 0x25:
        { /* DEC IXh */
            dec8(regIXY.byte8.hi);
            break;
        }
        case 0x26:
        { /* LD IXh,n */
            regIXY.byte8.hi = Z80opsImpl->peek8(REG_PC);
            REG_PC++;
            break;
        }
        case 0x29:
        { /* ADD IX,IX */
            Z80opsImpl->addressOnBus(getIRWord(), 7);
            add16(regIXY, regIXY.word);
            break;
        }
        case 0x2A:
        { /* LD IX,(nn) */
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            regIXY.word = Z80opsImpl->peek16(REG_WZ++);
            REG_PC += 2;
            break;
        }
        case 0x2B:
        { /* DEC IX */
            Z80opsImpl->addressOnBus(getIRWord(), 2);
            regIXY.word--;
            break;
        }
        case 0x2C:
        { /* INC IXl */
            inc8(regIXY.byte8.lo);
            break;
        }
        case 0x2D:
        { /* DEC IXl */
            dec8(regIXY.byte8.lo);
            break;
        }
        case 0x2E:
        { /* LD IXl,n */
            regIXY.byte8.lo = Z80opsImpl->peek8(REG_PC);
            REG_PC++;
            break;
        }
        case 0x34:
        { /* INC (IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            uint8_t work8 = Z80opsImpl->peek8(REG_WZ);
            Z80opsImpl->addressOnBus(REG_WZ, 1);
            inc8(work8);
            Z80opsImpl->poke8(REG_WZ, work8);
            break;
        }
        case 0x35:
        { /* DEC (IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            uint8_t work8 = Z80opsImpl->peek8(REG_WZ);
            Z80opsImpl->addressOnBus(REG_WZ, 1);
            dec8(work8);
            Z80opsImpl->poke8(REG_WZ, work8);
            break;
        }
        case 0x36:
        { /* LD (IX+d),n */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            REG_PC++;
            uint8_t work8 = Z80opsImpl->peek8(REG_PC);
            Z80opsImpl->addressOnBus(REG_PC, 2);
            REG_PC++;
            Z80opsImpl->poke8(REG_WZ, work8);
            break;
        }
        case 0x39:
        { /* ADD IX,SP */
            Z80opsImpl->addressOnBus(getIRWord(), 7);
            add16(regIXY, REG_SP);
            break;
        }
        case 0x44:
        { /* LD B,IXh */
            REG_B = regIXY.byte8.hi;
            break;
        }
        case 0x45:
        { /* LD B,IXl */
            REG_B = regIXY.byte8.lo;
            break;
        }
        case 0x46:
        { /* LD B,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_B = Z80opsImpl->peek8(REG_WZ);
            break;
        }
        case 0x4C:
        { /* LD C,IXh */
            REG_C = regIXY.byte8.hi;
            break;
        }
        case 0x4D:
        { /* LD C,IXl */
            REG_C = regIXY.byte8.lo;
            break;
        }
        case 0x4E:
        { /* LD C,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_C = Z80opsImpl->peek8(REG_WZ);
            break;
        }
        case 0x54:
        { /* LD D,IXh */
            REG_D = regIXY.byte8.hi;
            break;
        }
        case 0x55:
        { /* LD D,IXl */
            REG_D = regIXY.byte8.lo;
            break;
        }
        case 0x56:
        { /* LD D,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_D = Z80opsImpl->peek8(REG_WZ);
            break;
        }
        case 0x5C:
        { /* LD E,IXh */
            REG_E = regIXY.byte8.hi;
            break;
        }
        case 0x5D:
        { /* LD E,IXl */
            REG_E = regIXY.byte8.lo;
            break;
        }
        case 0x5E:
        { /* LD E,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_E = Z80opsImpl->peek8(REG_WZ);
            break;
        }
        case 0x60:
        { /* LD IXh,B */
            regIXY.byte8.hi = REG_B;
            break;
        }
        case 0x61:
        { /* LD IXh,C */
            regIXY.byte8.hi = REG_C;
            break;
        }
        case 0x62:
        { /* LD IXh,D */
            regIXY.byte8.hi = REG_D;
            break;
        }
        case 0x63:
        { /* LD IXh,E */
            regIXY.byte8.hi = REG_E;
            break;
        }
        case 0x64:
        { /* LD IXh,IXh */
            break;
        }
        case 0x65:
        { /* LD IXh,IXl */
            regIXY.byte8.hi = regIXY.byte8.lo;
            break;
        }
        case 0x66:
        { /* LD H,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_H = Z80opsImpl->peek8(REG_WZ);
            break;
        }
        case 0x67:
        { /* LD IXh,A */
            regIXY.byte8.hi = regA;
            break;
        }
        case 0x68:
        { /* LD IXl,B */
            regIXY.byte8.lo = REG_B;
            break;
        }
        case 0x69:
        { /* LD IXl,C */
            regIXY.byte8.lo = REG_C;
            break;
        }
        case 0x6A:
        { /* LD IXl,D */
            regIXY.byte8.lo = REG_D;
            break;
        }
        case 0x6B:
        { /* LD IXl,E */
            regIXY.byte8.lo = REG_E;
            break;
        }
        case 0x6C:
        { /* LD IXl,IXh */
            regIXY.byte8.lo = regIXY.byte8.hi;
            break;
        }
        case 0x6D:
        { /* LD IXl,IXl */
            break;
        }
        case 0x6E:
        { /* LD L,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_L = Z80opsImpl->peek8(REG_WZ);
            break;
        }
        case 0x6F:
        { /* LD IXl,A */
            regIXY.byte8.lo = regA;
            break;
        }
        case 0x70:
        { /* LD (IX+d),B */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            Z80opsImpl->poke8(REG_WZ, REG_B);
            break;
        }
        case 0x71:
        { /* LD (IX+d),C */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            Z80opsImpl->poke8(REG_WZ, REG_C);
            break;
        }
        case 0x72:
        { /* LD (IX+d),D */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            Z80opsImpl->poke8(REG_WZ, REG_D);
            break;
        }
        case 0x73:
        { /* LD (IX+d),E */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            Z80opsImpl->poke8(REG_WZ, REG_E);
            break;
        }
        case 0x74:
        { /* LD (IX+d),H */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            Z80opsImpl->poke8(REG_WZ, REG_H);
            break;
        }
        case 0x75:
        { /* LD (IX+d),L */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            Z80opsImpl->poke8(REG_WZ, REG_L);
            break;
        }
        case 0x77:
        { /* LD (IX+d),A */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            Z80opsImpl->poke8(REG_WZ, regA);
            break;
        }
        case 0x7C:
        { /* LD A,IXh */
            regA = regIXY.byte8.hi;
            break;
        }
        case 0x7D:
        { /* LD A,IXl */
            regA = regIXY.byte8.lo;
            break;
        }
        case 0x7E:
        { /* LD A,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            regA = Z80opsImpl->peek8(REG_WZ);
            break;
        }
        case 0x84:
        { /* ADD A,IXh */
            add(regIXY.byte8.hi);
            break;
        }
        case 0x85:
        { /* ADD A,IXl */
            add(regIXY.byte8.lo);
            break;
        }
        case 0x86:
        { /* ADD A,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            add(Z80opsImpl->peek8(REG_WZ));
            break;
        }
        case 0x8C:
        { /* ADC A,IXh */
            adc(regIXY.byte8.hi);
            break;
        }
        case 0x8D:
        { /* ADC A,IXl */
            adc(regIXY.byte8.lo);
            break;
        }
        case 0x8E:
        { /* ADC A,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            adc(Z80opsImpl->peek8(REG_WZ));
            break;
        }
        case 0x94:
        { /* SUB IXh */
            sub(regIXY.byte8.hi);
            break;
        }
        case 0x95:
        { /* SUB IXl */
            sub(regIXY.byte8.lo);
            break;
        }
        case 0x96:
        { /* SUB (IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            sub(Z80opsImpl->peek8(REG_WZ));
            break;
        }
        case 0x9C:
        { /* SBC A,IXh */
            sbc(regIXY.byte8.hi);
            break;
        }
        case 0x9D:
        { /* SBC A,IXl */
            sbc(regIXY.byte8.lo);
            break;
        }
        case 0x9E:
        { /* SBC A,(IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            sbc(Z80opsImpl->peek8(REG_WZ));
            break;
        }
        case 0xA4:
        { /* AND IXh */
            and_(regIXY.byte8.hi);
            break;
        }
        case 0xA5:
        { /* AND IXl */
            and_(regIXY.byte8.lo);
            break;
        }
        case 0xA6:
        { /* AND (IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            and_(Z80opsImpl->peek8(REG_WZ));
            break;
        }
        case 0xAC:
        { /* XOR IXh */
            xor_(regIXY.byte8.hi);
            break;
        }
        case 0xAD:
        { /* XOR IXl */
            xor_(regIXY.byte8.lo);
            break;
        }
        case 0xAE:
        { /* XOR (IX+d) */
            REG_WZ = regIXY.word + static_cast<int8_t>(Z80opsImpl->peek8(REG_PC));
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            xor_(Z80opsImpl->peek8(REG_WZ));
            break;
        }
        case 0xB4:
        { /* OR IXh */
            or_(regIXY.byte8.hi);
            break;
        }
        case 0xB5:
        { /* OR IXl */
            or_(regIXY.byte8.lo);
            break;
        }
        case 0xB6:
        { /* OR (IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80opsImpl->peek8(REG_PC);
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            or_(Z80opsImpl->peek8(REG_WZ));
            break;
        }
        case 0xBC:
        { /* CP IXh */
            cp(regIXY.byte8.hi);
            break;
        }
        case 0xBD:
        { /* CP IXl */
            cp(regIXY.byte8.lo);
            break;
        }
        case 0xBE:
        { /* CP (IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80opsImpl->peek8(REG_PC);
            Z80opsImpl->addressOnBus(REG_PC, 5);
            REG_PC++;
            cp(Z80opsImpl->peek8(REG_WZ));
            break;
        }
        case 0xCB:
        { /* Subconjunto de instrucciones */
            REG_WZ = regIXY.word + (int8_t) Z80opsImpl->peek8(REG_PC);
            REG_PC++;
            opCode = Z80opsImpl->peek8(REG_PC);
            Z80opsImpl->addressOnBus(REG_PC, 2);
            REG_PC++;
            decodeDDFDCB(opCode, REG_WZ);
            break;
        }
        case 0xDD:
            prefixOpcode = 0xDD;
            break;
        case 0xE1:
        { /* POP IX */
            regIXY.word = pop();
            break;
        }
        case 0xE3:
        { /* EX (SP),IX */
            // Instrucción de ejecución sutil como pocas... atento al dato.
            RegisterPair work16 = regIXY;
            regIXY.word = Z80opsImpl->peek16(REG_SP);
            Z80opsImpl->addressOnBus(REG_SP + 1, 1);
            /* I can't call poke16 from here because the Z80 CPU does the writes in inverted order.
             * Same thing goes for EX (SP), HL.
             */
            Z80opsImpl->poke8(REG_SP + 1, work16.byte8.hi);
            Z80opsImpl->poke8(REG_SP, work16.byte8.lo);
            Z80opsImpl->addressOnBus(REG_SP, 2);
            REG_WZ = regIXY.word;
            break;
        }
        case 0xE5:
        { /* PUSH IX */
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            push(regIXY.word);
            break;
        }
        case 0xE9:
        { /* JP (IX) */
            REG_PC = regIXY.word;
            break;
        }
        case 0xED:
        {
            prefixOpcode = 0xED;
            break;
        }
        case 0xF9:
        { /* LD SP,IX */
            Z80opsImpl->addressOnBus(getIRWord(), 2);
            REG_SP = regIXY.word;
            break;
        }
        case 0xFD:
        {
            prefixOpcode = 0xFD;
            break;
        }
        default:
        {
            // Detrás de un DD/FD o varios en secuencia venía un código
            // que no correspondía con una instrucción que involucra a
            // IX o IY. Se trata como si fuera un código normal.
            // Sin esto, además de emular mal, falla el test
            // ld <bcdexya>,<bcdexya> de ZEXALL.
#ifdef WITH_BREAKPOINT_SUPPORT
            if (breakpointEnabled && prefixOpcode == 0) {
                opCode = Z80opsImpl->breakpoint(REG_PC, opCode);
            }
#endif
            decodeOpcode(opCode);
            break;
        }
    }
}

// Subconjunto de instrucciones 0xDDCB
template<typename Derived>
void Z80<Derived>::decodeDDFDCB(uint8_t opCode, uint16_t address) {

    switch (opCode) {
        case 0x00: /* RLC (IX+d),B */
        case 0x01: /* RLC (IX+d),C */
        case 0x02: /* RLC (IX+d),D */
        case 0x03: /* RLC (IX+d),E */
        case 0x04: /* RLC (IX+d),H */
        case 0x05: /* RLC (IX+d),L */
        case 0x06: /* RLC (IX+d)   */
        case 0x07: /* RLC (IX+d),A */
        {
            uint8_t work8 = Z80opsImpl->peek8(address);
            rlc(work8);
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address);
            rrc(work8);
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address);
            rl(work8);
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address);
            rr(work8);
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
             uint8_t work8 = Z80opsImpl->peek8(address);
             sla(work8);
             Z80opsImpl->addressOnBus(address, 1);
             Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address);
            sra(work8);
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address);
            sll(work8);
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address);
            srl(work8);
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
        case 0x47:
        { /* BIT 0,(IX+d) */
            bitTest(0x01, Z80opsImpl->peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                    | ((address >> 8) & FLAG_53_MASK);
            Z80opsImpl->addressOnBus(address, 1);
            break;
        }
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B:
        case 0x4C:
        case 0x4D:
        case 0x4E:
        case 0x4F:
        { /* BIT 1,(IX+d) */
            bitTest(0x02, Z80opsImpl->peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                    | ((address >> 8) & FLAG_53_MASK);
            Z80opsImpl->addressOnBus(address, 1);
            break;
        }
        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x54:
        case 0x55:
        case 0x56:
        case 0x57:
        { /* BIT 2,(IX+d) */
            bitTest(0x04, Z80opsImpl->peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                    | ((address >> 8) & FLAG_53_MASK);
            Z80opsImpl->addressOnBus(address, 1);
            break;
        }
        case 0x58:
        case 0x59:
        case 0x5A:
        case 0x5B:
        case 0x5C:
        case 0x5D:
        case 0x5E:
        case 0x5F:
        { /* BIT 3,(IX+d) */
            bitTest(0x08, Z80opsImpl->peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                    | ((address >> 8) & FLAG_53_MASK);
            Z80opsImpl->addressOnBus(address, 1);
            break;
        }
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
        case 0x64:
        case 0x65:
        case 0x66:
        case 0x67:
        { /* BIT 4,(IX+d) */
            bitTest(0x10, Z80opsImpl->peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                    | ((address >> 8) & FLAG_53_MASK);
            Z80opsImpl->addressOnBus(address, 1);
            break;
        }
        case 0x68:
        case 0x69:
        case 0x6A:
        case 0x6B:
        case 0x6C:
        case 0x6D:
        case 0x6E:
        case 0x6F:
        { /* BIT 5,(IX+d) */
            bitTest(0x20, Z80opsImpl->peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                    | ((address >> 8) & FLAG_53_MASK);
            Z80opsImpl->addressOnBus(address, 1);
            break;
        }
        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x76:
        case 0x77:
        { /* BIT 6,(IX+d) */
            bitTest(0x40, Z80opsImpl->peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                    | ((address >> 8) & FLAG_53_MASK);
            Z80opsImpl->addressOnBus(address, 1);
            break;
        }
        case 0x78:
        case 0x79:
        case 0x7A:
        case 0x7B:
        case 0x7C:
        case 0x7D:
        case 0x7E:
        case 0x7F:
        { /* BIT 7,(IX+d) */
            bitTest(0x80, Z80opsImpl->peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                    | ((address >> 8) & FLAG_53_MASK);
            Z80opsImpl->addressOnBus(address, 1);
            break;
        }
        case 0x80: /* RES 0,(IX+d),B */
        case 0x81: /* RES 0,(IX+d),C */
        case 0x82: /* RES 0,(IX+d),D */
        case 0x83: /* RES 0,(IX+d),E */
        case 0x84: /* RES 0,(IX+d),H */
        case 0x85: /* RES 0,(IX+d),L */
        case 0x86: /* RES 0,(IX+d)   */
        case 0x87: /* RES 0,(IX+d),A */
        {
            uint8_t work8 = Z80opsImpl->peek8(address) & 0xFE;
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address) & 0xFD;
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address) & 0xFB;
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address) & 0xF7;
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address) & 0xEF;
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address) & 0xDF;
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address) & 0xBF;
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address) & 0x7F;
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address) | 0x01;
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address) | 0x02;
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address) | 0x04;
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address) | 0x08;
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address) | 0x10;
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address) | 0x20;
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address) | 0x40;
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
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
            uint8_t work8 = Z80opsImpl->peek8(address) | 0x80;
            Z80opsImpl->addressOnBus(address, 1);
            Z80opsImpl->poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
    }
}

//Subconjunto de instrucciones 0xED

template<typename Derived>
void Z80<Derived>::decodeED(uint8_t opCode) {
    switch (opCode) {
        case 0x40:
        { /* IN B,(C) */
            REG_WZ = REG_BC;
            REG_B = Z80opsImpl->inPort(REG_WZ);
            REG_WZ++;
            sz5h3pnFlags = sz53pn_addTable[REG_B];
            flagQ = true;
            break;
        }
        case 0x41:
        { /* OUT (C),B */
            REG_WZ = REG_BC;
            Z80opsImpl->outPort(REG_WZ, REG_B);
            REG_WZ++;
            break;
        }
        case 0x42:
        { /* SBC HL,BC */
            Z80opsImpl->addressOnBus(getIRWord(), 7);
            sbc16(REG_BC);
            break;
        }
        case 0x43:
        { /* LD (nn),BC */
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            Z80opsImpl->poke16(REG_WZ, regBC);
            REG_WZ++;
            REG_PC += 2;
            break;
        }
        case 0x44:
        case 0x4C:
        case 0x54:
        case 0x5C:
        case 0x64:
        case 0x6C:
        case 0x74:
        case 0x7C:
        { /* NEG */
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
        case 0x7D:
        { /* RETN */
            ffIFF1 = ffIFF2;
            REG_PC = REG_WZ = pop();
            break;
        }
        case 0x4D:
        { /* RETI */
            /* According to the Z80 documentation, RETI should not update IFF1 from
             * IFF2; only RETN does this (to restore interrupts after NMI). This affects
             * precise interrupt handling behavior.
             */
            REG_PC = REG_WZ = pop();
            break;
        }
        case 0x46:
        case 0x4E:
        case 0x66:
        case 0x6E:
        { /* IM 0 */
            modeINT = IntMode::IM0;
            break;
        }
        case 0x47:
        { /* LD I,A */
            /*
             * El par IR se pone en el bus de direcciones *antes*
             * de poner A en el registro I. Detalle importante.
             */
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            regI = regA;
            break;
        }
        case 0x48:
        { /* IN C,(C) */
            REG_WZ = REG_BC;
            REG_C = Z80opsImpl->inPort(REG_WZ);
            REG_WZ++;
            sz5h3pnFlags = sz53pn_addTable[REG_C];
            flagQ = true;
            break;
        }
        case 0x49:
        { /* OUT (C),C */
            REG_WZ = REG_BC;
            Z80opsImpl->outPort(REG_WZ, REG_C);
            REG_WZ++;
            break;
        }
        case 0x4A:
        { /* ADC HL,BC */
            Z80opsImpl->addressOnBus(getIRWord(), 7);
            adc16(REG_BC);
            break;
        }
        case 0x4B:
        { /* LD BC,(nn) */
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            REG_BC = Z80opsImpl->peek16(REG_WZ);
            REG_WZ++;
            REG_PC += 2;
            break;
        }
        case 0x4F:
        { /* LD R,A */
            /*
             * El par IR se pone en el bus de direcciones *antes*
             * de poner A en el registro R. Detalle importante.
             */
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            setRegR(regA);
            break;
        }
        case 0x50:
        { /* IN D,(C) */
            REG_WZ = REG_BC;
            REG_D = Z80opsImpl->inPort(REG_WZ);
            REG_WZ++;
            sz5h3pnFlags = sz53pn_addTable[REG_D];
            flagQ = true;
            break;
        }
        case 0x51:
        { /* OUT (C),D */
            REG_WZ = REG_BC;
            Z80opsImpl->outPort(REG_WZ++, REG_D);
            break;
        }
        case 0x52:
        { /* SBC HL,DE */
            Z80opsImpl->addressOnBus(getIRWord(), 7);
            sbc16(REG_DE);
            break;
        }
        case 0x53:
        { /* LD (nn),DE */
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            Z80opsImpl->poke16(REG_WZ++, regDE);
            REG_PC += 2;
            break;
        }
        case 0x56:
        case 0x76:
        { /* IM 1 */
            modeINT = IntMode::IM1;
            break;
        }
        case 0x57:
        { /* LD A,I */
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            regA = regI;
            sz5h3pnFlags = sz53n_addTable[regA];
            /*
             * The P / V flag should reflect IFF2 state regardless of whether an
             * interrupt is currently being signaled on the bus.
             */
            if (ffIFF2) {
                sz5h3pnFlags |= PARITY_MASK;
            }
            flagQ = true;
            break;
        }
        case 0x58:
        { /* IN E,(C) */
            REG_WZ = REG_BC;
            REG_E = Z80opsImpl->inPort(REG_WZ++);
            sz5h3pnFlags = sz53pn_addTable[REG_E];
            flagQ = true;
            break;
        }
        case 0x59:
        { /* OUT (C),E */
            REG_WZ = REG_BC;
            Z80opsImpl->outPort(REG_WZ++, REG_E);
            break;
        }
        case 0x5A:
        { /* ADC HL,DE */
            Z80opsImpl->addressOnBus(getIRWord(), 7);
            adc16(REG_DE);
            break;
        }
        case 0x5B:
        { /* LD DE,(nn) */
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            REG_DE = Z80opsImpl->peek16(REG_WZ++);
            REG_PC += 2;
            break;
        }
        case 0x5E:
        case 0x7E:
        { /* IM 2 */
            modeINT = IntMode::IM2;
            break;
        }
        case 0x5F:
        { /* LD A,R */
            Z80opsImpl->addressOnBus(getIRWord(), 1);
            regA = getRegR();
            sz5h3pnFlags = sz53n_addTable[regA];
            /*
             * The P / V flag should reflect IFF2 state regardless of whether an
             * interrupt is currently being signaled on the bus.
             */
            if (ffIFF2) {
                sz5h3pnFlags |= PARITY_MASK;
            }
            flagQ = true;
            break;
        }
        case 0x60:
        { /* IN H,(C) */
            REG_WZ = REG_BC;
            REG_H = Z80opsImpl->inPort(REG_WZ++);
            sz5h3pnFlags = sz53pn_addTable[REG_H];
            flagQ = true;
            break;
        }
        case 0x61:
        { /* OUT (C),H */
            REG_WZ = REG_BC;
            Z80opsImpl->outPort(REG_WZ++, REG_H);
            break;
        }
        case 0x62:
        { /* SBC HL,HL */
            Z80opsImpl->addressOnBus(getIRWord(), 7);
            sbc16(REG_HL);
            break;
        }
        case 0x63:
        { /* LD (nn),HL */
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            Z80opsImpl->poke16(REG_WZ++, regHL);
            REG_PC += 2;
            break;
        }
        case 0x67:
        { /* RRD */
            // A = A7 A6 A5 A4 (HL)3 (HL)2 (HL)1 (HL)0
            // (HL) = A3 A2 A1 A0 (HL)7 (HL)6 (HL)5 (HL)4
            // Los bits 3,2,1 y 0 de (HL) se copian a los bits 3,2,1 y 0 de A.
            // Los 4 bits bajos que había en A se copian a los bits 7,6,5 y 4 de (HL).
            // Los 4 bits altos que había en (HL) se copian a los 4 bits bajos de (HL)
            // Los 4 bits superiores de A no se tocan. ¡p'habernos matao!
            uint8_t aux = regA << 4;
            REG_WZ = REG_HL;
            uint16_t memHL = Z80opsImpl->peek8(REG_WZ);
            regA = (regA & 0xf0) | (memHL & 0x0f);
            Z80opsImpl->addressOnBus(REG_WZ, 4);
            Z80opsImpl->poke8(REG_WZ++, (memHL >> 4) | aux);
            sz5h3pnFlags = sz53pn_addTable[regA];
            flagQ = true;
            break;
        }
        case 0x68:
        { /* IN L,(C) */
            REG_WZ = REG_BC;
            REG_L = Z80opsImpl->inPort(REG_WZ++);
            sz5h3pnFlags = sz53pn_addTable[REG_L];
            flagQ = true;
            break;
        }
        case 0x69:
        { /* OUT (C),L */
            REG_WZ = REG_BC;
            Z80opsImpl->outPort(REG_WZ++, REG_L);
            break;
        }
        case 0x6A:
        { /* ADC HL,HL */
            Z80opsImpl->addressOnBus(getIRWord(), 7);
            adc16(REG_HL);
            break;
        }
        case 0x6B:
        { /* LD HL,(nn) */
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            REG_HL = Z80opsImpl->peek16(REG_WZ++);
            REG_PC += 2;
            break;
        }
        case 0x6F:
        { /* RLD */
            // A = A7 A6 A5 A4 (HL)7 (HL)6 (HL)5 (HL)4
            // (HL) = (HL)3 (HL)2 (HL)1 (HL)0 A3 A2 A1 A0
            // Los 4 bits bajos que había en (HL) se copian a los bits altos de (HL).
            // Los 4 bits altos que había en (HL) se copian a los 4 bits bajos de A
            // Los bits 3,2,1 y 0 de A se copian a los bits 3,2,1 y 0 de (HL).
            // Los 4 bits superiores de A no se tocan. ¡p'habernos matao!
            uint8_t aux = regA & 0x0f;
            REG_WZ = REG_HL;
            uint16_t memHL = Z80opsImpl->peek8(REG_WZ);
            regA = (regA & 0xf0) | (memHL >> 4);
            Z80opsImpl->addressOnBus(REG_WZ, 4);
            Z80opsImpl->poke8(REG_WZ++, (memHL << 4) | aux);
            sz5h3pnFlags = sz53pn_addTable[regA];
            flagQ = true;
            break;
        }
        case 0x70:
        { /* IN (C) */
            REG_WZ = REG_BC;
            uint8_t inPort = Z80opsImpl->inPort(REG_WZ++);
            sz5h3pnFlags = sz53pn_addTable[inPort];
            flagQ = true;
            break;
        }
        case 0x71:
        { /* OUT (C),0 */
            REG_WZ = REG_BC;
            Z80opsImpl->outPort(REG_WZ++, 0x00);
            break;
        }
        case 0x72:
        { /* SBC HL,SP */
            Z80opsImpl->addressOnBus(getIRWord(), 7);
            sbc16(REG_SP);
            break;
        }
        case 0x73:
        { /* LD (nn),SP */
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            Z80opsImpl->poke16(REG_WZ++, regSP);
            REG_PC += 2;
            break;
        }
        case 0x78:
        { /* IN A,(C) */
            REG_WZ = REG_BC;
            regA = Z80opsImpl->inPort(REG_WZ++);
            sz5h3pnFlags = sz53pn_addTable[regA];
            flagQ = true;
            break;
        }
        case 0x79:
        { /* OUT (C),A */
            REG_WZ = REG_BC;
            Z80opsImpl->outPort(REG_WZ++, regA);
            break;
        }
        case 0x7A:
        { /* ADC HL,SP */
            Z80opsImpl->addressOnBus(getIRWord(), 7);
            adc16(REG_SP);
            break;
        }
        case 0x7B:
        { /* LD SP,(nn) */
            REG_WZ = Z80opsImpl->peek16(REG_PC);
            REG_SP = Z80opsImpl->peek16(REG_WZ++);
            REG_PC += 2;
            break;
        }
        case 0xA0:
        { /* LDI */
            ldi();
            break;
        }
        case 0xA1:
        { /* CPI */
            cpi();
            break;
        }
        case 0xA2:
        { /* INI */
            ini();
            break;
        }
        case 0xA3:
        { /* OUTI */
            outi();
            break;
        }
        case 0xA8:
        { /* LDD */
            ldd();
            break;
        }
        case 0xA9:
        { /* CPD */
            cpd();
            break;
        }
        case 0xAA:
        { /* IND */
            ind();
            break;
        }
        case 0xAB:
        { /* OUTD */
            outd();
            break;
        }
        case 0xB0:
        { /* LDIR */
            ldi();
            if (REG_BC != 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                Z80opsImpl->addressOnBus(REG_DE - 1, 5);
                sz5h3pnFlags &= ~FLAG_53_MASK;
                sz5h3pnFlags |= (REG_PCh & FLAG_53_MASK);
            }
            break;
        }
        case 0xB1:
        { /* CPIR */
            cpi();
            if ((sz5h3pnFlags & PARITY_MASK) == PARITY_MASK
                    && (sz5h3pnFlags & ZERO_MASK) == 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                Z80opsImpl->addressOnBus(REG_HL - 1, 5);
                sz5h3pnFlags &= ~FLAG_53_MASK;
                sz5h3pnFlags |= (REG_PCh & FLAG_53_MASK);
            }
            break;
        }
        case 0xB2:
        { /* INIR */
            ini();
            if (REG_B != 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                Z80opsImpl->addressOnBus(REG_HL - 1, 5);
                adjustINxROUTxRFlags();
            }
            break;
        }
        case 0xB3:
        { /* OTIR */
            outi();
            if (REG_B != 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                Z80opsImpl->addressOnBus(REG_BC, 5);
                adjustINxROUTxRFlags();
            }
            break;
        }
        case 0xB8:
        { /* LDDR */
            ldd();
            if (REG_BC != 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                Z80opsImpl->addressOnBus(REG_DE + 1, 5);
                sz5h3pnFlags &= ~FLAG_53_MASK;
                sz5h3pnFlags |= (REG_PCh & FLAG_53_MASK);
            }
            break;
        }
        case 0xB9:
        { /* CPDR */
            cpd();
            if ((sz5h3pnFlags & PARITY_MASK) == PARITY_MASK
                    && (sz5h3pnFlags & ZERO_MASK) == 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                Z80opsImpl->addressOnBus(REG_HL + 1, 5);
                sz5h3pnFlags &= ~FLAG_53_MASK;
                sz5h3pnFlags |= (REG_PCh & FLAG_53_MASK);
            }
            break;
        }
        case 0xBA:
        { /* INDR */
            ind();
            if (REG_B != 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                Z80opsImpl->addressOnBus(REG_HL + 1, 5);
                adjustINxROUTxRFlags();
            }
            break;
        }
        case 0xBB:
        { /* OTDR */
            outd();
            if (REG_B != 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                Z80opsImpl->addressOnBus(REG_BC, 5);
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
        {
            break;
        }
    }
}

template<typename Derived>
void Z80<Derived>::copyToRegister(uint8_t opCode, uint8_t value)
{
    switch (opCode & 0x07)
    {
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

template<typename Derived>
void Z80<Derived>::adjustINxROUTxRFlags()
{
    sz5h3pnFlags &= ~FLAG_53_MASK;
    sz5h3pnFlags |= (REG_PCh & FLAG_53_MASK);

    uint8_t pf = sz5h3pnFlags & PARITY_MASK;
    if (carryFlag)
    {
        int8_t addsub = 1 - (sz5h3pnFlags & ADDSUB_MASK);
        pf ^= sz53pn_addTable[(REG_B + addsub) & 0x07] ^ PARITY_MASK;
        if ((REG_B & 0x0F) == (addsub != 1 ? 0x00 : 0x0F))
            sz5h3pnFlags |= HALFCARRY_MASK;
        else
            sz5h3pnFlags &= ~HALFCARRY_MASK;
    } else {
        pf ^= sz53pn_addTable[REG_B & 0x07] ^ PARITY_MASK;
        sz5h3pnFlags &= ~HALFCARRY_MASK;
    }

    if (pf & PARITY_MASK)
        sz5h3pnFlags |= PARITY_MASK;
    else
        sz5h3pnFlags &= ~PARITY_MASK;
}
#endif // Z80_IMPL_H
