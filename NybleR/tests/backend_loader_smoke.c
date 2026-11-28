#include "backend_loader.h"

#include <stdio.h>
#include <string.h>

typedef int (*TestSymbolFn)(void);

int main(int argc, char **argv) {
  static const char *const missing[] = {
    "nybler_no_such_backend_library.so",
    NULL
  };
  const char *names[2];
  void *library;
  void *symbol;
  TestSymbolFn function;
  int ok = 1;

  if (argc != 2) {
    fprintf(stderr, "usage: backend_loader_smoke <mock-library>\n");
    return 1;
  }

  if (nybler_load_library(missing) != NULL) {
    fprintf(stderr, "backend loader smoke failed: missing library loaded\n");
    ok = 0;
  }

  names[0] = argv[1];
  names[1] = NULL;
  library = nybler_load_library(names);
  if (!library) {
    fprintf(stderr, "backend loader smoke failed: mock library not loaded\n");
    return 1;
  }

  symbol = nybler_load_symbol(library, "nybler_backend_loader_test_symbol");
  function = (TestSymbolFn)symbol;
  if (!function || function() != 42) {
    fprintf(stderr, "backend loader smoke failed: symbol not resolved\n");
    ok = 0;
  }
  if (nybler_load_symbol(library, "nybler_missing_symbol") != NULL) {
    fprintf(stderr, "backend loader smoke failed: missing symbol resolved\n");
    ok = 0;
  }
  nybler_unload_library(library);

  if (ok) puts("nybler backend loader smoke: load/symbol/unload ok");
  return ok ? 0 : 1;
}
