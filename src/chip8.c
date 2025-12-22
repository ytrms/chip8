#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define RAM_SIZE 4096
#define STACK_DEPTH 16
#define CPU_HZ 700

#define FONT_SIZE 80
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SCREEN_MULTIPLIER 10

#define ROM_START_ADDRESS 0x200

// Defining a byte as being 8 bit
typedef uint8_t BYTE;
typedef uint16_t ADDRESS;

// Allocating RAM
BYTE ram[RAM_SIZE];

// Creating the stack
ADDRESS stack[STACK_DEPTH];
// Stack pointer that points at the next free slot in the stack
int8_t stack_pointer = 0;

// Creating timers
BYTE delay_timer = 0;
BYTE sound_timer = 0;

// Initialize program counter and index register
ADDRESS pc =
    ROM_START_ADDRESS; // Assuming that the ROM starts at this RAM addre
ADDRESS I = 0;

// Initializing registers
BYTE registers[16]; // 0-F

// Creating the "pixel" arrays
bool pixels[SCREEN_HEIGHT][SCREEN_WIDTH];

int push_to_stack(ADDRESS address)
{
  if (stack_pointer >= STACK_DEPTH)
  {
    perror("Stack overflow.");
    return 1;
  }

  stack[stack_pointer] = address;
  stack_pointer += 1;

  return 0;
}

ADDRESS pop_from_stack()
{
  if (stack_pointer == 0)
  {
    perror("Stack underflow.");
    // TODO: It doesn't make sense to return exit_failure here. Find something
    // else. (e.g., they are both fine and valid addresses, both returns)
    return 1;
  }

  stack_pointer -= 1;
  ADDRESS address = stack[stack_pointer];

  return address;
}

int load_rom_to_ram(char *filename)
{
  FILE *rom = fopen(filename, "rb");

  if (!rom)
  {
    perror("ROM not found.");
    return 1;
  }

  // int rom_length = GetFileLength(filename);

  int file_byte;
  int i = 0;
  // Reading the file one byte at a time into byte
  while ((file_byte = fgetc(rom)) != EOF)
  {
    // TODO: Risky - if the ROM is too large, you'll go oob. do ram size - rom
    // start addr to check
    ram[pc + i] = file_byte;
    i++;
  }

  fclose(rom);
  return 0;
}

int dump_ram(void)
{
  FILE *ram_dump = fopen("ram_dump.bin", "w");
  if (!ram_dump)
  {
    perror("Failed creating the file ram_dump.bin");
    return 1;
  }

  size_t num_bytes_dumped = fwrite(ram, sizeof ram[0], RAM_SIZE, ram_dump);
  printf("Dumped %zu bytes out of %d requested.", num_bytes_dumped, RAM_SIZE);
  fclose(ram_dump);

  if (num_bytes_dumped == RAM_SIZE)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

int reset_ram(void)
{
  // Blank out memory
  for (int i = 0; i < RAM_SIZE; i++)
  {
    ram[i] = 0;
  }

  return 0;
}

void burn_font_to_ram(void)
{
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

  for (int i = 0; i < FONT_SIZE; i++)
  {
    ram[font_area_start_address + i] = font[i];
  }
}

void init_ram(void)
{
  // 0x000 - 0x1FF are reserved by the interpreter. 0x200+ are for the ROM.
  reset_ram();
  burn_font_to_ram();
  // TODO: Many things to do here to init. Zero the timers, reset PC and I,
  // registers, stack_pointer, pixels. basically make it so that you can run it
  // while the emu is running.
}

void decrease_timers(void)
{
  if (sound_timer > 0)
  {
    sound_timer--;
  }

  if (delay_timer > 0)
  {
    delay_timer--;
  }
}

void paint_pixel_at_virtual_location(int x, int y)
{
  DrawRectangle(x * SCREEN_MULTIPLIER, y * SCREEN_MULTIPLIER,
                SCREEN_MULTIPLIER - 1, // -1 to get a "grid"
                SCREEN_MULTIPLIER - 1, RAYWHITE);
}

uint16_t fetch_instruction(void)
{
  // Instructions are 16 bit
  uint16_t instruction_high = ram[pc];
  uint8_t instruction_low = ram[pc + 1];

  uint16_t instruction = (instruction_high << 8) | instruction_low;

  // SIDE EFFECT: We also increase the PC by 2 to point it at the next
  // instruction.
  pc += 0x002;
  return instruction;
}

void clear_background(void)
{
  for (int i = 0; i < SCREEN_HEIGHT; i++)
  {
    for (int n = 0; n < SCREEN_WIDTH; n++)
    {
      pixels[i][n] = false;
    }
  }
}

void draw_screen(void)
{
  // Draws rectangles on screen based on the "pixels" matrix
  for (int i = 0; i < SCREEN_HEIGHT; i++)
  {
    for (int n = 0; n < SCREEN_WIDTH; n++)
    {
      if (pixels[i][n])
      {
        DrawRectangle(n * SCREEN_MULTIPLIER, i * SCREEN_MULTIPLIER,
                      SCREEN_MULTIPLIER - 1, SCREEN_MULTIPLIER - 1, RAYWHITE);
      }
      else
      {
        DrawRectangle(n * SCREEN_MULTIPLIER, i * SCREEN_MULTIPLIER,
                      SCREEN_MULTIPLIER - 1, SCREEN_MULTIPLIER - 1, BLACK);
      }
    }
  }
}

void decode_execute_instruction(uint16_t instruction)
{
  switch (instruction & 0xF000)
  {
  case 0x0000:
  {
    // 00E0 - CLS
    if (instruction == 0x00E0)
    {
      clear_background();
      break;
    }
    else if (instruction == 0x00EE)
    {
      /*
      We have received the command to exit the subroutine we're in.
      Let's pop the address from the stack and set the PC to it, so we can
      resume execution.
      */
      pc = pop_from_stack();
      break;
    }
  }

  case 0x1000:
  {
    // JMP (set pc to nnn)
    pc = (instruction & 0x0FFF);
    break;
  }

  case 0x2000:
  {
    /*
    The PC points at the instruction to execute.

    In a normal jump, we move the PC to the new instruction, and that's it.

    As this is a jump to subroutine though, this means that at some point we
    will exit it and resume execution.

    This is where the stack comes into play.

    Before moving the PC to the new location, we push the current location of
    the PC to the stack. Then, when we return from the subroutine (another
    opcode), we resume execution as normal.

    So here, just push the current PC to the stack and then jump to the
    subroutine's address.
    */
    push_to_stack(pc);
    pc = (instruction & 0x0FFF);

    break;
  }

  case 0x3000:
  {
    // 3XNN - Skip one instruction if value in VX == NN
    int vx_value = registers[(instruction & 0x0F00) >> 8];
    int nn = (instruction & 0x00FF);

    if (vx_value == nn)
    {
      pc += 2;
    }

    break;
  }

  case 0x4000:
  {
    // 4XNN - Skip one instruction if value in VX != NN
    int vx_value = registers[(instruction & 0x0F00) >> 8];
    int nn = (instruction & 0x00FF);

    if (vx_value != nn)
    {
      pc += 2;
    }

    break;
  }

  case 0x5000:
  {
    // 5XY0 - Skip one instruction if the value in VX == value in Vy
    int vx_value = registers[(instruction & 0x0F00) >> 8];
    int vy_value = registers[(instruction & 0x00F0) >> 4];

    if (vx_value == vy_value)
    {
      pc += 2;
    }

    break;
  }

  case 0x6000:
  {
    // Assuming 6XNN: Set register VX to NN
    registers[(instruction & 0x0F00) >> 8] = instruction & 0x00FF;
    break;
  }

  case 0x7000:
  {
    // 7XNN: Add NN to VX
    registers[(instruction & 0x0F00) >> 8] += instruction & 0x00FF;
    break;
  }

  case 0x8000:
  {
    /*
    There's a ton of opcodes starting with 8 - they are differentiated by
    their last hex digit. So we get that to have a nested switch.
    */
    int opflag = (instruction & 0x000F);
    int x = (instruction & 0x0F00) >> 8;
    int y = (instruction & 0x00F0) >> 4;

    switch (opflag)
    {
    case 0x0000:
    {
      // 8XY0: VX is set to the value of VY
      registers[x] = registers[y];
      break;
    }

    case 0x0001:
    {
      // 8XY1: VX is set to the bitwise OR or VX and VY
      registers[x] = (registers[x] | registers[y]);
      break;
    }

    case 0x0002:
    {
      // 8XY2: VX is set to the bitwise AND or VX and VY
      registers[x] = (registers[x] & registers[y]);
      break;
    }

    case 0x0003:
    {
      // 8XY3: VX is set to the bitwise XOR or VX and VY
      registers[x] = (registers[x] ^ registers[y]);
      break;
    }

    case 0x0004:
    {
      /*
      8XY4: The value of VX is set to the value of VX + value of XY
      If the result is larger than 255 (8 bits), only the lowest 8 bits are
      stored and VF is set to 1. Otherwise, VF is set to 0.
      */
      if (registers[x] + registers[y] > 0b1111)
      {
        // Result overflows. Only store lowest 8 bits
        registers[x] = (registers[x] + registers[y]) & 0b11111111; // I hope this is right
        registers[0xF] = 1;
        break;
      }
      else
      { // Result does not overflow
        registers[x] = registers[x] + registers[y];
        registers[0xF] = 0;
        break;
      }
    }

    case 0x0005:
    {
      // SUB Vx, Vy. If Vx > Vy -> VF = 1, otherwise VF = 0
      if (registers[x] > registers[y])
      {
        registers[0xF] = 1;
      }
      else
      {
        registers[0xF] = 0;
      }

      registers[x] = registers[x] - registers[y];
      break;
    }

    case 0x0006:
    {
      // SHR Vx {, Vy}
      /*
      Shift right. In case of odd numbers (last bit = 1), we set VF = 1

      Practically:
      If least-significant bit of Vx is 1, VF = 1. Otherwise, VF = 0.
      Then, shift Vx 1 to the right (floor divide by 2)
      */
      // WARNING: Different interpreters do this differently. Check docs.
      if ((registers[x] & 0b00000001) == 0b000000001) {
        registers[0xF] = 1;
      } else {
        registers[0xF] = 0;
      }
      registers[x] = (registers[x] >> 1);
      break;
    }

    case 0x0007: {
      // SUBN Vx, Vy
      if (registers[y] > registers[x])
      {
        registers[0xF] = 1;
      }
      else
      {
        registers[0xF] = 0;
      }

      registers[x] = registers[y] - registers[x];
      break;
    }

    case 0x000E:
    {
      // SHL Vx {, Vy}
      /*
      Shift left. If left-most bit is 1, set VF = 1, otherwise VF = 0.
      */
      // WARNING: Different interpreters do this differently. Check docs.
      if ((registers[x] & 0b10000000) == 0b10000000) {
        registers[0xF] = 1;
      } else {
        registers[0xF] = 0;
      }
      registers[x] = (registers[x] << 1);
      break;
    }
    }
    break;
  }

  case 0x9000:
  {
    // 9XY0: SNE Vx, Vy
    // Skip one instruction if value in VX != value in VY
    int vx_value = registers[(instruction & 0x0F00) >> 8];
    int vy_value = registers[(instruction & 0x00F0) >> 4];

    if (vx_value != vy_value)
    {
      pc += 2;
    }

    break;
  }

  case 0xA000:
  {
    // ANNN: LD I, addr
    // Set I to NNN
    I = instruction & 0x0FFF;
    break;
  }

  case 0xD000:
  {
    // DXYN: Draw sprite of N height from memory location at address I at X, Y

    // Get X and Y coords (if they overflow, they should wrap)
    int x = registers[(instruction & 0x0F00) >> 8] % SCREEN_WIDTH;
    int y = registers[(instruction & 0x00F0) >> 4] % SCREEN_HEIGHT;

    int sprite_height = instruction & 0x000F;

    // Reset collision flag
    registers[0xF] = 0;

    // For each sprite row
    for (int n = 0; n < sprite_height; n++)
    {
      uint8_t sprite_row = ram[I + n];
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
        bool pixel_to_draw = (sprite_row & mask) >> (7 - p);
        if (pixel_to_draw)
        {
          /*
          This pixel needs to be drawn on screen.
          We flip the pixel on screen.
          */
          pixels[(y + n) % SCREEN_HEIGHT][(x + p) % SCREEN_WIDTH] =
              !pixels[(y + n) % SCREEN_HEIGHT][(x + p) % SCREEN_WIDTH];

          // TODO: Set the collision register ONLY if a pixel goes from 1 -> 0
          // registers[0xF] = 1;
        }
      }
    }

    break;
  }

  default:
  {
    break;
  }
  }
}

int process_instruction(void)
{
  /*
  This should fetch, decode, and execute the instruction.
  */
  uint16_t instruction = 0;
  instruction = fetch_instruction();
  decode_execute_instruction(instruction);

  return 1;
}

int main(int argc, char *argv[])
{
  for (int i = 0; i < argc; i++)
  {
    printf("argv%d: %s\n", i, argv[i]);
  }

  if (argc < 2)
  {
    printf("Usage: chip8 <path_to_rom_file>\n");
    return 1;
  }

  init_ram();
  load_rom_to_ram(argv[1]);

  InitWindow(SCREEN_WIDTH * SCREEN_MULTIPLIER,
             SCREEN_HEIGHT * SCREEN_MULTIPLIER, "CHIP-8");

  SetTargetFPS(60);

  float timer_acc = 0;
  float cpu_acc = 0;
  float current_frame_time = 0;

  // pixels[3][3] = true;

  dump_ram();

  while (!WindowShouldClose())
  {
    BeginDrawing();
    current_frame_time = GetFrameTime();
    // DrawText(argv[1], 0, 0, 12, RAYWHITE);

    // Decrease timers once every 16ms
    timer_acc += current_frame_time;

    while (timer_acc >= (1.0f / 60.0f))
    {
      timer_acc -= (1.0f / 60.0f);
      decrease_timers();
    }

    // Process instructions at CPU_HZ
    cpu_acc += current_frame_time;

    while (cpu_acc >= (1.0f / CPU_HZ))
    {
      cpu_acc -= (1.0f / CPU_HZ);
      process_instruction();
    }

    /*
    // Convert timeElapsed to string
    char strTimeElapsed[16];
    snprintf(strTimeElapsed, 16, "%f", timeElapsed);
    DrawText(strTimeElapsed, 0, 0, 16, RAYWHITE);
    */
    ClearBackground(BLACK);
    draw_screen();

    EndDrawing();
  }

  CloseWindow();
  return 0;
}
