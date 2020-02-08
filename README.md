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
machine where a suitable ARM cross compiler has already been installed, you can
use one of the supplied cmake toolchain configuration files as shown in the
example below:

```sh
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-arm-eabi.cmake ..
make
```

When compiling inside a virtual machine you may occasionally warning messages warning you clock skew has been detected.

```text
make[2]: warning:  Clock skew detected.  Your build may be incomplete.
```

This is due to a clock drifting between the VM and the host machine which may occur when a VM is suspended.
  
The issue can be easily resolved using the `ntpupdate` command as shown here:

```sh
sudo apt-get install ntpdate
sudo ntpdate time.nist.gov
```

e.g.

```sh
$ sudo ntpdate time.nist.gov
8 Feb 13:43:28 ntpdate[7223]: step time server 132.163.96.2 offset 0.649422 sec
```

The core have the same features of [Z80Core](https://github.com/jsanchezv/Z80Core):

* Complete instruction set emulation
* Emulates the undocumented bits 3 & 5 from flags register
* Emulates the MEMPTR register (known as WZ in official Zilog documentation)
* Strict execution order for every instruction
* Precise timing for all instructions, totally decoupled from the core

*jspeccy at gmail dot com*
