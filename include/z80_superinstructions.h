// Z80 Superinstructions - Fused Instruction Pairs
// GPL 3 License
//
// Superinstructions combine frequently occurring instruction sequences
// into single optimized operations, eliminating decode overhead and
// improving instruction cache utilization.
//
// Expected performance improvement: 20-40% for code with common patterns

#ifndef Z80_SUPERINSTRUCTIONS_H
#define Z80_SUPERINSTRUCTIONS_H

// Enable/disable superinstructions at compile time
#ifndef Z80_ENABLE_SUPERINSTRUCTIONS
    #define Z80_ENABLE_SUPERINSTRUCTIONS 1
#endif

// Statistics tracking (disable in release builds for max performance)
#ifndef Z80_SUPERINSTRUCTION_STATS
    #define Z80_SUPERINSTRUCTION_STATS 0
#endif

#if Z80_ENABLE_SUPERINSTRUCTIONS

namespace Z80Super {
    // Superinstruction opcodes (use reserved/undefined opcodes space)
    // We use the 0xED prefix unused opcodes for superinstructions
    enum SuperOpcode : uint8_t {
        // Format: SUPER_<instr1>_<instr2>

        // Top priority: >2% frequency patterns
        SUPER_INC_HL_LD_A_HL    = 0xE0,  // INC HL + LD A,(HL) - 7.54%
        SUPER_LD_A_HL_INC_HL    = 0xE1,  // LD A,(HL) + INC HL - 7.54%
        SUPER_LD_HL_LD_HL_n     = 0xE2,  // LD HL,nn + LD (HL),n - 4.00%
        SUPER_LD_A_n_CP_n       = 0xE3,  // LD A,n + CP n - 3.52%
        SUPER_LD_B_n_LD_C_n     = 0xE4,  // LD B,n + LD C,n - 2.63%
        SUPER_LD_HL_A_INC_HL    = 0xE5,  // LD (HL),A + INC HL - 2.34%

        // High priority: 1-2% frequency
        SUPER_OR_A_JR_Z         = 0xE6,  // OR A + JR Z,e - 1.87%
        SUPER_LD_HL_nn_LD_A_HL  = 0xE7,  // LD HL,nn + LD A,(HL)
        SUPER_DEC_B_JR_NZ       = 0xE8,  // DEC B + JR NZ,e (DJNZ variant)
        SUPER_PUSH_BC_PUSH_DE   = 0xE9,  // PUSH BC + PUSH DE

        // Medium priority: 0.5-1% frequency
        SUPER_POP_DE_POP_BC     = 0xEA,  // POP DE + POP BC
        SUPER_LD_DE_nn_LD_HL_nn = 0xEB,  // LD DE,nn + LD HL,nn
        SUPER_CP_n_RET_Z        = 0xEC,  // CP n + RET Z
        SUPER_CP_n_RET_NZ       = 0xED,  // CP n + RET NZ
        SUPER_INC_A_CP_n        = 0xEE,  // INC A + CP n
        SUPER_DEC_HL_LD_A_HL    = 0xEF,  // DEC HL + LD A,(HL)

        // Stack/Call patterns
        SUPER_CALL_RET_trivial  = 0xF0,  // CALL + RET (for trivial functions)
        SUPER_PUSH_BC_CALL      = 0xF1,  // PUSH BC + CALL nn
        SUPER_RET_POP_BC        = 0xF2,  // RET + POP BC pattern (reversed)

        // Memory patterns
        SUPER_LD_HL_LD_DE       = 0xF3,  // LD (HL),r + LD (DE),r pattern
        SUPER_INC_HL_INC_HL     = 0xF4,  // INC HL + INC HL (HL+=2)
        SUPER_DEC_HL_DEC_HL     = 0xF5,  // DEC HL + DEC HL (HL-=2)

        // Arithmetic chains
        SUPER_ADD_A_n_CP_n      = 0xF6,  // ADD A,n + CP n
        SUPER_SUB_n_JR_Z        = 0xF7,  // SUB n + JR Z,e
        SUPER_AND_n_JR_Z        = 0xF8,  // AND n + JR Z,e

        // Loop patterns
        SUPER_INC_HL_DJNZ       = 0xF9,  // INC HL + DJNZ - 2.10%
        SUPER_LD_A_HL_CP_n      = 0xFA,  // LD A,(HL) + CP n
        SUPER_LD_HL_A_DEC_HL    = 0xFB,  // LD (HL),A + DEC HL

        // Conditional patterns
        SUPER_OR_A_JR_NZ        = 0xFC,  // OR A + JR NZ,e
        SUPER_AND_A_JR_Z        = 0xFD,  // AND A + JR Z,e
        SUPER_CP_n_JR_NZ        = 0xFE,  // CP n + JR NZ,e
        SUPER_CP_n_JR_Z         = 0xFF   // CP n + JR Z,e
    };

#if Z80_SUPERINSTRUCTION_STATS
    struct SuperStats {
        uint64_t hits[256] = {0};
        uint64_t total_hits = 0;
        uint64_t instruction_count = 0;

        void record_hit(uint8_t super_opcode) {
            hits[super_opcode]++;
            total_hits++;
        }

        void record_instruction() {
            instruction_count++;
        }

        double get_hit_rate() const {
            return instruction_count > 0
                ? (100.0 * total_hits) / instruction_count
                : 0.0;
        }

        void print_report() const;
    };
#endif

} // namespace Z80Super

#endif // Z80_ENABLE_SUPERINSTRUCTIONS

#endif // Z80_SUPERINSTRUCTIONS_H

