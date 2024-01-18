typedef struct {
  Vector3 position;
  Vector3 dims;
  BoundingBox bbox;
} cube_t;

typedef enum {
  TT_CUBE,
  TT_COUNT,
} target_type;

typedef struct {
  target_type shape;
  float hp;
  float spawn_chance;
  union {
    cube_t cube;
  };
} target_t;

typedef struct {
  struct {
    target_t *data;
    size_t len;
    size_t cap;
  } targets;
  size_t target_count;
  Vector3 spawn_min, spawn_max;
} spawn_pattern_t;

typedef struct {
  struct {
    float firerate;
    float damage;
  } player;
  struct {
    spawn_pattern_t *data;
    size_t len;
    size_t cap;
  } spawn_patterns;
  char name[128];
} scenario_t;

struct {
  scenario_t *data;
  size_t len;
  size_t cap;
} scenarios;

scenario_t _current_scenario;
spawn_pattern_t _current_spawn_pattern;
target_t _current_target;

void set_player_firerate(sv content) {
  char *new = strndup(content.data, content.len);
  char *end_ptr;
  float val = strtof(new, &end_ptr);
  // check whether the whole string was converted
  if ((end_ptr - new) < content.len) {
    assert(false && "SCENARIO FIRERATE VALUE IS INVALID");
  }
  _current_scenario.player.firerate = val;  
  free(new);
}

void set_player_damage(sv content) {
  char *new = strndup(content.data, content.len);
  char *end_ptr;
  float val = strtof(new, &end_ptr);
  // check whether the whole string was converted
  if ((end_ptr - new) < content.len) {
    assert(false && "SCENARIO DAMAGE VALUE IS INVALID");
  }
  _current_scenario.player.damage = val;  
  free(new);
}

void set_target_type(sv content) {
  if (strncmp(content.data, "Cube", 4) != 0) {
    assert(false && "ONLY CUBE IS IMPLEMENTED RIGHT NOW");
  }
  _current_target.shape = TT_CUBE;
}

void set_target_health(sv content) {
  char *new = strndup(content.data, content.len);
  char *end_ptr;
  float val = strtof(new, &end_ptr);
  // check whether the whole string was converted
  if ((end_ptr - new) < content.len) {
    assert(false && "TARGET HEALTH VALUE IS INVALID");
  }
  _current_target.hp = val;  
  free(new);
}

// TODO: THIS ONLY WORKS FOR CUBES
void set_target_dimensions(sv content) {
  char *tmp = strndup(content.data, content.len);
  char *start = tmp;
  float vals[3] = {};
  for (size_t i = 0; i < 3; ++i) {
    char *end;
    vals[i] = strtof(start, &end);
    //check for next character is ','
    if (i != 2 && *end != ',') {
      assert(false && "MUST BE COMMA SEPARATED LIST");
    }
    start = end+1;
  }
  _current_target.cube.dims = (Vector3) {
    vals[0], vals[1], vals[2],
  };
  free(tmp);  
}

void set_target_spawn_chance(sv content) {
  char *new = strndup(content.data, content.len);
  char *end_ptr;
  float val = strtof(new, &end_ptr);
  // check whether the whole string was converted
  if ((end_ptr - new) < content.len || val < 0 || val > 1) {
    assert(false && "SPAWN CHANCE IS INVALID");
  }
  _current_target.spawn_chance = val;
  free(new);
}

void set_spawn_pattern_target_count(sv content) {
  char *new = strndup(content.data, content.len);
  char *end_ptr;
  int val = strtol(new, &end_ptr, 10);
  // check whether the whole string was converted
  if ((end_ptr - new) < content.len) {
    assert(false && "SPAWN PATTERN TARGET COUNT IS INVALID");
  }
  _current_spawn_pattern.target_count = val;
  free(new);  
}

void set_spawn_pattern_area(sv content) {
  char *tmp = strndup(content.data, content.len);
  char *start = tmp;
  float vals[6] = {};
  for (size_t i = 0; i < 6; ++i) {
    char *end;
    vals[i] = strtof(start, &end);
    //check for next character is ','
    if (i != 5 && *end != ',') {
      assert(false && "MUST BE COMMA SEPARATED LIST");
    }
    start = end+1;
  }
  _current_spawn_pattern.spawn_min = (Vector3) {
    vals[0], vals[1], vals[2],
  };
  _current_spawn_pattern.spawn_max = (Vector3) {
    vals[3], vals[4], vals[5],
  };
  free(tmp);
}

void push_current_target(sv content) {
  (void)content;
  spawn_pattern_t *s = &_current_spawn_pattern;
  if (s->targets.len >= s->targets.cap) {
    if (s->targets.cap == 0) { s->targets.cap = 1; }
    s->targets.cap *= 2;
    s->targets.data = realloc(s->targets.data, s->targets.cap * sizeof(*(s->targets.data)));
    assert(s->targets.data && "REALLOC FAILED");
  }
  s->targets.data[s->targets.len++] = _current_target;
}

void push_current_spawn_pattern(sv content) {
  (void)content;
  scenario_t *s = &_current_scenario;
  if (s->spawn_patterns.len >= s->spawn_patterns.cap) {
    if (s->spawn_patterns.cap == 0) { s->spawn_patterns.cap = 1; }
    s->spawn_patterns.cap *= 2;
    s->spawn_patterns.data = realloc(s->spawn_patterns.data,
				     s->spawn_patterns.cap * sizeof(*(s->spawn_patterns.data)));
    assert(s->spawn_patterns.data && "REALLOC FAILED");
  }
  s->spawn_patterns.data[s->spawn_patterns.len++] =
    _current_spawn_pattern;
}

void push_current_scenario(sv content) {
  (void)content;
  if (scenarios.len >= scenarios.cap) {
    if (scenarios.cap == 0) { scenarios.cap = 1; }
    scenarios.cap *= 2;
    scenarios.data = realloc(scenarios.data, scenarios.cap  * sizeof(*scenarios.data));
    assert(scenarios.data && "REALLOC FAILED");
  }
  scenarios.data[scenarios.len++] = _current_scenario;
}

void load_scenario(const char *scenario_path) {
  str xml = {};
  // read file into buffer
  {
    FILE *fxml = fopen(scenario_path, "r");
    assert(fxml != NULL && "CANNOT READ FILE");

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
    assoc_add(&arr, sv_from("scenario"), push_current_scenario);
    assoc_add(&arr, sv_from("spawn_pattern"), push_current_spawn_pattern);
    assoc_add(&arr, sv_from("target"), push_current_target);
    
    assoc_add(&arr, sv_from("firerate"), set_player_firerate);
    assoc_add(&arr, sv_from("damage"), set_player_damage);
    
    assoc_add(&arr, sv_from("type"), set_target_type);
    assoc_add(&arr, sv_from("health"), set_target_health);
    assoc_add(&arr, sv_from("spawnChance"), set_target_spawn_chance);

    assoc_add(&arr, sv_from("targetCount"), set_spawn_pattern_target_count);
    assoc_add(&arr, sv_from("area"), set_spawn_pattern_area);
  }

  sv remaining = {.data = xml.data, .len = xml.len};
  parse_xml(remaining, arr);
  free(xml.data);
  assoc_free(&arr);
}

// TODO: requires thinking about more
// since the data required for serialization and for
// runtime are quite different - how do we handle this

// will set the positions of targets according to the rules
// of each spawn
void init_scenario(scenario_t *scen) {
  for (size_t i = 0; i < scen->spawn_patterns.len; ++i) {
    spawn_pattern_t *s = &scen->spawn_patterns.data[i];
    for (size_t j = 0; j < s->target_count; ++j) {
      
    }    
  }
}
// will check for collision with targets, draw the scenario
void update_scenario(scenario_t *scen);
