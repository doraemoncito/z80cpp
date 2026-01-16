cmake_minimum_required (VERSION 3.22.1)

project(
    z80cpp
    VERSION 1.0.0
    DESCRIPTION "Z80 emulator core"
    LANGUAGES CXX
)

# Prevent in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed. Please run cmake from a separate build directory.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Standard output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)
include(CheckIPOSupported)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# Set the rpath policy for MacOS builds
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_MACOSX_RPATH 1)
endif()

# Options
option(Z80CPP_NATIVE_ARCH "Use -march=native for local optimization" ON)
option(Z80CPP_ENABLE_LTO "Enable Link-Time Optimization" ON)
option(Z80CPP_ENABLE_CCACHE "Enable ccache for faster rebuilds" ON)
option(Z80CPP_PARALLEL_BUILD "Enable parallel builds using all CPU cores" ON)
option(Z80CPP_ENABLE_TESTING "Build test simulator target" ${BUILD_TESTING})

# LTO
if(Z80CPP_ENABLE_LTO)
    check_ipo_supported(RESULT Z80CPP_LTO_SUPPORTED OUTPUT Z80CPP_LTO_OUTPUT)
    if(Z80CPP_LTO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "Link-Time Optimization (LTO): Enabled")
    else()
        message(WARNING "LTO requested but not supported by compiler: ${Z80CPP_LTO_OUTPUT}")
    endif()
else()
    message(STATUS "Link-Time Optimization (LTO): Disabled")
endif()

# ccache
if(Z80CPP_ENABLE_CCACHE)
    find_program(CCACHE_EXECUTABLE ccache)
    if(CCACHE_EXECUTABLE)
        message(STATUS "ccache found: ${CCACHE_EXECUTABLE}")
        set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_EXECUTABLE})
        set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_EXECUTABLE})
        message(STATUS "ccache: Enabled for faster rebuilds")
    else()
        message(STATUS "ccache: Disabled")
    endif()
endif()

# Parallel builds hint
if(Z80CPP_PARALLEL_BUILD)
    include(ProcessorCount)
    ProcessorCount(N)
    if(NOT N EQUAL 0)
        set(CMAKE_BUILD_PARALLEL_LEVEL ${N})
        message(STATUS "Parallel Build: Enabled (${N} CPU cores)")
    else()
        message(STATUS "Parallel Build: Could not detect CPU cores, using system default")
    endif()
endif()

# Set optimization flags per compiler
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    message(STATUS "Configuring for GCC")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fomit-frame-pointer -funroll-loops")
        if(Z80CPP_NATIVE_ARCH)
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mtune=native")
        endif()
    endif()
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
    message(STATUS "Configuring for Clang/AppleClang")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fomit-frame-pointer -funroll-loops")
        if(Z80CPP_NATIVE_ARCH)
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mtune=native")
        endif()
    endif()
elseif (MSVC)
    message(STATUS "Configuring for MSVC")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
    if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /Ob2 /Oi /Ot /GL")
    endif()
endif ()

message(STATUS "Final CXX flags: ${CMAKE_CXX_FLAGS}")

# Verbose CTest output when tests are enabled
if(Z80CPP_ENABLE_TESTING)
    set(CMAKE_CTEST_ARGUMENTS --verbose --output-on-failure)
endif()

# Public headers
set(z80cpp_headers
    include/z80.h
    include/z80_bus_interface.h
    include/z80_types.h
)

# Configure INTERFACE target (header-only)
function(z80cpp_configure_target target_name)
    target_compile_features(${target_name} INTERFACE cxx_std_17)
    target_include_directories(${target_name}
        INTERFACE
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/z80cpp>
    )
endfunction()

# Create header-only libraries
add_library(z80cpp INTERFACE)
add_library(z80cpp::z80cpp ALIAS z80cpp)
z80cpp_configure_target(z80cpp)

add_library(z80cpp-static INTERFACE)
add_library(z80cpp::z80cpp-static ALIAS z80cpp-static)
z80cpp_configure_target(z80cpp-static)

# Install targets
install(
    TARGETS z80cpp z80cpp-static
    EXPORT z80cppTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/z80cpp
)

# Tests
if(Z80CPP_ENABLE_TESTING AND BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install headers
install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/z80cpp
    FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.tpp"
)

# Package configuration
install(
    EXPORT z80cppTargets
    NAMESPACE z80cpp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/z80cpp
)

configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/z80cppConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/z80cppConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/z80cpp
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/z80cppConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/z80cppConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/z80cppConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/z80cpp
)

# clang-format support
find_program(CLANG_FORMAT_EXECUTABLE clang-format)
if(CLANG_FORMAT_EXECUTABLE)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.tpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h
    )

    add_custom_target(
        format
        COMMENT "Running clang-format to format source files..."
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i ${ALL_SOURCE_FILES}
    )

    add_custom_target(
        format-check
        COMMENT "Checking code formatting with clang-format..."
        COMMAND ${CLANG_FORMAT_EXECUTABLE} --dry-run -Werror ${ALL_SOURCE_FILES}
    )
endif()

# clang-tidy support
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy clang-tidy-17 clang-tidy-16)
if(CLANG_TIDY_EXECUTABLE)
    file(GLOB_RECURSE ALL_SOURCE_FILES_CPP
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    )

    add_custom_target(lint
        COMMENT "Running clang-tidy..."
    )

    foreach(_file IN LISTS ALL_SOURCE_FILES_CPP)
        add_custom_command(TARGET lint POST_BUILD
            COMMAND ${CLANG_TIDY_EXECUTABLE} -quiet "${_file}" -- -std=c++17
            COMMENT "Linting ${_file}"
        )
    endforeach()
endif()
