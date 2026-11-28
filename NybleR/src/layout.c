#include "nybler/nybler.h"
#include "internal.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct LayoutBox {
  NyblerNode *node;
  NyblerRect rect;
} LayoutBox;

typedef struct LayoutEntry {
  NyblerDisplayItem item;
  char *owned_text;
} LayoutEntry;

typedef struct {
  int display_none;
  int display_inline;
  int has_width;
  int has_height;
  float width;
  float height;
  float margin_left;
  float margin_right;
  float margin_top;
  float margin_bottom;
  float padding_left;
  float padding_right;
  float padding_top;
  float padding_bottom;
  NyblerColor background;
  NyblerColor color;
} LayoutStyle;

struct NyblerLayout {
  NyblerDocument *document;
  NyblerViewport viewport;
  LayoutBox *boxes;
  size_t box_count;
  size_t box_capacity;
  LayoutEntry *entries;
  size_t entry_count;
  size_t entry_capacity;
};

struct NyblerDisplayList {
  NyblerDisplayItem *items;
  size_t count;
  size_t capacity;
};

typedef struct LayoutContext {
  NyblerLayout *layout;
  int failed;
} LayoutContext;

static int slice_equals_ci(const char *left, size_t left_length, const char *right) {
  size_t i;
  size_t right_length = strlen(right);

  if (left_length != right_length) return 0;
  for (i = 0; i < left_length; i++) {
    if (tolower((unsigned char)left[i]) != tolower((unsigned char)right[i])) {
      return 0;
    }
  }
  return 1;
}

static void trim_slice(const char **data, size_t *length) {
  while (*length && isspace((unsigned char)**data)) {
    (*data)++;
    (*length)--;
  }
  while (*length && isspace((unsigned char)(*data)[*length - 1])) {
    (*length)--;
  }
}

static int parse_float_slice(const char *data, size_t length, float *out) {
  char buffer[64];
  char *end;
  float value;

  if (!data || length == 0 || length >= sizeof(buffer)) return 0;
  memcpy(buffer, data, length);
  buffer[length] = '\0';

  value = strtof(buffer, &end);
  if (end == buffer) return 0;
  while (*end && isspace((unsigned char)*end)) end++;
  if (*end) return 0;

  *out = value;
  return 1;
}

static int parse_length(
  const char *data,
  size_t length,
  float parent_size,
  float *out
) {
  float value;

  trim_slice(&data, &length);
  if (length > 2 && data[length - 2] == 'p' && data[length - 1] == 'x') {
    length -= 2;
  } else if (length > 1 && data[length - 1] == '%') {
    if (!parse_float_slice(data, length - 1, &value)) return 0;
    *out = (value / 100.0f) * parent_size;
    return *out >= 0.0f;
  }

  if (!parse_float_slice(data, length, &value)) return 0;
  *out = value >= 0.0f ? value : 0.0f;
  return 1;
}

static int parse_hex_digit(char value, unsigned int *out) {
  if (value >= '0' && value <= '9') {
    *out = (unsigned int)(value - '0');
    return 1;
  }
  if (value >= 'a' && value <= 'f') {
    *out = (unsigned int)(value - 'a' + 10);
    return 1;
  }
  if (value >= 'A' && value <= 'F') {
    *out = (unsigned int)(value - 'A' + 10);
    return 1;
  }
  return 0;
}

static NyblerColor parse_color(const char *data, size_t length) {
  NyblerColor color = {0.0f, 0.0f, 0.0f, 1.0f};
  unsigned int values[4];
  unsigned int count = 0;
  size_t i;
  size_t component_start;
  size_t component_length;

  trim_slice(&data, &length);
  if (length == 0) return color;

  if (slice_equals_ci(data, length, "transparent")) {
    color.a = 0.0f;
    return color;
  }
  if (slice_equals_ci(data, length, "black")) return color;
  if (slice_equals_ci(data, length, "white")) {
    color.r = color.g = color.b = 1.0f;
    return color;
  }
  if (slice_equals_ci(data, length, "red")) {
    color.r = 1.0f;
    return color;
  }
  if (slice_equals_ci(data, length, "green")) {
    color.g = 0.5f;
    return color;
  }
  if (slice_equals_ci(data, length, "blue")) {
    color.b = 1.0f;
    return color;
  }

  if (data[0] == '#' && length > 1) {
    size_t digits = length - 1;
    if (digits == 3 || digits == 4 || digits == 6 || digits == 8) {
      size_t stride = digits <= 4 ? 1 : 2;
      size_t components = digits / stride;
      for (i = 0; i < components; i++) {
        unsigned int high;
        unsigned int low = 0;
        if (!parse_hex_digit(data[1 + i * stride], &high)) return color;
        if (stride == 2 && !parse_hex_digit(data[2 + i * stride], &low)) {
          return color;
        }
        if (stride == 1) {
          values[i] = high * 17;
        } else {
          values[i] = high * 16 + low;
        }
      }
      color.r = (float)values[0] / 255.0f;
      color.g = (float)values[1] / 255.0f;
      color.b = (float)values[2] / 255.0f;
      if (components == 4) color.a = (float)values[3] / 255.0f;
      return color;
    }
  }

  if (length >= 5 && (slice_equals_ci(data, 4, "rgb(") ||
                      slice_equals_ci(data, 5, "rgba("))) {
    size_t prefix = data[3] == 'a' ? 5 : 4;
    if (data[length - 1] == ')') {
      component_start = prefix;
      for (i = prefix; i <= length - 1; i++) {
        if (i == length - 1 || data[i] == ',') {
          const char *component_data = &data[component_start];
          component_length = i - component_start;
          float component;
          trim_slice(&component_data, &component_length);
          if (count >= 4 ||
              !parse_float_slice(component_data, component_length, &component)) {
            return color;
          }
          values[count++] = (unsigned int)(component + 0.5f);
          component_start = i + 1;
        }
      }
      if (count == 3 || count == 4) {
        color.r = (float)(values[0] > 255 ? 255 : values[0]) / 255.0f;
        color.g = (float)(values[1] > 255 ? 255 : values[1]) / 255.0f;
        color.b = (float)(values[2] > 255 ? 255 : values[2]) / 255.0f;
        if (count == 4) {
          color.a = (float)(values[3] > 255 ? 255 : values[3]) / 255.0f;
        }
      }
    }
  }

  return color;
}

static LayoutStyle default_style(int is_body) {
  LayoutStyle style;
  memset(&style, 0, sizeof(style));
  style.background.a = is_body ? 1.0f : 0.0f;
  if (is_body) {
    style.background.r = 1.0f;
    style.background.g = 1.0f;
    style.background.b = 1.0f;
  }
  style.color.a = 1.0f;
  return style;
}

static void apply_declaration(
  LayoutStyle *style,
  const char *name,
  size_t name_length,
  const char *value,
  size_t value_length,
  const NyblerRect *parent
) {
  float parsed;

  trim_slice(&name, &name_length);
  trim_slice(&value, &value_length);
  if (!name_length) return;

  if (slice_equals_ci(name, name_length, "display")) {
    if (slice_equals_ci(value, value_length, "none")) {
      style->display_none = 1;
    } else if (slice_equals_ci(value, value_length, "inline")) {
      style->display_inline = 1;
    }
  } else if (slice_equals_ci(name, name_length, "width")) {
    if (parse_length(value, value_length, parent->width, &parsed)) {
      style->width = parsed;
      style->has_width = 1;
    }
  } else if (slice_equals_ci(name, name_length, "height")) {
    if (parse_length(value, value_length, parent->height, &parsed)) {
      style->height = parsed;
      style->has_height = 1;
    }
  } else if (slice_equals_ci(name, name_length, "background-color") ||
             slice_equals_ci(name, name_length, "background")) {
    style->background = parse_color(value, value_length);
  } else if (slice_equals_ci(name, name_length, "color")) {
    style->color = parse_color(value, value_length);
  } else if (slice_equals_ci(name, name_length, "margin-left")) {
    if (parse_length(value, value_length, parent->width, &parsed)) {
      style->margin_left = parsed;
    }
  } else if (slice_equals_ci(name, name_length, "margin-right")) {
    if (parse_length(value, value_length, parent->width, &parsed)) {
      style->margin_right = parsed;
    }
  } else if (slice_equals_ci(name, name_length, "margin-top")) {
    if (parse_length(value, value_length, parent->height, &parsed)) {
      style->margin_top = parsed;
    }
  } else if (slice_equals_ci(name, name_length, "margin-bottom")) {
    if (parse_length(value, value_length, parent->height, &parsed)) {
      style->margin_bottom = parsed;
    }
  } else if (slice_equals_ci(name, name_length, "padding-left")) {
    if (parse_length(value, value_length, parent->width, &parsed)) {
      style->padding_left = parsed;
    }
  } else if (slice_equals_ci(name, name_length, "padding-right")) {
    if (parse_length(value, value_length, parent->width, &parsed)) {
      style->padding_right = parsed;
    }
  } else if (slice_equals_ci(name, name_length, "padding-top")) {
    if (parse_length(value, value_length, parent->height, &parsed)) {
      style->padding_top = parsed;
    }
  } else if (slice_equals_ci(name, name_length, "padding-bottom")) {
    if (parse_length(value, value_length, parent->height, &parsed)) {
      style->padding_bottom = parsed;
    }
  }
}

static void apply_inline_style(
  LayoutStyle *style,
  const char *data,
  size_t length,
  const NyblerRect *parent
) {
  size_t start = 0;
  size_t i;

  if (!data || !length) return;

  for (i = 0; i <= length; i++) {
    if (i != length && data[i] != ';') continue;

    const char *declaration = data + start;
    size_t declaration_length = i - start;
    size_t colon = 0;

    trim_slice(&declaration, &declaration_length);
    while (colon < declaration_length && declaration[colon] != ':') colon++;
    if (colon < declaration_length) {
      apply_declaration(
        style,
        declaration,
        colon,
        declaration + colon + 1,
        declaration_length - colon - 1,
        parent
      );
    }

    start = i + 1;
  }
}

static void apply_computed_style(
  NyblerDocument *document,
  NyblerNode *node,
  LayoutStyle *style,
  const NyblerRect *parent
) {
  static const char *const properties[] = {
    "display",
    "width",
    "height",
    "background-color",
    "background",
    "color",
    "margin-left",
    "margin-right",
    "margin-top",
    "margin-bottom",
    "padding-left",
    "padding-right",
    "padding-top",
    "padding-bottom"
  };
  size_t i;

  for (i = 0; i < sizeof(properties) / sizeof(properties[0]); i++) {
    size_t name_length = strlen(properties[i]);
    size_t value_length = 0;
    char *value = nybler_internal_computed_style_value(
      document,
      node,
      properties[i],
      name_length,
      &value_length
    );

    if (value) {
      apply_declaration(
        style,
        properties[i],
        name_length,
        value,
        value_length,
        parent
      );
      nybler_internal_computed_style_value_release(value);
    }
  }
}

static int grow_array(void **items, size_t *capacity, size_t count, size_t item_size) {
  size_t next;
  void *grown;

  if (count < *capacity) return 1;
  next = *capacity ? *capacity * 2 : 16;
  grown = realloc(*items, next * item_size);
  if (!grown) return 0;
  *items = grown;
  *capacity = next;
  return 1;
}

static int append_box(LayoutContext *context, NyblerNode *node, NyblerRect rect) {
  LayoutBox *box;

  if (context->failed) return 0;
  if (!grow_array(
        (void **)&context->layout->boxes,
        &context->layout->box_capacity,
        context->layout->box_count,
        sizeof(*context->layout->boxes)
      )) {
    context->failed = 1;
    return 0;
  }

  box = &context->layout->boxes[context->layout->box_count++];
  box->node = node;
  box->rect = rect;
  return 1;
}

static int append_item(
  LayoutContext *context,
  NyblerDisplayItemKind kind,
  NyblerRect rect,
  NyblerColor color,
  NyblerNode *node,
  char *owned_text,
  size_t text_length
) {
  LayoutEntry *entry;
  NyblerDisplayItem item;

  if (context->failed) {
    free(owned_text);
    return 0;
  }
  if (!grow_array(
        (void **)&context->layout->entries,
        &context->layout->entry_capacity,
        context->layout->entry_count,
        sizeof(*context->layout->entries)
      )) {
    context->failed = 1;
    free(owned_text);
    return 0;
  }

  memset(&item, 0, sizeof(item));
  item.kind = kind;
  item.rect = rect;
  item.color = color;
  item.node = node;
  item.text = owned_text;
  item.text_length = text_length;

  entry = &context->layout->entries[context->layout->entry_count++];
  entry->item = item;
  entry->owned_text = owned_text;
  return 1;
}

static char *normalize_text(const char *text, size_t length, size_t *out_length) {
  char *result;
  size_t i = 0;
  size_t written = 0;
  int pending_space = 0;

  *out_length = 0;
  if (!text || length == 0) return NULL;

  result = (char *)malloc(length + 1);
  if (!result) return NULL;

  while (i < length) {
    unsigned char ch = (unsigned char)text[i];
    if (isspace(ch)) {
      pending_space = written != 0;
      i++;
      continue;
    }
    if (pending_space) {
      result[written++] = ' ';
      pending_space = 0;
    }
    result[written++] = (char)ch;
    i++;
  }

  if (written == 0) {
    free(result);
    return NULL;
  }

  result[written] = '\0';
  *out_length = written;
  return result;
}

static void layout_element(
  LayoutContext *context,
  NyblerNode *node,
  const NyblerRect *parent,
  float *cursor_y,
  NyblerColor inherited_color
);

static void layout_children(
  LayoutContext *context,
  NyblerNode *node,
  const NyblerRect *content,
  float *cursor_y,
  NyblerColor color
) {
  NyblerNode *child;

  for (child = nybler_node_get_first_child(node);
       child && !context->failed;
       child = nybler_node_get_next_sibling(child)) {
    NyblerNodeType type = nybler_node_get_node_type(child);
    if (type == NYBLER_NODE_TYPE_ELEMENT) {
      layout_element(context, child, content, cursor_y, color);
    } else if (type == NYBLER_NODE_TYPE_TEXT) {
      char *raw = NULL;
      char *text;
      size_t raw_length = 0;
      size_t text_length = 0;
      NyblerRect rect;

      if (nybler_node_get_text_content(child, &raw, &raw_length) != NYBLER_OK) {
        context->failed = 1;
        break;
      }
      text = normalize_text(raw, raw_length, &text_length);
      nybler_string_release(raw);
      if (!text) continue;

      rect.x = content->x;
      rect.y = *cursor_y;
      rect.width = (float)text_length * 8.0f;
      if (content->width > 0.0f && rect.width > content->width) {
        rect.width = content->width;
      }
      rect.height = 16.0f;

      if (!append_item(
            context,
            NYBLER_DISPLAY_ITEM_TEXT,
            rect,
            color,
            child,
            text,
            text_length
          )) {
        break;
      }
      *cursor_y += rect.height;
    }
  }
}

static void layout_element(
  LayoutContext *context,
  NyblerNode *node,
  const NyblerRect *parent,
  float *cursor_y,
  NyblerColor inherited_color
) {
  size_t name_length = 0;
  const char *name = nybler_node_get_local_name(node, &name_length);
  int is_root = name && slice_equals_ci(name, name_length, "html");
  int is_body = name && slice_equals_ci(name, name_length, "body");
  LayoutStyle style = default_style(is_body);
  size_t style_length = 0;
  const char *style_value;
  NyblerRect rect;
  NyblerRect content;
  float child_y;
  size_t box_index;
  size_t entry_index;

  style.color = inherited_color;
  if (is_body) style.background = default_style(1).background;

  style_value = nybler_node_get_attribute(node, "style", 5, &style_length);
  apply_inline_style(&style, style_value, style_length, parent);
  apply_computed_style(context->layout->document, node, &style, parent);
  if (style.display_none) return;

  if (is_root) {
    rect.x = 0.0f;
    rect.y = 0.0f;
    rect.width = context->layout->viewport.width;
    rect.height = context->layout->viewport.height;
  } else {
    rect.x = parent->x + style.margin_left;
    rect.y = *cursor_y + style.margin_top;
    rect.width = style.has_width
      ? style.width
      : parent->width - style.margin_left - style.margin_right;
    rect.height = style.has_height ? style.height : 0.0f;
  }
  if (rect.width < 0.0f) rect.width = 0.0f;

  box_index = context->layout->box_count;
  entry_index = context->layout->entry_count;
  if (!append_box(context, node, rect)) return;
  if (!append_item(
        context,
        NYBLER_DISPLAY_ITEM_BOX,
        rect,
        style.background,
        node,
        NULL,
        0
      )) {
    return;
  }

  content.x = rect.x + style.padding_left;
  content.y = rect.y + style.padding_top;
  content.width = rect.width - style.padding_left - style.padding_right;
  if (content.width < 0.0f) content.width = 0.0f;
  content.height = style.has_height
    ? rect.height - style.padding_top - style.padding_bottom
    : context->layout->viewport.height;
  if (content.height < 0.0f) content.height = 0.0f;

  child_y = content.y;
  layout_children(context, node, &content, &child_y, style.color);
  if (context->failed) return;

  if (!is_root && !style.has_height) {
    rect.height = child_y - rect.y;
    if (rect.height < 0.0f) rect.height = 0.0f;
    context->layout->boxes[box_index].rect = rect;
    context->layout->entries[entry_index].item.rect = rect;
  }

  *cursor_y = rect.y + rect.height + style.margin_bottom;
}

NyblerResult nybler_layout_compute(
  NyblerDocument *document,
  const NyblerViewport *viewport,
  NyblerLayout **out_layout
) {
  NyblerLayout *layout;
  LayoutContext context;
  NyblerNode *root;
  float cursor_y = 0.0f;
  NyblerRect root_rect;
  NyblerColor inherited = {0.0f, 0.0f, 0.0f, 1.0f};

  if (!document || !viewport || !out_layout) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_layout = NULL;

  layout = (NyblerLayout *)calloc(1, sizeof(*layout));
  if (!layout) return NYBLER_ERROR_OUT_OF_MEMORY;
  layout->document = document;
  layout->viewport = *viewport;

  context.layout = layout;
  context.failed = 0;

  root = nybler_document_document_element(document);
  root_rect.x = 0.0f;
  root_rect.y = 0.0f;
  root_rect.width = viewport->width;
  root_rect.height = viewport->height;
  if (root) layout_element(&context, root, &root_rect, &cursor_y, inherited);

  if (context.failed) {
    nybler_layout_release(layout);
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }

  *out_layout = layout;
  return NYBLER_OK;
}

void nybler_layout_release(NyblerLayout *layout) {
  size_t i;

  if (!layout) return;
  for (i = 0; i < layout->entry_count; i++) {
    free(layout->entries[i].owned_text);
  }
  free(layout->entries);
  free(layout->boxes);
  free(layout);
}

int nybler_layout_get_rect(
  const NyblerLayout *layout,
  NyblerNode *node,
  NyblerRect *out_rect
) {
  size_t i;

  if (!layout || !node || !out_rect) return 0;
  for (i = 0; i < layout->box_count; i++) {
    if (layout->boxes[i].node == node) {
      *out_rect = layout->boxes[i].rect;
      return 1;
    }
  }
  return 0;
}

NyblerResult nybler_layout_display_list(
  const NyblerLayout *layout,
  NyblerDisplayList **out_list
) {
  NyblerDisplayList *list;
  size_t i;

  if (!layout || !out_list) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_list = NULL;

  list = (NyblerDisplayList *)calloc(1, sizeof(*list));
  if (!list) return NYBLER_ERROR_OUT_OF_MEMORY;
  if (layout->entry_count == 0) {
    *out_list = list;
    return NYBLER_OK;
  }

  list->items = (NyblerDisplayItem *)calloc(
    layout->entry_count,
    sizeof(*list->items)
  );
  if (!list->items) {
    free(list);
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }
  list->capacity = layout->entry_count;

  for (i = 0; i < layout->entry_count; i++) {
    const NyblerDisplayItem *source = &layout->entries[i].item;
    NyblerDisplayItem *target = &list->items[i];

    *target = *source;
    if (source->text && source->text_length) {
      char *copy = (char *)malloc(source->text_length + 1);
      if (!copy) {
        nybler_display_list_release(list);
        return NYBLER_ERROR_OUT_OF_MEMORY;
      }
      memcpy(copy, source->text, source->text_length);
      copy[source->text_length] = '\0';
      target->text = copy;
    }
    list->count++;
  }

  *out_list = list;
  return NYBLER_OK;
}

size_t nybler_display_list_length(const NyblerDisplayList *list) {
  return list ? list->count : 0;
}

const NyblerDisplayItem *nybler_display_list_item(
  const NyblerDisplayList *list,
  size_t index
) {
  if (!list || index >= list->count) return NULL;
  return &list->items[index];
}

static NyblerResult display_list_append_item(
  NyblerDisplayList *list,
  NyblerDisplayItem item,
  const char *text,
  size_t text_length
) {
  if (!grow_array(
        (void **)&list->items,
        &list->capacity,
        list->count,
        sizeof(*list->items)
      )) {
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }
  if (text && text_length) {
    char *copy = (char *)malloc(text_length + 1);
    if (!copy) return NYBLER_ERROR_OUT_OF_MEMORY;
    memcpy(copy, text, text_length);
    copy[text_length] = '\0';
    item.text = copy;
  } else {
    item.text = NULL;
  }
  item.text_length = text_length;
  list->items[list->count++] = item;
  return NYBLER_OK;
}

NyblerResult nybler_display_list_create(NyblerDisplayList **out_list) {
  NyblerDisplayList *list;

  if (!out_list) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_list = NULL;
  list = (NyblerDisplayList *)calloc(1, sizeof(*list));
  if (!list) return NYBLER_ERROR_OUT_OF_MEMORY;
  *out_list = list;
  return NYBLER_OK;
}

NyblerResult nybler_display_list_append_rect(
  NyblerDisplayList *list,
  const NyblerRect *rect,
  const NyblerColor *color
) {
  NyblerDisplayItem item;

  if (!list || !rect || !color) return NYBLER_ERROR_INVALID_ARGUMENT;
  memset(&item, 0, sizeof(item));
  item.kind = NYBLER_DISPLAY_ITEM_BOX;
  item.rect = *rect;
  item.color = *color;
  return display_list_append_item(list, item, NULL, 0);
}

NyblerResult nybler_display_list_append_text(
  NyblerDisplayList *list,
  const NyblerRect *rect,
  const NyblerColor *color,
  const char *text,
  size_t text_length
) {
  NyblerDisplayItem item;

  if (!list || !rect || !color || !text || !text_length) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  memset(&item, 0, sizeof(item));
  item.kind = NYBLER_DISPLAY_ITEM_TEXT;
  item.rect = *rect;
  item.color = *color;
  return display_list_append_item(list, item, text, text_length);
}

void nybler_display_list_release(NyblerDisplayList *list) {
  size_t i;

  if (!list) return;
  for (i = 0; i < list->count; i++) {
    if (list->items[i].text && list->items[i].text_length) {
      free((void *)list->items[i].text);
    }
  }
  free(list->items);
  free(list);
}
