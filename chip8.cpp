#include "chip8.h"

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
