// copyright @mattdrinksglue 2024
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "raylib-5.0/src/raylib.h"
#include "raylib-5.0/src/raymath.h"

// the smartest idea ever
// order is important :D
#include "xml.c"
#include "settings.c"

// FIXME: text has large numbers of hardcoded constants
// TODO: fetch text information from settings.xml

// TODO: scoring
// TODO: local leaderboard
//   NOTE: store past scores in the scenario file
//   TODO: write to xml files as well as read
// TODO: press any key to start
// TODO: different gamemodes
//   NOTE: this means parsing the scen.xml file to produce a scenario

// TODO: Change window sz in settings
// TODO: fullscreen
// TODO: change input mode?
// TODO: input offset to make resetting tablet position easier

// UI BUTTONS

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

// returns true if a confirmation button is pressed
// and writes the value to out
// out is malloced by this function
// out should be freed 
bool entry_box(const char *label, int x, int y, str *out) {
  static bool active = false;
  float spacing = 2.5f;
  Vector2 sz = MeasureTextEx(GetFontDefault(), label, 36, spacing);
  int pad = 10;  

  x = x - sz.x - pad;
  y = y - sz.y/2;
  // label  
  DrawRectangle(x, y, sz.x + pad*2, sz.y + pad*2, BLACK);
  DrawTextEx(GetFontDefault(), label, (Vector2){x+pad, y+pad}, 36, spacing, WHITE);

  // entry box
  DrawRectangle(x+sz.x+pad*2, y, sz.x + pad*2, sz.y + pad*2, (active) ? GREEN : RED);
  // draw text
  char *text_cstr = strndup(out->data, out->len);
  DrawTextEx(GetFontDefault(), text_cstr,
	     (Vector2){x+sz.x+3*pad, y+pad}, 36, spacing, BLACK);
  free(text_cstr);

  // update state
  Vector2 m = GetMousePosition();
  if (m.x > x + sz.x + 2*pad &&
      m.x < x + 2*sz.x + pad * 4 &&
      m.y > y && m.y < y + sz.y + pad * 2) {
    active = active || IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
  } else if (active) {
    active = !IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
  }
  if (active) {
    char x;
    if ((x = GetCharPressed()) > 0) {
      str_append(out, x);
    }
    int key_press = GetKeyPressed();
    if (key_press == KEY_BACKSPACE) {
      out->len--;
    }
    if (key_press == KEY_ENTER) {
      return true;
    }
  }

  return false;
}

// assumes normalized view vector
void set_camera_rotation(Camera *camera, Vector2 mouse_position) {
  Vector2 angles;
  // Y = 0..height -> -PI..PI
  // and invert
  angles.y = -(float)mouse_position.y / global_settings.height * 2 * M_PI + M_PI;
  // X = 0..width -> 0..2*PI
  // and invert
  angles.x = -(float)mouse_position.x / global_settings.width * 2 * M_PI;
  // FIXME: sensitivity only makes sensse >1 for now
  // NOTE: resetting mouse position without changing view angle
  // (see input offset above)
  //angles.x *= global_settings.sensitivity;
  //angles.y *= global_settings.sensitivity;
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

void initialize_cubes(size_t n, cube_t cubes[static n]) {
  for (size_t i = 0; i < n; ++i) {
    float x = (float)rand() / RAND_MAX * 2 - 1;
    float y = (float)rand() / RAND_MAX * 2 - 1;
    float z = -2;
    cubes[i].position = (Vector3){ x, y, z };

    cubes[i].dims = (Vector3) {0.1, 0.1, 0.1};
    Vector3 d = cubes[i].dims;

    BoundingBox b;
    b.min.x = x - d.x/2;
    b.min.y = y - d.y/2;
    b.min.z = z - d.z/2;
    b.max.x = x + d.x/2;
    b.max.y = y + d.y/2;
    b.max.z = z + d.z/2;
    cubes[i].bbox = b;
  }
}

size_t check_collision(Ray r, size_t n, cube_t cubes[static n]) {
  RayCollision rc;
  for (size_t i = 0; i < n; ++i) {
    rc = GetRayCollisionBox(r, cubes[i].bbox);
    if (rc.hit) return i;
  }
  return n;
}

void respawn_cube(size_t i, size_t n, cube_t cubes[static n]) {
  float x = (float)rand() / RAND_MAX * 2 - 1;
  float y = (float)rand() / RAND_MAX * 2 - 1;
  float z = -2;
  cubes[i].position = (Vector3){ x, y, z };

  Vector3 d = cubes[i].dims;
  BoundingBox b;
  b.min.x = x - d.x/2;
  b.min.y = y - d.y/2;
  b.min.z = z - d.z/2;
  b.max.x = x + d.x/2;
  b.max.y = y + d.y/2;
  b.max.z = z + d.z/2;

  cubes[i].bbox = b;
}

void draw_cubes(size_t n, cube_t cubes[static n]) {
  // wall TODO: remove hardcoding
  DrawCube((Vector3){0, 0, -2}, 2, 2, 0.05, GRAY);

  for (size_t i = 0; i < n; ++i) {
    Vector3 d = cubes[i].dims;
    DrawCube(cubes[i].position, d.x, d.y, d.z, ORANGE);
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

cube_t cubes[CUBE_CNT];
Texture2D crosshair;
float time_remaining;
float score;

// draws time_remaining
// draws score
void draw_game_stats(void) {
  float pad = 10.f;
  // time remaining
  char text[128];
  snprintf(text, 128, "%.2f", time_remaining);
  float spacing = 1.5f;
  Vector2 sz = MeasureTextEx(GetFontDefault(), text, 36, spacing);
  
  DrawText(text, global_settings.width/2 - sz.x/2, pad, 36, BLACK);
  // score
  snprintf(text, 128, "%.0f", score);

  sz = MeasureTextEx(GetFontDefault(), text, 24, spacing);
  DrawTextEx(GetFontDefault(), text,
	     (Vector2){global_settings.width - sz.x - pad, pad},
	     24, spacing, BLACK);

}

game_state_e update_gameplay(void) {
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

  set_camera_rotation(&camera, GetMousePosition());

  if (IsKeyPressed(KEY_A)) {
    Ray r = GetMouseRay((Vector2){global_settings.width/2,
				  global_settings.height/2}, camera);
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

    DrawTexture(crosshair, global_settings.width/2 - crosshair.width/2,
		global_settings.height/2 - crosshair.height/2, WHITE);

    draw_game_stats();
    DrawFPS(0, 0);
  }
  EndDrawing();

  return GS_GAMEPLAY;
}

game_state_e update_menu(void) {
  BeginDrawing();
  ClearBackground(RAYWHITE);
  if (menu_button("Play", global_settings.width/2, global_settings.height/2)) {
    initialize_cubes(CUBE_CNT, cubes);
    HideCursor();
    time_remaining = 60.f;
    return GS_GAMEPLAY;
  }
  if (menu_button("Options", global_settings.width/2, global_settings.height/2 + 80)) {
    return GS_OPTIONS;
  }
  if (menu_button("Quit", global_settings.width/2, global_settings.height/2 + 160)) {
    return GS_QUIT;
  }
  DrawFPS(0, 0);
  EndDrawing();
  return GS_MENU;
}

game_state_e update_options(void) {
  BeginDrawing();
  ClearBackground(RAYWHITE);
  float pad = 100.f;
  if (menu_button("Apply", global_settings.width/2, pad)) {
    printf("Applied :3\n");
  }
  
  if (entry_box("Target FPS", global_settings.width/2, 2*pad, &global_settings.desired_fps_str)) {
  }
  
  DrawFPS(0, 0);
  EndDrawing();
  return GS_OPTIONS;
  assert(false && "TODO LATER");
}

int main(void) {

  load_settings();
  
  InitWindow(global_settings.width, global_settings.height, "Hello, world window");
  // TODO: change target FPS in settings
  SetTargetFPS(global_settings.desired_fps);


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
