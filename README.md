# z80cpp

## A Z80 core in C++

This project is a port from Java to C++ of my [Zilog Z80 CPU emulator core](https://github.com/jsanchezv/Z80Core).

Run the following commands to build the project:

```
mkdir build
cd build
cmake ..
make
```

The build process generates an executable test file called z80sim that exercises the Z80 instruction set.

The core have the same features of [Z80Core](https://github.com/jsanchezv/Z80Core):

* Complete instruction set emulation
* Emulates the undocumented bits 3 & 5 from the flags register
* Emulates the MEMPTR register (known as WZ in the official Zilog documentation)
* Strict execution order for every instruction
* Precise timing for all instructions, totally decoupled from the core

## Cross-compiling for Raspberry Pi

Please note that these instructions assume you have already installed a suitable ARM cross-compiler on your machine.

### 32 bits (all RPi versions)

Run the commands shown below to build the 32 release version of the project for Raspberry Pi 1, 2, 3 and 4.

```
mkdir cmake-build-rpi-release
cd cmake-build-rpi-release
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm-none-eabi.cmake ..
make
```

Alternatively, run these commands to build the 32 bit debug version:

```
mkdir cmake-build-rpi-debug
cd cmake-build-rpi-debug
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm-none-eabi.cmake ..
make
```

email: *jspeccy at gmail dot com*

