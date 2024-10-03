#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <map>
#include <iomanip>
#include <vector>
#include <ios>
#include <fstream>

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
	// 0x6000 - 7FFF is used for I/O (not implemented)

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

		C = N = V = Z = D = I = 0;
		A = X = Y = 0;

		// Little Endian
		// Read start vector from 0xFFFC and 0xFFFD
		PC = FetchWord(bus);
	}

	// Converts stack pointer location to valid address; adds digit 1 at the front (eg. sp = 0x5E, function returns 0x015E)
	Word SToAddress()
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

	// TODO: Add page boundry crossing cycle duration increases (eg. +1 clock cycle taken if crosses page boundry)

	// IMM = IMMEDIATE
	// ABS = ABSOLUTE
	// IMP = IMPLIED
	// ZPG = ZERO PAGE
	// ACC = ACCUMULATOR
	// ABX = ABSOLUTE, X
	// ABY = ABSOLUTE, Y
	// ZPX = ZERO PAGE, X
	// ZPY = ZERO PAGE, Y
	const std::map<Byte, void(CPU6502::*)(BUS&)> INSTRUCTIONS = {
		{0x4C, &CPU6502::JMP_ABS}, {0x8C, &CPU6502::STY_ABS}, {0x2D, &CPU6502::AND_ABS}, {0x06, &CPU6502::ASL_ZPG}, {0xD0, &CPU6502::BNE_REL}, // ALL IN THIS COLUMN NEED TO BE MARKED ON TODOLIST
		{0x8D, &CPU6502::STA_ABS}, {0xAA, &CPU6502::TAX_IMP}, {0x0D, &CPU6502::ORA_ABS}, {0xC6, &CPU6502::DEC_ZPG},
		{0xA9, &CPU6502::LDA_IMM}, {0xA8, &CPU6502::TAY_IMP}, {0x4D, &CPU6502::EOR_ABS}, {0x45, &CPU6502::EOR_ZPG},
		{0xAD, &CPU6502::LDA_ABS}, {0x8A, &CPU6502::TXA_IMP}, {0x6D, &CPU6502::ADC_ABS}, {0x66, &CPU6502::ROR_ZPG},
		{0xCE, &CPU6502::DEC_ABS}, {0x98, &CPU6502::TYA_IMP}, {0xED, &CPU6502::SBC_ABS}, {0x26, &CPU6502::ROL_ZPG},
		{0xEA, &CPU6502::NOP_IMP}, {0xE8, &CPU6502::INX_IMP}, {0x0A, &CPU6502::ASL_ACC}, {0x4A, &CPU6502::LSR_ACC},
		{0xA2, &CPU6502::LDX_IMM}, {0xC8, &CPU6502::INY_IMP}, {0xBA, &CPU6502::TSX_IMP}, {0xCD, &CPU6502::CMP_ABS},
		{0xA0, &CPU6502::LDY_IMM}, {0xCA, &CPU6502::DEX_IMP}, {0x9A, &CPU6502::TXS_IMP}, {0xEC, &CPU6502::CPX_ABS},
		{0xEE, &CPU6502::INC_ABS}, {0x88, &CPU6502::DEY_IMP}, {0x2A, &CPU6502::ROL_ACC}, {0xCC, &CPU6502::CPY_ABS},
		{0xAE, &CPU6502::LDX_ABS}, {0x09, &CPU6502::ORA_IMM}, {0x6A, &CPU6502::ROR_ACC}, {0x2C, &CPU6502::BIT_ABS},
		{0xAC, &CPU6502::LDY_ABS}, {0x49, &CPU6502::EOR_IMM}, {0xA5, &CPU6502::LDA_ZPG}, {0x24, &CPU6502::BIT_ZPG},
		{0x69, &CPU6502::ADC_IMM}, {0x29, &CPU6502::AND_IMM}, {0x65, &CPU6502::ADC_ZPG}, {0x25, &CPU6502::AND_ZPG},
		{0x6E, &CPU6502::ROR_ABS}, {0x18, &CPU6502::CLC_IMP}, {0xA6, &CPU6502::LDX_ZPG}, {0x05, &CPU6502::ORA_ZPG},
		{0xE9, &CPU6502::SBC_IMM}, {0xD8, &CPU6502::CLD_IMP}, {0xA4, &CPU6502::LDY_ZPG}, {0x4E, &CPU6502::LSR_ABS},
		{0x38, &CPU6502::SEC_IMP}, {0x58, &CPU6502::CLI_IMP}, {0x85, &CPU6502::STA_ZPG}, {0x46, &CPU6502::LSR_ZPG},
		{0x2E, &CPU6502::ROL_ABS}, {0xB8, &CPU6502::CLV_IMP}, {0x86, &CPU6502::STX_ZPG}, {0xC5, &CPU6502::CMP_ZPG},
		{0xF8, &CPU6502::SED_IMP}, {0xE0, &CPU6502::CPX_IMM}, {0x84, &CPU6502::STY_ZPG}, {0xE4, &CPU6502::CPX_ZPG},
		{0x78, &CPU6502::SEI_IMP}, {0xC0, &CPU6502::CPY_IMM}, {0x0E, &CPU6502::ASL_ABS}, {0xC4, &CPU6502::CPY_ZPG},
		{0x8E, &CPU6502::STX_ABS}, {0xC9, &CPU6502::CMP_IMM}, {0xE6, &CPU6502::INC_ZPG}, {0xE5, &CPU6502::SBC_ZPG}
	};

	// INSTRUCTION_ADRESSINGMODE
	void BNE_REL(BUS& bus)
	{
		Byte offset = FetchByte(bus);

		if (Z == 0)
		{
			// if negative, subtract positive value
			if (offset & 0b10000000)
			{
				offset--;
				offset = ~offset;
				PC -= offset;
			}
			// if positive, add value
			else
			{
				PC += offset;
			}
		}

		numCycles++;
	}

	void SBC_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		Byte b = ReadByte(bus, addr);

		Word value = (Word)b ^ 0x00FF;
		Word sum = value + A + C;

		V = ((A ^ sum) & (value ^ sum) & 0x0080);
		C = sum & 0xFF00;
		Z = ((sum & 0x00FF) == 0);
		N = (sum & 0x0080);

		A = sum & 0x00FF;
	}

	void CMP_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		Byte b = ReadByte(bus, addr);

		C = A >= b;
		Z = A == b;
		N = (0b10000000 & (A - b)) > 0;
	}

	void CPX_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		Byte b = ReadByte(bus, addr);

		C = X >= b;
		Z = X == b;
		N = (0b10000000 & (X - b)) > 0;
	}

	void CPY_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		Byte b = ReadByte(bus, addr);

		C = Y >= b;
		Z = Y == b;
		N = (0b10000000 & (Y - b)) > 0;
	}

	void LSR_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		Byte b = ReadByte(bus, addr);

		C = b & 0b00000001;

		b >>= 1;
		numCycles++;

		SendByte(bus, addr, b);

		Z = b == 0;
		N = (b & 0b10000000) > 0;
	}

	void LSR_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		Byte b = ReadByte(bus, addr);

		C = b & 0b00000001;

		b >>= 1;
		numCycles++;

		SendByte(bus, addr, b);

		Z = b == 0;
		N = (b & 0b10000000) > 0;
	}

	void ORA_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		Byte b = ReadByte(bus, addr);
		A |= b;

		Z = A == 0;
		N = (A & 0b10000000) > 0;
	}

	void AND_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		Byte b = ReadByte(bus, addr);
		A &= b;

		Z = A == 0;
		N = (A & 0b10000000) > 0;
	}

	void BIT_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		Byte b = ReadByte(bus, addr);

		Byte value = A & b;

		Z = value == 0;
		N = b & 0b10000000;
		V = b & 0b01000000;
	}

	void BIT_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		Byte b = ReadByte(bus, addr);

		Byte value = A & b;

		Z = value == 0;
		N = b & 0b10000000;
		V = b & 0b01000000;
	}

	void CMP_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		Byte b = ReadByte(bus, addr);

		C = A >= b;
		Z = A == b;
		N = (0b10000000 & (A - b)) > 0;
	}

	void CPX_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		Byte b = ReadByte(bus, addr);

		C = X >= b;
		Z = X == b;
		N = (0b10000000 & (X - b)) > 0;
	}

	void CPY_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		Byte b = ReadByte(bus, addr);

		C = Y >= b;
		Z = Y == b;
		N = (0b10000000 & (Y - b)) > 0;
	}

	void LSR_ACC(BUS& bus)
	{
		C = A & 0b00000001;

		A >>= 1;
		numCycles++;

		Z = A == 0;
		N = (A & 0b10000000) > 0;
	}

	void ROR_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		Byte b = ReadByte(bus, addr);

		Byte value = b >> 1;
		numCycles++;

		// Set bit 7 of value byte to value of the carry flag
		value |= C << 7;
		// Set carry bit to bit 0 of original byte
		C = 0b00000001 & b;

		SendByte(bus, addr, value);

		Z = (value == 0);
		N = (value & 0b10000000) > 0;
	}

	void ROL_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		Byte b = ReadByte(bus, addr);

		Byte value = b << 1;
		numCycles++;

		// Set bit 0 of value byte to value of the carry flag
		value |= C;
		// Set carry bit to bit 7 of original byte
		C = 0b10000000 & b;

		SendByte(bus, addr, value);

		Z = (value == 0);
		N = (value & 0b10000000) > 0;
	}

	void EOR_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		Byte b = ReadByte(bus, addr);
		A ^= b;

		Z = A == 0;
		N = (A & 0b10000000) > 0;
	}

	void DEC_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		Byte value = ReadByte(bus, addr);
		value--;
		SendByte(bus, addr, value);
		numCycles++;

		Z = (value == 0);
		N = (value & 0b10000000) > 0;
	}

	void INC_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		Byte value = ReadByte(bus, addr);
		value++;
		SendByte(bus, addr, value);
		numCycles++;

		Z = (value == 0);
		N = (value & 0b10000000) > 0;
	}

	void ASL_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		Byte b = ReadByte(bus, addr);
		C = b & 0b10000000;

		b <<= 1;
		numCycles++;

		SendByte(bus, addr, b);

		Z = b == 0;
		N = (b & 0b10000000) > 0;
	}

	void ASL_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		Byte b = ReadByte(bus, addr);
		C = b & 0b10000000;

		b <<= 1;
		numCycles++;

		SendByte(bus, addr, b);

		Z = b == 0;
		N = (b & 0b10000000) > 0;
	}

	void STA_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		SendByte(bus, addr, A);
	}

	void STX_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		SendByte(bus, addr, X);
	}

	void STY_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		SendByte(bus, addr, Y);
	}

	void LDY_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		Y = ReadByte(bus, addr);

		Z = (Y == 0);
		N = (Y & 0x0080);
	}

	void LDX_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		X = ReadByte(bus, addr);

		Z = (X == 0);
		N = (X & 0x0080);
	}

	void ADC_ZPG(BUS& bus)
	{
		Word addr = FetchByte(bus);
		Byte value = ReadByte(bus, addr);
		Word sum = value + A + C;

		V = ((A ^ sum) & (value ^ sum) & 0x0080);
		C = sum & 0xFF00;
		Z = ((sum & 0x00FF) == 0);
		N = (sum & 0x0080);

		A = sum & 0x00FF;
	}

	void LDA_ZPG(BUS& bus)
	{
		// Automatically casts byte to word, leaving most significant byte zeroed
		Word addr = FetchByte(bus);
		A = ReadByte(bus, addr);

		Z = (A == 0);
		N = (A & 0b10000000) > 0;
	}

	void ROR_ACC(BUS& bus)
	{
		Byte value = A >> 1;
		numCycles++;

		// Set bit 7 of value byte to value of the carry flag
		value |= C << 7;
		// Set carry bit to bit 0 of accumulator
		C = 0b00000001 & A;

		Z = (value == 0);
		N = (value & 0b10000000) > 0;
	}

	void ROL_ACC(BUS& bus)
	{
		Byte value = A << 1;
		numCycles++;

		// Set bit 0 of value byte to value of the carry flag
		value |= C;
		// Set carry bit to bit 7 of accumulator
		C = 0b10000000 & A;

		Z = (value == 0);
		N = (value & 0b10000000) > 0;
	}

	void TXS_IMP(BUS& bus)
	{
		S = X;
	}

	void TSX_IMP(BUS& bus)
	{
		X = S;

		Z = X == 0;
		N = (X & 0b10000000) > 0;
	}

	void ASL_ACC(BUS& bus)
	{
		C = A & 0b10000000;

		A <<= 1;
		numCycles++;

		Z = A == 0;
		N = (A & 0b10000000) > 0;
	}

	void CMP_IMM(BUS& bus)
	{
		Byte b = FetchByte(bus);
		Byte result = A - b;

		C = A >= b;
		Z = A == b;
		N = (result & 0b10000000) > 0;
	}

	void CPX_IMM(BUS& bus)
	{
		Byte b = FetchByte(bus);
		Byte result = X - b;

		C = X >= b;
		Z = X == b;
		N = (result & 0b10000000) > 0;
	}

	void CPY_IMM(BUS& bus)
	{
		Byte b = FetchByte(bus);
		Byte result = Y - b;

		C = Y >= b;
		Z = Y == b;
		N = (result & 0b10000000) > 0;
	}

	void AND_IMM(BUS& bus)
	{
		Byte b = FetchByte(bus);
		A &= b;

		Z = A == 0;
		N = (A & 0b10000000) > 0;
	}

	void AND_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		Byte b = ReadByte(bus, addr);
		A &= b;

		Z = A == 0;
		N = (A & 0b10000000) > 0;
	}

	void EOR_IMM(BUS& bus)
	{
		Byte b = FetchByte(bus);
		A ^= b;

		Z = A == 0;
		N = (A & 0b10000000) > 0;
	}

	void EOR_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		Byte b = ReadByte(bus, addr);
		A ^= b;

		Z = A == 0;
		N = (A & 0b10000000) > 0;
	}

	void ORA_IMM(BUS& bus)
	{
		Byte b = FetchByte(bus);
		A |= b;

		Z = A == 0;
		N = (A & 0b10000000) > 0;
	}

	void ORA_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		Byte b = ReadByte(bus, addr);
		A |= b;

		Z = A == 0;
		N = (A & 0b10000000) > 0;
	}

	void DEY_IMP(BUS& bus)
	{
		Y--;
		numCycles++;

		Z = Y == 0;
		N = (Y & 0b10000000) > 0;
	}

	void DEX_IMP(BUS& bus)
	{
		X--;
		numCycles++;

		Z = X == 0;
		N = (X & 0b10000000) > 0;
	}

	void INX_IMP(BUS& bus)
	{
		X++;
		numCycles++;

		Z = X == 0;
		N = (X & 0b10000000) > 0;
	}

	void INY_IMP(BUS& bus)
	{
		Y++;
		numCycles++;

		Z = Y == 0;
		N = (Y & 0b10000000) > 0;
	}

	void TXA_IMP(BUS& bus)
	{
		A = X;
		numCycles++;

		Z = A == 0;
		N = (A & 0b10000000) > 0;
	}

	void TYA_IMP(BUS& bus)
	{
		A = Y;
		numCycles++;

		Z = A == 0;
		N = (A & 0b10000000) > 0;
	}

	void TAX_IMP(BUS& bus)
	{
		X = A;
		numCycles++;

		Z = X == 0;
		N = (X & 0b10000000) > 0;
	}

	void TAY_IMP(BUS& bus)
	{
		Y = A;
		numCycles++;

		Z = Y == 0;
		N = (Y & 0b10000000) > 0;
	}

	void ROR_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		Byte b = ReadByte(bus, addr);

		Byte value = b >> 1;
		numCycles++;

		// Set bit 7 of value byte to value of the carry flag
		value |= C << 7;
		// Set carry bit to bit 0 of original byte
		C = 0b00000001 & b;

		SendByte(bus, addr, value);

		Z = (value == 0);
		N = (value & 0b10000000) > 0;
	}

	void ROL_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		Byte b = ReadByte(bus, addr);

		Byte value = b << 1;
		numCycles++;

		// Set bit 0 of value byte to value of the carry flag
		value |= C;
		// Set carry bit to bit 7 of original byte
		C = 0b10000000 & b;

		SendByte(bus, addr, value);

		Z = (value == 0);
		N = (value & 0b10000000) > 0;
	}

	void SEC_IMP(BUS& bus)
	{
		C = 1;
		numCycles++;
	}

	void SED_IMP(BUS& bus)
	{
		D = 1;
		numCycles++;
	}

	void SEI_IMP(BUS& bus)
	{
		I = 1;
		numCycles++;
	}

	void CLC_IMP(BUS& bus)
	{
		C = 0;
		numCycles++;
	}

	void CLD_IMP(BUS& bus)
	{
		D = 0;
		numCycles++;
	}

	void CLI_IMP(BUS& bus)
	{
		I = 0;
		numCycles++;
	}

	void CLV_IMP(BUS& bus)
	{
		V = 0;
		numCycles++;
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

	void STX_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		SendByte(bus, addr, X);
	}

	void STY_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		SendByte(bus, addr, Y);
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

	void ADC_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		Byte value = ReadByte(bus, addr);
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

	void SBC_ABS(BUS& bus)
	{
		Word addr = FetchWord(bus);
		Byte b = ReadByte(bus, addr);

		Word value = (Word)b ^ 0x00FF;
		Word sum = value + A + C;

		V = ((A ^ sum) & (value ^ sum) & 0x0080);
		C = sum & 0xFF00;
		Z = ((sum & 0x00FF) == 0);
		N = (sum & 0x0080);

		A = sum & 0x00FF;
	}

	// Excecutes based on num clock cycles
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
			catch (std::out_of_range)
			{
				// std::cout << "Instruction " << std::hex << std::setw(3) << instruction << " not recognized" << std::endl;
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
	//bus.rom.data[0x7FFC] = 0x00;
	//bus.rom.data[0x7FFD] = 0x80;

	// Load a program
	std::vector<Byte> prg;

	std::ifstream f;
	f.open("program.bin", std::ios::binary | std::ios::in);

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

	// Format console
	std::cout << std::internal << std::setfill('0') << std::uppercase;

	// Write program to ROM
	for (uint i = 0; i <= prg.size(); i++)
	{
		bus.rom.data[i] = prg[i];
	}

	pc.Reset(bus);
	pc.Execute(bus, 100);
	std::cout << std::hex << std::setw(4) << "Accumulator: " <<  +pc.A << std::endl;
	std::cout << std::nouppercase;
	std::cout << std::dec << "Clock cycles taken: " << pc.numCycles << std::endl;

	return 0;
}
