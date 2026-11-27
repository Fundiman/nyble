#include "nybler/nybler.h"

#include <stdio.h>
#include <string.h>

static int is_space(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static const char *trim(const char *text, size_t *len) {
  while (*len && is_space((unsigned char)text[0])) {
    text++;
    (*len)--;
  }
  while (*len && is_space((unsigned char)text[*len - 1])) {
    (*len)--;
  }
  return text;
}

int main(void) {
  static const char html[] = "<h1> hello world </h1>";
  static const char expected[] = "hello world";

  NyblerDocument *document = NULL;
  NyblerNode **elements = NULL;
  NyblerResult result;
  size_t count = 0;
  size_t name_length = 0;
  size_t text_length = 0;
  char *text = NULL;
  const char *trimmed;
  int ok;

  result = nybler_document_parse(html, sizeof(html) - 1, &document);
  if (result != NYBLER_OK || !document) return 2;

  result = nybler_document_elements_by_tag_name(
    document, "h1", 2, &elements, &count
  );
  if (result != NYBLER_OK || count != 1) {
    nybler_document_release(document);
    return 3;
  }

  if (nybler_node_get_node_type(elements[0]) != NYBLER_NODE_TYPE_ELEMENT) {
    nybler_node_list_release(elements, count);
    nybler_document_release(document);
    return 4;
  }

  if (!nybler_node_get_local_name(elements[0], &name_length) ||
      name_length != 2 ||
      memcmp(nybler_node_get_local_name(elements[0], NULL), "h1", 2) != 0) {
    nybler_node_list_release(elements, count);
    nybler_document_release(document);
    return 5;
  }

  result = nybler_node_get_text_content(elements[0], &text, &text_length);
  if (result != NYBLER_OK || !text) {
    nybler_node_list_release(elements, count);
    nybler_document_release(document);
    return 6;
  }

  trimmed = trim(text, &text_length);
  ok = text_length == strlen(expected) &&
       memcmp(trimmed, expected, text_length) == 0;

  printf("%.*s\n", (int)text_length, trimmed);

  nybler_string_release(text);
  nybler_node_list_release(elements, count);
  nybler_document_release(document);

  if (!ok) return 7;
  puts("nybler dom smoke: <h1> hello world </h1> -> hello world");
  return 0;
}
