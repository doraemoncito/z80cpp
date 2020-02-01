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

set( CMAKE_SYSTEM_NAME Generic )
set( CMAKE_SYSTEM_PROCESSOR arm )

set( toolchain_prefix aarch64-elf- )

set( CMAKE_C_COMPILER ${toolchain_prefix}gcc )
set( CMAKE_C_COMPILER_TARGET ${toolchain_prefix}gcc )
set( CMAKE_CXX_COMPILER ${toolchain_prefix}g++ )
set( CMAKE_CXX_COMPILER_TARGET ${toolchain_prefix}g++ )
set( CMAKE_LINKER ${toolchain_prefix}ld )

# SET( LINK_FLAGS "--map --ro-base=0x0 --rw-base=0x0008000 --first='boot.o(RESET)' --datacompressor=off")
# set( CMAKE_C_LINK_EXECUTABLE "<CMAKE_LINKER> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>" )
# set( CMAKE_C_LINK_EXECUTABLE "<CMAKE_LINKER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>" )
# set( CMAKE_C_LINK_EXECUTABLE ${toolchain_prefix}ld )
# set( CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_LINKER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>" )

# https://stackoverflow.com/questions/43781207/how-to-cross-compile-with-cmake-arm-none-eabi-on-windows)
set( CMAKE_EXE_LINKER_FLAGS "--specs=nosys.specs" CACHE INTERNAL "" )

# set( CMAKE_EXE_LINKER_FLAGS CACHE INTERNAL "" )
# unset( CMAKE_EXE_LINKER_FLAGS CACHE )
# set( CMAKE_EXE_LINKER_FLAGS "" CACHE STRING "" FORCE )

set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER )
set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY )
