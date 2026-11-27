#include "nybler/nybler.h"

#include <stdio.h>
#include <string.h>

static int check(int condition, const char *message) {
  if (condition) return 1;
  fprintf(stderr, "layout smoke failed: %s\n", message);
  return 0;
}

int main(void) {
  static const char html[] =
    "<div id='panel' style='width: 120px; height: 40px; background-color: #ff0000'>"
    "<h1 style='color: blue; margin-top: 4px'>hello world</h1>"
    "</div>";
  NyblerDocument *document = NULL;
  NyblerLayout *layout = NULL;
  NyblerDisplayList *list = NULL;
  NyblerNode **headings = NULL;
  NyblerNode **panels = NULL;
  size_t heading_count = 0;
  size_t panel_count = 0;
  size_t attribute_length = 0;
  size_t i;
  int ok = 1;
  NyblerViewport viewport = {800.0f, 600.0f};
  NyblerRect rect;
  NyblerRect div_rect = {0.0f, 0.0f, 0.0f, 0.0f};
  NyblerRect heading_rect = {0.0f, 0.0f, 0.0f, 0.0f};
  const char *attribute;
  int found_text = 0;

  if (!check(
        nybler_document_parse(html, sizeof(html) - 1, &document) == NYBLER_OK,
        "parse document"
      )) {
    return 1;
  }

  if (!check(
        nybler_document_elements_by_tag_name(
          document, "h1", 2, &headings, &heading_count
        ) == NYBLER_OK && heading_count == 1,
        "find h1"
      )) {
    ok = 0;
    goto done;
  }

  if (!check(
        nybler_document_elements_by_tag_name(
          document, "div", 3, &panels, &panel_count
        ) == NYBLER_OK && panel_count == 1,
        "find div"
      )) {
    ok = 0;
    goto done;
  }

  attribute = nybler_node_get_attribute(
    panels[0], "id", 2, &attribute_length
  );
  ok = check(
    attribute && attribute_length == 5 && memcmp(attribute, "panel", 5) == 0,
    "read id attribute"
  ) && ok;

  if (!check(
        nybler_layout_compute(document, &viewport, &layout) == NYBLER_OK,
        "compute layout"
      )) {
    ok = 0;
    goto done;
  }

  ok = check(nybler_layout_get_rect(layout, panels[0], &rect), "div rect") && ok;
  ok = check(rect.width == 120.0f, "div width") && ok;
  ok = check(rect.height == 40.0f, "div height") && ok;
  div_rect = rect;

  ok = check(nybler_layout_get_rect(layout, headings[0], &rect), "h1 rect") && ok;
  heading_rect = rect;
  ok = check(rect.width == 120.0f, "h1 width") && ok;
  ok = check(rect.y == 4.0f, "h1 margin-top") && ok;
  ok = check(rect.height == 16.0f, "h1 text height") && ok;

  if (!check(
        nybler_layout_display_list(layout, &list) == NYBLER_OK,
        "build display list"
      )) {
    ok = 0;
    goto done;
  }

  for (i = 0; i < nybler_display_list_length(list); i++) {
    const NyblerDisplayItem *item = nybler_display_list_item(list, i);
    if (!item || item->kind != NYBLER_DISPLAY_ITEM_TEXT) continue;
    if (item->text_length != 11 || memcmp(item->text, "hello world", 11) != 0) {
      continue;
    }
    found_text = 1;
    ok = check(item->color.b == 1.0f && item->color.r == 0.0f, "blue text") && ok;
    ok = check(item->rect.width == 88.0f, "text width") && ok;
  }
  ok = check(found_text, "hello world text item") && ok;

  printf(
    "nybler layout smoke: div %.0fx%.0f h1 %.0fx%.0f text items=%zu\n",
    div_rect.width,
    div_rect.height,
    heading_rect.width,
    heading_rect.height,
    nybler_display_list_length(list)
  );

done:
  nybler_display_list_release(list);
  nybler_layout_release(layout);
  nybler_node_list_release(headings, heading_count);
  nybler_node_list_release(panels, panel_count);
  nybler_document_release(document);
  return ok ? 0 : 1;
}
