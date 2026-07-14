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

    // Initialize our function table
    table[0x0] = [this]()
        {
            uint8_t index = opcode & 0x00FFu;

            if(table0[index])
                table0[index]();
        };
    table[0x1] = [this]() { Chip8::OP_1nnn(); };
    table[0x2] = [this]() { Chip8::OP_2nnn(); };
    table[0x3] = [this]() { Chip8::OP_3xkk(); };
    table[0x4] = [this]() { Chip8::OP_4xkk(); };
    table[0x5] = [this]() { Chip8::OP_5xy0(); };
    table[0x6] = [this]() { Chip8::OP_6xkk(); };
    table[0x7] = [this]() { Chip8::OP_7xkk(); };
    table[0x8] = [this]()
        {
            uint8_t index = opcode & 0x00FFu;

            if(table8[index])
                table8[index]();
        };
    table[0x9] = [this]() { Chip8::OP_9xy0(); };
    table[0xA] = [this]() { Chip8::OP_Annn(); };
    table[0xB] = [this]() { Chip8::OP_Bnnn(); };
    table[0xC] = [this]() { Chip8::OP_Cxkk(); };
    table[0xD] = [this]() { Chip8::OP_Dxyn(); };
    table[0xE] = [this]()
        {
            uint8_t index = opcode & 0x00FFu;

            if(tableE[index])
                tableE[index]();
        };
    table[0xF] = [this]()
        {
            uint8_t index = opcode & 0x00FFu;

            if(tableF[index])
                tableF[index]();
        };

    // Initialize table0, table8, and tableD
    for(int i {}; i <= 0xE; ++i)
    {
        table0[i] = [this]() { Chip8::OP_NULL(); };
        table8[i] = [this]() { Chip8::OP_NULL(); };
        tableE[i] = [this]() { Chip8::OP_NULL(); };
    }

    table0[0x0] = [this]() { Chip8::OP_00E0(); };
    table0[0xE] = [this]() { Chip8::OP_00EE(); };

    table8[0x0] = [this]() { Chip8::OP_8xy0(); };
    table8[0x1] = [this]() { Chip8::OP_8xy1(); };
    table8[0x2] = [this]() { Chip8::OP_8xy2(); };
    table8[0x3] = [this]() { Chip8::OP_8xy3(); };
    table8[0x4] = [this]() { Chip8::OP_8xy4(); };
    table8[0x5] = [this]() { Chip8::OP_8xy5(); };
    table8[0x6] = [this]() { Chip8::OP_8xy6(); };
    table8[0x7] = [this]() { Chip8::OP_8xy7(); };
    table8[0xE] = [this]() { Chip8::OP_8xyE(); };

    tableE[0x1] = [this]() { Chip8::OP_ExA1(); };
    tableE[0xE] = [this]() { Chip8::OP_Ex9E(); };

    // tableF
    for (int i {}; i <= 0x65; ++i)
    {
        tableF[i] = [this]() { Chip8::OP_NULL(); };
    }

    tableF[0x07] = [this]() { Chip8::OP_Fx07(); };
	tableF[0x0A] = [this]() { Chip8::OP_Fx0A(); };
	tableF[0x15] = [this]() { Chip8::OP_Fx15(); };
	tableF[0x18] = [this]() { Chip8::OP_Fx18(); };
	tableF[0x1E] = [this]() { Chip8::OP_Fx1E(); };
	tableF[0x29] = [this]() { Chip8::OP_Fx29(); };
	tableF[0x33] = [this]() { Chip8::OP_Fx33(); };
	tableF[0x55] = [this]() { Chip8::OP_Fx55(); };
	tableF[0x65] = [this]() { Chip8::OP_Fx65(); };
}

void Chip8::Cycle()
{
    // Fetch the next instruction
    opcode = (memory[pc] << 8u) | memory[pc+1];

    // Increment the pc
    pc += 2;

    // execute the opcode
    table[ (opcode & 0xF000u) >> 12u ]();

    if (delayTimer > 0)
        --delayTimer;

    if (soundTimer > 0)
        --soundTimer;
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
// No OP
void Chip8::OP_NULL()
{}

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

// Skip next instruction if Vx != Vy
void Chip8::OP_9xy0()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;

    if (registers[Vx] != registers[Vy])
        pc += 2;
}

// Set I = nnn
void Chip8::OP_Annn()
{
    index = opcode & 0x0FFFu;
}

// Jump to location nnn + V0
void Chip8::OP_Bnnn()
{
    uint16_t address = opcode & 0x0FFFu;
    pc = address + registers[0x0];
}

// Set Vx = random byte AND kk
void Chip8::OP_Cxkk()
{
    uint8_t Vx = (opcode & 0x0F00) >> 8u;
    uint8_t byte = opcode & 0x00FFu;

    registers[Vx] = randByte(randGen) & byte;
}

//
void Chip8::OP_Dxyn()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t Vy = (opcode & 0x00F0u) >> 4u;
    uint8_t height = opcode & 0x000Fu;

    // Wrap if going beyond screen boundaries
    uint8_t xPos = registers[Vx] % VIDEO_WIDTH;
    uint8_t yPos = registers[Vy] % VIDEO_HEIGHT;

    registers[0xF] = 0;

    for (int row {}; row < height; ++row)
    {
        uint8_t spriteByte = memory[index + row];

        for (int col {}; col < 8; ++col)
        {
            uint8_t spritePixel = spriteByte & (0x80 >> col);
            uint32_t* screenPixel = &video[(yPos + row) * VIDEO_WIDTH + (xPos + col)];

            // Sprite Pixel is on
            if (spritePixel)
            {
                if (*screenPixel == 0xFFFFFFFF)
                {
                    registers[0xF] = 1u;
                }

                // Effectively XOR with the sprite pixel
                *screenPixel ^= 0xFFFFFFFF;
            }
        }
    }
}

// Skip next instruction if key with the value of Vx is pressed
void Chip8::OP_Ex9E()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;

    uint8_t key = registers[Vx];

    if (keypad[key])
        pc += 2;
}

void Chip8::OP_ExA1()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;

    uint8_t key = registers[Vx];

    if (!keypad[key])
        pc += 2;
}

// Set Vx = delay timer value
void Chip8::OP_Fx07()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;

    registers[Vx] = delayTimer;
}

// Wait for a key press, store the value of the key in Vx
void Chip8::OP_Fx0A()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    if(keypad[0])
    {
        registers[Vx] = 0;
    }
    else if (keypad[1])
    {
        registers[Vx] = 1;
    }
    else if (keypad[2])
    {
        registers[Vx] = 2;
    }
    else if (keypad[3])
    {
        registers[Vx] = 3;
    }
    else if (keypad[4])
    {
        registers[Vx] = 4;
    }
    else if (keypad[5])
    {
        registers[Vx] = 5;
    }
    else if (keypad[6])
    {
        registers[Vx] = 6;
    }
    else if (keypad[7])
    {
        registers[Vx] = 7;
    }
    else if (keypad[8])
    {
        registers[Vx] = 8;
    }
    else if (keypad[9])
    {
        registers[Vx] = 9;
    }
    else if (keypad[10])
    {
        registers[Vx] = 10;
    }
    else if (keypad[11])
    {
        registers[Vx] = 11;
    }
    else if (keypad[12])
    {
        registers[Vx] = 12;
    }
    else if (keypad[13])
    {
        registers[Vx] = 13;
    }
    else if (keypad[14])
    {
        registers[Vx] = 14;
    }
    else if (keypad[15])
    {
        registers[Vx] = 15;
    }
    else
    {
        pc -= 2;
    }
}

// Set delay timer = Vx
void Chip8::OP_Fx15()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    delayTimer = registers[Vx];
}

// Set sound timer = Vx
void Chip8::OP_Fx18()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    soundTimer = registers[Vx];
}

// Set I = I + Vx
void Chip8::OP_Fx1E()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    index += registers[Vx];
}

// Set I = location of sprite for digit Vx
void Chip8::OP_Fx29()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t digit = registers[digit];

    index = FONTSET_START_ADDRESS + (5 * digit);
}

// Store BCD representation of Vx in memory locations I, I+1, and I+2
void Chip8::OP_Fx33()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;
    uint8_t value = registers[Vx];

    memory[index] = value % 10;
    value /= 10;

    memory[index + 1] = value % 10;
    value /= 10;

    memory[index + 2] = value % 10;
}

// Store registers V0 through Vx in memory starting at location I
void Chip8::OP_Fx55()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;

    for (int i {}; i < Vx; ++i)
    {
        memory[index + i] = registers[i];
    }
}

void Chip8::OP_Fx65()
{
    uint8_t Vx = (opcode & 0x0F00u) >> 8u;

    for (int i {}; i < Vx; ++i)
    {
        registers[i] = memory[index + i];
    }
}
