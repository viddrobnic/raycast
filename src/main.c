#include "raylib.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WIDTH 1280
#define HEIGHT 720
#define SCALE 2

#define FLOOR_COLOR 0xff666666
#define CEIL_COLOR 0xff444444

#define CAMERA_WIDTH 1.0f

#define DARK_MASK 0xff7f7f7f

#define SPEED 1.8f
#define ROT_SPEED 1.8f

#define CONN_UP 0b0001
#define CONN_DOWN 0b0010
#define CONN_LEFT 0b0100
#define CONN_RIGHT 0b1000

#define MAZE_WIDTH 10
#define MAZE_HEIGHT 10

typedef struct {
  float x, y;
} Vec;

void shuffle(uint8_t *data, int size) {
  for (int n = size - 1; n > 0; n--) {
    int index = rand() % n;
    uint8_t temp = data[index];
    data[index] = data[n];
    data[n] = temp;
  }
}

void generateMaze(uint8_t *map, uint8_t *visited, int x, int y, int width,
                  int height) {
  uint8_t indices[4] = {0, 1, 2, 3};
  shuffle(indices, 4);

  int idx = y * width + x;

  int dir, dir2;
  for (int i = 0; i < 4; i++) {
    int dx, dy;
    if (indices[i] == 0) {
      dx = -1;
      dy = 0;
      dir = CONN_LEFT;
      dir2 = CONN_RIGHT;
    } else if (indices[i] == 1) {
      dx = 1;
      dy = 0;
      dir = CONN_RIGHT;
      dir2 = CONN_LEFT;
    } else if (indices[i] == 2) {
      dx = 0;
      dy = -1;
      dir = CONN_UP;
      dir2 = CONN_DOWN;
    } else if (indices[i] == 3) {
      dx = 0;
      dy = 1;
      dir = CONN_DOWN;
      dir2 = CONN_UP;
    } else {
      dx = 0;
      dy = 0;
      dir = 0;
      dir2 = 0;
    }

    int x2 = x + dx;
    int y2 = y + dy;

    if (x2 < 0 || x2 >= width || y2 < 0 || y2 >= height) {
      continue;
    }

    int idx2 = y2 * width + x2;

    if (visited[idx2]) {
      continue;
    }
    visited[idx2] = 1;

    map[idx] |= dir;
    map[idx2] |= dir2;

    generateMaze(map, visited, x2, y2, width, height);
  }
}

void convertMazeToMapData(uint8_t *maze, uint8_t *map, int width, int height) {
  int mapW = 2 * width + 1;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      if ((maze[y * width + x] & CONN_DOWN) == 0) {
        map[(y * 2 + 2) * mapW + (x * 2 + 1)] = 1;
      }
      if ((maze[y * width + x] & CONN_RIGHT) == 0) {
        map[(y * 2 + 1) * mapW + (x * 2 + 2)] = 1;
      }

      map[(y * 2 + 2) * mapW + (x * 2 + 2)] = 1;
    }
  }

  for (int x = 0; x < 2 * width + 1; x++) {
    map[x] = 1;
    map[2 * height * mapW + x] = 1;
  }

  for (int y = 0; y < 2 * height + 1; y++) {
    map[y * mapW] = 1;
    map[y * mapW + 2 * width] = 1;
  }
}

void printMap(uint8_t *map, int width, int height) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      if (map[y * width + x]) {
        printf("#");
      } else {
        printf(".");
      }
    }
    printf("\n");
  }
}

void drawColumn(uint32_t *pixels, int column, int height, uint32_t color) {
  int startY = HEIGHT / 2 - height / 2;
  int endY = startY + height;

  for (int y = startY; y < endY; y++) {
    pixels[y * WIDTH + column] = color;
  }
}

int idxDir(float comp) {
  if (comp < 0) {
    return -1;
  } else if (comp > 0) {
    return 1;
  } else {
    return 0;
  }
}

// x(t) = xRem + ray.x * t
// y(t) = yRem + ray.y * t
//
// 0 = xRem + ray.x * t
// => -xRem / ray.x = t
//
// 1 = xRem + ray.x * t
// => (1 - xRem) / ray.x = t
float dt(float rem, float comp) {
  if (comp < 0) {
    return -rem / comp;
  }

  if (comp > 0) {
    return (1.0 - rem) / comp;
  }

  return INFINITY;
}

float length(Vec pos, Vec dir, Vec point) {
  Vec pos2 = {
      .x = pos.x + dir.x,
      .y = pos.y + dir.y,
  };

  float num = (pos2.y - pos.y) * point.x - (pos2.x - pos.x) * point.y +
              pos2.x * pos.y - pos2.y * pos.x;
  if (num < 0) {
    num *= -1;
  }

  float denum = sqrtf(dir.y * dir.y + dir.x * dir.x);

  return num / denum;
}

void renderSprite(uint32_t *pixels, Vec pos, float rot, Vec spritePos) {
  int height = 50;
  int width = 50;
  float sY = 0.5; // ceiling offset [0, 1]

  Vec cameraDir = {.x = cosf(rot), .y = sinf(rot)};
  Vec cameraPlane = {.x = -cameraDir.y, .y = cameraDir.x};

  Vec s = {
      .x = spritePos.x - pos.x,
      .y = spritePos.y - pos.y,
  };

  float scalar = cameraDir.x * s.x + cameraDir.y * s.y;
  if (scalar <= 0) {
    return;
  }

  float den = s.x * cameraPlane.y - s.y * cameraPlane.x;
  if (den == 0) {
    return;
  }

  float num = s.y * cameraDir.x - s.x * cameraDir.y;

  float cameraX = num / den;
  if (cameraX < -CAMERA_WIDTH || cameraX > CAMERA_WIDTH) {
    return;
  }

  int w = floorf((cameraX + CAMERA_WIDTH) / 2.0 / CAMERA_WIDTH * WIDTH);

  float len = length(pos, cameraPlane, spritePos);
  int h = HEIGHT / len;
  if (h > HEIGHT) {
    h = HEIGHT;
  }

  int rW = width / len;
  int rH = height / len;

  int startW = w - rW / 2;
  if (startW < 0) {
    startW = 0;
  }
  int endW = startW + rW;
  if (endW >= WIDTH) {
    endW = WIDTH - 1;
  }

  int startY = HEIGHT / 2 - h / 2 + (int)(h * sY);
  if (startY < 0) {
    startY = 0;
  }
  int endY = startY + rH;
  if (endY >= HEIGHT) {
    endY = HEIGHT - 1;
  }

  for (int px = startW; px < endW; px++) {
    for (int py = startY; py < endY; py++) {
      pixels[py * WIDTH + px] = 0xff00ff00;
    }
  }
}

void drawFloor(uint32_t *pixels, Vec pos, float rot) {
  Vec cameraDir = {.x = cosf(rot), .y = sinf(rot)};
  Vec cameraPlane = {.x = -cameraDir.y, .y = cameraDir.x};

  for (int y = 0; y < HEIGHT / 2; y += SCALE) {
    float cameraFactor = 1.0 - 2.0 * y / HEIGHT;
    float t = 1.0 / cameraFactor;

    Vec left = {
        .x = pos.x + t * (cameraDir.x - CAMERA_WIDTH * cameraPlane.x),
        .y = pos.y + t * (cameraDir.y - CAMERA_WIDTH * cameraPlane.y),
    };
    Vec right = {
        .x = pos.x + t * (cameraDir.x + CAMERA_WIDTH * cameraPlane.x),
        .y = pos.y + t * (cameraDir.y + CAMERA_WIDTH * cameraPlane.y),
    };

    Vec diff = {
        .x = right.x - left.x,
        .y = right.y - left.y,
    };

    for (int x = 0; x < WIDTH; x += SCALE) {
      float step = (float)x / WIDTH;
      Vec pos = {
          .x = left.x + step * diff.x,
          .y = left.y + step * diff.y,
      };

      uint32_t floorC = FLOOR_COLOR;
      uint32_t ceilC = CEIL_COLOR;
      int xI = pos.x;
      int yI = pos.y;
      if ((xI + yI) % 2 == 0) {
        floorC = CEIL_COLOR;
        ceilC = FLOOR_COLOR;
      }

      // Draw ceil
      for (int i = 0; i < SCALE; i++) {
        for (int j = 0; j < SCALE; j++) {
          pixels[(y + i) * WIDTH + x + j] = floorC;

          // Draw floor
          pixels[(HEIGHT - (y + i) - 1) * WIDTH + x + j] = floorC;
        }
      }
    }
  }
}

void render(uint32_t *pixels, Vec pos, float rot, uint8_t *mapData, int mapW) {
  Vec cameraDir = {.x = cosf(rot), .y = sinf(rot)};
  Vec cameraPlane = {.x = -cameraDir.y, .y = cameraDir.x};

  int xIdxPlayer = floorf(pos.x);
  int yIdxPlayer = floorf(pos.y);
  float xRemPlayer = pos.x - floorf(pos.x);
  float yRemPlayer = pos.y - floorf(pos.y);

  for (int x = 0; x < WIDTH; x += SCALE) {
    float cameraFactor = CAMERA_WIDTH * (2.0 * x / WIDTH - 1.0);
    Vec cameraPoint = {
        .x = cameraPlane.x * cameraFactor,
        .y = cameraPlane.y * cameraFactor,
    };
    Vec ray = {
        .x = cameraDir.x + cameraPoint.x,
        .y = cameraDir.y + cameraPoint.y,
    };

    int xIdx = xIdxPlayer;
    int yIdx = yIdxPlayer;
    float xRem = xRemPlayer;
    float yRem = yRemPlayer;

    int dxIdxDir = idxDir(ray.x);
    int dyIdxDir = idxDir(ray.y);

    // Make sure to not take too many steps.
    int step;
    int hit = 0;
    for (step = 0; step < 100; step++) {
      float dtX = dt(xRem, ray.x);
      float dtY = dt(yRem, ray.y);

      int dxIdx = 0;
      int dyIdx = 0;
      float dt;
      if (dtX < dtY) {
        dt = dtX;
        dxIdx = dxIdxDir;
        hit = 1;
      } else {
        dt = dtY;
        dyIdx = dyIdxDir;
        hit = 2;
      }

      xIdx += dxIdx;
      yIdx += dyIdx;

      // rem = rem + ray * dt
      // If rem.x == 1.0 and ray.x > 0, set rem.x to 0 (we are in new tile)
      // If rem.x == 0.0 and ray.x < 0, set rem.x to 1.0 (we are in new tile)
      // This is done with -dxIdx
      xRem += ray.x * dt - dxIdx;
      yRem += ray.y * dt - dyIdx;

      if (mapData[yIdx * mapW + xIdx] > 0) {
        break;
      }
    }

    Vec collision = {
        .x = xIdx + xRem,
        .y = yIdx + yRem,
    };
    // Length between camera plane (line) and collision (point);
    float len = length(pos, cameraPlane, collision);

    int h = HEIGHT / len;
    if (h > HEIGHT) {
      h = HEIGHT;
    }

    float tCoord = 0;
    if (hit == 1) {
      tCoord = yRem;
    } else if (hit == 2) {
      tCoord = xRem;
    }

    uint8_t stripe = ((int)(tCoord * 10)) % 2;

    uint32_t color = 0xff0000ff;
    if (stripe) {
      color = 0xff00ffff;
    }

    if (hit == 2) {
      color = color & DARK_MASK;
    }

    for (int i = 0; i < SCALE; i++) {
      drawColumn(pixels, x + i, h, color);
    }
  }
}

void handleEvents(Vec *pos, float *rot, uint8_t *mapData, int mapW) {
  float t = GetFrameTime();

  if (IsKeyDown(KEY_A)) {
    *rot = fmod(*rot - ROT_SPEED * t, 2 * PI);
  } else if (IsKeyDown(KEY_D)) {
    *rot = fmod(*rot + ROT_SPEED * t, 2 * PI);
  }

  Vec dir = {
      .x = cosf(*rot) * SPEED * t,
      .y = sinf(*rot) * SPEED * t,
  };
  Vec newPos = *pos;
  if (IsKeyDown(KEY_W)) {
    newPos.x = pos->x + dir.x;
    newPos.y = pos->y + dir.y;
  } else if (IsKeyDown(KEY_S)) {
    newPos.x = pos->x - dir.x;
    newPos.y = pos->y - dir.y;
  }

  // check if x can be changed
  int idx = ((int)pos->y) * mapW + ((int)newPos.x);
  if (!mapData[idx]) {
    pos->x = newPos.x;
  }

  // check if y can be changed
  idx = ((int)newPos.y) * mapW + ((int)pos->x);
  if (!mapData[idx]) {
    pos->y = newPos.y;
  }
}

void generateMapData(uint8_t *mapData) {
  srand(time(NULL));

  uint8_t maze[MAZE_WIDTH * MAZE_HEIGHT];
  memset(maze, 0, MAZE_WIDTH * MAZE_HEIGHT);

  uint8_t visited[MAZE_WIDTH * MAZE_HEIGHT];
  memset(visited, 0, MAZE_WIDTH * MAZE_HEIGHT);
  visited[0] = 1;
  generateMaze(maze, visited, 0, 0, MAZE_WIDTH, MAZE_HEIGHT);

  const int mapW = 2 * MAZE_WIDTH + 1;
  const int mapH = 2 * MAZE_HEIGHT + 1;
  memset(mapData, 0, mapW * mapH);
  convertMazeToMapData(maze, mapData, MAZE_WIDTH, MAZE_HEIGHT);
  printMap(mapData, mapW, mapH);
}

int main(void) {
  const int mapW = 2 * MAZE_WIDTH + 1;
  const int mapH = 2 * MAZE_HEIGHT + 1;
  uint8_t mapData[mapW * mapH];
  generateMapData(mapData);

  InitWindow(WIDTH, HEIGHT, "Maze Runner");

  /* SetTargetFPS(60); */

  Vec pos = {1.0, 1.0};
  float rot = 0;

  uint32_t pixels[WIDTH * HEIGHT];
  Image image = {.data = pixels,
                 .width = WIDTH,
                 .height = HEIGHT,
                 .mipmaps = 1,
                 .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  Texture2D texture = LoadTextureFromImage(image);

  int i = 0;
  while (!WindowShouldClose()) {
    if (i % 1000 == 0) {
      printf("fps: %d\n", GetFPS());
      i = 0;
    }
    handleEvents(&pos, &rot, mapData, mapW);
    drawFloor(pixels, pos, rot);
    render(pixels, pos, rot, mapData, mapW);
    renderSprite(pixels, pos, rot, (Vec){1.5, 1.5});

    UpdateTexture(texture, pixels);

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexture(texture, 0, 0, WHITE);
    EndDrawing();
  }

  UnloadTexture(texture);
  CloseWindow();

  return 0;
}
