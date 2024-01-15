#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "raylib-5.0/src/raylib.h"
#include "raylib-5.0/src/raymath.h"

// TODO: scoring
// TODO: press any key to start
// TODO: different gamemodes
// TODO: local leaderboard
// TODO: Change window sz in settings
// TODO: fullscreen
// TODO: change input mode?
// TODO: input offset to make resetting tablet position easier
const int width = 800;
const int height = 600;

Vector2 get_angles_from_mouse_position(Vector2 mouse_position) {
  Vector2 res;
  // Y = 0..height -> -PI..PI
  // and invert
  res.y = -(float)mouse_position.y / height * 2 * M_PI + M_PI;
  // X = 0..width -> 0..2*PI
  // and invert
  res.x = -(float)mouse_position.x / width * 2 * M_PI;

  return res;
}

// assumes normalized view vector
void set_camera_rotation(Camera *camera, Vector2 angles) {
  Vector3 view_zero = (Vector3) {0, 0, 1};
  Matrix m = MatrixRotateXYZ((Vector3){angles.y, angles.x, 0});
  Vector3 view_dir = Vector3Transform(view_zero, m);
  camera->target = (Vector3) {
    camera->position.x + view_dir.x,
    camera->position.y + view_dir.y,
    camera->position.z + view_dir.z
  };
}

typedef struct {
  Vector3 position;
  Vector3 dims;
  BoundingBox bbox;
} cube_t;

typedef enum {
  HT_CUBE,
  HT_COUNT,
} target_type;

typedef struct {
  target_type type;
  float hp;
  union {
    cube_t cube;
  };
} target_t;

void initialize_cubes(size_t n, Cube cubes[static n]) {
  for (size_t i = 0; i < n; ++i) {
    float x = (float)rand() / RAND_MAX * 2 - 1;
    float y = (float)rand() / RAND_MAX * 2 - 1;
    float z = -2;
    cubes[i].position = (Vector3){ x, y, z };

    BoundingBox b;
    b.min.x = x - 0.05;
    b.min.y = y - 0.05;
    b.min.z = z - 0.05;
    b.max.x = x + 0.05;
    b.max.y = y + 0.05;
    b.max.z = z + 0.05;
    cubes[i].bbox = b;
  }
}

size_t check_collision(Ray r, size_t n, Cube cubes[static n]) {
  RayCollision rc;
  for (size_t i = 0; i < n; ++i) {
    rc = GetRayCollisionBox(r, cubes[i].bbox);
    if (rc.hit) return i;
  }
  return n;
}

void respawn_cube(size_t i, size_t n, Cube cubes[static n]) {
  float x = (float)rand() / RAND_MAX * 2 - 1;
  float y = (float)rand() / RAND_MAX * 2 - 1;
  float z = -2;
  cubes[i].position = (Vector3){ x, y, z };

  BoundingBox b;
  b.min.x = x - 0.05;
  b.min.y = y - 0.05;
  b.min.z = z - 0.05;
  b.max.x = x + 0.05;
  b.max.y = y + 0.05;
  b.max.z = z + 0.05;
  cubes[i].bbox = b;
}

void draw_cubes(size_t n, Cube cubes[static n]) {
  float sz = 0.1f;

  DrawCube((Vector3){0, 0, -2}, 2, 2, 0.05, GRAY);

  for (size_t i = 0; i < n; ++i) {
    DrawCube(cubes[i].position, sz, sz, sz, ORANGE);
  }
}


typedef enum {
  GS_MENU,
  GS_GAMEPLAY,
  GS_OPTIONS,
  GS_QUIT,
  GS_CNT
} game_state_e;


#define CUBE_CNT 5

Cube cubes[CUBE_CNT];
Texture2D crosshair;
float time_remaining;
float score;

game_state_e update_gameplay() {
  static Camera camera = {
    .position = (Vector3) { 0, 0, 0 },
    .target   = (Vector3) { 0, 0, -1 },
    .up       = (Vector3) { 0, 1, 0 },
    .fovy     = 60.f,
    .projection = CAMERA_PERSPECTIVE,
  };

  time_remaining -= GetFrameTime();
  if (time_remaining <= 0) {
    printf("%f\n", score);
    assert(false && "GAME OVER TODO");
  }
  
  Vector2 mouse_position = GetMousePosition();

  set_camera_rotation(&camera, get_angles_from_mouse_position(mouse_position));

  if (IsKeyPressed(KEY_A)) {
    Ray r = GetMouseRay((Vector2){width/2, height/2}, camera);
    size_t i = check_collision(r, CUBE_CNT, cubes);
    if (i < CUBE_CNT) {
      score += 1;
      respawn_cube(i, CUBE_CNT, cubes);
    }
  }

  BeginDrawing();
  {
    ClearBackground(RAYWHITE);
      
    BeginMode3D(camera);
    {
      draw_cubes(CUBE_CNT, cubes);
    }
    EndMode3D();

    DrawTexture(crosshair, width/2 - crosshair.width/2, height/2 - crosshair.height/2, WHITE);
    float pad = 10.f;
    // time remaining
    char text[128];
    snprintf(text, 128, "%.2f", time_remaining);
    DrawText(text, pad, pad, 24, BLACK);
    // score
    snprintf(text, 128, "%.0f", score);
    float spacing = 1.5f;
    Vector2 sz = MeasureTextEx(GetFontDefault(), text, 24, spacing);
    DrawTextEx(GetFontDefault(), text, (Vector2){width - sz.x - pad, pad}, 24, spacing, BLACK);
  }
  EndDrawing();

  return GS_GAMEPLAY;
}

// x, y are centre coords
// returns true if clicked
bool menu_button(const char *text, int x, int y) {

  float spacing = 2.5f;
  Vector2 sz = MeasureTextEx(GetFontDefault(), text, 36, spacing);
  int pad = 10;

  x = x - sz.x/2;
  y = y - sz.y/2;

  bool active = false;
  Vector2 m = GetMousePosition();
  if (m.x > x && m.x < x + sz.x + pad * 2 && m.y > y && m.y < y + sz.y + pad * 2) {
    active = true;
  }
  
  DrawRectangle(x, y, sz.x + pad*2, sz.y + pad*2, BLACK);
  DrawTextEx(GetFontDefault(), text, (Vector2){x+pad, y+pad}, 36, spacing, WHITE);
  
  if (active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return true;
  return false;
}

game_state_e update_menu() {
  BeginDrawing();
  ClearBackground(RAYWHITE);
  if (menu_button("Play", width/2, height/2)) {
    initialize_cubes(CUBE_CNT, cubes);
    HideCursor();
    time_remaining = 60.f;
    return GS_GAMEPLAY;
  }
  if (menu_button("Options", width/2, height/2 + 80)) {
    return GS_OPTIONS;
  }
  if (menu_button("Quit", width/2, height/2 + 160)) {
    return GS_QUIT;
  }

  EndDrawing();
  return GS_MENU;
}

game_state_e update_options() {
  assert(false && "TODO LATER");
}

int main(void) {
  InitWindow(width, height, "Hello, world window");
  // TODO: change target FPS in settings
  SetTargetFPS(120);


  Vector3 position = {0, 0, 0};

  Image crosshair_image = LoadImage("./crosshair.png");
  crosshair = LoadTextureFromImage(crosshair_image);
  UnloadImage(crosshair_image);

  Ray r;

  game_state_e cstate = GS_MENU;
  bool done = false;
  while (!WindowShouldClose() && !done) {
    switch(cstate) {
    case GS_MENU: { cstate = update_menu(); break; }
    case GS_GAMEPLAY: { cstate = update_gameplay(); break; }
    case GS_OPTIONS: { cstate = update_options(); break; }
    case GS_QUIT: { done = true; break; }
    default: assert(false && "UNREACHABLE");
    }    
  }
  return 0;
}
