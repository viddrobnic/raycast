#include "raylib.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 800
#define HEIGHT 450

#define FLOOR_COLOR 0xff666666
#define CEIL_COLOR 0xff444444

#define CAMERA_WIDTH 0.7f

#define MAP_WIDTH 8
#define MAP_HEIGHT 8

#define DARK_MASK 0xff7f7f7f

#define SPEED 1.5f
#define ROT_SPEED 1.5f

typedef struct {
  float x, y;
} Vec;

uint8_t mapData[MAP_HEIGHT][MAP_WIDTH] = {
    {1, 1, 1, 1, 1, 1, 1, 1}, {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 2, 0, 0, 1}, {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1}, {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1}, {1, 1, 1, 1, 1, 1, 1, 1},
};

void drawColumn(uint32_t *pixels, int column, int height, uint32_t color) {
  int startY = HEIGHT / 2 - height / 2;
  int endY = startY + height;

  for (int y = 0; y < HEIGHT; y++) {
    uint32_t c = color;
    if (y < startY) {
      c = CEIL_COLOR;
    } else if (y > endY) {
      c = FLOOR_COLOR;
    }

    pixels[y * WIDTH + column] = c;
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

void render(uint32_t *pixels, Vec pos, float rot) {
  Vec cameraDir = {.x = cosf(rot), .y = sinf(rot)};
  Vec cameraPlane = {.x = -cameraDir.y, .y = cameraDir.x};

  int xIdxPlayer = floorf(pos.x);
  int yIdxPlayer = floorf(pos.y);
  float xRemPlayer = pos.x - floorf(pos.x);
  float yRemPlayer = pos.y - floorf(pos.y);

  for (int x = 0; x < WIDTH; x++) {
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

      if (mapData[yIdx][xIdx] > 0) {
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

    uint32_t color = 0;
    if (mapData[yIdx][xIdx] == 1) {
      color = 0xff0000ff;
    } else if (mapData[yIdx][xIdx] > 0) {
      color = 0xff00ff00;
    }

    if (hit == 2) {
      color = color & DARK_MASK;
    }

    drawColumn(pixels, x, h, color);
  }
}

void handleEvents(Vec *pos, float *rot) {
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
  if (IsKeyDown(KEY_W)) {
    pos->x += dir.x;
    pos->y += dir.y;
  } else if (IsKeyDown(KEY_S)) {
    pos->x -= dir.x;
    pos->y -= dir.y;
  }
}

int main(void) {
  InitWindow(WIDTH, HEIGHT, "raycast");

  SetTargetFPS(60);

  Vec pos = {5.5, 5.5};
  float rot = 0;

  uint32_t pixels[WIDTH * HEIGHT];
  Image image = {.data = pixels,
                 .width = WIDTH,
                 .height = HEIGHT,
                 .mipmaps = 1,
                 .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  Texture2D texture = LoadTextureFromImage(image);

  while (!WindowShouldClose()) {
    handleEvents(&pos, &rot);
    render(pixels, pos, rot);

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