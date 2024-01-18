// copyright @mattdrinksglue 2024
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

// XML
typedef struct {
  const char *data;
  size_t len;
} sv;

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} str;

#define strf "%.*s"
#define strfmt(str) (int)(str).len, (str).data

#define sv_from(cstr) (sv) { .data = cstr, .len = sizeof(cstr)-1 }

bool sv_cmp(sv a, sv b) {
  if (a.len != b.len) return false;
  return strncmp(a.data, b.data, a.len) == 0;
}

void str_append(str *s, char c) {
  if (s->len+1 >= s->cap) {
    if (s->cap == 0) { s->cap = 1; }
    s->cap *= 2;
    s->data = realloc(s->data, s->cap);
    assert(s->data && "REALLOC FAILED");
  }
  s->data[s->len++] = c;
  s->data[s->len] = '\0';
}

// error data set when xml parsing fails
struct {
  bool has_error;
  size_t index;  
  // the string of the error that caused xml parsing to fail
  char *strerror;
} xml_errordata;
  
// get the index of the end of a comment given that the current
// index is the start of the same comment
// does not support nested comments
// return of 0 means that the comment is malformed
size_t skip_comment(sv xml, size_t i_start) {
  size_t i = i_start;
  if (i + 5 >= xml.len) {
    xml_errordata.has_error = true;
    xml_errordata.strerror = "Comment not properly closed";
    xml_errordata.index = i;    
    return 0;    
  }

  if (!(xml.data[i+1] == xml.data[i+2] && xml.data[i+1] == '-')) {
    xml_errordata.has_error = true;
    xml_errordata.strerror = "Comment opening malformed, should be: <!--";
    xml_errordata.index = i;
    return 0;
  }
  i += 2;

  for (;;) {
    while (xml.data[i] != '-') {
      ++i;
      if (i >= xml.len) {
	xml_errordata.has_error = true;
	xml_errordata.strerror = "Comment not properly closed";
	xml_errordata.index = i;
	return 0;
      }
    }
    if (i + 2 >= xml.len) {
      xml_errordata.has_error = true;
      xml_errordata.strerror = "Comment not properly closed";
      xml_errordata.index = i;
      return 0;
    }
    if (xml.data[++i] == '-' && xml.data[++i] == '>') {
      return i;
    }
  }
  xml_errordata.has_error = true;
  xml_errordata.strerror = "Something went wrong skipping a comment";
  xml_errordata.index = i;
  return 0;
}

// read until the first tag
// then return the string of that tag
// moves the start of the string view to the end of the tag
// returning sv.data = NULL means no tag was found, or an error has occured
sv read_until_tag(sv *xml) {
  size_t i = 0;
  // find start of tag ("<" without a !)
  while (1) {
    while (xml->data[i] != '<') {
      ++i;
      if (i >= xml->len) {
	return (sv) {};
      }
    }
    i++;
    if (i >= xml->len) {
      xml_errordata.has_error = true;
      xml_errordata.index = i-1;
      xml_errordata.strerror = "'<' at end of file";
      return (sv) {};
    }
    // comment
    if (xml->data[i] == '!') {
      i = skip_comment(*xml, i);
    } else {
      break;
    }
  }

  // find end of tag
  size_t j = i;
  while (xml->data[j] != '>') {
    ++j;
    if (j >= xml->len) {
      xml_errordata.has_error = true;
      xml_errordata.index = i;
      xml_errordata.strerror = "Tag has no closing '>'";
    }
  }
  
  sv res = {
    .data = &xml->data[i],
    .len = j-i
  };
  xml->data += j + 1;
  xml->len -= j + 1;
  return res;
}

typedef void(*callback_pf)(sv);

typedef struct {
  sv *keys;
  callback_pf *values; // functions
  size_t count;
  size_t capacity;
} assoc_arr;

assoc_arr assoc_init(size_t cap) {
  assoc_arr res = {};
  res.keys = malloc(sizeof(*res.keys) * cap);
  res.values = malloc(sizeof(*res.values) * cap);
  res.count = 0;
  res.capacity = cap;

  return res;
}

// TODO: check for duplicates
void assoc_add(assoc_arr *res, sv key, callback_pf val) {
  size_t i = res->count++;
  if (res->count > res->capacity) {
    res->capacity *= 2;
    res->keys = realloc(res->keys, sizeof(*res->keys) * res->capacity);
    res->values = realloc(res->values, sizeof(*res->values) * res->capacity);
  }
  res->keys[i] = key;
  res->values[i] = val;
}
 
// get the value associated with a key
callback_pf assoc_search(assoc_arr arr, sv key) {
  for (size_t i = 0; i < arr.count; ++i) {
    if (sv_cmp(arr.keys[i], key)) {
      return arr.values[i];
    }
  }
  return NULL;
}

void assoc_free(assoc_arr *arr) {
  free(arr->keys);
  free(arr->values);
  arr->capacity = 0;
  arr->count = 0;
}

struct {
  // used for checking that closing tags are correct
  sv *labels;
  size_t *indices; // index of the start of the content of the associated tag
  size_t count;
  size_t capacity;
} stack;

void stack_init(size_t cap) {
  assert(cap > 0);
  if (stack.labels) {
    free(stack.labels);
  }
  if (stack.indices) {
    free(stack.indices);
  }
  stack.count = 0;
  stack.capacity = cap;
  stack.labels  = malloc(sizeof(*stack.labels)  * cap);
  stack.indices = malloc(sizeof(*stack.indices) * cap);
}

void stack_push(sv label, size_t index) {
  stack.count++;
  if (stack.count > stack.capacity) {
    stack.capacity *= 2;
    size_t cap = stack.capacity;
    stack.labels = realloc(stack.labels, sizeof(*stack.labels) * cap);
    stack.indices = realloc(stack.indices, sizeof(*stack.indices) * cap);
  }
  stack.labels[stack.count-1] = label;
  stack.indices[stack.count-1] = index;
}

void stack_pop(sv *label, size_t *index) {
  assert(stack.count > 0 && "Popping from empty stack");
  stack.count--;
  *label = stack.labels[stack.count];
  *index = stack.indices[stack.count];
}

void parse_xml(sv xml, assoc_arr cbs) {
  sv remaining = xml;
  stack_init(10);
  while (1) {
    // TODO: check that the first tag is an opening tag!
    sv next_tag = read_until_tag(&remaining);
    if (!next_tag.data) { break; }
    // closing tag
    if (next_tag.data[0] == '/') {
      size_t index;
      sv name;
      stack_pop(&name, &index);
      // check if the opening and closing tags match
      sv tag_name = {.data = next_tag.data+1, .len = (size_t)next_tag.len-1};
      if (!sv_cmp(name, tag_name)) {
	xml_errordata.has_error = true;
	xml_errordata.index = next_tag.data - xml.data;
	xml_errordata.strerror = "Mismatched opening/closing tags";
	break;
      }
      callback_pf cb = assoc_search(cbs, name);
      if (cb) {
	sv content = {
	  .data = (const char *)index,
	  .len = (size_t)next_tag.data - 1 - index
	};
	cb(content);
      }
    }
    else {
      // push opening tag onto stack
      stack_push(next_tag, (size_t)remaining.data);
    }
  }
  if (xml_errordata.has_error == true) {
    printf("Error %s\n\tAt:%lu (%.*s...)\n", xml_errordata.strerror, xml_errordata.index, (int)((remaining.len > 10) ? 10 : remaining.len), xml.data + xml_errordata.index);
  }
  free(stack.labels);
  free(stack.indices);
  stack.labels = NULL;
  stack.indices = NULL;
}

/*
int main(void) {
  str xml = {};
  // read file into buffer
  {
    FILE *fxml = fopen("./scen.xml", "r");
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
    assoc_add(&arr, sv_from("firerate"), set_firerate);
    assoc_add(&arr, sv_from("damage"), set_damage);
  }

  sv remaining = {.data = xml.data, .len = xml.len};
  parse_xml(remaining, arr);
  free(xml.data);
  assoc_free(&arr);

  printf("firerate: %f\ndamage: %f\n", level.player_info.firerate, level.player_info.damage);
  
  return 0;
}
*/
