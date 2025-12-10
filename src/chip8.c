#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define RAM_SIZE 4096
#define STACK_DEPTH 16
#define CPU_HZ 700

#define FONT_SIZE 80
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SCREEN_MULTIPLIER 10

// Defining a byte as being 8 bit
typedef uint8_t BYTE;
typedef uint16_t ADDRESS;

// Allocating RAM
BYTE ram[RAM_SIZE];

// Creating the stack
ADDRESS stack[STACK_DEPTH];
// Stack pointer that points at the next free slot in the stack
uint8_t stack_pointer = 0;

// Creating timers
BYTE delayTimer = 0;
BYTE soundTimer = 0;

// Initialize program counter and index register
ADDRESS pc = 0x200; // Assuming that the ROM starts at this RAM addre
ADDRESS I = 0;

// Initializing registers
BYTE registers[16]; // 0-F

// Creating the "pixel" arrays
bool pixels[SCREEN_HEIGHT][SCREEN_WIDTH];


int push_to_stack(ADDRESS address) {
  if (stack_pointer >= STACK_DEPTH) {
    perror("Stack overflow.");
    return EXIT_FAILURE;
  }

  stack[stack_pointer] = address;
  stack_pointer += 1;

  return EXIT_SUCCESS;
}

ADDRESS pop_from_stack() {
  if (stack_pointer < 0) {
    perror("Stack underflow.");
    return EXIT_FAILURE;
  }

  ADDRESS address = stack[stack_pointer];
  stack_pointer -= 1;

  return address;
}

int dump_ram(void) {
  FILE *ram_dump = fopen("ram_dump.bin", "w");
  if (!ram_dump) {
    perror("Failed creating the file ram_dump.bin");
    return EXIT_FAILURE;
  }

  size_t num_bytes_dumped = fwrite(ram, sizeof ram[0], RAM_SIZE, ram_dump);
  printf("Dumped %zu bytes out of %d requested.", num_bytes_dumped, RAM_SIZE);
  fclose(ram_dump);

  if (num_bytes_dumped == RAM_SIZE) {
    return EXIT_SUCCESS;
  } else {
    return EXIT_FAILURE;
  }
}

int reset_ram(void) {
  // Blank out memory
  for (int i = 0; i < RAM_SIZE; i++) {
    ram[i] = 0;
  }

  return EXIT_SUCCESS;
}

void burn_font_to_ram(void) {
  int font_area_start_address = 0x050;

  int font[FONT_SIZE] = {
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

  for (int i = 0; i < FONT_SIZE; i++) {
    ram[font_area_start_address + i] = font[i];
  }
}

void init_ram(void) {
  // 0x000 - 0x1FF are reserved by the interpreter. 0x200+ are for the ROM.
  reset_ram();
  burn_font_to_ram();
}

void decreaseTimers(void)
{
  if (soundTimer > 0)
  {
    soundTimer--;
  }

  if (delayTimer > 0)
  {
    delayTimer--;
  }
}

void paintPixelAtVirtualLocation(int x, int y) {
  DrawRectangle(
      x * SCREEN_MULTIPLIER,
      y * SCREEN_MULTIPLIER,
      SCREEN_MULTIPLIER - 1, // -1 to get a "grid"
      SCREEN_MULTIPLIER - 1,
      RAYWHITE);
}

uint16_t fetchInstruction(void) {
  uint16_t instruction = ram[pc];
  // SIDE EFFECT: We also increase the PC by 2 to point it at the next instruction.
  pc += 0x2;
  return instruction;
}

void clearBackground(void) {
  for (int i = 0; i < SCREEN_HEIGHT; i++)
  {
    for (int n = 0; n < SCREEN_WIDTH; n++)
    {
      pixels[i][n] = false;
    }
  }
}

void drawScreen(void) {
  // Draws rectangles on screen based on the "pixels" matrix
  for (int i = 0; i < SCREEN_HEIGHT; i++)
  {
    for (int n = 0; n < SCREEN_WIDTH; n++)
    {
      if (pixels[i][n]) {
        DrawRectangle(
          n * SCREEN_MULTIPLIER, 
          i * SCREEN_MULTIPLIER,
          SCREEN_MULTIPLIER - 1,
          SCREEN_MULTIPLIER -1,
          RAYWHITE
        );
      }
      else {
        DrawRectangle(
          n * SCREEN_MULTIPLIER,
          i * SCREEN_MULTIPLIER,
          SCREEN_MULTIPLIER - 1,
          SCREEN_MULTIPLIER - 1,
          BLACK
        );
      }
    }
  }
}

void decodeAndExecuteInstruction(uint16_t instruction) {
  switch (instruction&0x1000) {
    case 0x0000: {
      // Assuming 00E0 (for now), clear screen
      clearBackground();
      break;
    }

    case 0x1000: {
      // JMP (set pc to nnn)
      pc = instruction & 0x0FFF;
      break;
    }
  
    case 0x6000: {
      // Assuming 6XNN: Set register VX to NN
      registers[(instruction & 0x0F00) >> 8] = instruction & 0x00FF;
      break;
    }

    case 0x7000: {
      // 7XNN: Add NN to VX
      registers[(instruction & 0x0F00) >> 8] += instruction & 0x00FF;
      break;
    }

    case 0xA000: {
      // ANNN: Set I to NNN
      I = instruction & 0x0FFF;
      break;
    }

    case 0xD000: {
      // DXYN: Draw sprite of N height from memory location at address I at X, Y

      // Get X and Y coords (if they overflow, they should wrap)
      int x = registers[(instruction & 0x0F00) >> 8] % SCREEN_WIDTH;
      int y = registers[(instruction & 0x00F0) >> 4] % SCREEN_HEIGHT;

      int spriteHeight = instruction & 0x000F;

      // For each sprite row
      for (int n = 0; n < spriteHeight; n++)
      {
        uint8_t spriteRow = ram[I + n];
        // That's something like 11110000

        for (int p = 0; p < 8; p++)
        {
          // For each bit in the row, we get a single "pixel" (l to r)

          // Sliding mask to get one bit ("pixel") at a time
          uint8_t mask = 0b10000000 >> p;
          /*
          p: 0
          spriteRow: 11110000
          mask:      10000000

          pixelToDraw: 10000000 >> (7-p) --> 00000001
          */
          bool pixelToDraw = (spriteRow & mask) >> (7 - p);
          if (pixelToDraw) {
            /*
            This pixel needs to be drawn on screen.
            We flip the pixel on screen. And we set VF to 1 (collision)
            TODO: Clip sprites that go off screen. (right now they would probably segfault)
            */
            pixels[x + n][y + p] = !pixels[x + n][y + p];
            registers[0xF] = 1;
          }
        }
      }

      break;
    }

    default: {
      break;
    }
  }
}

int processInstruction(void) {
  /*
  TODO: Implement processing all instructions.
  This should fetch, decode, and execute the instruction.
  */
  uint16_t instruction = 0;
  instruction = fetchInstruction();
  decodeAndExecuteInstruction(instruction);

  return 1;
}

int main(int argc, char *argv[]) {

  if (argc < 2) {
    printf("Usage: chip8 <rom_file>\n");
    return EXIT_FAILURE;
  }

  init_ram();
  InitWindow(SCREEN_WIDTH * SCREEN_MULTIPLIER,
             SCREEN_HEIGHT * SCREEN_MULTIPLIER, "CHIP-8");

  SetTargetFPS(60);

  float timerAcc = 0;
  float cpuAcc = 0;
  float currentFrameTime = 0;

  pixels[3][3] = true;

  while (!WindowShouldClose()) {
    BeginDrawing();
    currentFrameTime = GetFrameTime();
    // DrawText(argv[1], 0, 0, 12, RAYWHITE);

    // Decrease timers once every 16ms
    timerAcc += currentFrameTime;

    while (timerAcc >= (1.0f / 60.0f)) {
      timerAcc -= (1.0f / 60.0f);
      decreaseTimers();
    }

    // Process instructions at CPU_HZ
    cpuAcc += currentFrameTime;

    while (cpuAcc >= (1.0f / CPU_HZ)) {
      cpuAcc -= (1.0f / CPU_HZ);
      // processInstruction();
    }

    /*
    // Convert timeElapsed to string
    char strTimeElapsed[16];
    snprintf(strTimeElapsed, 16, "%f", timeElapsed);
    DrawText(strTimeElapsed, 0, 0, 16, RAYWHITE);
    */
    ClearBackground(BLACK);
    drawScreen();

    EndDrawing();
  }

  CloseWindow();
  return EXIT_SUCCESS;
}
