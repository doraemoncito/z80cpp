# Z80 Example Simulator

Example usage of the z80cpp library running ZEXALL test suite.

## Quick Start

```bash
cd build
./z80sim
```

This runs the ZEXALL comprehensive Z80 instruction test (~30 seconds).

## Building

The simulator is built automatically with the project:

```bash
cd build
cmake .. && make
```

Or manually:

```bash
g++ -Wall -O3 -std=c++14 -I../include z80sim.cpp -o z80sim -L../build -lz80cpp -Wl,-rpath=../build/
```

## Files

- `z80sim.cpp` / `z80sim.h` - Example simulator implementation
- `zexall.bin` - ZEXALL test suite binary

## See Also

For complete documentation, benchmarking, and performance testing, see the main [README.md](../README.md).

