cmake_minimum_required (VERSION 3.25.1)

# Simulator test
add_executable(z80_sim_test
    z80_sim_test.cpp
    z80_sim_test.h
)

target_compile_features(z80_sim_test PRIVATE cxx_std_17)

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
    COMMAND ${CMAKE_COMMAND} -E env CTEST_INTERACTIVE_DEBUG_MODE=1 $<TARGET_FILE:z80_sim_test>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
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

# Game Benchmark Tests (only if .tap files exist)
file(GLOB TAP_FILES "${CMAKE_CURRENT_SOURCE_DIR}/roms/*.tap")
list(LENGTH TAP_FILES TAP_FILES_COUNT)

if(TAP_FILES_COUNT GREATER 0)
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

    # Copy roms directory to build directory
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/roms DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
else()
    message(STATUS "No .tap files found in roms directory - skipping z80_game_test")
endif()
