#include "chip8.h"

#include <cstdint>
#include <fstream>
#include <filesystem>

const unsigned int START_ADDRESS = 0x200;
const unsigned int MAX_ADDRESS = 4096;

const unsigned int FONTSET_START_ADDRESS = 0x50;
const unsigned int FONTSET_SIZE = 80;
uint8_t fontset[FONTSET_SIZE] =
{
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

Chip8::Chip8() : randGen(std::chrono::system_clock::now().time_since_epoch().count())
{
    this->pc = START_ADDRESS;

    // Load fonts into memory
    for (int i {}; i < FONTSET_SIZE; ++i){
        memory[FONTSET_START_ADDRESS + i] = fontset[i];
    }

    // Initialize RNG
    randByte = std::uniform_int_distribution<uint8_t>(0, 255U);
}

// Updated this function from the tutorial to use more modern C++ features
void Chip8::LoadROM(char const* filename)
{
    // Check to see if the file exists
    if(!std::filesystem::exists(filename))
        return;

    std::uintmax_t size { std::filesystem::file_size(filename) };

    // Check if the data in the file is too large to fit in the Chip 8's memory
    if (size > (MAX_ADDRESS - START_ADDRESS) )
        return;

    // Open the file as a stream of binary
    std::ifstream file(filename, std::ios::binary);

    if (file.is_open())
    {
        // Load the contents of the file into the Chip 8's memory
        file.read(reinterpret_cast<char*>(&memory[START_ADDRESS]), size);
        file.close();
    }
}

// Instructions

// CLS (Clear Screen)
void Chip8::OP_00E0()
{
    memset(video, 0, sizeof(video));
}

// RET
void Chip8::OP_00EE()
{
    --sp;
    pc = stack[sp];
}

// JP addr
void Chip8::OP_1nnn()
{
    uint16_t address = opcode & 0x0FFFu;
    pc = address;
}

// CALL addr
void Chip8::OP_2nnn()
{
    stack[sp] = pc;
    ++sp;

    uint16_t address = opcode & 0x0FFFu;
    pc = address;
}

// Skip next instruction if Vx = kk
void Chip8::OP_3xkk()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t byte = opcode & 0x00FFu;

    if(registers[Vx] == byte)
        pc +=2;
}

// Skip next instruction if Vx != kk
void Chip8::OP_4xkk()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t byte = opcode & 0x00FFu;

    if(registers[Vx] != byte)
        pc += 2;
}

// Skip next instruction if Vx == Vy
void Chip8::OP_5xy0()
{
    uint8_t Vx = (opcode & 0x0F00) >> 8u;
    uint8_t Vy = (opcode & 0x00F0) >> 8u;

    if(registers[Vx] == registers[Vy])
        pc += 2;
}

// Set Vx = kk
void Chip8::OP_6xkk()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t byte = opcode & 0x00FFu;

    registers[Vx] = byte;
}

// Set Vx = Vx + kk
void Chip8::OP_7xkk()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t byte = opcode & 0x00FFu;

    registers[Vx] += byte;
}

// Set Vx = Vy
void Chip8::OP_8xy0()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;

    registers[Vx] = registers[Vy];
}

// Set Vx = Vx OR Vy
void Chip8::OP_8xy1()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;

    registers[Vx] |= registers[Vy];
}

// Set Vx = Vx AND Vy
void Chip8::OP_8xy2()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;

    registers[Vx] &= registers[Vy];
}

// Set Vx = Vx XOR Vy
void Chip8::OP_8xy3()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;

    registers[Vx] ^= registers[Vy];
}

// Set Vx = Vx + Vy, set VF = carry
void Chip8::OP_8xy4()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;

    uint16_t sum = Vx + Vy;

    registers[Vx] = sum & 0x00FFu;

    if (sum > 255u)
        registers[0xF] = 1u;
    else
        registers[0xF] = 0u;
}

// Set Vx = Vx - Vy, set VF = NOT borrow
void Chip8::OP_8xy5()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;

    if (Vx > Vy)
        registers[0xF] = 1;
    else
        registers[0xF] = 0;

    registers[Vx] -= registers[Vy];
}

// Set Vx = Vx SHR 1
void Chip8::OP_8xy6()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;

    registers[0xF] = (registers[Vx] & 0x0001u);

    registers[Vx] >>= 1;
}

// Set Vx = Vy - Vx, set VF = NOT borrow
void Chip8::OP_8xy7()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;

    if (Vy > Vx)
        registers[0xF] = 1;
    else
        registers[0xF] = 0;

    registers[Vx] = registers[Vy] - registers[Vx];
}

// Set Vx = Vx SHL 1
void Chip8::OP_8xyE()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;

    registers[0xF] = (registers[Vx] & 0x8000u);

    registers[Vx] <<= 1;
}
