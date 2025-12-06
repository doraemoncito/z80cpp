# ZX Spectrum Games - Ready to Test

6 real ZX Spectrum games with extracted machine code ready for benchmarking.

## Games Collection

| Game | Type | Size | Performance |
|------|------|------|-------------|
| Ant Attack | Isometric 3D | 32.7 KB | 310.33 MIPS |
| Jet Set Willy | Platform | 32 KB | 305.29 MIPS |
| Pyjamarama | Platform | 39 KB | 303.33 MIPS |
| Arkanoid | Arcade | 1.7 KB | 282.45 MIPS |
| Horace Skiing | Arcade | 7.9 KB | 251.76 MIPS |
| Manic Miner | Platform | 32 KB | 224.90 MIPS |

## Files

Each game has:
- `.tap` - Original ZX Spectrum TAP file
- `.bin` or `_code2.bin` - Extracted machine code ready for testing

## Testing

```bash
cd build
./z80_benchmark ../tests/spectrum-roms/games/ant_attack.bin -i 10000000
```

## Complete Documentation

See [main README.md](../../../README.md) for:
- Full benchmark results
- Testing procedures
- How to add more games

