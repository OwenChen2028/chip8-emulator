#include <fcntl.h>   // for open, O_RDONLY
#include <stdbool.h> // for bool, true, false
#include <stddef.h>  // for NULL
#include <stdint.h>  // for uint32_t
#include <stdio.h>   // for printf, fflush, stdout
#include <stdlib.h>  // for exit, EXIT_*, srand, rand
#include <time.h>    // for time
#include <unistd.h>  // for ssize_t, read, close

#include <SDL2/SDL.h> // for SDL_*

#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SCALE_FACTOR 10

#define CYCLES_PER_FRAME 8
#define FRAMES_PER_SECOND 60

#define PROGRAM_ADDRESS 512
#define FONT_ADDRESS 0

unsigned char memory[4096]; // 4 KB

bool display[SCREEN_WIDTH * SCREEN_HEIGHT]; // black or white

unsigned short PC = PROGRAM_ADDRESS; // 16 bit
unsigned short I = 0;                // 16 bit

unsigned short stack[16]; // 16 bit
unsigned char stack_top = 0;

unsigned char delay_timer; // 8 bit
unsigned char sound_timer; // 8 bit

unsigned char V[16]; // 8 bit

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *screen;

uint32_t colors[2]; // black and white

bool key[16]; // defaults to false
short key_wait = -1;

unsigned char font[] = {
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

unsigned short opcode; // 16 bit

bool draw_flag = true;

void sdl_setup() {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    exit(EXIT_FAILURE);
  }

  window =
      SDL_CreateWindow("CHIP8", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH * SCALE_FACTOR,
                       SCREEN_HEIGHT * SCALE_FACTOR, SDL_WINDOW_SHOWN);

  if (window == NULL) {
    exit(EXIT_FAILURE);
  }

  renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  if (renderer == NULL) {
    exit(EXIT_FAILURE);
  }

  SDL_PixelFormat *format = SDL_AllocFormat(SDL_GetWindowPixelFormat(window));

  if (format == NULL) {
    exit(EXIT_FAILURE);
  }

  colors[0] = SDL_MapRGB(format, 0x00, 0x00, 0x00); // black
  colors[1] = SDL_MapRGB(format, 0xFF, 0xFF, 0xFF); // white

  SDL_FreeFormat(format);

  screen = SDL_CreateTexture(renderer, SDL_GetWindowPixelFormat(window),
                             SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH,
                             SCREEN_HEIGHT);

  if (screen == NULL) {
    exit(EXIT_FAILURE);
  }
}

void handle_events() {
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      SDL_DestroyTexture(screen);
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();

      exit(EXIT_SUCCESS);
    }

    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
      bool keydown = (event.type == SDL_KEYDOWN);

      switch (event.key.keysym.sym) {
      case SDLK_1:
        key[0x1] = keydown;
        break;
      case SDLK_2:
        key[0x2] = keydown;
        break;
      case SDLK_3:
        key[0x3] = keydown;
        break;
      case SDLK_4:
        key[0xC] = keydown;
        break;

      case SDLK_q:
        key[0x4] = keydown;
        break;
      case SDLK_w:
        key[0x5] = keydown;
        break;
      case SDLK_e:
        key[0x6] = keydown;
        break;
      case SDLK_r:
        key[0xD] = keydown;
        break;

      case SDLK_a:
        key[0x7] = keydown;
        break;
      case SDLK_s:
        key[0x8] = keydown;
        break;
      case SDLK_d:
        key[0x9] = keydown;
        break;
      case SDLK_f:
        key[0xE] = keydown;
        break;

      case SDLK_z:
        key[0xA] = keydown;
        break;
      case SDLK_x:
        key[0x0] = keydown;
        break;
      case SDLK_c:
        key[0xB] = keydown;
        break;
      case SDLK_v:
        key[0xF] = keydown;
        break;
      }
    }
  }
}

void fetch_opcode() {
  opcode = (memory[PC] << 8) | memory[PC + 1]; // 2 bytes
  PC += 2;
}

void decode_execute() {
  unsigned short X;
  unsigned short Y;

  unsigned char VX;
  unsigned char VY;

  switch (opcode & 0xF000) {
  case 0x0000:
    switch (opcode) {
    case 0x00E0: // clear screen
      draw_flag = true;
      for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        display[i] = false;
      }
      break;
    case 0x00EE: // return
      if (stack_top > 0) {
        stack_top--;
        PC = stack[stack_top];
      }
      break;
    }
    break;
  case 0x1000: // jump
    PC = opcode & 0x0FFF;
    break;
  case 0x2000: // call
    if (stack_top < 16) {
      stack[stack_top] = PC;
      stack_top++;
    }
    PC = opcode & 0x0FFF;
    break;
  case 0x3000: // skip if equals
    if (V[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF)) {
      PC += 2;
    }
    break;
  case 0x4000: // skip if not equals
    if (V[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF)) {
      PC += 2;
    }
    break;
  case 0x5000: // skip if equals register
    if (V[(opcode & 0x0F00) >> 8] == V[(opcode & 0x00F0) >> 4]) {
      PC += 2;
    }
    break;
  case 0x6000: // set register
    V[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
    break;
  case 0x7000: // add to register
    V[(opcode & 0x0F00) >> 8] += opcode & 0x00FF;
    break;
  case 0x8000:
    switch (opcode & 0x000F) {
    case 0x0000: // set to register
      V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4];
      break;
    case 0x0001: // binary or
      V[(opcode & 0x0F00) >> 8] |= V[(opcode & 0x00F0) >> 4];
      V[0xF] = 0;
      break;
    case 0x0002: // binary and
      V[(opcode & 0x0F00) >> 8] &= V[(opcode & 0x00F0) >> 4];
      V[0xF] = 0;
      break;
    case 0x0003: // binary xor
      V[(opcode & 0x0F00) >> 8] ^= V[(opcode & 0x00F0) >> 4];
      V[0xF] = 0;
      break;
    case 0x0004: // add registers
      X = (opcode & 0x0F00) >> 8;
      Y = (opcode & 0x00F0) >> 4;

      unsigned short sum = V[X] + V[Y]; // 2 bytes

      V[X] = sum & 0xFF;
      V[0xF] = sum > 255;

      break;
    case 0x0005: // subtract registers
      X = (opcode & 0x0F00) >> 8;
      Y = (opcode & 0x00F0) >> 4;

      VX = V[X];
      VY = V[Y];

      V[X] = VX - VY;
      V[0xF] = VX >= VY;

      break;
    case 0x0006: // right shift
      X = (opcode & 0x0F00) >> 8;
      Y = (opcode & 0x00F0) >> 4;

      VY = V[Y];

      V[X] = VY >> 1;
      V[0xF] = VY & 0x1;

      break;
    case 0x0007: // subtract registers reversed
      X = (opcode & 0x0F00) >> 8;
      Y = (opcode & 0x00F0) >> 4;

      VX = V[X];
      VY = V[Y];

      V[X] = VY - VX;
      V[0xF] = VY >= VX;

      break;
    case 0x000E: // left shift
      X = (opcode & 0x0F00) >> 8;
      Y = (opcode & 0x00F0) >> 4;

      VY = V[Y];

      V[X] = VY << 1;
      V[0xF] = VY >> 7;

      break;
    }
    break;
  case 0x9000: // skip if not equals register
    if (V[(opcode & 0x0F00) >> 8] != V[(opcode & 0x00F0) >> 4]) {
      PC += 2;
    }
    break;
  case 0xA000: // set index
    I = opcode & 0x0FFF;
    break;
  case 0xB000: // jump offset
    PC = (opcode & 0x0FFF) + V[0];
    break;
  case 0xC000: // random
    V[(opcode & 0x0F00) >> 8] = rand() & (opcode & 0x00FF);
    break;
  case 0xD000: // draw sprite
    draw_flag = true;

    VX = V[(opcode & 0x0F00) >> 8] % SCREEN_WIDTH;
    VY = V[(opcode & 0x00F0) >> 4] % SCREEN_HEIGHT;

    unsigned short N = opcode & 0x000F; // height

    V[0xF] = 0;

    for (int dy = 0; dy < N; dy++) {
      unsigned short row = memory[I + dy];

      int y = VY + dy;

      if (y >= SCREEN_HEIGHT) {
        break;
      }

      for (int dx = 0; dx < 8; dx++) {
        if (row >> (7 - dx) & 0x1) {
          int x = VX + dx;

          if (x >= SCREEN_WIDTH) {
            break;
          }

          int pos = (y * SCREEN_WIDTH + x);

          if (display[pos]) {
            V[0xF] = 1;
          }

          display[pos] ^= true;
        }
      }
    }

    break;
  case 0xE000:
    switch (opcode & 0x00FF) {
    case 0x009E: // skip if key
      if (key[V[(opcode & 0x0F00) >> 8]]) {
        PC += 2;
      }
      break;
    case 0x00A1: // skip if not key
      if (!key[V[(opcode & 0x0F00) >> 8]]) {
        PC += 2;
      }
      break;
    }
    break;
  case 0xF000:
    switch (opcode & 0x00FF) {
    case 0x0007: // set to delay timer
      V[(opcode & 0x0F00) >> 8] = delay_timer;
      break;
    case 0x000A: // get key
      if (key_wait != -1) {
        if (!key[V[key_wait]]) {
          key_wait = -1;
        } else {
          PC -= 2;
        }
      } else {
        bool key_found = false;

        for (int i = 0; i < 16; i++) {
          if (key[i]) {
            unsigned short X = (opcode & 0x0F00) >> 8;

            V[X] = i;

            key_wait = X;
            key_found = true;

            break;
          }
        }

        PC -= 2;
      }
      break;
    case 0x0015: // set delay timer
      delay_timer = V[(opcode & 0x0F00) >> 8];
      break;
    case 0x0018: // set sound timer
      sound_timer = V[(opcode & 0x0F00) >> 8];
      break;
    case 0x001E: // add to index
      I += V[(opcode & 0x0F00) >> 8];
      break;
    case 0x0029: // set to font char
      I = V[(opcode & 0x0F00) >> 8] * 5 + FONT_ADDRESS;
      break;
    case 0x0033: // bcd conversion
      unsigned char VX = V[(opcode & 0x0F00) >> 8];

      memory[I] = VX / 100;
      memory[I + 1] = (VX / 10) % 10;
      memory[I + 2] = VX % 10;

      break;
    case 0x0055: // store memory
      X = (opcode & 0x0F00) >> 8;

      for (int i = 0; i <= X; i++) {
        memory[I + i] = V[i];
      }

      I += X + 1; // spec dependent
      break;
    case 0x0065: // load memory
      X = (opcode & 0x0F00) >> 8;

      for (int i = 0; i <= X; i++) {
        V[i] = memory[I + i];
      }

      I += X + 1; // spec dependent
      break;
    }
    break;
  }
}

void handle_timers() {
  if (delay_timer > 0) {
    delay_timer--;
  }

  if (sound_timer > 0) {;
    printf("\a");
    fflush(stdout);
    sound_timer--;
  }
}

void draw_screen() {
  uint32_t *pixels;
  int pitch;

  SDL_LockTexture(screen, NULL, (void **)&pixels, &pitch);

  for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
    pixels[i] = colors[display[i]];
  }

  SDL_UnlockTexture(screen);
  SDL_RenderCopy(renderer, screen, NULL, NULL);
  SDL_RenderPresent(renderer);
}

void game_loop() {
  int duration = 1000 / FRAMES_PER_SECOND;

  while (true) {
    uint32_t start = SDL_GetTicks();

      for (int i = 0; i < CYCLES_PER_FRAME; i++) {
        handle_events();

        fetch_opcode();
        decode_execute();
      }

      handle_timers();

    if (draw_flag) {
      draw_screen();
      draw_flag = false;
    }

    uint32_t time = SDL_GetTicks() - start;
    if (time < duration) {
      SDL_Delay(duration - time);
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc >= 2) {
    int fd = open(argv[1], O_RDONLY);

    char buffer[4096 - PROGRAM_ADDRESS];
    ssize_t bytes = read(fd, buffer, 4096 - PROGRAM_ADDRESS);

    if (bytes < 0) {
      exit(EXIT_FAILURE);
    }

    close(fd);

    for (ssize_t i = 0; i < bytes; i++) {
      memory[i + PROGRAM_ADDRESS] = buffer[i];
    }
  }

  sdl_setup();

  for (int i = 0; i < sizeof(font); i++) {
    memory[i + FONT_ADDRESS] = font[i];
  }

  srand(time(NULL));

  game_loop();
}
