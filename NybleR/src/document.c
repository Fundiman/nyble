#include "nybler/nybler.h"

#include <lexbor/dom/dom.h>
#include <lexbor/html/html.h>

#include <stdlib.h>
#include <string.h>

struct NyblerDocument {
  lxb_html_document_t *document;
};

/* Node handles are borrowed pointers into the owning NyblerDocument. */
static lxb_dom_node_t *nybler_dom_node(NyblerNode *handle) {
  return (lxb_dom_node_t *)handle;
}

static NyblerNode *nybler_node_handle(lxb_dom_node_t *node) {
  return (NyblerNode *)node;
}

NyblerResult nybler_document_parse(
  const char *html,
  size_t length,
  NyblerDocument **out_document
) {
  lxb_html_parser_t *parser;
  lxb_html_document_t *document;
  NyblerDocument *result;

  if (!html || !out_document) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_document = NULL;

  parser = lxb_html_parser_create();
  if (!parser) return NYBLER_ERROR_OUT_OF_MEMORY;

  if (lxb_html_parser_init(parser) != LXB_STATUS_OK) {
    lxb_html_parser_destroy(parser);
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }

  document = lxb_html_parse(parser, (const lxb_char_t *)html, length);
  lxb_html_parser_destroy(parser);
  if (!document) return NYBLER_ERROR_OUT_OF_MEMORY;

  result = (NyblerDocument *)malloc(sizeof(*result));
  if (!result) {
    lxb_html_document_destroy(document);
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }

  result->document = document;
  *out_document = result;
  return NYBLER_OK;
}

void nybler_document_release(NyblerDocument *document) {
  if (!document) return;
  lxb_html_document_destroy(document->document);
  free(document);
}

NyblerNode *nybler_document_document_element(NyblerDocument *document) {
  lxb_dom_element_t *element;

  if (!document) return NULL;
  element = lxb_dom_document_element(&document->document->dom_document);
  if (!element) return NULL;
  return nybler_node_handle(lxb_dom_interface_node(element));
}

NyblerNode *nybler_document_body(NyblerDocument *document) {
  lxb_html_body_element_t *body;

  if (!document) return NULL;
  body = lxb_html_document_body_element(document->document);
  if (!body) return NULL;
  return nybler_node_handle(lxb_dom_interface_node(body));
}

NyblerResult nybler_document_elements_by_tag_name(
  NyblerDocument *document,
  const char *tag_name,
  size_t tag_length,
  NyblerNode ***out_elements,
  size_t *out_count
) {
  lxb_dom_document_t *dom_document;
  lxb_dom_element_t *root;
  lxb_dom_collection_t *collection;
  lxb_status_t status;
  NyblerNode **elements;
  size_t count;
  size_t i;

  if (!document || !tag_name || !out_elements || !out_count) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }

  *out_elements = NULL;
  *out_count = 0;

  dom_document = &document->document->dom_document;
  root = lxb_dom_document_element(dom_document);
  if (!root) return NYBLER_OK;

  collection = lxb_dom_collection_make(dom_document, 16);
  if (!collection) return NYBLER_ERROR_OUT_OF_MEMORY;

  status = lxb_dom_elements_by_tag_name(
    root,
    collection,
    (const lxb_char_t *)tag_name,
    tag_length
  );
  if (status != LXB_STATUS_OK) {
    lxb_dom_collection_destroy(collection, true);
    return NYBLER_ERROR_UNSUPPORTED;
  }

  count = lxb_dom_collection_length(collection);
  if (count == 0) {
    lxb_dom_collection_destroy(collection, true);
    return NYBLER_OK;
  }

  elements = (NyblerNode **)malloc(count * sizeof(*elements));
  if (!elements) {
    lxb_dom_collection_destroy(collection, true);
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }

  for (i = 0; i < count; i++) {
    lxb_dom_element_t *element = lxb_dom_collection_element(collection, i);
    elements[i] = nybler_node_handle(lxb_dom_interface_node(element));
  }

  lxb_dom_collection_destroy(collection, true);
  *out_elements = elements;
  *out_count = count;
  return NYBLER_OK;
}

void nybler_node_list_release(NyblerNode **elements, size_t count) {
  (void)count;
  free(elements);
}

NyblerNodeType nybler_node_get_node_type(NyblerNode *handle) {
  lxb_dom_node_t *node;

  if (!handle) return NYBLER_NODE_TYPE_OTHER;
  node = nybler_dom_node(handle);

  switch (lxb_dom_node_type(node)) {
    case LXB_DOM_NODE_TYPE_ELEMENT:
      return NYBLER_NODE_TYPE_ELEMENT;
    case LXB_DOM_NODE_TYPE_ATTRIBUTE:
      return NYBLER_NODE_TYPE_ATTRIBUTE;
    case LXB_DOM_NODE_TYPE_TEXT:
      return NYBLER_NODE_TYPE_TEXT;
    case LXB_DOM_NODE_TYPE_COMMENT:
      return NYBLER_NODE_TYPE_COMMENT;
    case LXB_DOM_NODE_TYPE_DOCUMENT:
      return NYBLER_NODE_TYPE_DOCUMENT;
    case LXB_DOM_NODE_TYPE_DOCUMENT_TYPE:
      return NYBLER_NODE_TYPE_DOCUMENT_TYPE;
    case LXB_DOM_NODE_TYPE_DOCUMENT_FRAGMENT:
      return NYBLER_NODE_TYPE_DOCUMENT_FRAGMENT;
    default:
      return NYBLER_NODE_TYPE_OTHER;
  }
}

NyblerNode *nybler_node_get_first_child(NyblerNode *handle) {
  lxb_dom_node_t *node;

  if (!handle) return NULL;
  node = nybler_dom_node(handle);
  if (!node->first_child) return NULL;
  return nybler_node_handle(node->first_child);
}

NyblerNode *nybler_node_get_next_sibling(NyblerNode *handle) {
  lxb_dom_node_t *node;

  if (!handle) return NULL;
  node = nybler_dom_node(handle);
  if (!node->next) return NULL;
  return nybler_node_handle(node->next);
}

const char *nybler_node_get_local_name(
  NyblerNode *handle,
  size_t *out_length
) {
  lxb_dom_node_t *node;
  const lxb_char_t *name;

  if (out_length) *out_length = 0;
  if (!handle) return NULL;

  node = nybler_dom_node(handle);
  if (lxb_dom_node_type(node) == LXB_DOM_NODE_TYPE_ELEMENT) {
    name = lxb_dom_element_local_name(lxb_dom_interface_element(node), out_length);
  } else {
    name = lxb_dom_node_name(node, out_length);
  }

  return (const char *)name;
}

const char *nybler_node_get_attribute(
  NyblerNode *handle,
  const char *name,
  size_t name_length,
  size_t *out_length
) {
  lxb_dom_node_t *node;
  const lxb_char_t *value;

  if (out_length) *out_length = 0;
  if (!handle || !name) return NULL;

  node = nybler_dom_node(handle);
  if (lxb_dom_node_type(node) != LXB_DOM_NODE_TYPE_ELEMENT) return NULL;

  value = lxb_dom_element_get_attribute(
    lxb_dom_interface_element(node),
    (const lxb_char_t *)name,
    name_length,
    out_length
  );
  return (const char *)value;
}

NyblerResult nybler_node_get_text_content(
  NyblerNode *handle,
  char **out_text,
  size_t *out_length
) {
  lxb_dom_node_t *node;
  const lxb_char_t *text;
  size_t length = 0;
  char *copy;

  if (!handle || !out_text || !out_length) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }

  *out_text = NULL;
  *out_length = 0;

  node = nybler_dom_node(handle);
  text = lxb_dom_node_text_content(node, &length);

  copy = (char *)malloc(length + 1);
  if (!copy) return NYBLER_ERROR_OUT_OF_MEMORY;

  if (text && length) memcpy(copy, text, length);
  copy[length] = '\0';

  *out_text = copy;
  *out_length = length;
  return NYBLER_OK;
}

void nybler_string_release(char *value) {
  free(value);
}
