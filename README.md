## z80cpp
#### Z80 core in C++

That's a port from Java to C++ of my [Z80Core](https://github.com/jsanchezv/Z80Core).

To build for the host platform:

```sh
mkdir build
cd build
cmake ..
make
```

Then, you have an use case at dir *example*.

Alternatively, if you whish to cross compile the code for Raspberry Pi on a
machine where a suitable ARM cross compiler has alredy been installed, you can
use one of the supplied cmake toolchain configuration files as shown in the
example below:

```sh
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-arm-none-eabi.cmake ..
make
```

The core have the same features of [Z80Core](https://github.com/jsanchezv/Z80Core):

* Complete instruction set emulation
* Emulates the undocumented bits 3 & 5 from flags register
* Emulates the MEMPTR register (known as WZ in official Zilog documentation)
* Strict execution order for every instruction
* Precise timing for all instructions, totally decoupled from the core

*jspeccy at gmail dot com*
