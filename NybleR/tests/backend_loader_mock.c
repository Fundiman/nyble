/* Shared object used by backend_loader_smoke to exercise the same dynamic
 * loading path that resolves Mesa's EGL/GLES entry points. */
#if defined(_WIN32)
#define NYBLER_MOCK_EXPORT __declspec(dllexport)
#else
#define NYBLER_MOCK_EXPORT
#endif

NYBLER_MOCK_EXPORT int nybler_backend_loader_test_symbol(void) {
  return 42;
}
