#include <streambuf>
#include <istream>
#include <string.h>
#include "z80emu.h"

// https://github.com/devkitPro/buildscripts/issues/26#issuecomment-315311408
//extern "C" void __sync_synchronize() {}

struct membuf: std::streambuf {
    membuf(unsigned char const* base, size_t size) {
        char* p(const_cast<char*>(reinterpret_cast<const char *>(base)));
        this->setg(p, p, p + size);
    }
};

struct imemstream: virtual membuf, std::istream {
    imemstream(unsigned char const* base, size_t size)
        : membuf(base, size)
        , std::istream(static_cast<std::streambuf*>(this)) {
    }
};

using namespace std;

Z80emu::Z80emu(void) : cpu(this)
{

}

Z80emu::~Z80emu() {}

uint8_t Z80emu::fetchOpcode(uint16_t address) {
    // 3 clocks to fetch opcode from RAM and 1 execution clock
    tstates += 4;
    return z80Ram[address];
}

uint8_t Z80emu::peek8(uint16_t address) {
    // 3 clocks for read byte from RAM
    tstates += 3;
    return z80Ram[address];
}

void Z80emu::poke8(uint16_t address, uint8_t value) {
    // 3 clocks for write byte to RAM
    tstates += 3;
    z80Ram[address] = value;
}

uint16_t Z80emu::peek16(uint16_t address) {
    // Order matters, first read lsb, then read msb, don't "optimize"
    uint8_t lsb = peek8(address);
    uint8_t msb = peek8(address + 1);
    return (msb << 8) | lsb;
}

void Z80emu::poke16(uint16_t address, RegisterPair word) {
    // Order matters, first write lsb, then write msb, don't "optimize"
    poke8(address, word.byte8.lo);
    poke8(address + 1, word.byte8.hi);
}

uint8_t Z80emu::inPort(uint16_t port) {
    // 4 clocks for read byte from bus
    tstates += 3;
    return z80Ports[port];
}

void Z80emu::outPort(uint16_t port, uint8_t value) {
    // 4 clocks for write byte to bus
    tstates += 4;
    z80Ports[port] = value;
}

void Z80emu::addressOnBus(uint16_t address, int32_t tstates) {
    // Additional clocks to be added on some instructions
    this->tstates += tstates;
}

void Z80emu::interruptHandlingTime(int32_t tstates) {
    this->tstates += tstates;
}

bool Z80emu::isActiveINT(void) {
	// Put here the needed logic to trigger an INT
    return false;
}

#ifdef WITH_EXEC_DONE
void Z80emu::execDone(void) {}
#endif

uint8_t Z80emu::breakpoint(uint16_t address, uint8_t opcode) {
    // Emulate CP/M Syscall at address 5
    switch (cpu.getRegC()) {
        case 0: // BDOS 0 System Reset
        {
            cout << "Z80 reset after " << tstates << " t-states" << endl;
            finish = true;
            break;
        }
        case 2: // BDOS 2 console char output
        {
            cout << (char) cpu.getRegE();
            break;
        }
        case 9: // BDOS 9 console string output (string terminated by "$")
        {
            uint16_t strAddr = cpu.getRegDE();
            uint16_t endAddr = cpu.getRegDE();
            while (z80Ram[endAddr++] != '$');
            std::string message((const char *) &z80Ram[strAddr], endAddr - strAddr - 1);
            cout << message;
            cout.flush();
            break;
        }
        default:
        {
            cout << "BDOS Call " << cpu.getRegC() << endl;
            finish = true;
            cout << finish << endl;
        }
    }
    // opcode would be modified before the decodeOpcode method
    return opcode;
}

void Z80emu::runTest(std::ifstream* f) {
    streampos size;
    if (!f->is_open()) {
        cout << "file NOT OPEN" << endl;
        return;
    } else cout << "file open" << endl;

    size = f->tellg();
    cout << "Test size: " << size << endl;
    f->seekg(0, ios::beg);
    f->read((char *) &z80Ram[0x100], size);
    f->close();

    cpu.reset();
    finish = false;

    z80Ram[0] = (uint8_t) 0xC3;
    z80Ram[1] = 0x00;
    z80Ram[2] = 0x01; // JP 0x100 CP/M TPA
    z80Ram[5] = (uint8_t) 0xC9; // Return from BDOS call

    cpu.setBreakpoint(0x0005, true);
    while (!finish) {
        cpu.execute();
    }
}


void Z80emu::initialise(unsigned char const* base, size_t size) {

    memcpy(&z80Ram[0x0000],  base,  size);
    cpu.reset();
}


// void Z80emu::initialise(unsigned char const* base, size_t size) {

//     memcpy(&z80Ram[0x0100],  base,  size);
//     cpu.reset();

//     z80Ram[0] = (uint8_t) 0xC3;
//     z80Ram[1] = 0x00;
//     z80Ram[2] = 0x01; // JP 0x100 CP/M TPA
//     z80Ram[5] = (uint8_t) 0xC9; // Return from BDOS call

//     cpu.setBreakpoint(0x0005, true);
// }

static unsigned int c = 0;

void Z80emu::run() {
    cpu.execute();
}


uint8_t *Z80emu::getRam() {
    return z80Ram;
}
