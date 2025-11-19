/*
  Portable Snake Game in C
  - Works on Windows (MinGW / MSVC) and POSIX (Linux / macOS)
  - Controls: W (up), A (left), S (down), D (right)
  - Pause: p, Quit: q
  - Fruit character: 'F', Head: 'O', Body: 'o'
  - Compile:
      On Linux/macOS: gcc -O2 -o snake snake.c
      On Windows (MinGW): gcc -O2 -o snake.exe snake.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#ifdef _WIN32
  #include <conio.h>
  #include <windows.h>
  #define CLEAR_SCREEN() system("cls")
  #define SLEEP_MS(ms) Sleep(ms)
#else
  #include <termios.h>
  #include <unistd.h>
  #include <sys/time.h>
  #define CLEAR_SCREEN() printf("\x1b[2J\x1b[H")
  static struct termios oldt;
  static void enable_raw_mode() {
    struct termios newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN] = 0;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  }
  static void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  }
  static int kbhit(void) {
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO+1, &fds, NULL, NULL, &tv) > 0;
  }
  static int getch_nonblock() {
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1) return c;
    return -1;
  }
  #define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

/* Game configuration (easy to tweak) */
#define DEFAULT_WIDTH 30
#define DEFAULT_HEIGHT 20
#define MAX_SNAKE (DEFAULT_WIDTH * DEFAULT_HEIGHT)
#define STARTING_LENGTH 4
#define INITIAL_DELAY_MS 200   /* lower = faster */
#define MIN_DELAY_MS 50

typedef enum { UP, DOWN, LEFT, RIGHT } Direction;

typedef struct {
  int x, y;
} Point;

/* Game state */
int width = DEFAULT_WIDTH;
int height = DEFAULT_HEIGHT;
Point snake[MAX_SNAKE];
int snake_len;
Direction dir;
Point fruit;
int score;
int delay_ms;
int level;
int game_over = 0;

/* Helpers */
int point_equal(Point a, Point b) { return a.x == b.x && a.y == b.y; }

void place_fruit() {
  int tries = 0;
  while (1) {
    fruit.x = rand() % width;
    fruit.y = rand() % height;
    int coll = 0;
    for (int i = 0; i < snake_len; ++i)
      if (point_equal(snake[i], fruit)) { coll = 1; break; }
    if (!coll) return;
    if (++tries > 10000) return; /* safety */
  }
}

void init_game() {
  score = 0;
  level = 1;
  delay_ms = INITIAL_DELAY_MS;
  snake_len = STARTING_LENGTH;
  /* place snake in center moving right */
  int cx = width / 2;
  int cy = height / 2;
  for (int i = 0; i < snake_len; ++i) {
    snake[i].x = cx - i;
    snake[i].y = cy;
  }
  dir = RIGHT;
  place_fruit();
  game_over = 0;
}

/* Draw the game board */
void draw() {
  CLEAR_SCREEN();
  /* Top border */
  for (int i = 0; i < width + 2; ++i) putchar('#');
  putchar('\n');

  for (int y = 0; y < height; ++y) {
    putchar('#');
    for (int x = 0; x < width; ++x) {
      char ch = ' ';
      Point p = {x, y};
      if (point_equal(p, snake[0])) ch = 'O';     /* head */
      else if (point_equal(p, fruit)) ch = 'F';   /* fruit */
      else {
        for (int k = 1; k < snake_len; ++k) {
          if (point_equal(p, snake[k])) { ch = 'o'; break; }
        }
      }
      putchar(ch);
    }
    putchar('#');
    putchar('\n');
  }

  /* Bottom border */
  for (int i = 0; i < width + 2; ++i) putchar('#');
  putchar('\n');

  printf("Score: %d   Length: %d   Level: %d   Delay: %d ms\n", score, snake_len, level, delay_ms);
  printf("Controls: W/A/S/D to move | p = pause | q = quit\n");
}

/* Move snake; returns 0 if collision (game over), 1 otherwise */
int step_snake() {
  Point new_head = snake[0];
  switch (dir) {
    case UP: new_head.y -= 1; break;
    case DOWN: new_head.y += 1; break;
    case LEFT: new_head.x -= 1; break;
    case RIGHT: new_head.x += 1; break;
  }

  /* Wall collision */
  if (new_head.x < 0 || new_head.x >= width || new_head.y < 0 || new_head.y >= height)
    return 0;

  /* Self collision */
  for (int i = 0; i < snake_len; ++i)
    if (point_equal(new_head, snake[i])) return 0;

  /* Move body */
  for (int i = snake_len; i > 0; --i)
    snake[i] = snake[i-1];
  snake[0] = new_head;

  /* Fruit? */
  if (point_equal(new_head, fruit)) {
    snake_len++;
    score += 10;
    place_fruit();
    /* speed up every 3 fruits */
    if (snake_len % 3 == 0 && delay_ms > MIN_DELAY_MS) {
      delay_ms -= 10;
      level++;
    }
    if (snake_len >= MAX_SNAKE-1) {
      /* filled board: win - we'll just stop */
      return 0;
    }
  } else {
    /* normal move: drop tail */
    /* we already shifted; just reduce the length effect by not increasing snake_len */
    /* to keep length, ensure we don't overflow; all fine */
  }
  return 1;
}

void change_direction(Direction newd) {
  /* prevent reversing directly */

 if ((dir == UP && newd == DOWN) || (dir == DOWN && newd == UP) ||
      (dir == LEFT && newd == RIGHT) || (dir == RIGHT && newd == LEFT)) return;
  dir = newd;
}

/* Platform-independent input poll */
int poll_input() {
#ifdef _WIN32
  if (!_kbhit()) return 0;
  int ch = _getch();
  /* arrow keys on windows: return 0 or 224 then code; we ignore arrows to keep it simple */
  if (ch == 0 || ch == 224) {
    int ch2 = _getch(); (void)ch2; return 0;
  }
  ch = tolower(ch);
  if (ch == 'w') change_direction(UP);
  else if (ch == 's') change_direction(DOWN);
  else if (ch == 'a') change_direction(LEFT);
  else if (ch == 'd') change_direction(RIGHT);
  else if (ch == 'q') return 'q';
  else if (ch == 'p') return 'p';
  return 1;
#else
  if (!kbhit()) return 0;
  int c = getch_nonblock();
  if (c == -1) return 0;
  c = tolower(c);
  if (c == 'w') change_direction(UP);
  else if (c == 's') change_direction(DOWN);
  else if (c == 'a') change_direction(LEFT);
  else if (c == 'd') change_direction(RIGHT);
  else if (c == 'q') return 'q';
  else if (c == 'p') return 'p';
  return 1;
#endif
}

int main(int argc, char **argv) {
  /* optional width/height from command line: snake.exe 30 20 */
  if (argc >= 3) {
    int w = atoi(argv[1]);
    int h = atoi(argv[2]);
    if (w >= 10 && h >= 5 && w*h <= 10000) { width = w; height = h; }
  }

  srand((unsigned int)time(NULL));

#ifndef _WIN32
  enable_raw_mode();
#endif

  init_game();

  draw();
  printf("Press any key to start... (W/A/S/D to control). \n");
#ifdef _WIN32
  while (!_kbhit()) Sleep(10);
  _getch();
#else
  while (!kbhit()) usleep(10000);
  getch_nonblock();
#endif

  /* main loop */
  while (!game_over) {
    int input = poll_input();
    if (input == 'q') break;
    if (input == 'p') {
      /* pause */
      printf("Paused. Press 'p' again to resume.\n");
      while (1) {
        int i = poll_input();
        if (i == 'p') break;
        SLEEP_MS(50);
      }
    }

    int alive = step_snake();
    if (!alive) { game_over = 1; break; }

    draw();
    SLEEP_MS(delay_ms);
  }

  CLEAR_SCREEN();
  printf("Game Over!\n");
  printf("Final score: %d\n", score);
  printf("Final length: %d\n", snake_len);
  printf("Level reached: %d\n", level);

#ifndef _WIN32
  disable_raw_mode();
#endif

  return 0;
}
