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
#include "scenario.c"

// TODO: scoring
// TODO: local leaderboard
//   NOTE: store past scores in the scenario file?
//   TODO: write to xml files as well as read
// TODO: custom text position in scenarios
// TODO: press any key to start
// TODO: different gamemodes

// TODO: fullscreen
//   NOTE: done? but just blackscreens - test offstream because it fks with monitor settings too
// TODO: change input mode?
// TODO: input offset to make resetting tablet position easier

// UI BUTTONS

Font menu_font;

// x, y are centre coords
// returns true if clicked
bool menu_button(const char *text, int x, int y) {

  float spacing = 2.5f;
  Vector2 sz = MeasureTextEx(menu_font, text,
			     menu_theme_settings.font_size,
			     menu_theme_settings.font_spacing);
  int pad = 10;

  x = x - sz.x/2;
  y = y - sz.y/2;

  bool active = false;
  Vector2 m = GetMousePosition();
  if (m.x > x && m.x < x + sz.x + pad * 2 && m.y > y && m.y < y + sz.y + pad * 2) {
    active = true;
  }
  
  DrawRectangle(x, y, sz.x + pad*2, sz.y + pad*2, BLACK);
  DrawTextEx(menu_font, text, (Vector2){x+pad, y+pad},
	     menu_theme_settings.font_size,
	     menu_theme_settings.font_spacing,
	     menu_theme_settings.font_colour);
  
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
  Vector2 sz = MeasureTextEx(menu_font, label,
			     menu_theme_settings.font_size,
			     menu_theme_settings.font_spacing);
  int pad = 10;  

  x = x - sz.x - pad;
  y = y - sz.y/2;
  // label  
  DrawRectangle(x, y, sz.x + pad*2, sz.y + pad*2, BLACK);
  DrawTextEx(menu_font, label, (Vector2){x+pad, y+pad},
	     menu_theme_settings.font_size,
	     menu_theme_settings.font_spacing,
	     menu_theme_settings.font_colour);

  // entry box
  DrawRectangle(x+sz.x+pad*2, y, sz.x + pad*2, sz.y + pad*2, (active) ? GREEN : RED);
  // draw text
  DrawTextEx(menu_font, out->data,
	     (Vector2){x+sz.x+3*pad, y+pad},
	     menu_theme_settings.font_size,
	     menu_theme_settings.font_spacing,
	     menu_theme_settings.font_colour);

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
      out->data[--out->len] = '\0';
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
  GS_GAMEOVER,
  GS_OPTIONS,
  GS_QUIT,
  GS_CNT
} game_state_e;


#define CUBE_CNT 5

cube_t cubes[CUBE_CNT];
Texture2D crosshair;
float time_remaining;
float score;
Font game_font;

// draws time_remaining
// draws score
void draw_game_stats(void) {
  float pad = 10.f;
  // time remaining
  char text[128];
  snprintf(text, 128, "%.2f", time_remaining);
  float spacing = 1.5f;
  Vector2 sz = MeasureTextEx(game_font, text, 36, spacing);

  DrawTextEx(game_font, text,
	     (Vector2){global_settings.width/2 - sz.x/2, pad},
	     scen_theme_settings.font_size,
	     scen_theme_settings.font_spacing,
	     scen_theme_settings.font_colour);
  // score
  snprintf(text, 128, "%.0f", score);

  sz = MeasureTextEx(game_font, text,
		     scen_theme_settings.font_size,
		     scen_theme_settings.font_spacing);
  DrawTextEx(game_font, text,
	     (Vector2){global_settings.width - sz.x - pad, pad},
	     scen_theme_settings.font_size,
	     scen_theme_settings.font_spacing,
	     scen_theme_settings.font_colour);
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
    ShowCursor();
    return GS_GAMEOVER;
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

game_state_e update_gameover(void) {
  game_state_e ns = GS_GAMEOVER;
  BeginDrawing();
  ClearBackground(RAYWHITE);

  // draw score
  char text[128];
  snprintf(text, 128, "%.0f", score);
  Vector2 dims = MeasureTextEx(menu_font, text,
			       menu_theme_settings.font_size,
			       menu_theme_settings.font_spacing);
  Vector2 pos = {
    global_settings.width/2  - dims.x/2,
    global_settings.height/2 - dims.y/2
  };
  DrawTextEx(menu_font, text, pos,
	     menu_theme_settings.font_size,
	     menu_theme_settings.font_spacing,
	     BLACK);
  if (menu_button("Continue", global_settings.width/2, global_settings.height/2 + 100.)) {
    // TODO: save score somewhere
    score = 0;
    ns = GS_MENU;
  }
  
  EndDrawing();

  return ns;
}

game_state_e update_menu(void) {
  game_state_e ns = GS_MENU;
  BeginDrawing();
  ClearBackground(RAYWHITE);
  if (menu_button("Play", global_settings.width/2, global_settings.height/2)) {
    initialize_cubes(CUBE_CNT, cubes);
    HideCursor();
    time_remaining = 5.f;
    ns = GS_GAMEPLAY;
  }
  if (menu_button("Options", global_settings.width/2, global_settings.height/2 + 80)) {
    ns = GS_OPTIONS;
  }
  if (menu_button("Quit", global_settings.width/2, global_settings.height/2 + 160)) {
    ns = GS_QUIT;
  }
  DrawFPS(0, 0);
  EndDrawing();
  return ns;
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
}

void load_fonts(void) {
  if (menu_theme_settings.font_path) {
    printf("Loading font from %s...\n", menu_theme_settings.font_path);
    menu_font = LoadFontEx(menu_theme_settings.font_path, 512, NULL, 0);
    assert(IsFontReady(menu_font) && "Font path specified is invalid or another font loading error has occurred");
  } else {
    menu_font = GetFontDefault();
  }
  if (scen_theme_settings.font_path) {
    printf("Loading font from %s...\n", scen_theme_settings.font_path);
    game_font = LoadFontEx(scen_theme_settings.font_path, 512, NULL, 0);
    assert(IsFontReady(game_font) && "Font path specified is invalid or another font loading error has occurred");
  } else {
    game_font = GetFontDefault();
  }
}

int main(void) {
  load_settings();
  load_scenario("scen.xml");
  
  InitWindow(global_settings.width, global_settings.height, "Hello, world window");
  // TODO: change target FPS in settings
  SetTargetFPS(global_settings.desired_fps);
  if (global_settings.desire_fullscreen) {
    ToggleFullscreen();
  }

  load_fonts();
  
  Vector3 position = {0, 0, 0};

  Image crosshair_image = LoadImage("./crosshair.png");
  crosshair = LoadTextureFromImage(crosshair_image);
  UnloadImage(crosshair_image);

  Ray r;

  game_state_e cstate = GS_MENU;
  bool done = false;
  while (!WindowShouldClose() && !done) {
    switch(cstate) {
    case GS_MENU:     { cstate = update_menu();     break; }
    case GS_GAMEPLAY: { cstate = update_gameplay(); break; }
    case GS_GAMEOVER: { cstate = update_gameover(); break; }
    case GS_OPTIONS:  { cstate = update_options();  break; }
    case GS_QUIT:     { done = true; break; }
    default: assert(false && "UNREACHABLE");
    }    
  }
  // FIXME this is bad
  if (menu_theme_settings.font_path) {
    UnloadFont(menu_font);
    free(menu_theme_settings.font_path);
  }
  CloseWindow();
  return 0;
}
