# Toolchain for Raspberry Pi 4B
#
# Invoke CMake with the arguments bellow to build the project for Rasperry PI:                                                              
#                                                                                                                                           
#   cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-arm-aarch64-elf.cmake ..
#                                                                                                                                           
# the machine architecture of the compiler can be found by running:                                                                         
#                                                                                                                                           
#   aarch64-elf-g++ -dumpmachine
#
# which should return "aarch64-elf".
#

include(CMakeForceCompiler)

set( CMAKE_SYSTEM_NAME Generic )
set( CMAKE_SYSTEM_PROCESSOR arm )

# set the toolchain prefix for cross compilation
set( CROSS_COMPILE aarch64-elf- )

set( CMAKE_C_COMPILER ${CROSS_COMPILE}gcc )
set( CMAKE_C_COMPILER_TARGET ${CROSS_COMPILE}gcc )
set( CMAKE_CXX_COMPILER ${CROSS_COMPILE}g++ )
set( CMAKE_CXX_COMPILER_TARGET ${CROSS_COMPILE}g++ )
set( CMAKE_LINKER ${CROSS_COMPILE}ld )

# https://stackoverflow.com/questions/43781207/how-to-cross-compile-with-cmake-arm-none-eabi-on-windows)
set( CMAKE_EXE_LINKER_FLAGS "--specs=nosys.specs" CACHE INTERNAL "" )

set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER )
set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY )
