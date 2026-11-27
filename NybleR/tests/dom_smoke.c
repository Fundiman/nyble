#include <lexbor/dom/dom.h>
#include <lexbor/html/html.h>

#include <stdio.h>
#include <string.h>

static int is_space(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static const lxb_char_t *trim(const lxb_char_t *text, size_t *len) {
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
  static const lxb_char_t html[] = "<h1> hello world </h1>";
  static const char expected[] = "hello world";

  lxb_status_t status;
  lxb_html_parser_t *parser = lxb_html_parser_create();
  if (!parser) return 2;

  status = lxb_html_parser_init(parser);
  if (status != LXB_STATUS_OK) {
    lxb_html_parser_destroy(parser);
    return 3;
  }

  lxb_html_document_t *document = lxb_html_parse(parser, html, sizeof(html) - 1);
  lxb_html_parser_destroy(parser);
  if (!document) return 4;

  lxb_dom_collection_t *collection =
    lxb_dom_collection_make(&document->dom_document, 16);
  if (!collection) {
    lxb_html_document_destroy(document);
    return 5;
  }

  status = lxb_dom_elements_by_tag_name(
    lxb_dom_interface_element(document->body),
    collection,
    (const lxb_char_t *)"h1",
    2
  );
  if (status != LXB_STATUS_OK || lxb_dom_collection_length(collection) != 1) {
    lxb_dom_collection_destroy(collection, true);
    lxb_html_document_destroy(document);
    return 6;
  }

  lxb_dom_element_t *element = lxb_dom_collection_element(collection, 0);
  size_t text_len = 0;
  const lxb_char_t *text =
    lxb_dom_node_text_content(lxb_dom_interface_node(element), &text_len);

  if (!text) {
    lxb_dom_collection_destroy(collection, true);
    lxb_html_document_destroy(document);
    return 7;
  }

  text = trim(text, &text_len);
  int ok = text_len == strlen(expected) &&
           memcmp(text, expected, text_len) == 0;

  printf("%.*s", (int)text_len, (const char *)text);
  printf("\n");

  lxb_dom_collection_destroy(collection, true);
  lxb_html_document_destroy(document);

  if (!ok) return 8;
  puts("nybler dom smoke: <h1> hello world </h1> -> hello world");
  return 0;
}
