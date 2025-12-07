#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>

#define RAM_SIZE 4096
#define FONT_SIZE 80
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SCREEN_MULTIPLIER 10

// Defining a byte as being 8 bit
typedef u_int8_t BYTE;

// Allocating RAM
BYTE ram[RAM_SIZE];

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
  reset_ram();
  burn_font_to_ram();
}

int main(int argc, char *argv[]) {
  init_ram();
  InitWindow(SCREEN_WIDTH * SCREEN_MULTIPLIER,
             SCREEN_HEIGHT * SCREEN_MULTIPLIER, "CHIP-8");

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLACK);
    EndDrawing();
  }

  CloseWindow();
  return EXIT_SUCCESS;
}
