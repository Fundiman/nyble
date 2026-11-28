#ifndef NYBLER_BACKEND_LOADER_H_
#define NYBLER_BACKEND_LOADER_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Platform abstraction used by the Mesa backend to resolve EGL/GLES entry
 * points without linking against a particular driver. Exposed internally so
 * the smoke suite can test the dynamic loader on each supported platform. */
void *nybler_load_library(const char *const *names);
void nybler_unload_library(void *library);
void *nybler_load_symbol(void *library, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* NYBLER_BACKEND_LOADER_H_ */
