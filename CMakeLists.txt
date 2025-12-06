# By default, CMake generates standard UNIX makefiles.  To generate an XCode
# project on MacOS, invoke CMake from the build directory as shown below:
#
#   cmake -G Xcode ..
#

cmake_minimum_required (VERSION 3.25.1)
project (z80cpp)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build (Debug Release RelWithDebInfo MinSizeRel)" FORCE)
    message(STATUS "Build type not specified, defaulting to Release")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")

# Enable Link-Time Optimization (LTO) for Release builds
include(CheckIPOSupported)
check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_ERROR)
if(LTO_SUPPORTED)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "LTO enabled for optimized builds")
    endif()
else()
    message(STATUS "LTO not supported: ${LTO_ERROR}")
endif()

# Set the rpath policy and C++ version for MacOS builds
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # set the RPATH policy
    set(CMAKE_MACOSX_RPATH 1)

    # specify the C++ standard
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED True)
endif()

set (API_REVISION 1)
set (VERSION_MAJOR 0)
set (VERSION_MINOR 0)
set (RELEASE_TYPE "")
set (VERSION_STR "${API_REVISION}.${VERSION_MAJOR}.${VERSION_MINOR}${RELEASE_TYPE}")

# Set optimization flags for all compilers
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # GCC
    message(STATUS "Configuring for GCC")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++14")
    if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -mtune=native -fomit-frame-pointer -funroll-loops")
        message(STATUS "GCC Optimization flags: -O3 -march=native -mtune=native -fomit-frame-pointer -funroll-loops")
    endif()
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
    # Clang and AppleClang
    message(STATUS "Configuring for Clang/AppleClang")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++14")
    if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -mtune=native -fomit-frame-pointer -funroll-loops")
        message(STATUS "Clang Optimization flags: -O3 -march=native -mtune=native -fomit-frame-pointer -funroll-loops")
    endif()
elseif (MSVC)
    # MSVC
    message(STATUS "Configuring for MSVC")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
    if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /Ob2 /Oi /Ot /GL")
        message(STATUS "MSVC Optimization flags: /O2 /Ob2 /Oi /Ot /GL")
    endif()
endif ()

# Display dispatch optimization status
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
    message(STATUS "Z80 Dispatch: Computed goto (high-performance indirect dispatch)")
else()
    message(STATUS "Z80 Dispatch: Standard switch statement")
endif()

message(STATUS "Final CXX flags: ${CMAKE_CXX_FLAGS}")

include_directories(BEFORE . include)

# Z80 is now a header-only template library (CRTP)
# Create an INTERFACE library for proper CMake integration
add_library(z80cpp-static INTERFACE)
target_include_directories(z80cpp-static INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Also provide z80cpp as alias for compatibility
if (NOT DEFINED Z80CPP_STATIC_ONLY)
    add_library(z80cpp INTERFACE)
    target_include_directories(z80cpp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif ()

if ("${CMAKE_CPP_IMPLICIT_LINK_DIRECTORIES}" MATCHES "lib64")
    set (LIB_DIR "lib64")
else ()
    set (LIB_DIR "lib")
endif ()
if (NOT DEFINED Z80CPP_STATIC_ONLY)
    install (TARGETS z80cpp LIBRARY DESTINATION ${LIB_DIR} ARCHIVE DESTINATION ${LIB_DIR})
endif ()

# Enable running the test simulator as part the `make test` target
set( TEST_SOURCES example/z80sim.cpp example/z80sim.h )
add_executable( z80sim ${TEST_SOURCES} )
target_link_libraries( z80sim z80cpp-static )
configure_file( example/zexall.bin zexall.bin COPYONLY )

# ============================================================================
# Performance Benchmark Tests (Integrated with CTest)
# ============================================================================

# Benchmark test suite (synthetic tests)
add_executable( benchmark_tests tests/benchmark_tests.cpp example/z80sim.cpp )
target_include_directories( benchmark_tests PRIVATE include example )
target_compile_definitions( benchmark_tests PRIVATE Z80SIM_NO_MAIN )
target_link_libraries( benchmark_tests z80cpp-static )

# Individual game benchmark test
add_executable( game_benchmark_test tests/game_benchmark_test.cpp example/z80sim.cpp )
target_include_directories( game_benchmark_test PRIVATE include example )
target_compile_definitions( game_benchmark_test PRIVATE Z80SIM_NO_MAIN )
target_link_libraries( game_benchmark_test z80cpp-static )

# Z80 Benchmark tool (simple version)
add_executable( z80_benchmark tests/z80_benchmark_simple.cpp example/z80sim.cpp )
target_include_directories( z80_benchmark PRIVATE include example )
target_compile_definitions( z80_benchmark PRIVATE Z80SIM_NO_MAIN )
target_link_libraries( z80_benchmark z80cpp-static )


# ============================================================================
# Generate Synthetic Tests
# ============================================================================

# Tool to generate synthetic tests
add_executable(generate_test_programs tests/generate_test_programs.cpp)

# Custom target to generate synthetic test programs
add_custom_target( generate_synthetic_tests
    DEPENDS generate_test_programs
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/tests/spectrum-roms/synthetic
    COMMAND $<TARGET_FILE:generate_test_programs> ${CMAKE_SOURCE_DIR}/tests/spectrum-roms/synthetic
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating synthetic Z80 test programs"
)

# ============================================================================
# Copy Test Files to Build Directory
# ============================================================================

# Copy ZEXALL
configure_file( ${CMAKE_SOURCE_DIR}/example/zexall.bin ${CMAKE_BINARY_DIR}/zexall.bin COPYONLY )

# Custom command to copy synthetic tests (executed at build time)
add_custom_command(
    TARGET benchmark_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/tests/spectrum-roms/synthetic
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/tests/spectrum-roms/synthetic
            ${CMAKE_BINARY_DIR}/tests/spectrum-roms/synthetic
    COMMENT "Copying synthetic test files to build directory"
)

# Custom command to copy game files (executed at build time)
add_custom_command(
    TARGET game_benchmark_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/tests/spectrum-roms/games
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/tests/spectrum-roms/games
            ${CMAKE_BINARY_DIR}/tests/spectrum-roms/games
    COMMENT "Copying game files to build directory"
)

# Make benchmark_tests depend on synthetic test generation
add_dependencies( benchmark_tests generate_synthetic_tests )

# ============================================================================
# CTest Integration
# ============================================================================

enable_testing()

# Test 1: ZEXALL correctness test (must always pass)
add_test( NAME correctness_zexall COMMAND z80sim )
set_tests_properties( correctness_zexall PROPERTIES TIMEOUT 60 )

# Test 2: Benchmark test suite (synthetic tests)
add_test( NAME performance_benchmark_suite COMMAND benchmark_tests )
set_tests_properties( performance_benchmark_suite PROPERTIES
    TIMEOUT 120
    LABELS "performance;benchmark"
)

# Test 3: Individual game performance tests
# Note: Min MIPS thresholds are conservative to avoid intermittent failures due to system load
if( EXISTS "${CMAKE_SOURCE_DIR}/tests/spectrum-roms/games/ant_attack.bin" )
    add_test( NAME performance_ant_attack
              COMMAND game_benchmark_test
                      tests/spectrum-roms/games/ant_attack.bin
                      10000000
                      150.0 )
    set_tests_properties( performance_ant_attack PROPERTIES
        TIMEOUT 30
        LABELS "performance;game"
    )
endif()

if( EXISTS "${CMAKE_SOURCE_DIR}/tests/spectrum-roms/games/manic_miner_code2.bin" )
    add_test( NAME performance_manic_miner
              COMMAND game_benchmark_test
                      tests/spectrum-roms/games/manic_miner_code2.bin
                      10000000
                      100.0 )
    set_tests_properties( performance_manic_miner PROPERTIES
        TIMEOUT 30
        LABELS "performance;game"
    )
endif()

if( EXISTS "${CMAKE_SOURCE_DIR}/tests/spectrum-roms/games/jet_set_willy_code2.bin" )
    add_test( NAME performance_jet_set_willy
              COMMAND game_benchmark_test
                      tests/spectrum-roms/games/jet_set_willy_code2.bin
                      10000000
                      150.0 )
    set_tests_properties( performance_jet_set_willy PROPERTIES
        TIMEOUT 30
        LABELS "performance;game"
    )
endif()

if( EXISTS "${CMAKE_SOURCE_DIR}/tests/spectrum-roms/games/pyjamarama_code2.bin" )
    add_test( NAME performance_pyjamarama
              COMMAND game_benchmark_test
                      tests/spectrum-roms/games/pyjamarama_code2.bin
                      10000000
                      150.0 )
    set_tests_properties( performance_pyjamarama PROPERTIES
        TIMEOUT 30
        LABELS "performance;game"
    )
endif()

if( EXISTS "${CMAKE_SOURCE_DIR}/tests/spectrum-roms/games/arkanoid.bin" )
    add_test( NAME performance_arkanoid
              COMMAND game_benchmark_test
                      tests/spectrum-roms/games/arkanoid.bin
                      5000000
                      120.0 )
    set_tests_properties( performance_arkanoid PROPERTIES
        TIMEOUT 20
        LABELS "performance;game"
    )
endif()

if( EXISTS "${CMAKE_SOURCE_DIR}/tests/spectrum-roms/games/horace_skiing_code2.bin" )
    add_test( NAME performance_horace_skiing
              COMMAND game_benchmark_test
                      tests/spectrum-roms/games/horace_skiing_code2.bin
                      5000000
                      100.0 )
    set_tests_properties( performance_horace_skiing PROPERTIES
        TIMEOUT 20
        LABELS "performance;game"
    )
endif()

# ============================================================================
# Individual Synthetic Benchmark Tests (using z80_benchmark)
# ============================================================================

# Test: Instruction Mix
add_test( NAME benchmark_instruction_mix
          COMMAND z80_benchmark
                  tests/spectrum-roms/synthetic/instruction_mix.bin
                  -i 5000000
                  -s )
set_tests_properties( benchmark_instruction_mix PROPERTIES
    TIMEOUT 15
    LABELS "benchmark;synthetic"
    PASS_REGULAR_EXPRESSION "MIPS"
)

# Test: Memory Intensive
add_test( NAME benchmark_memory_intensive
          COMMAND z80_benchmark
                  tests/spectrum-roms/synthetic/memory_intensive.bin
                  -i 5000000
                  -s )
set_tests_properties( benchmark_memory_intensive PROPERTIES
    TIMEOUT 15
    LABELS "benchmark;synthetic"
    PASS_REGULAR_EXPRESSION "MIPS"
)

# Test: Arithmetic Heavy
add_test( NAME benchmark_arithmetic_heavy
          COMMAND z80_benchmark
                  tests/spectrum-roms/synthetic/arithmetic_heavy.bin
                  -i 5000000
                  -s )
set_tests_properties( benchmark_arithmetic_heavy PROPERTIES
    TIMEOUT 15
    LABELS "benchmark;synthetic"
    PASS_REGULAR_EXPRESSION "MIPS"
)

# Test: Branch Heavy
add_test( NAME benchmark_branch_heavy
          COMMAND z80_benchmark
                  tests/spectrum-roms/synthetic/branch_heavy.bin
                  -i 5000000
                  -s )
set_tests_properties( benchmark_branch_heavy PROPERTIES
    TIMEOUT 15
    LABELS "benchmark;synthetic"
    PASS_REGULAR_EXPRESSION "MIPS"
)

# ============================================================================
# Test Labels and Organization
# ============================================================================

# Quick tests (fast benchmarks only)
set_tests_properties(
    benchmark_instruction_mix
    benchmark_memory_intensive
    benchmark_arithmetic_heavy
    benchmark_branch_heavy
    PROPERTIES LABELS "quick"
)

# All performance tests
set_tests_properties(
    performance_benchmark_suite
    benchmark_instruction_mix
    benchmark_memory_intensive
    benchmark_arithmetic_heavy
    benchmark_branch_heavy
    PROPERTIES LABELS "performance"
)

install( TARGETS z80cpp-static LIBRARY DESTINATION ${LIB_DIR} ARCHIVE DESTINATION ${LIB_DIR} RUNTIME DESTINATION bin )
install( DIRECTORY include/ DESTINATION include/z80cpp PATTERN "*.h" )
