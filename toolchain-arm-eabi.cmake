# Invoke CMake with the arguments bellow to build the project for Rasperry PI:                                                              
#                                                                                                                                           
#   cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-arm-none-eabi.cmake ..
#                                                                                                                                           
# the machine architecture of the compiler can be found by running:                                                                         
#                                                                                                                                           
#   arm-eabi-g++ -dumpmachine
#
# which should return "arm-eabi".
#

include(CMakeForceCompiler)

set( CMAKE_SYSTEM_NAME Generic )
set( CMAKE_SYSTEM_PROCESSOR arm )

set( toolchain_prefix arm-eabi )

set( CMAKE_C_COMPILER ${toolchain_prefix}-gcc )
set( CMAKE_C_COMPILER_TARGET ${toolchain_prefix}-gcc )
set( CMAKE_CXX_COMPILER ${toolchain_prefix}-g++ )
set( CMAKE_CXX_COMPILER_TARGET ${toolchain_prefix}-g++ )
set( CMAKE_LINKER ${toolchain_prefix}-ld )

# https://stackoverflow.com/questions/43781207/how-to-cross-compile-with-cmake-arm-none-eabi-on-windows)
set( CMAKE_EXE_LINKER_FLAGS "--specs=nosys.specs" CACHE INTERNAL "" )

set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER )
set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY )