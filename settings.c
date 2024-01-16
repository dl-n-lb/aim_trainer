#include "raylib-5.0/src/raylib.h"

// TODO: Loading custom fonts
// TODO: how to manage strings in the case of options filling in values, etc.
// SETTINGS
struct {
  int width, height;
  float sensitivity;
  int desired_fps;
  

  str desired_fps_str;
} global_settings;

typedef struct {
  Font font;
  int font_size;
  float font_spacing;
  Color font_colour;  
} theme_settings_t;

theme_settings_t menu_theme_settings;
theme_settings_t scen_theme_settings;
theme_settings_t *_current_theme_settings = &menu_theme_settings;

void set_font(sv content) {
  assert(false && "TODO: fetch font from the name passed here");
}

void set_font_size(sv content) {
  char *new = strndup(content.data, content.len);
  char *end_ptr;
  int val = strtol(new, &end_ptr, 10);
  // check whether the whole string was converted
  if ((end_ptr - new) < content.len) {
    assert(false && "DESIRED FONT SIZE IS INVALID");
  }
  _current_theme_settings->font_size = val;
  free(new);
}

void set_font_colour(sv content) {
  // skip the #
  char *new = strndup(content.data+1, content.len);
  printf(strf"\n", strfmt(content));
  assert(content.data[0] == '#' && "COLOUR MUST START WITH # SYMBOL");
  assert(content.len == 7 && "COLOUR MUST HAVE FORMAT #RRGGBB");
  char *end_ptr;
  int val = strtol(new, &end_ptr, 16);
  // check whether the whole string was converted
  if ((end_ptr - new) < content.len - 1) {
    assert(false && "DESIRED COLOUR IS INVALID");
  }
  _current_theme_settings->font_colour =(Color) {
    .r = (val >> 16) & 0xFF,
    .g = (val >> 8)  & 0xFF,
    .b = (val >> 0)  & 0xFF,
    .a = 0xFF,
  };
  free(new);
}

void set_desired_fps(sv content) {
  char *new = strndup(content.data, content.len);
  char *end_ptr;
  int val = strtol(new, &end_ptr, 10);
  // check whether the whole string was converted
  if ((end_ptr - new) < content.len) {
    assert(false && "DESIRED FPS VALUE IS INVALID");
  }
  global_settings.desired_fps = val;
  free(new);
}

// expects a width,height pair with no space
void set_resolution(sv content) {
  char *new = strndup(content.data, content.len);
  char *comma, *end_ptr;
  int w = strtol(new, &comma, 10);
  assert(*comma == ',' && "COMMA MISSING");
  int h = strtol(++comma, &end_ptr, 10);
  assert((end_ptr - new) >= content.len && "DESIRED VALUE IS INVALID");  
  global_settings.width = w;
  global_settings.height = h;
  free(new);
}

void set_sensitivity(sv content) {
  char *new = strndup(content.data, content.len);
  char *end_ptr;
  float val = strtof(new, &end_ptr);
  // check whether the whole string was converted
  if ((end_ptr - new) < content.len) {
    assert(false && "DESIRED SENSITIVITY VALUE IS INVALID");
  }
  global_settings.sensitivity = val;
  free(new);
}

void load_settings(void) {
  str xml = {};
  // read file into buffer
  {
    FILE *fxml = fopen("./settings.xml", "r");
    assert(fxml != NULL && "CANNOT READ FILE settings.xml");

    fseek(fxml, 0, SEEK_END);
    long int flen = ftell(fxml);
    assert(flen > 0 && "COULD NOT READ FILE LENGTH");
    fseek(fxml, 0, SEEK_SET);

    xml.data = malloc(sizeof(*xml.data) * flen);
    xml.len = flen;
    assert(fread(xml.data, xml.len, 1, fxml) > 0 && "COULD NOT READ FILE INTO BUFFER");
    fclose(fxml);
  }
  assoc_arr arr = assoc_init(10);
  {
    assoc_add(&arr, sv_from("resolution"), set_resolution);
    assoc_add(&arr, sv_from("sensitivity"), set_sensitivity);
    assoc_add(&arr, sv_from("targetFPS"), set_desired_fps);
    assoc_add(&arr, sv_from("fontSize"), set_font_size);
    assoc_add(&arr, sv_from("colour"), set_font_colour);
  }

  sv remaining = {.data = xml.data, .len = xml.len};
  parse_xml(remaining, arr);
  free(xml.data);
  assoc_free(&arr);
}
