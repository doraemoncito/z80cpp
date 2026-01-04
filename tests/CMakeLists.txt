cmake_minimum_required (VERSION 3.25.1)

add_executable(z80_sim_test
    z80_sim_test.cpp
    z80_sim_test.hpp
)

target_compile_features(z80_sim_test PRIVATE cxx_std_14)

if(TARGET z80cpp-static)
    target_link_libraries(z80_sim_test PRIVATE z80cpp::z80cpp-static)
elseif(TARGET z80cpp)
    target_link_libraries(z80_sim_test PRIVATE z80cpp::z80cpp)
else()
    message(FATAL_ERROR "No z80cpp library target available for z80_sim_test")
endif()

# Copy test ROM if it exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/zexall.bin)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/zexall.bin
        ${CMAKE_CURRENT_BINARY_DIR}/zexall.bin
        COPYONLY
    )
else()
    message(WARNING "zexall.bin not found - z80_sim_test will fail without it")
endif()

add_test(
    NAME z80_sim_test
    COMMAND $<TARGET_FILE:z80_sim_test>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
# Configure to always show output
set_tests_properties(z80_sim_test PROPERTIES
    VERBOSE_FAIL ON
    VERBOSE_PASS ON
)

# Benchmark Tests
add_executable(z80_benchmark_test
    z80_benchmark_test.cpp
)

target_compile_features(z80_benchmark_test PRIVATE cxx_std_17)

if(TARGET z80cpp-static)
    target_link_libraries(z80_benchmark_test PRIVATE z80cpp::z80cpp-static)
elseif(TARGET z80cpp)
    target_link_libraries(z80_benchmark_test PRIVATE z80cpp::z80cpp)
endif()

add_test(
    NAME z80_benchmark_test
    COMMAND $<TARGET_FILE:z80_benchmark_test>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
# Configure to always show output
set_tests_properties(z80_benchmark_test PROPERTIES
    VERBOSE_FAIL ON
    VERBOSE_PASS ON
)

# Game Benchmark Tests
add_executable(z80_game_test
    z80_game_test.cpp
)

target_compile_features(z80_game_test PRIVATE cxx_std_17)

if(TARGET z80cpp-static)
    target_link_libraries(z80_game_test PRIVATE z80cpp::z80cpp-static)
elseif(TARGET z80cpp)
    target_link_libraries(z80_game_test PRIVATE z80cpp::z80cpp)
endif()

add_test(
    NAME z80_game_test
    COMMAND $<TARGET_FILE:z80_game_test>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
# Configure to always show output
set_tests_properties(z80_game_test PROPERTIES
    VERBOSE_FAIL ON
    VERBOSE_PASS ON
)

# Copy roms directory to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/roms DESTINATION ${CMAKE_CURRENT_BINARY_DIR})


