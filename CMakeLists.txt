# By default, CMake generates standard UNIX makefiles.  To generate an XCode
# project on MacOS, invoke CMake from the build directory as shown below:
#
#   cmake -G Xcode ..
#

cmake_minimum_required (VERSION 3.25.1)
project (z80cpp)

# Set the rpath policy and C++ version
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # set the RPATH policy
    set(CMAKE_MACOSX_RPATH 1)
endif()

# Specify the C++ standard for all platforms
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set (API_REVISION 1)
set (VERSION_MAJOR 0)
set (VERSION_MINOR 0)
set (RELEASE_TYPE "")
set (VERSION_STR "${API_REVISION}.${VERSION_MAJOR}.${VERSION_MINOR}${RELEASE_TYPE}")

if (CMAKE_COMPILER_IS_GNUCXX)
    set (CMAKE_CXX_FLAGS "-Wall -O3")
endif ()

include_directories(BEFORE . include)

set (z80cpp_sources src/z80.cpp include/z80.h include/z80operations.h )
add_library (z80cpp-static STATIC ${z80cpp_sources})
set_target_properties (z80cpp-static PROPERTIES OUTPUT_NAME z80cpp)

# Create alias for the target
add_library(z80cpp::z80cpp-static ALIAS z80cpp-static)

if (NOT DEFINED Z80CPP_STATIC_ONLY)
    add_library (z80cpp SHARED ${z80cpp_sources})
# Affects Win32 only: avoid dynamic/static *.lib files naming conflict 
    set_target_properties (z80cpp-static PROPERTIES PREFIX "lib")
endif ()

if (NOT DEFINED Z80CPP_STATIC_ONLY)
    set_target_properties(z80cpp
        PROPERTIES VERSION ${VERSION_STR} SOVERSION ${API_REVISION}
    )
endif ()
set_target_properties(z80cpp-static
    PROPERTIES VERSION ${VERSION_STR} SOVERSION ${API_REVISION}
)

if ("${CMAKE_CPP_IMPLICIT_LINK_DIRECTORIES}" MATCHES "lib64")
    set (LIB_DIR "lib64")
else ()
    set (LIB_DIR "lib")
endif ()
if (NOT DEFINED Z80CPP_STATIC_ONLY)
    install (TARGETS z80cpp LIBRARY DESTINATION ${LIB_DIR} ARCHIVE DESTINATION ${LIB_DIR})
endif ()

configure_file( tests/zexall.bin zexall.bin COPYONLY )

enable_testing( )

# Configure CTest to always show output for all tests (pass or fail)
set(CTEST_OUTPUT_ON_FAILURE ON)

# Set verbose output for all CTest invocations
list(APPEND CMAKE_CTEST_ARGUMENTS "--verbose" "--output-on-failure")

# Add tests subdirectory
add_subdirectory(tests)

# Add a custom test target that runs with verbose output
add_custom_target(test-verbose
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS z80_sim_test z80_benchmark_test z80_game_test
    COMMENT "Running all tests with verbose output"
)

install( TARGETS z80cpp-static LIBRARY DESTINATION ${LIB_DIR} ARCHIVE DESTINATION ${LIB_DIR} )
install( DIRECTORY include/ DESTINATION include/z80cpp PATTERN "*.h" )
