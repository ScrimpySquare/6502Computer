#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <SDL.h>
#include <thread>
#include <chrono>

typedef unsigned char Byte;
typedef unsigned short Word;
typedef unsigned int uint;

int width = 100;
int height = 64;
SDL_Window* window;
SDL_Renderer* renderer;
SDL_Event e;

// Not realistic, would just be going as fast as it can IRL
// Just for emu purposes
// (ms)
float frameDelay = 100;

// Time for each clock cycle (ms)
float clockTime = 1.0f;
// Use clock time or just go as fast as possible
bool useClockTime = false;

bool running = true;

struct ROM
{
    // 32k of address space
    static constexpr Word MEM_SIZE = 1024 * 32;
    Byte data[MEM_SIZE];

    void Initialize()
    {
        // Initialize data to all 0s
        std::fill(std::begin(data), std::end(data), 0);
    }

    void Load(const std::vector<Byte>& rom)
    {
        for (int i = 0; i < rom.size(); i++)
        {
                data[i] = rom[i];
        }
    }

    Byte ReadByte(const Word addr) const
    {
        return data[addr];
    }
};

struct RAM : ROM
{
    void WriteByte(const Word addr, const Byte b)
    {
        data[addr] = b;
    }
};

struct Bus
{
    // TODO: Make modular so that custom PC memory layouts can be created with components (i.e. custom memory map)
    RAM ram; // 0x0000 - 0x5FFF
    RAM vram; // 0x6000 - 0x7FFF
    ROM rom; // 0x8000 - 0xFFFF

    Byte ReadByte(const Word addr) const
    {
        if (addr < 0x6000)
        {
            return ram.ReadByte(addr);
        }
        if (addr < 0x8000)
        {
            // Properly address vram in its relative address space
            return vram.ReadByte(addr - 0x6000);
        }

        // Properly address rom in its relative address space
        return rom.ReadByte(addr - 0x8000);
    }

    void WriteByte(const Word addr, const Byte d)
    {
        if (addr < 0x6000)
        {
            ram.WriteByte(addr, d);
        }
        if (addr < 0x8000)
        {
            // Properly address vram in its relative address space
            vram.WriteByte(addr - 0x6000, d);
        }
    }
};

struct CPU6502
{
    bool debug = false; // Determines whether debug text will be printed to the screen

    Bus* bus;

    uint numCycles = 0;

    Word PC = 0xFFFC; // Program Counter
    Byte SP = 0xFF; // Stack Pointer

    // Registers
    Byte A = 0; // Accumulator
    Byte X = 0;
    Byte Y = 0;

    // Status Flags
    bool N = false; // Negative
    bool V = false; // Overflow
    bool B = false; // Break
    bool D = false; // Decimal
    bool I = false; // Interrupt Disable
    bool Z = false; // Zero
    bool C = false; // Carry

    explicit CPU6502(Bus* bus)
    {
        this->bus = bus;
    }

    void Clock(const uint c = 1)
    {
        if (useClockTime) std::this_thread::sleep_for(std::chrono::nanoseconds(static_cast<int>(clockTime * 1000000.0f * c)));
        numCycles += c;
    }

    void Reset()
    {
        // ADD CLOCK TIMINGS FOR RESET

        // Set PC to position to read start vector
        PC = 0xFFFC;
        // Reset stack pointer to top of stack
        SP = 0xFF;

        C = N = V = Z = D = I = false;
        A = X = Y = 0;

        // Read start vector
        PC = FetchWord();
    }

    // Converts stack pointer to absolute address
    Word SPToAddress() const
    {
        return 0x100 + SP;
    }

    // Fetches next byte at the PC and increments the PC
    Byte FetchByte()
    {
        return ReadByte(PC++);
    }

    // Fetches next word at the PC in little endian
    Word FetchWord()
    {
        Word w = FetchByte();
        w |= FetchByte() << 8;
        return w;
    }

    // Gets byte at address
    Byte ReadByte(const Word addr)
    {
        Clock(1);
        const Byte b = bus->ReadByte(addr);
        if (debug) std::cout << std::hex << std::setw(4) << addr << " READ " << std::setw(2) << +b << std::endl;
        return b;
    }

    // Gets word in little endian at address
    Word ReadWord(const Word addr)
    {
        return ReadByte(addr) + (static_cast<Word>(ReadByte(addr + 1)) << 8);
    }

    // Writes byte to address
    void WriteByte(const Word addr, const Byte b)
    {
        bus->WriteByte(addr, b);
        Clock(1);
        if (debug) std::cout << std::hex << std::setw(4) << addr << " WRITE " << std::setw(2) << +b << std::endl;
    }

    // Writes word in little endian to address
    void WriteWord(const Word addr, const Word w)
    {
        WriteByte(addr, w & 0b00001111);
        WriteByte(addr + 1, w >> 8);
    }

    void IRQ()
    {
        if (I == false) {
            WriteWord(SPToAddress() - 1, PC + 1);
            SP -= 2;

            B = false;
            I = true;
            PHP();

            // Read IRQ interrupt vector
            PC = ReadWord(0xFFFE);
            Clock(1);
        }
    }

    void NMI()
    {
        WriteWord(SPToAddress() - 1, PC + 1);
        SP -= 2;

        B = false;
        I = true;
        PHP();

        // Read NMI interrupt vector
        PC = ReadWord(0xFFFA);
        Clock(1);
    }

    // Executes the number of cycles provided
    void Execute(const uint cycles)
    {
        const int startCycles = numCycles;
        int deltaCycles = 0;
        while (deltaCycles < cycles && running)
        {
            // clock counts for all instructions include fetching the instruction itself
            // CHANGE TO MAP
            switch (FetchByte())
            {
                // No addressing mode function call = implied
                // "ReadByte(Absolute())" means the instruction uses the byte at the supplied address (e.g. ADC, LDA)
                // "Absolute()" means the instruction uses the address itself (e.g. STA, JMP)
                // A boolean value in the instruction arguments represents the Accumulator addressing mode
                case 0xEA:
                    NOP();
                    break;

                case 0x2C:
                    BIT(ReadByte(Absolute()));
                    break;
                case 0x24:
                    BIT(ReadByte(ZeroPage()));
                    break;

                case 0xA9:
                    LDA(Immediate());
                    break;
                case 0xAD:
                    LDA(ReadByte(Absolute()));
                    break;
                case 0xA5:
                    LDA(ReadByte(ZeroPage()));
                    break;
                case 0xB5:
                    LDA(ReadByte(ZeroPageX()));
                    break;
                case 0xBD:
                    LDA(ReadByte(AbsoluteX()));
                    break;
                case 0xB9:
                    LDA(ReadByte(AbsoluteY()));
                    break;
                case 0xA1:
                    LDA(ReadByte(IndirectX()));
                    break;
                case 0xB1:
                    LDA(ReadByte(IndirectY()));
                    break;

                case 0xA2:
                    LDX(Immediate());
                    break;
                case 0xAE:
                    LDX(ReadByte(Absolute()));
                    break;
                case 0xA6:
                    LDX(ReadByte(ZeroPage()));
                    break;
                case 0xB6:
                    LDX(ReadByte(ZeroPageY()));
                    break;
                case 0xBE:
                    LDX(ReadByte(AbsoluteY()));
                    break;

                case 0xA0:
                    LDY(Immediate());
                    break;
                case 0xAC:
                    LDY(ReadByte(Absolute()));
                    break;
                case 0xA4:
                    LDY(ReadByte(ZeroPage()));
                    break;
                case 0xB4:
                    LDY(ReadByte(ZeroPageX()));
                    break;
                case 0xBC:
                    LDY(ReadByte(AbsoluteX()));
                    break;

                case 0x8D:
                    STA(Absolute());
                    break;
                case 0x85:
                    STA(ZeroPage());
                    break;
                case 0x95:
                    STA(ZeroPageX());
                    break;
                case 0x9D:
                    STA(AbsoluteX());
                    break;
                case 0x99:
                    STA(AbsoluteY());
                    break;
                case 0x81:
                    STA(IndirectX());
                    break;
                case 0x91:
                    STA(IndirectY());
                    break;

                case 0x8E:
                    STX(Absolute());
                    break;
                case 0x86:
                    STX(ZeroPage());
                    break;
                case 0x96:
                    STX(ZeroPageY());
                    break;

                case 0x8C:
                    STY(Absolute());
                    break;
                case 0x84:
                    STY(ZeroPage());
                    break;
                case 0x94:
                    STY(ZeroPageX());
                    break;

                case 0xAA:
                    TAX();
                    break;

                case 0xA8:
                    TAY();
                    break;

                case 0xBA:
                    TSX();
                    break;

                case 0x8A:
                    TXA();
                    break;

                case 0x9A:
                    TXS();
                    break;

                case 0x98:
                    TYA();
                    break;

                case 0x48:
                    PHA();
                    break;

                case 0x68:
                    PLA();
                    break;

                case 0x08:
                    PHP();
                    break;

                case 0x28:
                    PLP();
                    break;

                case 0xEE:
                    INC(Absolute());
                    break;
                case 0xE6:
                    INC(ZeroPage());
                    break;
                case 0xF6:
                    INC(ZeroPageX());
                    break;
                case 0xFE:
                    INC(AbsoluteX());
                    break;

                case 0xE8:
                    INX();
                    break;

                case 0xC8:
                    INY();
                    break;

                case 0xCE:
                    DEC(Absolute());
                    break;
                case 0xC6:
                    DEC(ZeroPage());
                    break;
                case 0xD6:
                    DEC(ZeroPageX());
                    break;
                case 0xDE:
                    DEC(AbsoluteX());
                    break;

                case 0xCA:
                    DEX();
                    break;

                case 0x88:
                    DEY();
                    break;

                case 0x29:
                    AND(Immediate());
                    break;
                case 0x2D:
                    AND(ReadByte(Absolute()));
                    break;
                case 0x25:
                    AND(ReadByte(ZeroPage()));
                    break;
                case 0x35:
                    AND(ReadByte(ZeroPageX()));
                    break;
                case 0x3D:
                    AND(ReadByte(AbsoluteX()));
                    break;
                case 0x39:
                    AND(ReadByte(AbsoluteY()));
                    break;
                case 0x21:
                    AND(ReadByte(IndirectX()));
                    break;
                case 0x31:
                    AND(ReadByte(IndirectY()));
                    break;

                case 0x09:
                    ORA(Immediate());
                    break;
                case 0x0D:
                    ORA(ReadByte(Absolute()));
                    break;
                case 0x05:
                    ORA(ReadByte(ZeroPage()));
                    break;
                case 0x15:
                    ORA(ReadByte(ZeroPageX()));
                    break;
                case 0x1D:
                    ORA(ReadByte(AbsoluteX()));
                    break;
                case 0x19:
                    ORA(ReadByte(AbsoluteY()));
                    break;
                case 0x01:
                    ORA(ReadByte(IndirectX()));
                    break;
                case 0x11:
                    ORA(ReadByte(IndirectY()));
                    break;

                case 0x49:
                    EOR(Immediate());
                    break;
                case 0x4D:
                    EOR(ReadByte(Absolute()));
                    break;
                case 0x45:
                    EOR(ReadByte(ZeroPage()));
                    break;
                case 0x55:
                    EOR(ReadByte(ZeroPageX()));
                    break;
                case 0x5D:
                    EOR(ReadByte(AbsoluteX()));
                    break;
                case 0x59:
                    EOR(ReadByte(AbsoluteY()));
                    break;
                case 0x41:
                    EOR(ReadByte(IndirectX()));
                    break;
                case 0x51:
                    EOR(ReadByte(IndirectY()));
                    break;

                case 0xC9:
                    CMP(Immediate());
                    break;
                case 0xCD:
                    CMP(ReadByte(Absolute()));
                    break;
                case 0xC5:
                    CMP(ReadByte(ZeroPage()));
                    break;
                case 0xD5:
                    CMP(ReadByte(ZeroPageX()));
                    break;
                case 0xDD:
                    CMP(ReadByte(AbsoluteX()));
                    break;
                case 0xD9:
                    CMP(ReadByte(AbsoluteY()));
                    break;
                case 0xC1:
                    CMP(ReadByte(IndirectX()));
                    break;
                case 0xD1:
                    CMP(ReadByte(IndirectY()));
                    break;

                case 0xE0:
                    CPX(Immediate());
                    break;
                case 0xEC:
                    CPX(ReadByte(Absolute()));
                    break;
                case 0xE4:
                    CPX(ReadByte(ZeroPage()));
                    break;

                case 0xC0:
                    CPY(Immediate());
                    break;
                case 0xCC:
                    CPY(ReadByte(Absolute()));
                    break;
                case 0xC4:
                    CPY(ReadByte(ZeroPage()));
                    break;

                case 0x0A:
                    ASL(0x00, true);
                    break;
                case 0x0E:
                    ASL(Absolute(), false);
                    break;
                case 0x06:
                    ASL(ZeroPage(), false);
                    break;
                case 0x16:
                    ASL(ZeroPageX(), false);
                    break;
                case 0x1E:
                    ASL(AbsoluteX(), false);
                    break;

                case 0x4A:
                    LSR(0x00, true);
                    break;
                case 0x4E:
                    LSR(Absolute(), false);
                    break;
                case 0x46:
                    LSR(ZeroPage(), false);
                    break;
                case 0x56:
                    LSR(ZeroPageX(), false);
                    break;
                case 0x5E:
                    LSR(AbsoluteX(), false);
                    break;

                case 0x2A:
                    ROL(0x00, true);
                    break;
                case 0x2E:
                    ROL(Absolute(), false);
                    break;
                case 0x26:
                    ROL(ZeroPage(), false);
                    break;
                case 0x36:
                    ROL(ZeroPageX(), false);
                    break;
                case 0x3E:
                    ROL(AbsoluteX(), false);
                    break;

                case 0x6A:
                    ROR(0x00, true);
                    break;
                case 0x6E:
                    ROR(Absolute(), false);
                    break;
                case 0x66:
                    ROR(ZeroPage(), false);
                    break;
                case 0x76:
                    ROR(ZeroPageX(), false);
                    break;
                case 0x7E:
                    ROR(AbsoluteX(), false);
                    break;

                case 0x4C:
                    JMP(Absolute());
                    break;
                case 0x6C:
                    JMP(Indirect());
                    break;

                case 0x20:
                    JSR(Absolute());
                    break;

                case 0x60:
                    RTS();
                    break;

                case 0xF0:
                    BEQ(Relative());
                    break;

                case 0xD0:
                    BNE(Relative());
                    break;

                case 0xB0:
                    BCS(Relative());
                    break;

                case 0x90:
                    BCC(Relative());
                    break;

                case 0x10:
                    BPL(Relative());
                    break;

                case 0x30:
                    BMI(Relative());
                    break;

                case 0x50:
                    BVC(Relative());
                    break;

                case 0x07:
                    BVS(Relative());
                    break;

                case 0x00:
                    BRK();
                    break;

                case 0x40:
                    RTI();
                    break;

                case 0x18:
                    CLC();
                    break;

                case 0x38:
                    SEC();
                    break;

                case 0xD8:
                    CLD();
                    break;

                case 0xF8:
                    SED();
                    break;

                case 0x58:
                    CLI();
                    break;

                case 0x78:
                    SEI();
                    break;

                case 0xB8:
                    CLV();
                    break;

                case 0x69:
                    ADC(Immediate());
                    break;
                case 0x6D:
                    ADC(ReadByte(Absolute()));
                    break;
                case 0x65:
                    ADC(ReadByte(ZeroPage()));
                    break;
                case 0x75:
                    ADC(ReadByte(ZeroPageX()));
                    break;
                case 0x7D:
                    ADC(ReadByte(AbsoluteX()));
                    break;
                case 0x79:
                    ADC(ReadByte(AbsoluteY()));
                    break;
                case 0x61:
                    ADC(ReadByte(IndirectX()));
                    break;
                case 0x71:
                    ADC(ReadByte(IndirectY()));
                    break;

                case 0xE9:
                    SBC(Immediate());
                    break;
                case 0xED:
                    SBC(ReadByte(Absolute()));
                    break;
                case 0xE5:
                    SBC(ReadByte(ZeroPage()));
                    break;
                case 0xF5:
                    SBC(ReadByte(ZeroPageX()));
                    break;
                case 0xFD:
                    SBC(ReadByte(AbsoluteX()));
                    break;
                case 0xF9:
                    SBC(ReadByte(AbsoluteY()));
                    break;
                case 0xE1:
                    SBC(ReadByte(IndirectX()));
                    break;
                case 0xF1:
                    SBC(ReadByte(IndirectY()));
                    break;

                default:
                    std::cout << "Instruction not recognized" << std::endl;
            }
            // std::cout << std::hex << std::setw(2) << +bus->ram.data[0x0001] << +bus->ram.data[0x0000] << std::endl;
            deltaCycles = numCycles - startCycles;
        }
    }

    // Addressing mode helpers (Implied and Accumulator are one byte instructions so no function required)
    Byte Immediate()
    {
        return FetchByte();
    }

    Word Absolute()
    {
        return FetchWord();
    }

    Word AbsoluteX()
    {
        const Word addr = FetchWord();

        if ((addr & 0x00FF) + X > 0x00FF)
        {
            Clock(1);
        }

        return addr + X;
    }

    Word AbsoluteY()
    {
        const Word addr = FetchWord();

        if ((addr & 0x00FF) + Y > 0x00FF)
        {
            Clock(1);
        }

        return addr + Y;
    }

    Word ZeroPage()
    {
        return 0x00FF & FetchByte();
    }

    Word ZeroPageX()
    {
        Clock(1);
        return 0x00FF & FetchByte() + X;
    }

    Word ZeroPageY()
    {
        Clock(1);
        return 0x00FF & FetchByte() + Y;
    }

    Word Indirect()
    {
        return ReadWord(FetchWord());
    }

    Word IndirectX()
    {
        return ReadWord(ZeroPageX());
    }

    Word IndirectY()
    {
        const Word addr = ReadWord(ZeroPage());
        if ((addr & 0x00FF) + Y > 0x00FF)
        {
            Clock(1);
        }
        return addr + Y;
    }

    Word Relative()
    {
        Word rel = ZeroPage();

        // if is negative
        if (rel & 0x80)
        {
            rel |= 0xFF00;
        }

        return rel + PC;
    }

    // Make set flags function for auto setting flags based on value?
    // TODO: Make helper functions for instructions (push to stack, have IRQ and Reset vectors stored somewhere)
    // INSTRUCTIONS
    void NOP()
    {
        Clock(1);
    }

    void BIT(const Byte b)
    {
        N = b & 0x80;
        V = b & 0x40;
        Z = A & b == 0;
    }

    // Transfers
    void LDA(const Byte b)
    {
        A = b;

        Z = A == 0;
        N = A & 0x80;
    }

    void LDX(const Byte b)
    {
        X = b;

        Z = X == 0;
        N = X & 0x80;
    }

    void LDY(const Byte b)
    {
        Y = b;

        Z = Y == 0;
        N = Y & 0x80;
    }

    void STA(const Word addr)
    {
        WriteByte(addr, A);
    }

    void STX(const Word addr)
    {
        WriteByte(addr, X);
    }

    void STY(const Word addr)
    {
        WriteByte(addr, Y);
    }

    void TAX()
    {
        X = A;
        Z = X == 0;
        N = X & 0x80;
        Clock(1);
    }

    void TAY()
    {
        Y = A;
        Z = Y == 0;
        N = Y & 0x80;
        Clock(1);
    }

    void TSX()
    {
        X = SP;
        Z = X == 0;
        N = X & 0x80;
        Clock(1);
    }

    void TXA()
    {
        A = X;
        Z = A == 0;
        N = A & 0x80;
        Clock(1);
    }

    void TXS()
    {
        SP = X;
        Clock(1);
    }

    void TYA()
    {
        A = Y;
        Z = A == 0;
        N = A & 0x80;
        Clock(1);
    }

    // Stack
    void PHA()
    {
        WriteByte(SPToAddress(), A);
        SP--;
        Clock(1);
    }

    void PLA()
    {
        SP++;
        A = ReadByte(SPToAddress());
        Clock(2);
        Z = A == 0;
        N = A & 0x80;
    }

    void PHP()
    {
        B = true;
        WriteByte(SPToAddress(), N << 7 + V << 6 + 1 << 5 + B << 4 + D << 3 + I << 2 + Z << 1 + C << 0);
        SP--;
        Clock(1);
        B = false;
    }

    void PLP()
    {
        SP++;
        const Byte status = ReadByte(SPToAddress());
        C = status & 0b00000001;
        Z = status & 0b00000010;
        I = status & 0b00000100;
        D = status & 0b00001000;
        B = status & 0b00010000;
        V = status & 0b01000000;
        N = status & 0b10000000;
        Clock(2);
    }

    // Increments
    void INC(const Word addr)
    {
        const Byte b = ReadByte(addr);
        WriteByte(addr, b + 1);
        Clock(1);

        Z = (b + 1 & 0xFF) == 0;
        N = b + 1 & 0xFF & 0x80;
    }

    void INX()
    {
        X++;
        Clock(1);

        Z = X == 0;
        N = X & 0x80;
    }

    void INY()
    {
        Y++;
        Clock(1);

        Z = Y == 0;
        N = Y & 0x80;
    }

    // Decrements
    void DEC(const Word addr)
    {
        const Byte b = ReadByte(addr);
        WriteByte(addr, b - 1);
        Clock(1);

        Z = (b - 1 & 0xFF) == 0;
        N = b - 1 & 0xFF & 0x80;
    }

    void DEX()
    {
        X--;
        Clock(1);

        Z = X == 0;
        N = X & 0x80;
    }

    void DEY()
    {
        Y--;
        Clock(1);

        Z = Y == 0;
        N = Y & 0x80;
    }

    // Logic
    void AND(const Byte b)
    {
        A &= b;

        Z = A == 0;
        N = A & 0x80;
    }

    void ORA(const Byte b)
    {
        A |= b;

        Z = A == 0;
        N = A & 0x80;
    }

    void EOR(const Byte b)
    {
        A ^= b;

        Z = A == 0;
        Z = A & 0x80;
    }

    // Comparisons
    void CMP(const Byte b)
    {
        const Byte diff = static_cast<Word>(A) - static_cast<Word>(b) & 0xFF;

        N = diff & 0x80;
        C = A >= b;
        Z = A == b;
    }

    void CPX(const Byte b)
    {
        const Byte diff = static_cast<Word>(X) - static_cast<Word>(b) & 0xFF;

        N = diff & 0x80;
        C = X >= b;
        Z = X == b;
    }

    void CPY(const Byte b)
    {
        const Byte diff = static_cast<Word>(Y) - static_cast<Word>(b) & 0xFF;

        N = diff & 0x80;
        C = Y >= b;
        Z = Y == b;
    }

    // Shifts
    void ASL(const Word addr, const bool acc)
    {
        if (acc)
        {
            C = A & 0x80;

            A <<= 1;

            Z = A == 0;
            N = A & 0x80;
        }
        else
        {
            Byte b = ReadByte(addr);

            C = b & 0x80;
            b <<= 1;

            Z = b == 0;
            N = b & 0x80;

            WriteByte(addr, b);
        }

        Clock(1);
    }

    void LSR(const Word addr, const bool acc)
    {
        if (acc)
        {
            C = A & 0x01;
            A >>= 1;

            Z = A == 0;
            N = A & 0x80;
        }
        else
        {
            Byte b = ReadByte(addr);

            C = b & 0x01;
            b >>= 1;

            Z = b == 0;
            N = b & 0x80;

            WriteByte(addr, b);
        }

        Clock(1);
    }

    // Rotations
    void ROL(const Word addr, const bool acc)
    {
        if (acc)
        {
            Byte temp = A;
            temp <<= 1;
            temp &= 0b11111110;
            temp += C;
            C = A >> 7;

            A = temp;

            Z = A == 0;
            N = A & 0x80;
            Clock(1);
        }
        else
        {
            const Byte b = ReadByte(addr);
            Byte temp = b;
            temp <<= 1;
            temp &= 0b11111110;
            temp += C;

            C = b >> 7;

            WriteByte(addr, temp);
            Z = temp == 0;
            N = temp & 0x80;
            Clock(1);
        }
    }

    void ROR(const Word addr, const bool acc)
    {
        if (acc)
        {
            Byte temp = A;
            temp >>= 1;
            temp &= 0b01111111;
            temp += C << 7;
            C = A & 1;

            A = temp;

            Z = A == 0;
            N = A & 0x80;
            Clock(1);
        }
        else
        {
            const Byte b = ReadByte(addr);
            Byte temp = b;
            temp >>= 1;
            temp &= 0b01111111;
            temp += C << 7;

            C = b & 1;

            WriteByte(addr, temp);
            Z = temp == 0;
            N = temp & 0x80;
            Clock(1);
        }
    }

    // Jumps/Subroutines
    void JMP(const Word addr)
    {
        PC = addr;
    }

    void JSR(const Word addr)
    {
        PC--;

        WriteWord(SPToAddress() - 1, PC);
        SP -= 2;

        PC = addr;
        Clock(1);
    }

    void RTS()
    {
        SP++;
        PC = ReadByte(SPToAddress());
        SP++;
        PC |= ReadByte(SPToAddress()) << 8;
        PC++;
        Clock(3);
    }

    // Branches
    void BEQ(const Word addr)
    {
        if (Z == 1)
        {
            Clock(1);

            if ((addr & 0xFF00) != (PC & 0xFF00))
            {
                Clock(1);
            }

            PC = addr;
        }
    }

    void BNE(const Word addr)
    {
        if (Z == 0)
        {
            Clock(1);

            if ((addr & 0xFF00) != (PC & 0xFF00))
            {
                Clock(1);
            }

            PC = addr;
        }
    }

    void BCS(const Word addr)
    {
        if (C == 1)
        {
            Clock(1);

            if ((addr & 0xFF00) != (PC & 0xFF00))
            {
                Clock(1);
            }

            PC = addr;
        }
    }

    void BCC(const Word addr)
    {
        if (C == 0)
        {
            Clock(1);

            if ((addr & 0xFF00) != (PC & 0xFF00))
            {
                Clock(1);
            }

            PC = addr;
        }
    }

    void BPL(const Word addr)
    {
        if (N == 0)
        {
            Clock(1);

            if ((addr & 0xFF00) != (PC & 0xFF00))
            {
                Clock(1);
            }

            PC = addr;
        }
    }

    void BMI(const Word addr)
    {
        if (N == 1)
        {
            Clock(1);

            if ((addr & 0xFF00) != (PC & 0xFF00))
            {
                Clock(1);
            }

            PC = addr;
        }
    }

    void BVC(const Word addr)
    {
        if (V == 0)
        {
            Clock(1);

            if ((addr & 0xFF00) != (PC & 0xFF00))
            {
                Clock(1);
            }

            PC = addr;
        }
    }

    void BVS(const Word addr)
    {
        if (V == 1)
        {
            Clock(1);

            if ((addr & 0xFF00) != (PC & 0xFF00))
            {
                Clock(1);
            }

            PC = addr;
        }
    }

    // Interrupts
    void BRK()
    {
        WriteWord(SPToAddress() - 1, PC + 1);
        SP -= 2;

        PHP();
        B = true;

        // Read IRQ interrupt vector
        PC = ReadWord(0xFFFE);
    }

    void RTI()
    {
        SP++;
        const Byte status = ReadByte(SPToAddress());
        C = status & 0b00000001;
        Z = status & 0b00000010;
        I = status & 0b00000100;
        D = status & 0b00001000;
        V = status & 0b01000000;
        N = status & 0b10000000;

        SP++;
        PC = ReadByte(SPToAddress());
        SP++;
        PC |= ReadByte(SPToAddress()) << 8;

        Clock(2);
    }

    // Flags
    void CLC()
    {
        C = false;
        Clock(1);
    }

    void SEC()
    {
        C = true;
        Clock(1);
    }

    void CLD()
    {
        D = false;
        Clock(1);
    }

    void SED()
    {
        D = true;
        Clock(1);
    }

    void CLI()
    {
        I = false;
        Clock(1);
    }

    void SEI()
    {
        I = true;
        Clock(1);
    }

    void CLV()
    {
        V = false;
        Clock(1);
    }

    // TODO: Add decimal flag support for math instructions
    // Arithmetic
    void ADC(const Byte b)
    {
        const Word sum = b + A + C;

        V = (A ^ sum) & (b ^ sum) & 0x0080;
        C = sum & 0xFF00;
        Z = (sum & 0x00FF) == 0;
        N = sum & 0x0080;

        A = sum & 0x00FF;
    }

    void SBC(const Byte b)
    {
        ADC(b ^ 0x00FF);
    }
};

struct Screen
{
    int x = 0;
    int y = 0;

    void Draw(const Byte color)
    {
        SDL_SetRenderDrawColor(renderer, (color >> 4 & 0b11) / 3.0f * 255, (color >> 2 & 0b11) / 3.0f * 255, (color & 0b11) / 3.0f * 255, 255);
        SDL_RenderDrawPoint(renderer, x, y);

        x++;
        if (x >= width)
        {
            y++;
            x = 0;
            if (y >= height)
            {
                y = 0;
                SDL_RenderPresent(renderer);
            }
        }
    }
};

struct GPU
{
    Bus* bus;

    Screen* screen;
    Byte x = 0;
    Byte y = 0;

    explicit GPU(Bus* bus, Screen* screen)
    {
        this->bus = bus;
        this->screen = screen;
    }

    void Run()
    {
        SDL_Init(SDL_INIT_EVERYTHING);
        SDL_CreateWindowAndRenderer(width*12, height*12, 0, &window, &renderer);
        SDL_RenderSetScale(renderer, 12, 12);

        while (running)
        {
            while(SDL_PollEvent(&e))
            {
                if (e.type == SDL_QUIT)
                {
                    running = false;
                }
            }

            // first 3 bits are 011 to address the vram through the bus correctly, next 6 bits of addr are y val, last 7 are x val
            // color is stored in a byte: 2 bits for each color -> 64 colors
            screen->Draw(bus->ReadByte((0b011 << 13) + (y << 7) + x));

            x++;
            if (x >= width)
            {
                y++;
                x = 0;
                if (y >= height)
                {
                    y = 0;

                    SDL_Delay(frameDelay);
                }
            }
        }
    }
};


int main(int argc, char** argv)
{
    // Format console
    std::cout << std::internal << std::setfill('0') << std::uppercase;

    Bus bus;
    CPU6502 cpu(&bus);
    cpu.debug = false;
    Screen screen;
    GPU gpu(&bus, &screen);

    // store rom and ram as files instead and read and write from them?
    bus.ram.Initialize();
    bus.rom.Initialize();
    bus.vram.Initialize();

    // Load a program
    std::vector<Byte> prg;
    std::ifstream f;
    f.open("../program.bin", std::ios::binary | std::ios::in);

    f >> std::noskipws;
    if (f.fail())
    {
        // err
    }

    while (!f.eof())
    {
        Byte b;

        f >> b;

        if (f.fail())
        {
            // err
            break;
        }

        prg.push_back(b);
    }
    f.close();

    bus.rom.Load(prg);
    //std::fill(std::begin(bus.vram.data), std::end(bus.vram.data), 0xFF);

    cpu.Reset();
    std::thread cpuThread(cpu.Execute, &cpu, 1000000000000);
    std::thread gpuThread(gpu.Run, &gpu);

    cpuThread.join();
    gpuThread.join();

    std::cout << std::endl << "Accumulator: " << std::hex << std::setw(2) << +cpu.A << std::endl;
    std::cout << "X: " << std::hex << std::setw(2) << +cpu.X << std::endl;
    std::cout << "Y: " << std::hex << std::setw(2) << +cpu.Y << std::endl;

    std::cout << std::endl << "N V D I Z C" << std::endl;
    std::cout << std::setw(1) << +cpu.N << " " << +cpu.V << " " << +cpu.D << " " << +cpu.I << " " << +cpu.Z << " " << +cpu.C << std::endl;
    return 0;
}
