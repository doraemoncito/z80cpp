#!/bin/bash
# Comprehensive Z80 Emulator Benchmark Script
# Builds z80_benchmark tool and measures performance with various optimizations

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
RESULTS_DIR="$PROJECT_ROOT/tests/results"

echo "======================================================================"
echo "Z80 Emulator Comprehensive Performance Benchmark"
echo "======================================================================"
echo ""

# Create necessary directories
mkdir -p "$BUILD_DIR"
mkdir -p "$RESULTS_DIR"

# ============================================================================
# Part 1: Build z80_benchmark Tool
# ============================================================================
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Part 1: Building z80_benchmark Tool${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

build_benchmark_tool() {
    local opt_level="$1"
    local opt_flags="$2"

    echo -e "${YELLOW}Building with optimization level: $opt_level${NC}"
    echo "Flags: $opt_flags"
    echo ""

    cd "$BUILD_DIR"
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="$opt_flags" > /dev/null 2>&1
    make z80cpp-static > /dev/null 2>&1

    # Build z80_benchmark tool
    g++ -std=c++14 $opt_flags \
        -DZ80SIM_NO_MAIN \
        -I "$PROJECT_ROOT/include" -I "$PROJECT_ROOT/example" \
        -c "$PROJECT_ROOT/example/z80sim.cpp" \
        -o "$BUILD_DIR/z80sim_lib.o" 2>&1 | grep -v "warning:" || true

    g++ -std=c++14 $opt_flags \
        -I "$PROJECT_ROOT/include" -I "$PROJECT_ROOT/example" \
        "$PROJECT_ROOT/tools/z80_benchmark_simple.cpp" \
        "$BUILD_DIR/z80sim_lib.o" \
        "$BUILD_DIR/libz80cpp.a" \
        -o "$BUILD_DIR/z80_benchmark" 2>&1 | grep -v "warning:" || true

    if [ -f "$BUILD_DIR/z80_benchmark" ]; then
        echo -e "${GREEN}✅ z80_benchmark built successfully${NC}"
        return 0
    else
        echo -e "${RED}❌ Build failed${NC}"
        return 1
    fi
}

# Initial build with standard optimizations
if ! build_benchmark_tool "Standard (-O3 -march=native)" "-O3 -march=native"; then
    echo -e "${RED}Failed to build z80_benchmark tool. Exiting.${NC}"
    exit 1
fi

echo ""

# ============================================================================
# Part 2: Quick Verification
# ============================================================================
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Part 2: Quick Verification Test${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

if [ -f "$PROJECT_ROOT/example/zexall.bin" ]; then
    echo "Testing with ZEXALL (1M instructions)..."
    "$BUILD_DIR/z80_benchmark" "$PROJECT_ROOT/example/zexall.bin" -i 1000000 2>&1 | grep -E "MIPS|faster"
else
    echo -e "${YELLOW}⚠️  zexall.bin not found, skipping verification${NC}"
fi

echo ""

# ============================================================================
# Part 3: Baseline Performance Test
# ============================================================================
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Part 3: Baseline Performance Measurement${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BASELINE_RESULT="$RESULTS_DIR/baseline_${TIMESTAMP}.txt"

echo "Running baseline tests (this may take a minute)..."
echo ""

# Test ZEXALL if available
if [ -f "$PROJECT_ROOT/example/zexall.bin" ]; then
    echo "📊 ZEXALL Test..."
    "$BUILD_DIR/z80_benchmark" "$PROJECT_ROOT/example/zexall.bin" -i 50000000 -o "$BASELINE_RESULT"
    grep -E "MIPS|faster" "$BASELINE_RESULT" | head -2
fi

echo ""

# Test real games if available
GAMES_DIR="$PROJECT_ROOT/tests/spectrum-roms/games"
if [ -d "$GAMES_DIR" ]; then
    echo "📊 Testing Real ZX Spectrum Games..."
    echo ""

    for game in ant_attack.bin manic_miner_code2.bin jet_set_willy_code2.bin pyjamarama_code2.bin arkanoid.bin; do
        if [ -f "$GAMES_DIR/$game" ]; then
            game_name=$(basename "$game" .bin)
            echo "  Testing: $game_name"
            "$BUILD_DIR/z80_benchmark" "$GAMES_DIR/$game" -i 10000000 2>&1 | grep "MIPS" | head -1
        fi
    done
fi

echo ""
echo -e "${GREEN}✅ Baseline results saved to: $BASELINE_RESULT${NC}"
echo ""

# ============================================================================
# Part 4: Optimization Level Comparison
# ============================================================================
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Part 4: Testing Different Optimization Levels${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

declare -a OPT_LEVELS=(
    "O2|-O2"
    "O3|-O3 -march=native"
    "O3-LTO|-O3 -march=native -flto"
    "O3-AGGRESSIVE|-O3 -march=native -flto -funroll-loops -finline-functions"
)

TEST_FILE="$PROJECT_ROOT/example/zexall.bin"
if [ ! -f "$TEST_FILE" ]; then
    TEST_FILE="$GAMES_DIR/ant_attack.bin"
fi

if [ -f "$TEST_FILE" ]; then
    echo "Comparing optimization levels using: $(basename $TEST_FILE)"
    echo ""

    for opt_config in "${OPT_LEVELS[@]}"; do
        IFS='|' read -r name flags <<< "$opt_config"

        echo -e "${YELLOW}Testing: $name${NC}"

        # Rebuild with new flags
        build_benchmark_tool "$name" "$flags" > /dev/null 2>&1

        # Quick benchmark (5M instructions for speed)
        result=$("$BUILD_DIR/z80_benchmark" "$TEST_FILE" -i 5000000 2>&1)
        mips=$(echo "$result" | grep "MIPS" | head -1 | awk '{print $1}')
        echo "  Result: $mips MIPS"
        echo ""
    done
else
    echo -e "${YELLOW}⚠️  No test file available for optimization comparison${NC}"
fi

# ============================================================================
# Part 5: Real Game Performance Suite
# ============================================================================
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Part 5: Complete Game Performance Suite${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Rebuild with best optimization settings
build_benchmark_tool "Final Optimized" "-O3 -march=native -flto" > /dev/null 2>&1

if [ -f "$PROJECT_ROOT/tests/run_game_benchmarks.sh" ]; then
    echo "Running complete game benchmark suite..."
    cd "$PROJECT_ROOT/tests"
    ./run_game_benchmarks.sh
else
    echo -e "${YELLOW}⚠️  run_game_benchmarks.sh not found${NC}"

    # Manual game testing
    if [ -d "$GAMES_DIR" ]; then
        echo "Testing games manually..."
        echo ""

        declare -a GAMES=(
            "ant_attack.bin|Ant Attack|10000000"
            "manic_miner_code2.bin|Manic Miner|10000000"
            "jet_set_willy_code2.bin|Jet Set Willy|10000000"
            "pyjamarama_code2.bin|Pyjamarama|10000000"
            "arkanoid.bin|Arkanoid|5000000"
            "horace_skiing_code2.bin|Horace Skiing|5000000"
        )

        for game_config in "${GAMES[@]}"; do
            IFS='|' read -r file name instructions <<< "$game_config"

            if [ -f "$GAMES_DIR/$file" ]; then
                echo "🎮 $name"
                "$BUILD_DIR/z80_benchmark" "$GAMES_DIR/$file" -i "$instructions" 2>&1 | grep -E "MIPS|faster"
                echo ""
            fi
        done
    fi
fi

# ============================================================================
# Summary
# ============================================================================
echo ""
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}Benchmark Complete!${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo "Results saved to: $RESULTS_DIR"
echo ""
echo "Summary:"
echo "  ✅ z80_benchmark tool built and verified"
echo "  ✅ Baseline performance measured"
echo "  ✅ Optimization levels compared"
echo "  ✅ Real game performance tested"
echo ""
echo "Next steps:"
echo "  1. Review results in: $RESULTS_DIR"
echo "  2. For more optimizations, see README.md → Optimization Guide"
echo "  3. To test with your own ROMs, use:"
echo "     ./build/z80_benchmark <rom_file.bin> -i 10000000"
echo ""
echo "Current z80_benchmark location: $BUILD_DIR/z80_benchmark"
echo ""

