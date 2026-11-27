#include "nybler/nybler.h"

#ifndef NYBLER_VERSION_MAJOR
#define NYBLER_VERSION_MAJOR 0
#endif
#ifndef NYBLER_VERSION_MINOR
#define NYBLER_VERSION_MINOR 1
#endif
#ifndef NYBLER_VERSION_PATCH
#define NYBLER_VERSION_PATCH 0
#endif

#define NYBLER_STRINGIFY_IMPL(x) #x
#define NYBLER_STRINGIFY(x) NYBLER_STRINGIFY_IMPL(x)

#define NYBLER_VERSION_TEXT \
  NYBLER_STRINGIFY(NYBLER_VERSION_MAJOR) "." \
  NYBLER_STRINGIFY(NYBLER_VERSION_MINOR) "." \
  NYBLER_STRINGIFY(NYBLER_VERSION_PATCH)

const char *nybler_version_string(void) {
  return NYBLER_VERSION_TEXT;
}

uint32_t nybler_version_number(void) {
  return ((uint32_t)NYBLER_VERSION_MAJOR << 24) |
         ((uint32_t)NYBLER_VERSION_MINOR << 16) |
         (uint32_t)NYBLER_VERSION_PATCH;
}
