#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <map>

typedef unsigned char Byte;
typedef unsigned short Word;
typedef unsigned int uint;

struct ROM256
{
	static constexpr uint MEM_SIZE = 1024 * 32;
	Byte data[MEM_SIZE];

	void Initialize()
	{
		for (uint i = 0; i < MEM_SIZE; i++)
		{
			data[i] = 0;
		}
	}

	Byte ReadByte(Word addr)
	{
		return data[addr];
	}
};

struct RAM256 : ROM256
{
	void WriteByte(Word addr, Byte d)
	{
		data[addr] = d;
	}
};

struct BUS
{
	RAM256 ram;	// 0x0000 - 0x5FFF
	ROM256 rom; // 0x8000 - 0xFFFF

	Byte ReadByte(Word addr)
	{
		if (addr > 0x7FFF)
		{
			// "addr & 0x7FFF" turns bit 15 to 0 to properly address the memory
			return rom.ReadByte(addr & 0x7FFF);
		}
		else
		{
			return ram.ReadByte(addr);
		}
	}

	void WriteByte(Word addr, Byte d)
	{
		if (addr < 0x6000)
		{
			ram.WriteByte(addr, d);
		}
		else 
		{
			printf("Invalid write adress.");
		}
	}
};

struct CPU6502
{
	uint numCycles = 0;

	Word PC = 0xFFFC; // Program Counter
	Byte S = 0xFF;  // Stack Pointer

	// Registers
	Byte A = 0;
	Byte X = 0;
	Byte Y = 0;

	Byte C : 1; // Carry
	Byte N : 1; // Negative
	Byte V : 1; // Overflow
	Byte Z : 1; // Zero
	Byte D : 1; // Decimal
	Byte I : 1; // Interrupt Disable

	void Reset(BUS& bus)
	{
		PC = 0xFFFC;
		S = 0xFF;

		C, N, V, Z, D, I = 0;
		A, X, Y = 0;

		// Little Endian
		// Read start vector from 0xFFFC and 0xFFFD
		PC = FetchWord(bus);
	}

	Word SPToAddress()
	{
		return 0x100 | S;
	}

	Word FetchWord(BUS& bus)
	{
		Word w = FetchByte(bus);
		w |= (FetchByte(bus) << 8);
		return w;
	}

	Byte FetchByte(BUS& bus)
	{
		Byte b = bus.ReadByte(PC);
		printf("%04X READ  %02X\n", PC, b);

		PC++;
		numCycles++;
		return b;
	}

	Byte ReadByte(BUS& bus, Word addr)
	{
		Byte b = bus.ReadByte(addr);
		printf("%04X READ  %02X\n", addr, b);

		numCycles++;
		return b;
	}


	void SendByte(BUS& bus, Word addr, Byte b)
	{
		printf("%04X WRITE %02X\n", addr, b);
		bus.WriteByte(addr, b);

		numCycles++;
	}

	void SendWord(BUS& bus, Word addr, Word w)
	{
		Byte b1 = w & 0b00001111;
		Byte b2 = w >> 8;

		SendByte(bus, addr, b1);
		SendByte(bus, addr, b2);

		numCycles++;
	}


	// IMM = IMMEDIATE
	// ABS = ABSOLUTE
	// IMP = IMPLIED
	const std::map<Byte, void(CPU6502::*)(BUS&)>INSTRUCTIONS = {
		{0x4C, &CPU6502::JMP_ABS}, 
		{0x8D, &CPU6502::STA_ABS},
		{0xA9, &CPU6502::LDA_IMM}, 
		{0xAD, &CPU6502::LDA_ABS},
		{0xCE, &CPU6502::DEC_ABS}, 
		{0xEA, &CPU6502::NOP_IMP},
		{0xA2, &CPU6502::LDX_IMM}, 
		{0xA0, &CPU6502::LDY_IMM},
		{0xEE, &CPU6502::INC_ABS}, 
		{0xAE, &CPU6502::LDX_ABS},
		{0xAC, &CPU6502::LDY_ABS}, 
		{0x69, &CPU6502::ADC_IMM},
		{0x6E, &CPU6502::ROR_ABS},
		{0xE9, &CPU6502::SBC_IMM}
	};
	
	void ROR_ABS(BUS& bus) 
	{
		Word addr = FetchWord(bus);
		Byte b = ReadByte(bus, addr);
		Byte value = b >> 1;
		numCycles++;

		// Setting bit 7 of value to current carry bit
		value |= C << 7;
		// Setting carry bit to value of bit 0 of original byte b
		C = 0b00000001 & b;

		SendByte(bus, addr, value; 
		
		Z = (value == 0);
		N = (value & 0b10000000) > 0;
	}

	void LDA_IMM(BUS& bus)
	{
		Byte b = FetchByte(bus);
		A = b;
		Z = (A == 0);
		N = (A & 0x0080);
	}

	void LDA_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		A = ReadByte(bus, addr);
		Z = (A == 0);
		N = (A & 0x0080);
	}

	void STA_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		SendByte(bus, addr, A);
	}

	void JMP_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		PC = addr;
	}

	void INC_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		Byte value = ReadByte(bus, addr);
		value++;
		SendByte(bus, addr, value);
		numCycles++;

		Z = (value == 0);
		N = (value & 0b10000000) > 0;
	}

	void DEC_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		Byte value = ReadByte(bus, addr);
		value--;
		SendByte(bus, addr, value);
		numCycles++;

		Z = (value == 0);
		N = (value & 0b10000000) > 0;
	}

	void NOP_IMP(BUS& bus)
	{
		numCycles++;
	}

	void LDX_IMM(BUS& bus)
	{
		Byte b = FetchByte(bus);
		X = b;
		Z = (X == 0);
		N = (X & 0x0080);
	}

	void LDY_IMM(BUS& bus)
	{
		Byte b = FetchByte(bus);
		Y = b;
		Z = (Y == 0);
		N = (Y & 0x0080);
	}

	void LDX_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		X = ReadByte(bus, addr);
		Z = (X == 0);
		N = (X & 0x0080);
	}

	void LDY_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		Y = ReadByte(bus, addr);
		Z = (Y == 0);
		N = (Y & 0x0080);
	}
	// TODO: Add decimal flag support for math instructions? (ADC, SBC, etc.)
	void ADC_IMM(BUS& bus)
	{
		Byte value = FetchByte(bus);
		Word sum = value + A + C;

		V = ((A ^ sum) & (value ^ sum) & 0x0080);
		C = sum & 0xFF00;
		Z = ((sum & 0x00FF) == 0);
		N = (sum & 0x0080);

		A = sum & 0x00FF;
	}

	void SBC_IMM(BUS& bus)
	{
		Word value = ((Word)FetchByte(bus)) ^ 0x00FF;
		Word sum = value + A + C;

		V = ((A ^ sum) & (value ^ sum) & 0x0080);
		C = sum & 0xFF00;
		Z = ((sum & 0x00FF) == 0);
		N = (sum & 0x0080);

		A = sum & 0x00FF;
	}

	void Execute(BUS& bus, uint cycles)
	{
		while (numCycles < cycles + 2)
		{
			// ALL CLOCK CYCLE COUNTS FOR ALL INSTRUCTIONS INCLUDE FETCHING THE INSTRUCTION ITSELF
			Byte instruction = FetchByte(bus);

			try 
			{
				// Find and call relavent function from instructions map
				void(CPU6502:: * func)(BUS&) = INSTRUCTIONS.at(instruction);
				(this->*func)(bus);
			}
			// If instruction opcode is not found in instructions map, print	
			catch (std::out_of_range) 
			{
				printf("Instruction '%02X' not recognized.\n", instruction);
			}
		}
	}
};

int main()
{
	CPU6502 pc{};
	BUS bus;

	bus.ram.Initialize();
	bus.rom.Initialize();

	// Set reset vector
	bus.rom.data[0x7FFC] = 0x00;
	bus.rom.data[0x7FFD] = 0x80;

	Byte prg[] = { 0xA9,  0x80,  0xE9,  0x20,  0x8D,  0x00,  0x10 };

	// Write program to ROM
	for (uint i = 0; i < (sizeof(prg) / sizeof(prg[0])); i++)
	{
		bus.rom.data[i] = prg[i];
	}

	pc.Reset(bus);
	pc.Execute(bus, 15);
	printf("Clock cycles taken: %d", pc.numCycles);

	return 0;
}
