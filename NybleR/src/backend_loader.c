#include "backend_loader.h"

#include <stddef.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

void *nybler_load_library(const char *const *names) {
  size_t i;

  if (!names) return NULL;
  for (i = 0; names[i]; ++i) {
#if defined(_WIN32)
    HMODULE library = LoadLibraryA(names[i]);
    if (library) return (void *)library;
#else
    void *library = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
    if (library) return library;
#endif
  }
  return NULL;
}

void nybler_unload_library(void *library) {
  if (!library) return;
#if defined(_WIN32)
  FreeLibrary((HMODULE)library);
#else
  dlclose(library);
#endif
}

void *nybler_load_symbol(void *library, const char *name) {
  if (!library || !name) return NULL;
#if defined(_WIN32)
  return (void *)GetProcAddress((HMODULE)library, name);
#else
  return dlsym(library, name);
#endif
}
