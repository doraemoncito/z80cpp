#!/usr/bin/env python3
"""
Extract machine code from ZX Spectrum TAP files and save as .bin files
This allows direct testing with z80_benchmark
"""

import struct
import sys
from pathlib import Path

def read_tap_block(f):
    """Read one TAP block from file"""
    # Read block length (2 bytes, little-endian)
    length_bytes = f.read(2)
    if len(length_bytes) < 2:
        return None

    length = struct.unpack('<H', length_bytes)[0]

    # Read block data
    data = f.read(length)
    if len(data) < length:
        return None

    return data

def extract_machine_code_from_tap(tap_filename, output_dir=None):
    """
    Extract machine code from TAP file

    TAP file structure:
    - Header block: [Length(2)] [Flag(1)] [Type(1)] [Name(10)] [DataLen(2)] [Param1(2)] [Param2(2)] [Checksum(1)]
    - Data block: [Length(2)] [Flag(1)] [Data(N)] [Checksum(1)]

    Flag: 0x00 = Header, 0xFF = Data
    Type: 0x00 = BASIC, 0x03 = Code/Machine code
    """

    if output_dir is None:
        output_dir = Path(tap_filename).parent
    else:
        output_dir = Path(output_dir)

    output_dir.mkdir(parents=True, exist_ok=True)

    base_name = Path(tap_filename).stem

    print(f"\n{'='*70}")
    print(f"Processing: {tap_filename}")
    print(f"{'='*70}")

    extracted_files = []

    with open(tap_filename, 'rb') as f:
        block_num = 0
        code_block_num = 0

        while True:
            block = read_tap_block(f)
            if block is None:
                break

            block_num += 1

            if len(block) < 2:
                continue

            flag = block[0]

            # Header block
            if flag == 0x00 and len(block) >= 19:
                block_type = block[1]
                name = block[2:12].decode('ascii', errors='ignore').strip()
                data_length = struct.unpack('<H', block[12:14])[0]
                param1 = struct.unpack('<H', block[14:16])[0]
                param2 = struct.unpack('<H', block[16:18])[0]

                type_names = {0x00: 'BASIC', 0x01: 'Number array', 0x02: 'Char array', 0x03: 'Code'}
                type_name = type_names.get(block_type, f'Unknown({block_type})')

                print(f"\nBlock {block_num}: Header")
                print(f"  Type: {type_name}")
                print(f"  Name: '{name}'")
                print(f"  Data length: {data_length} bytes")

                if block_type == 0x03:  # Machine code
                    print(f"  Load address: 0x{param1:04X}")
                    print(f"  Param2: 0x{param2:04X}")

                    # Next block should be the data
                    data_block = read_tap_block(f)
                    block_num += 1

                    if data_block and data_block[0] == 0xFF:
                        # Remove flag and checksum
                        code_data = data_block[1:-1]

                        code_block_num += 1
                        if code_block_num == 1:
                            output_filename = f"{base_name}.bin"
                        else:
                            output_filename = f"{base_name}_code{code_block_num}.bin"

                        output_path = output_dir / output_filename

                        with open(output_path, 'wb') as out:
                            out.write(code_data)

                        print(f"\n  ✓ Extracted {len(code_data)} bytes to: {output_filename}")
                        print(f"    Load address: 0x{param1:04X}")
                        extracted_files.append(output_filename)

                elif block_type == 0x00:  # BASIC
                    print(f"  Auto-start line: {param1}")
                    print(f"  Variables offset: {param2}")

            # Data block (without preceding header)
            elif flag == 0xFF:
                print(f"\nBlock {block_num}: Data block ({len(block)-2} bytes)")

    print(f"\n{'='*70}")
    print(f"Summary for {base_name}:")
    print(f"  Total blocks processed: {block_num}")
    print(f"  Machine code files extracted: {len(extracted_files)}")

    if extracted_files:
        print(f"\n  Extracted files:")
        for filename in extracted_files:
            filepath = output_dir / filename
            size = filepath.stat().st_size
            print(f"    - {filename} ({size:,} bytes)")
    else:
        print(f"  ⚠️  No machine code blocks found")

    print(f"{'='*70}\n")

    return extracted_files

def main():
    if len(sys.argv) < 2:
        print("TAP to BIN Extractor")
        print("\nUsage: tap2bin.py <tap_file> [output_dir]")
        print("\nExample:")
        print("  tap2bin.py manic_miner.tap")
        print("  tap2bin.py manic_miner.tap ../extracted/")
        return 1

    tap_file = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else None

    if not Path(tap_file).exists():
        print(f"Error: File not found: {tap_file}")
        return 1

    try:
        extracted_files = extract_machine_code_from_tap(tap_file, output_dir)

        if extracted_files:
            print("\n✓ Extraction successful!")
            print(f"\nYou can now benchmark with:")
            for filename in extracted_files:
                print(f"  ./z80_benchmark {filename}")
        else:
            print("\n⚠️  No machine code found in TAP file")
            print("This TAP file may only contain BASIC code")

        return 0

    except Exception as e:
        print(f"\n✗ Error: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == '__main__':
    sys.exit(main())

