// Z80 Instruction Pair Profiler
// Use this to identify which instruction sequences are most common
// and would benefit most from superinstruction optimization

#ifndef Z80_PROFILER_H
#define Z80_PROFILER_H

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <string>

namespace Z80Profiling {

struct InstructionPairStats {
    uint8_t first;
    uint8_t second;
    uint64_t count;
    double percentage;

    bool operator>(const InstructionPairStats& other) const {
        return count > other.count;
    }
};

class InstructionProfiler {
private:
    // Map from 16-bit pair (first << 8 | second) to count
    std::unordered_map<uint16_t, uint64_t> pairCounts;

    // Track individual instruction frequencies too
    std::unordered_map<uint8_t, uint64_t> singleCounts;

    uint8_t lastOpcode;
    uint64_t totalPairs;
    uint64_t totalInstructions;
    bool enabled;

    // Track prefix sequences separately (DD, ED, FD)
    std::unordered_map<uint16_t, uint64_t> prefixPairCounts;
    bool inPrefixMode;
    uint8_t prefixByte;

public:
    InstructionProfiler()
        : lastOpcode(0), totalPairs(0), totalInstructions(0),
          enabled(true), inPrefixMode(false), prefixByte(0) {}

    // Record an instruction (call this for each executed instruction)
    inline void recordInstruction(uint8_t opcode) {
        if (!enabled) return;

        totalInstructions++;
        singleCounts[opcode]++;

        // Check if this is a prefix instruction
        if (opcode == 0xDD || opcode == 0xED || opcode == 0xFD || opcode == 0xCB) {
            inPrefixMode = true;
            prefixByte = opcode;
        } else if (inPrefixMode) {
            // Record prefix + instruction pair
            uint16_t prefixPair = (prefixByte << 8) | opcode;
            prefixPairCounts[prefixPair]++;
            inPrefixMode = false;
        }

        // Record normal instruction pair
        if (totalInstructions > 1) {
            uint16_t pair = (lastOpcode << 8) | opcode;
            pairCounts[pair]++;
            totalPairs++;
        }

        lastOpcode = opcode;
    }

    // Get top N instruction pairs
    std::vector<InstructionPairStats> getTopPairs(size_t n = 50) const {
        std::vector<InstructionPairStats> result;

        for (const auto& [pair, count] : pairCounts) {
            InstructionPairStats stats;
            stats.first = pair >> 8;
            stats.second = pair & 0xFF;
            stats.count = count;
            stats.percentage = totalPairs > 0 ? (100.0 * count / totalPairs) : 0.0;
            result.push_back(stats);
        }

        std::sort(result.begin(), result.end(), std::greater<InstructionPairStats>());

        if (result.size() > n) {
            result.resize(n);
        }

        return result;
    }

    // Get top prefix pairs
    std::vector<InstructionPairStats> getTopPrefixPairs(size_t n = 20) const {
        std::vector<InstructionPairStats> result;

        uint64_t totalPrefix = 0;
        for (const auto& [pair, count] : prefixPairCounts) {
            totalPrefix += count;
        }

        for (const auto& [pair, count] : prefixPairCounts) {
            InstructionPairStats stats;
            stats.first = pair >> 8;
            stats.second = pair & 0xFF;
            stats.count = count;
            stats.percentage = totalPrefix > 0 ? (100.0 * count / totalPrefix) : 0.0;
            result.push_back(stats);
        }

        std::sort(result.begin(), result.end(), std::greater<InstructionPairStats>());

        if (result.size() > n) {
            result.resize(n);
        }

        return result;
    }

    // Print report to stdout
    void printReport() const {
        printf("\n");
        printf("================================================================================\n");
        printf("Z80 INSTRUCTION PROFILING REPORT\n");
        printf("================================================================================\n");
        printf("Total instructions executed: %llu\n", totalInstructions);
        printf("Total instruction pairs: %llu\n", totalPairs);
        printf("Unique pairs observed: %zu\n", pairCounts.size());
        printf("\n");

        // Top instruction pairs
        printf("TOP 50 INSTRUCTION PAIRS (candidates for superinstructions):\n");
        printf("--------------------------------------------------------------------------------\n");
        printf("Rank  First Second  Count          Percentage  Cumulative  Mnemonic Hint\n");
        printf("--------------------------------------------------------------------------------\n");

        auto topPairs = getTopPairs(50);
        double cumulative = 0.0;
        int rank = 1;

        for (const auto& stats : topPairs) {
            cumulative += stats.percentage;
            printf("%-4d  0x%02X  0x%02X    %-14llu %6.2f%%    %6.2f%%    %s\n",
                   rank++,
                   stats.first,
                   stats.second,
                   stats.count,
                   stats.percentage,
                   cumulative,
                   getMnemonicHint(stats.first, stats.second).c_str());
        }

        printf("\n");
        printf("Coverage: Top 10 pairs = %.2f%%, Top 20 = %.2f%%, Top 50 = %.2f%%\n",
               getCoverage(10), getCoverage(20), getCoverage(50));

        // Prefix pairs
        if (!prefixPairCounts.empty()) {
            printf("\n");
            printf("TOP 20 PREFIX INSTRUCTION PAIRS:\n");
            printf("--------------------------------------------------------------------------------\n");
            printf("Rank  Prefix Second  Count          Percentage  Mnemonic\n");
            printf("--------------------------------------------------------------------------------\n");

            auto topPrefix = getTopPrefixPairs(20);
            rank = 1;

            for (const auto& stats : topPrefix) {
                printf("%-4d  0x%02X   0x%02X    %-14llu %6.2f%%    %s\n",
                       rank++,
                       stats.first,
                       stats.second,
                       stats.count,
                       stats.percentage,
                       getPrefixMnemonicHint(stats.first, stats.second).c_str());
            }
        }

        printf("\n");
        printf("RECOMMENDATIONS:\n");
        printf("--------------------------------------------------------------------------------\n");

        double top20Coverage = getCoverage(20);
        if (top20Coverage > 40.0) {
            printf("✓ EXCELLENT: Top 20 pairs cover %.1f%% of all pairs.\n", top20Coverage);
            printf("  Implementing superinstructions for these 20 pairs could yield 2-3x speedup.\n");
        } else if (top20Coverage > 25.0) {
            printf("✓ GOOD: Top 20 pairs cover %.1f%% of all pairs.\n", top20Coverage);
            printf("  Implementing superinstructions could yield 1.5-2x speedup.\n");
        } else {
            printf("○ MODERATE: Top 20 pairs cover %.1f%% of all pairs.\n", top20Coverage);
            printf("  Consider profiling with different workloads.\n");
        }

        printf("\n");
        printf("Next steps:\n");
        printf("1. Implement superinstructions for top 10-20 pairs above\n");
        printf("2. Focus on pairs with >1%% frequency first\n");
        printf("3. Re-profile after optimization to measure improvement\n");
        printf("================================================================================\n");
    }

    // Save report to file
    void saveReport(const char* filename) const {
        FILE* f = fopen(filename, "w");
        if (!f) {
            fprintf(stderr, "Error: Could not open %s for writing\n", filename);
            return;
        }

        fprintf(f, "# Z80 Instruction Pair Profiling Report\n\n");
        fprintf(f, "Total instructions: %llu\n", totalInstructions);
        fprintf(f, "Total pairs: %llu\n\n", totalPairs);

        fprintf(f, "## Top 50 Instruction Pairs\n\n");
        fprintf(f, "| Rank | First | Second | Count | Percentage | Cumulative | Hint |\n");
        fprintf(f, "|------|-------|--------|-------|------------|------------|------|\n");

        auto topPairs = getTopPairs(50);
        double cumulative = 0.0;
        int rank = 1;

        for (const auto& stats : topPairs) {
            cumulative += stats.percentage;
            fprintf(f, "| %4d | 0x%02X  | 0x%02X   | %llu | %.2f%% | %.2f%% | %s |\n",
                    rank++, stats.first, stats.second, stats.count,
                    stats.percentage, cumulative,
                    getMnemonicHint(stats.first, stats.second).c_str());
        }

        fclose(f);
        printf("Report saved to %s\n", filename);
    }

    void enable() { enabled = true; }
    void disable() { enabled = false; }
    void reset() {
        pairCounts.clear();
        singleCounts.clear();
        prefixPairCounts.clear();
        totalPairs = 0;
        totalInstructions = 0;
        lastOpcode = 0;
        inPrefixMode = false;
    }

private:
    // Calculate coverage of top N pairs
    double getCoverage(size_t n) const {
        auto topPairs = getTopPairs(n);
        uint64_t topCount = 0;
        for (const auto& stats : topPairs) {
            topCount += stats.count;
        }
        return totalPairs > 0 ? (100.0 * topCount / totalPairs) : 0.0;
    }

    // Provide mnemonic hints for common opcodes
    std::string getMnemonicHint(uint8_t first, uint8_t second) const {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s → %s",
                 getOpcodeName(first).c_str(),
                 getOpcodeName(second).c_str());
        return std::string(buf);
    }

    std::string getPrefixMnemonicHint(uint8_t prefix, uint8_t opcode) const {
        const char* prefixName = "??";
        if (prefix == 0xDD) prefixName = "DD";
        else if (prefix == 0xED) prefixName = "ED";
        else if (prefix == 0xFD) prefixName = "FD";
        else if (prefix == 0xCB) prefixName = "CB";

        char buf[64];
        snprintf(buf, sizeof(buf), "%s %02X (%s %s)",
                 prefixName, opcode, prefixName, getOpcodeName(opcode).c_str());
        return std::string(buf);
    }

    // Basic opcode to mnemonic mapping (simplified)
    std::string getOpcodeName(uint8_t opcode) const {
        static const char* names[256] = {
            "NOP", "LD BC,nn", "LD (BC),A", "INC BC", "INC B", "DEC B", "LD B,n", "RLCA",
            "EX AF,AF'", "ADD HL,BC", "LD A,(BC)", "DEC BC", "INC C", "DEC C", "LD C,n", "RRCA",
            "DJNZ", "LD DE,nn", "LD (DE),A", "INC DE", "INC D", "DEC D", "LD D,n", "RLA",
            "JR", "ADD HL,DE", "LD A,(DE)", "DEC DE", "INC E", "DEC E", "LD E,n", "RRA",
            "JR NZ", "LD HL,nn", "LD (nn),HL", "INC HL", "INC H", "DEC H", "LD H,n", "DAA",
            "JR Z", "ADD HL,HL", "LD HL,(nn)", "DEC HL", "INC L", "DEC L", "LD L,n", "CPL",
            "JR NC", "LD SP,nn", "LD (nn),A", "INC SP", "INC (HL)", "DEC (HL)", "LD (HL),n", "SCF",
            "JR C", "ADD HL,SP", "LD A,(nn)", "DEC SP", "INC A", "DEC A", "LD A,n", "CCF"
        };

        if (opcode < 64) {
            return names[opcode];
        } else if (opcode >= 0x40 && opcode < 0x80) {
            return "LD r,r'";
        } else if (opcode >= 0x80 && opcode < 0xC0) {
            return "ALU";
        } else {
            return "misc";
        }
    }
};

} // namespace Z80Profiling

#endif // Z80_PROFILER_H

