#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define RAM_SIZE 4096
#define STACK_DEPTH 16
#define CPU_HZ 700

#define FONT_SIZE 80
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SCREEN_MULTIPLIER 10

#define ROM_START_ADDRESS 0x200
#define FONT_START_ADDRESS 0x050

// Defining a byte as being 8 bit
typedef uint8_t BYTE;
typedef uint16_t ADDRESS;

BYTE ram[RAM_SIZE];

ADDRESS stack[STACK_DEPTH];
// Stack pointer that points at the next free slot in the stack
int8_t stack_pointer = 0;

BYTE delay_timer = 0;
BYTE sound_timer = 0;

ADDRESS pc = ROM_START_ADDRESS;
ADDRESS I = 0;

BYTE registers[16]; // 0-F

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
    exit(EXIT_FAILURE);
  }

  stack_pointer -= 1;
  ADDRESS address = stack[stack_pointer];

  return address;
}

/*
Takes the file provided as argument and loads it into ram starting at
ROM_START_ADDRESS.
*/
int load_rom_to_ram(char *filename)
{
  FILE *rom = fopen(filename, "rb");

  if (!rom)
  {
    perror("ROM not found.");
    return 1;
  }

  int file_byte;
  int i = 0;
  // Reading the file one byte at a time into byte
  while ((file_byte = fgetc(rom)) != EOF)
  {
    // TODO: Unsafe. If the ROM is too large, you'll go oob. do ram size - rom
    // start addr to check
    ram[pc + i] = file_byte;
    i++;
  }

  fclose(rom);
  return 0;
}

/*
Dumps the contents of the Chip-8 RAM to a file in the root folder.

IMPROVEMENT: Make the filename an argument.
*/
int dump_ram(void)
{
  FILE *ram_dump = fopen("ram_dump.bin", "w");
  if (!ram_dump)
  {
    perror("Failed creating the file ram_dump.bin");
    return 1;
  }

  size_t num_bytes_dumped = fwrite(ram, sizeof ram[0], RAM_SIZE, ram_dump);
  printf("Dumped %zu bytes out of %d requested.\n", num_bytes_dumped, RAM_SIZE);
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

/*
Zeroes out the Chip-8's RAM.

IMPROVEMENT: Make the ram array an argument. But remember how variables work in C, and how functions only alter a local version of what you pass in. So you may need some pointer stuff.

IMPROVEMENT: Don't rely on the global RAM_SIZE define, measure instead the size of the RAM from within the function.
*/
int reset_ram(void)
{
  // Blank out memory
  for (int i = 0; i < RAM_SIZE; i++)
  {
    ram[i] = 0;
  }

  return 0;
}

/*
Places font information in RAM, starting at address 0x050.

IMPROVEMENT: Pass the RAM in as argument. Again remember how C passes values.

IMPROVEMENT: Pass the start address in as argument.
*/
void burn_font_to_ram(void)
{
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
    ram[FONT_START_ADDRESS + i] = font[i];
  }
}

/*
Initializes the Chip-8 RAM, making it ready for execution.

TODO: Many things to do here to init. Zero the timers, reset PC and I, registers, stack_pointer, pixels. basically make it so that you can reset it while the emu is running.
*/
void init_ram(void)
{
  // 0x000 - 0x1FF are reserved by the interpreter. 0x200+ are for the ROM.
  reset_ram();
  burn_font_to_ram();
}

/*
Plays the given tone, unless the tone is already playing.
*/
void play_tone_if_not_already_playing(Sound tone)
{
  if (!IsSoundPlaying(tone))
  {
    PlaySound(tone);
  }
}

/*
Decreases by 1 the sound and delay timers.
*/
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

/*
Draws a Raylib rectangle of size (SCREEN_MULTIPLIER - 1) * (SCREEN_MULTIPLIER - 1) at the given x, y location of the Chip-8's display. x, y must be within the bounds of the Chip-8 display size.

IMPROVEMENT: Return an error upon receiving a non-valid x or y value.

IMPROVEMENT: Make the "black" line around the rectangles configurable as argument.
*/
void paint_pixel_at_virtual_location(int x, int y, Color color, int border_width)
{
  DrawRectangle(x * SCREEN_MULTIPLIER, y * SCREEN_MULTIPLIER,
                SCREEN_MULTIPLIER - border_width, // -1 to get a "grid"
                SCREEN_MULTIPLIER - border_width, color);
}

/*
Returns the instruction in RAM at the current PC. Then increases the PC by 2, so that it points to the next instruction.
*/
uint16_t fetch_instruction_and_increment_pc(void)
{
  // Instructions are 16 bit
  uint16_t instruction_high = ram[pc];
  uint8_t instruction_low = ram[pc + 1];

  uint16_t instruction = (instruction_high << 8) | instruction_low;

  pc += 0x002;
  return instruction;
}

/*
Sets all virtual pixels to 0.
*/
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

/*
Draws Raylib rectangles on screen based on the values of the virtual pixels.

Draws white rectangles if true, black if false.

IMPROVEMENT: Draw nothing if false.
*/
void draw_screen(void)
{
  // Draws rectangles on screen based on the "pixels" matrix
  for (int i = 0; i < SCREEN_HEIGHT; i++)
  {
    for (int n = 0; n < SCREEN_WIDTH; n++)
    {
      if (pixels[i][n])
      {
        paint_pixel_at_virtual_location(n, i, RAYWHITE, 0);
      }
      else
      {
        paint_pixel_at_virtual_location(n, i, BLACK, 0);
      }
    }
  }
}

/*
Executes the given instruction.

TODO: Add support for all instructions.
*/
void execute_instruction(uint16_t instruction)
{
  switch (instruction & 0xF000)
  {
  case 0x0000:
  {
    /*
    00E0 - CLS
    Clear the display.
    */
    if (instruction == 0x00E0)
    {
      clear_background();
      break;
    }
    /*
    00EE - RET
    Return from a subroutine.
    */
    else if (instruction == 0x00EE)
    {
      /*
      Pop the address from the stack and set the PC to it, so we can
      resume execution.
      */
      pc = pop_from_stack();
      break;
    }

    break;
  }

  case 0x1000:
  {
    /*
    nnn - JP addr
    Jump to location nnn.
    */
    pc = (instruction & 0x0FFF);
    break;
  }

  case 0x2000:
  {
    /*
    2nnn - CALL addr
    Call subroutine at nnn.
    */

    /*
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
    /*
    3xkk - SE Vx, byte
    Skip next instruction if Vx = kk
    */
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
    /*
    4xkk - SNE Vx, byte
    Skip next instruction if Vx != kk
    */
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
    /*
    5xy0 - SE Vx, Vy
    Skip next instruction if Vx = Vy
    */
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
    /*
    6xkk - LD Vx, byte
    Set Vx = kk
    */
    registers[(instruction & 0x0F00) >> 8] = instruction & 0x00FF;
    break;
  }

  case 0x7000:
  {
    /*
    7xkk - ADD Vx, byte
    Adds the value kk to the value of the register Vx, then stores the result in
    Vx.
    */
    registers[(instruction & 0x0F00) >> 8] += instruction & 0x00FF;
    break;
  }

  case 0x8000:
  {
    int opflag = (instruction & 0x000F);
    int x = (instruction & 0x0F00) >> 8;
    int y = (instruction & 0x00F0) >> 4;

    switch (opflag)
    {
    case 0x0000:
    {
      /*
      8XY0 - LD Vx, Vy
      Set Vx = Vy
      */
      registers[x] = registers[y];
      break;
    }

    case 0x0001:
    {
      /*
      8xy1 - OR Vx, Vy
      Set Vx = Vx OR Vy
      */
      registers[x] = (registers[x] | registers[y]);
      break;
    }

    case 0x0002:
    {
      /*
      8xy2 - AND Vx, Vy
      Set Vx = Vx AND Vy
      */
      registers[x] = (registers[x] & registers[y]);
      break;
    }

    case 0x0003:
    {
      /*
      8xy3 - XOR Vx, Vy
      Set Vx = Vx XOR Vy
      */
      registers[x] = (registers[x] ^ registers[y]);
      break;
    }

    case 0x0004:
    {
      /*
      8xy4 - ADD Vx, Vy
      Set Vx = Vx + Vy, set VF = carry.
      */

      /*
      The value of VX is set to the value of VX + value of XY
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
      /*
      8xy5 - SUB Vx, Vy
      Set Vx = Vx - Vy, set VF = NOT borrow.
      */

      // If Vx > Vy -> VF = 1, otherwise VF = 0
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
      /*
      8xy6 - SHR Vx {, Vy}
      Set Vx = Vx SHR 1.
      */

      /*
      Shift right. In case of odd numbers (last bit = 1), we set VF = 1

      Practically:
      If least-significant bit of Vx is 1, VF = 1. Otherwise, VF = 0.
      Then, shift Vx 1 to the right (floor divide by 2)
      */
      // WARNING: Different interpreters do this differently. Check docs.
      if ((registers[x] & 0b00000001) == 0b000000001)
      {
        registers[0xF] = 1;
      }
      else
      {
        registers[0xF] = 0;
      }
      registers[x] = (registers[x] >> 1);
      break;
    }

    case 0x0007:
    {
      /*
      8xy7 - SUBN Vx, Vy
      Set Vx = Vy - Vx, set VF = NOT borrow.
      */

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
      /*
      8xyE - SHL Vx {, Vy}
      Set Vx = Vx SHL 1.
      */

      /*
      Shift left. If left-most bit is 1, set VF = 1, otherwise VF = 0.
      */
      // WARNING: Different interpreters do this differently. Check docs.
      if ((registers[x] & 0b10000000) == 0b10000000)
      {
        registers[0xF] = 1;
      }
      else
      {
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
    /*
    9xy0 - SNE Vx, Vy
    Skip next instruction if Vx != Vy.
    */
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
    /*
    Annn - LD I, addr
    Set I = nnn.
    */
    I = instruction & 0x0FFF;
    break;
  }

  case 0xB000:
  {
    /*
    Bnnn - JP V0, addr
    Jump to location nnn + V0.
    */

    // WARNING: Implementations vary. Check documentation.
    pc = (instruction & 0x0FFF) + registers[0];
    break;
  }

  case 0xC000:
  {
    /*
    Cxkk - RND Vx, byte
    Set Vx = random byte AND kk.
    */
    int random_byte = 256;

    // I got this from cppreference. It's probably flawed in some way.
    while (random_byte > 255)
    {
      random_byte = 1 + rand() / ((RAND_MAX + 1u) / 255);
    }

    registers[(instruction & 0x0F00) >> 8] = random_byte & (instruction & 0x00FF);
    break;
  }

  case 0xD000:
  {
    /*
    Dxyn - DRW Vx, Vy, nibble
    Display n-byte sprite starting at memory location I at (Vx, Vy), set VF = collision.
    */

    // Draw sprite of N height from memory location at address I at X, Y

    // Get X and Y coords (if they overflow, they should wrap)
    int x = registers[(instruction & 0x0F00) >> 8] % SCREEN_WIDTH;
    int y = registers[(instruction & 0x00F0) >> 4] % SCREEN_HEIGHT;

    int sprite_height = (instruction & 0x000F);

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
          bool pixel_on_screen = pixels[(y + n) % SCREEN_HEIGHT][(x + p) % SCREEN_WIDTH];

          /*
          If pixel on screen is 1, and we flip it to 0, set VF to 1.
          Otherwise, set it to 0.
          */
          if (pixel_to_draw && pixel_on_screen)
          {
            registers[0xF] = 1;
          }
          else
          {
            registers[0xF] = 0;
          }

          /*
          This pixel needs to be drawn on screen.
          We flip the pixel on screen.
          */
          pixels[(y + n) % SCREEN_HEIGHT][(x + p) % SCREEN_WIDTH] = !pixel_on_screen;
        }
      }
    }

    break;
  }

  case 0xE000:
  {
    switch (instruction & 0x00FF)
    {
    case 0x009E:
    {
      /*
      Ex9E - SKP Vx
      Skip next instruction if key with the value of Vx is pressed.
      */

      /*
      Checks the keyboard, and if the key corresponding to the value of Vx is currently in the down position, PC is increased by 2.
      */

      // TODO: Implement
      break;
    }

    case 0x00A1:
    {
      /*
      ExA1 - SKNP Vx
      Skip next instruction if key with the value of Vx is not pressed.
      */

      /*
      Checks the keyboard, and if the key corresponding to the value of Vx is currently in the up position, PC is increased by 2.
      */

      // TODO: Implement

      break;
    }

    default:
      break;
    }
  }

  case 0xF000:
  {
    switch (instruction & 0x00FF)
    {
    case 0x0007:
    {
      /*
      Fx07 - LD Vx, DT
      Set Vx = delay timer value.

      The value of DT is placed into Vx.
      */
      registers[(instruction & 0x0F00) >> 8] = delay_timer;
      break;
    }

    case 0x000A:
    {
      /*
      Fx0A - LD Vx, K
      Wait for a key press, store the value of the key in Vx.

      All execution stops until a key is pressed, then the value of that key is stored in Vx.
      */
      // TODO: Implement
      break;
    }

    case 0x0015:
    {
      /*
      Fx15: LD DT, Vx
      Set delay timer = Vx.
      */

      delay_timer = registers[(instruction & 0x0F00) >> 8];

      break;
    }

    case 0x0018:
    {
      /*
      Fx18 - LD ST, Vx
      Set sound timer = Vx.
      */

      sound_timer = registers[(instruction & 0x0F00) >> 8];
      break;
    }

    case 0x001E:
    {
      /*
      Fx1E - ADD I, Vx
      Set I = I + Vx.
      */
      I = I + registers[(instruction & 0x0F00) >> 8];
      break;
    }

    case 0x0029:
    {
      /*
      Fx29 - LD F, Vx
      Set I = location of sprite for digit Vx.
      */

      // A single char takes this many bytes (font)
      int character_size_on_disk = 5;

      I = FONT_START_ADDRESS + (character_size_on_disk * (instruction & 0x0F00) >> 8);

      break;
    }

    case 0x0033:
    {
      /*
      Fx33 - LD B, Vx
      Store BCD representation of Vx in memory locations I, I+1, and I+2.
      */

      /*
      The interpreter takes the decimal value of Vx, and places the hundreds digit in memory at location in I, the tens digit at location I+1, and the ones digit at location I+2.
      */

      // TODO: Implement
      /*
      x is an eight-bit number, meaning from 0 to 255.
      taking the hundredths digit:
      - modulo by 100
      */
      int num = registers[(instruction & 0x0F00) >> 8];
      int hundredths_digit = floor(num / 100);
      int tens_digit = floor((num % 100) / 10);
      int singles_digit = num % 10;

      ram[I] = hundredths_digit;
      ram[I + 1] = tens_digit;
      ram[I + 2] = singles_digit;

      break;
    }

    case 0x0055:
    {
      /*
      Fx55 - LD [I], Vx
      Store registers V0 through Vx in memory starting at location I.
      */

      for (int j = 0; j <= ((instruction & 0x0F00) >> 8); j++)
      {
        ram[I + j] = registers[j];
      }

      break;
    }

    case 0x0065:
    {
      /*
      Fx65 - LD Vx, [I]
      Read registers V0 through Vx from memory starting at location I.
      */

      /*
      The interpreter reads values from memory starting at location I into registers V0 through Vx.
      */

      for (int j = 0; j <= ((instruction & 0x0F00) >> 8); j++)
      {
        registers[j] = ram[I + j];
      }

      break;
    }

    default:
      break;
    }
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
  instruction = fetch_instruction_and_increment_pc();
  execute_instruction(instruction);

  return 1;
}

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    // User specified the wrong number of arguments
    printf("Usage: chip8 <path_to_rom_file>\n");
    return 1;
  }

  // MEMORY INIT
  init_ram();
  load_rom_to_ram(argv[1]);
  dump_ram();

  // VIDEO INIT
  InitWindow(SCREEN_WIDTH * SCREEN_MULTIPLIER,
             SCREEN_HEIGHT * SCREEN_MULTIPLIER, "CHIP-8");

  SetTargetFPS(60);

  // AUDIO INIT
  InitAudioDevice();
  Wave tone_wave = LoadWave("assets/tone.wav");
  Sound the_tone = LoadSoundFromWave(tone_wave);

  float timer_acc = 0;
  float cpu_acc = 0;
  float current_frame_time = 0;

  while (!WindowShouldClose())
  {
    BeginDrawing();
    current_frame_time = GetFrameTime();

    // SOUND
    if (sound_timer > 0)
    {
      play_tone_if_not_already_playing(the_tone);
    }

    // TIMER MGMT
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
      int keyPressed = GetKeyPressed();
      if (keyPressed != 0)
      {
        printf("%d\n", keyPressed);
      }
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

  CloseAudioDevice();
  CloseWindow();
  return 0;
}
