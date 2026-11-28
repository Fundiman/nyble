#include "mesa_backend.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#if !defined(_WIN64)
#define NYBLER_APIENTRY __stdcall
#endif
#else
#include <dlfcn.h>
#endif

#ifndef NYBLER_APIENTRY
#define NYBLER_APIENTRY
#endif

typedef void *NyblerEGLDisplay;
typedef void *NyblerEGLConfig;
typedef void *NyblerEGLContext;
typedef void *NyblerEGLSurface;
typedef int NyblerEGLBoolean;
typedef int NyblerEGLint;
typedef unsigned int NyblerEGLenum;

typedef unsigned int NyblerGLenum;
typedef int NyblerGLint;
typedef int NyblerGLsizei;
typedef unsigned int NyblerGLuint;
typedef unsigned char NyblerGLboolean;
typedef ptrdiff_t NyblerGLsizeiptr;

#define NYBLER_EGL_NONE 0x3038
#define NYBLER_EGL_SURFACE_TYPE 0x3033
#define NYBLER_EGL_PBUFFER_BIT 0x0001
#define NYBLER_EGL_RENDERABLE_TYPE 0x3040
#define NYBLER_EGL_OPENGL_ES3_BIT 0x0040
#define NYBLER_EGL_RED_SIZE 0x3024
#define NYBLER_EGL_GREEN_SIZE 0x3023
#define NYBLER_EGL_BLUE_SIZE 0x3022
#define NYBLER_EGL_ALPHA_SIZE 0x3021
#define NYBLER_EGL_WIDTH 0x3057
#define NYBLER_EGL_HEIGHT 0x3056
#define NYBLER_EGL_CONTEXT_CLIENT_VERSION 0x3098
#define NYBLER_EGL_PLATFORM_SURFACELESS_MESA 0x31DD

#define NYBLER_GL_POINTS 0x0000
#define NYBLER_GL_LINES 0x0001
#define NYBLER_GL_LINE_STRIP 0x0003
#define NYBLER_GL_TRIANGLES 0x0004
#define NYBLER_GL_TRIANGLE_STRIP 0x0005
#define NYBLER_GL_FALSE 0
#define NYBLER_GL_FLOAT 0x1406
#define NYBLER_GL_UNSIGNED_BYTE 0x1401
#define NYBLER_GL_RGBA 0x1908
#define NYBLER_GL_RGBA8 0x8058
#define NYBLER_GL_TEXTURE_2D 0x0DE1
#define NYBLER_GL_TEXTURE_MIN_FILTER 0x2801
#define NYBLER_GL_TEXTURE_MAG_FILTER 0x2800
#define NYBLER_GL_NEAREST 0x2600
#define NYBLER_GL_FRAMEBUFFER 0x8D40
#define NYBLER_GL_COLOR_ATTACHMENT0 0x8CE0
#define NYBLER_GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define NYBLER_GL_PACK_ALIGNMENT 0x0D05
#define NYBLER_GL_ARRAY_BUFFER 0x8892
#define NYBLER_GL_STATIC_DRAW 0x88E4
#define NYBLER_GL_COLOR_BUFFER_BIT 0x00004000
#define NYBLER_GL_VERTEX_SHADER 0x8B31
#define NYBLER_GL_FRAGMENT_SHADER 0x8B30
#define NYBLER_GL_COMPILE_STATUS 0x8B81
#define NYBLER_GL_LINK_STATUS 0x8B82

typedef NyblerEGLBoolean(NYBLER_APIENTRY *NyblerEGLInitializeFn)(
  NyblerEGLDisplay display,
  NyblerEGLint *major,
  NyblerEGLint *minor
);
typedef NyblerEGLBoolean(NYBLER_APIENTRY *NyblerEGLTerminateFn)(
  NyblerEGLDisplay display
);
typedef NyblerEGLDisplay(NYBLER_APIENTRY *NyblerEGLGetDisplayFn)(
  void *native_display
);
typedef NyblerEGLDisplay(NYBLER_APIENTRY *NyblerEGLGetPlatformDisplayFn)(
  NyblerEGLenum platform,
  void *native_display,
  const NyblerEGLint *attributes
);
typedef void *(NYBLER_APIENTRY *NyblerEGLGetProcAddressFn)(const char *name);
typedef NyblerEGLBoolean(NYBLER_APIENTRY *NyblerEGLChooseConfigFn)(
  NyblerEGLDisplay display,
  const NyblerEGLint *attributes,
  NyblerEGLConfig *configs,
  NyblerEGLint max_count,
  NyblerEGLint *count
);
typedef NyblerEGLContext(NYBLER_APIENTRY *NyblerEGLCreateContextFn)(
  NyblerEGLDisplay display,
  NyblerEGLConfig config,
  NyblerEGLContext share_context,
  const NyblerEGLint *attributes
);
typedef NyblerEGLSurface(NYBLER_APIENTRY *NyblerEGLCreatePbufferSurfaceFn)(
  NyblerEGLDisplay display,
  NyblerEGLConfig config,
  const NyblerEGLint *attributes
);
typedef NyblerEGLBoolean(NYBLER_APIENTRY *NyblerEGLMakeCurrentFn)(
  NyblerEGLDisplay display,
  NyblerEGLSurface draw,
  NyblerEGLSurface read,
  NyblerEGLContext context
);
typedef NyblerEGLBoolean(NYBLER_APIENTRY *NyblerEGLDestroySurfaceFn)(
  NyblerEGLDisplay display,
  NyblerEGLSurface surface
);
typedef NyblerEGLBoolean(NYBLER_APIENTRY *NyblerEGLDestroyContextFn)(
  NyblerEGLDisplay display,
  NyblerEGLContext context
);

typedef NyblerGLuint(NYBLER_APIENTRY *NyblerGLGenFramebuffersFn)(
  NyblerGLsizei count,
  NyblerGLuint *framebuffers
);
typedef void(NYBLER_APIENTRY *NyblerGLBindFramebufferFn)(
  NyblerGLenum target,
  NyblerGLuint framebuffer
);
typedef void(NYBLER_APIENTRY *NyblerGLFramebufferTexture2DFn)(
  NyblerGLenum target,
  NyblerGLenum attachment,
  NyblerGLenum textarget,
  NyblerGLuint texture,
  NyblerGLint level
);
typedef NyblerGLenum(NYBLER_APIENTRY *NyblerGLCheckFramebufferStatusFn)(
  NyblerGLenum target
);
typedef void(NYBLER_APIENTRY *NyblerGLDeleteFramebuffersFn)(
  NyblerGLsizei count,
  const NyblerGLuint *framebuffers
);
typedef void(NYBLER_APIENTRY *NyblerGLGenTexturesFn)(
  NyblerGLsizei count,
  NyblerGLuint *textures
);
typedef void(NYBLER_APIENTRY *NyblerGLBindTextureFn)(
  NyblerGLenum target,
  NyblerGLuint texture
);
typedef void(NYBLER_APIENTRY *NyblerGLTexImage2DFn)(
  NyblerGLenum target,
  NyblerGLint level,
  NyblerGLint internal_format,
  NyblerGLsizei width,
  NyblerGLsizei height,
  NyblerGLint border,
  NyblerGLenum format,
  NyblerGLenum type,
  const void *data
);
typedef void(NYBLER_APIENTRY *NyblerGLTexParameteriFn)(
  NyblerGLenum target,
  NyblerGLenum parameter,
  NyblerGLint value
);
typedef void(NYBLER_APIENTRY *NyblerGLDeleteTexturesFn)(
  NyblerGLsizei count,
  const NyblerGLuint *textures
);
typedef void(NYBLER_APIENTRY *NyblerGLViewportFn)(
  NyblerGLint x,
  NyblerGLint y,
  NyblerGLsizei width,
  NyblerGLsizei height
);
typedef void(NYBLER_APIENTRY *NyblerGLClearColorFn)(
  float red,
  float green,
  float blue,
  float alpha
);
typedef void(NYBLER_APIENTRY *NyblerGLClearFn)(NyblerGLenum mask);
typedef void(NYBLER_APIENTRY *NyblerGLReadPixelsFn)(
  NyblerGLint x,
  NyblerGLint y,
  NyblerGLsizei width,
  NyblerGLsizei height,
  NyblerGLenum format,
  NyblerGLenum type,
  void *pixels
);
typedef void(NYBLER_APIENTRY *NyblerGLPixelStoreiFn)(
  NyblerGLenum parameter,
  NyblerGLint value
);
typedef void(NYBLER_APIENTRY *NyblerGLGenBuffersFn)(
  NyblerGLsizei count,
  NyblerGLuint *buffers
);
typedef void(NYBLER_APIENTRY *NyblerGLBindBufferFn)(
  NyblerGLenum target,
  NyblerGLuint buffer
);
typedef void(NYBLER_APIENTRY *NyblerGLBufferDataFn)(
  NyblerGLenum target,
  NyblerGLsizeiptr size,
  const void *data,
  NyblerGLenum usage
);
typedef void(NYBLER_APIENTRY *NyblerGLDeleteBuffersFn)(
  NyblerGLsizei count,
  const NyblerGLuint *buffers
);
typedef void(NYBLER_APIENTRY *NyblerGLEnableVertexAttribArrayFn)(
  NyblerGLuint index
);
typedef void(NYBLER_APIENTRY *NyblerGLDisableVertexAttribArrayFn)(
  NyblerGLuint index
);
typedef void(NYBLER_APIENTRY *NyblerGLVertexAttribPointerFn)(
  NyblerGLuint index,
  NyblerGLint size,
  NyblerGLenum type,
  NyblerGLboolean normalized,
  NyblerGLsizei stride,
  const void *pointer
);
typedef void(NYBLER_APIENTRY *NyblerGLDrawArraysFn)(
  NyblerGLenum mode,
  NyblerGLint first,
  NyblerGLsizei count
);
typedef NyblerGLuint(NYBLER_APIENTRY *NyblerGLCreateShaderFn)(
  NyblerGLenum type
);
typedef void(NYBLER_APIENTRY *NyblerGLShaderSourceFn)(
  NyblerGLuint shader,
  NyblerGLsizei count,
  const char *const *strings,
  const NyblerGLint *lengths
);
typedef void(NYBLER_APIENTRY *NyblerGLCompileShaderFn)(NyblerGLuint shader);
typedef void(NYBLER_APIENTRY *NyblerGLGetShaderivFn)(
  NyblerGLuint shader,
  NyblerGLenum parameter,
  NyblerGLint *value
);
typedef void(NYBLER_APIENTRY *NyblerGLGetShaderInfoLogFn)(
  NyblerGLuint shader,
  NyblerGLsizei max_length,
  NyblerGLsizei *length,
  char *log
);
typedef void(NYBLER_APIENTRY *NyblerGLDeleteShaderFn)(NyblerGLuint shader);
typedef NyblerGLuint(NYBLER_APIENTRY *NyblerGLCreateProgramFn)(void);
typedef void(NYBLER_APIENTRY *NyblerGLAttachShaderFn)(
  NyblerGLuint program,
  NyblerGLuint shader
);
typedef void(NYBLER_APIENTRY *NyblerGLLinkProgramFn)(NyblerGLuint program);
typedef void(NYBLER_APIENTRY *NyblerGLGetProgramivFn)(
  NyblerGLuint program,
  NyblerGLenum parameter,
  NyblerGLint *value
);
typedef void(NYBLER_APIENTRY *NyblerGLGetProgramInfoLogFn)(
  NyblerGLuint program,
  NyblerGLsizei max_length,
  NyblerGLsizei *length,
  char *log
);
typedef void(NYBLER_APIENTRY *NyblerGLUseProgramFn)(NyblerGLuint program);
typedef void(NYBLER_APIENTRY *NyblerGLDeleteProgramFn)(NyblerGLuint program);
typedef void(NYBLER_APIENTRY *NyblerGLFinishFn)(void);

struct NyblerMesaContext {
  void *egl_library;
  void *gles_library;
  NyblerEGLDisplay display;
  NyblerEGLContext context;
  NyblerEGLSurface surface;
  int ready;

  NyblerEGLInitializeFn egl_initialize;
  NyblerEGLTerminateFn egl_terminate;
  NyblerEGLGetDisplayFn egl_get_display;
  NyblerEGLGetProcAddressFn egl_get_proc_address;
  NyblerEGLChooseConfigFn egl_choose_config;
  NyblerEGLCreateContextFn egl_create_context;
  NyblerEGLCreatePbufferSurfaceFn egl_create_pbuffer_surface;
  NyblerEGLMakeCurrentFn egl_make_current;
  NyblerEGLDestroySurfaceFn egl_destroy_surface;
  NyblerEGLDestroyContextFn egl_destroy_context;

  NyblerGLGenFramebuffersFn gl_gen_framebuffers;
  NyblerGLBindFramebufferFn gl_bind_framebuffer;
  NyblerGLFramebufferTexture2DFn gl_framebuffer_texture_2d;
  NyblerGLCheckFramebufferStatusFn gl_check_framebuffer_status;
  NyblerGLDeleteFramebuffersFn gl_delete_framebuffers;
  NyblerGLGenTexturesFn gl_gen_textures;
  NyblerGLBindTextureFn gl_bind_texture;
  NyblerGLTexImage2DFn gl_tex_image_2d;
  NyblerGLTexParameteriFn gl_tex_parameteri;
  NyblerGLDeleteTexturesFn gl_delete_textures;
  NyblerGLViewportFn gl_viewport;
  NyblerGLClearColorFn gl_clear_color;
  NyblerGLClearFn gl_clear;
  NyblerGLReadPixelsFn gl_read_pixels;
  NyblerGLPixelStoreiFn gl_pixel_storei;
  NyblerGLGenBuffersFn gl_gen_buffers;
  NyblerGLBindBufferFn gl_bind_buffer;
  NyblerGLBufferDataFn gl_buffer_data;
  NyblerGLDeleteBuffersFn gl_delete_buffers;
  NyblerGLEnableVertexAttribArrayFn gl_enable_vertex_attrib_array;
  NyblerGLDisableVertexAttribArrayFn gl_disable_vertex_attrib_array;
  NyblerGLVertexAttribPointerFn gl_vertex_attrib_pointer;
  NyblerGLDrawArraysFn gl_draw_arrays;
  NyblerGLCreateShaderFn gl_create_shader;
  NyblerGLShaderSourceFn gl_shader_source;
  NyblerGLCompileShaderFn gl_compile_shader;
  NyblerGLGetShaderivFn gl_get_shaderiv;
  NyblerGLGetShaderInfoLogFn gl_get_shader_info_log;
  NyblerGLDeleteShaderFn gl_delete_shader;
  NyblerGLCreateProgramFn gl_create_program;
  NyblerGLAttachShaderFn gl_attach_shader;
  NyblerGLLinkProgramFn gl_link_program;
  NyblerGLGetProgramivFn gl_get_programiv;
  NyblerGLGetProgramInfoLogFn gl_get_program_info_log;
  NyblerGLUseProgramFn gl_use_program;
  NyblerGLDeleteProgramFn gl_delete_program;
  NyblerGLFinishFn gl_finish;
};

typedef struct NyblerMesaTexture {
  NyblerGLuint texture;
  NyblerGLuint framebuffer;
  NyblerGLint width;
  NyblerGLint height;
} NyblerMesaTexture;

typedef struct NyblerMesaPipeline {
  NyblerGLuint program;
} NyblerMesaPipeline;

static void *nybler_load_library(const char *const *names) {
  size_t i;

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

static void nybler_unload_library(void *library) {
  if (!library) return;
#if defined(_WIN32)
  FreeLibrary((HMODULE)library);
#else
  dlclose(library);
#endif
}

static void *nybler_load_symbol(void *library, const char *name) {
  if (!library) return NULL;
#if defined(_WIN32)
  return (void *)GetProcAddress((HMODULE)library, name);
#else
  return dlsym(library, name);
#endif
}

/* The loader macros build type names from the exported symbol name. */
#define NyblerEGLeglInitializeFn NyblerEGLInitializeFn
#define NyblerEGLeglTerminateFn NyblerEGLTerminateFn
#define NyblerEGLeglGetDisplayFn NyblerEGLGetDisplayFn
#define NyblerEGLeglGetProcAddressFn NyblerEGLGetProcAddressFn
#define NyblerEGLeglChooseConfigFn NyblerEGLChooseConfigFn
#define NyblerEGLeglCreateContextFn NyblerEGLCreateContextFn
#define NyblerEGLeglCreatePbufferSurfaceFn NyblerEGLCreatePbufferSurfaceFn
#define NyblerEGLeglMakeCurrentFn NyblerEGLMakeCurrentFn
#define NyblerEGLeglDestroySurfaceFn NyblerEGLDestroySurfaceFn
#define NyblerEGLeglDestroyContextFn NyblerEGLDestroyContextFn

#define NyblerGLglGenFramebuffersFn NyblerGLGenFramebuffersFn
#define NyblerGLglBindFramebufferFn NyblerGLBindFramebufferFn
#define NyblerGLglFramebufferTexture2DFn NyblerGLFramebufferTexture2DFn
#define NyblerGLglCheckFramebufferStatusFn NyblerGLCheckFramebufferStatusFn
#define NyblerGLglDeleteFramebuffersFn NyblerGLDeleteFramebuffersFn
#define NyblerGLglGenTexturesFn NyblerGLGenTexturesFn
#define NyblerGLglBindTextureFn NyblerGLBindTextureFn
#define NyblerGLglTexImage2DFn NyblerGLTexImage2DFn
#define NyblerGLglTexParameteriFn NyblerGLTexParameteriFn
#define NyblerGLglDeleteTexturesFn NyblerGLDeleteTexturesFn
#define NyblerGLglViewportFn NyblerGLViewportFn
#define NyblerGLglClearColorFn NyblerGLClearColorFn
#define NyblerGLglClearFn NyblerGLClearFn
#define NyblerGLglReadPixelsFn NyblerGLReadPixelsFn
#define NyblerGLglPixelStoreiFn NyblerGLPixelStoreiFn
#define NyblerGLglGenBuffersFn NyblerGLGenBuffersFn
#define NyblerGLglBindBufferFn NyblerGLBindBufferFn
#define NyblerGLglBufferDataFn NyblerGLBufferDataFn
#define NyblerGLglDeleteBuffersFn NyblerGLDeleteBuffersFn
#define NyblerGLglEnableVertexAttribArrayFn NyblerGLEnableVertexAttribArrayFn
#define NyblerGLglDisableVertexAttribArrayFn NyblerGLDisableVertexAttribArrayFn
#define NyblerGLglVertexAttribPointerFn NyblerGLVertexAttribPointerFn
#define NyblerGLglDrawArraysFn NyblerGLDrawArraysFn
#define NyblerGLglCreateShaderFn NyblerGLCreateShaderFn
#define NyblerGLglShaderSourceFn NyblerGLShaderSourceFn
#define NyblerGLglCompileShaderFn NyblerGLCompileShaderFn
#define NyblerGLglGetShaderivFn NyblerGLGetShaderivFn
#define NyblerGLglGetShaderInfoLogFn NyblerGLGetShaderInfoLogFn
#define NyblerGLglDeleteShaderFn NyblerGLDeleteShaderFn
#define NyblerGLglCreateProgramFn NyblerGLCreateProgramFn
#define NyblerGLglAttachShaderFn NyblerGLAttachShaderFn
#define NyblerGLglLinkProgramFn NyblerGLLinkProgramFn
#define NyblerGLglGetProgramivFn NyblerGLGetProgramivFn
#define NyblerGLglGetProgramInfoLogFn NyblerGLGetProgramInfoLogFn
#define NyblerGLglUseProgramFn NyblerGLUseProgramFn
#define NyblerGLglDeleteProgramFn NyblerGLDeleteProgramFn
#define NyblerGLglFinishFn NyblerGLFinishFn

#define NYBLER_LOAD_EGL(context, field, name) \
  do { \
    (context)->field = (NyblerEGL##name##Fn)nybler_load_symbol( \
      (context)->egl_library, #name); \
    if (!(context)->field) goto fail; \
  } while (0)

#define NYBLER_LOAD_GLES(context, field, name) \
  do { \
    (context)->field = (NyblerGL##name##Fn)nybler_load_symbol( \
      (context)->gles_library, #name); \
    if (!(context)->field) goto fail; \
  } while (0)

NyblerResult nybler_mesa_init(NyblerMesaContext **out_context) {
  static const char *const egl_names[] = {
#if defined(_WIN32)
    "libEGL.dll", "EGL.dll",
#else
    "libEGL.so.1", "libEGL.so",
#endif
    NULL
  };
  static const char *const gles_names[] = {
#if defined(_WIN32)
    "libGLESv2.dll", "GLESv2.dll",
#else
    "libGLESv2.so.2", "libGLESv2.so",
#endif
    NULL
  };
  NyblerEGLint config_count = 0;
  NyblerEGLConfig config = NULL;
  NyblerEGLDisplay display = NULL;
  const NyblerEGLint config_attributes[] = {
    NYBLER_EGL_SURFACE_TYPE, NYBLER_EGL_PBUFFER_BIT,
    NYBLER_EGL_RENDERABLE_TYPE, NYBLER_EGL_OPENGL_ES3_BIT,
    NYBLER_EGL_RED_SIZE, 8,
    NYBLER_EGL_GREEN_SIZE, 8,
    NYBLER_EGL_BLUE_SIZE, 8,
    NYBLER_EGL_ALPHA_SIZE, 8,
    NYBLER_EGL_NONE
  };
  const NyblerEGLint context_attributes[] = {
    NYBLER_EGL_CONTEXT_CLIENT_VERSION, 3,
    NYBLER_EGL_NONE
  };
  const NyblerEGLint surface_attributes[] = {
    NYBLER_EGL_WIDTH, 1,
    NYBLER_EGL_HEIGHT, 1,
    NYBLER_EGL_NONE
  };
  NyblerEGLGetPlatformDisplayFn get_platform_display = NULL;
  NyblerMesaContext *context;

  if (!out_context) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_context = NULL;

  context = (NyblerMesaContext *)calloc(1, sizeof(*context));
  if (!context) return NYBLER_ERROR_OUT_OF_MEMORY;

  context->egl_library = nybler_load_library(egl_names);
  context->gles_library = nybler_load_library(gles_names);
  if (!context->egl_library || !context->gles_library) goto fail;

  NYBLER_LOAD_EGL(context, egl_initialize, eglInitialize);
  NYBLER_LOAD_EGL(context, egl_terminate, eglTerminate);
  NYBLER_LOAD_EGL(context, egl_get_display, eglGetDisplay);
  NYBLER_LOAD_EGL(context, egl_get_proc_address, eglGetProcAddress);
  NYBLER_LOAD_EGL(context, egl_choose_config, eglChooseConfig);
  NYBLER_LOAD_EGL(context, egl_create_context, eglCreateContext);
  NYBLER_LOAD_EGL(context, egl_create_pbuffer_surface, eglCreatePbufferSurface);
  NYBLER_LOAD_EGL(context, egl_make_current, eglMakeCurrent);
  NYBLER_LOAD_EGL(context, egl_destroy_surface, eglDestroySurface);
  NYBLER_LOAD_EGL(context, egl_destroy_context, eglDestroyContext);

  NYBLER_LOAD_GLES(context, gl_gen_framebuffers, glGenFramebuffers);
  NYBLER_LOAD_GLES(context, gl_bind_framebuffer, glBindFramebuffer);
  NYBLER_LOAD_GLES(context, gl_framebuffer_texture_2d, glFramebufferTexture2D);
  NYBLER_LOAD_GLES(context, gl_check_framebuffer_status, glCheckFramebufferStatus);
  NYBLER_LOAD_GLES(context, gl_delete_framebuffers, glDeleteFramebuffers);
  NYBLER_LOAD_GLES(context, gl_gen_textures, glGenTextures);
  NYBLER_LOAD_GLES(context, gl_bind_texture, glBindTexture);
  NYBLER_LOAD_GLES(context, gl_tex_image_2d, glTexImage2D);
  NYBLER_LOAD_GLES(context, gl_tex_parameteri, glTexParameteri);
  NYBLER_LOAD_GLES(context, gl_delete_textures, glDeleteTextures);
  NYBLER_LOAD_GLES(context, gl_viewport, glViewport);
  NYBLER_LOAD_GLES(context, gl_clear_color, glClearColor);
  NYBLER_LOAD_GLES(context, gl_clear, glClear);
  NYBLER_LOAD_GLES(context, gl_read_pixels, glReadPixels);
  NYBLER_LOAD_GLES(context, gl_pixel_storei, glPixelStorei);
  NYBLER_LOAD_GLES(context, gl_gen_buffers, glGenBuffers);
  NYBLER_LOAD_GLES(context, gl_bind_buffer, glBindBuffer);
  NYBLER_LOAD_GLES(context, gl_buffer_data, glBufferData);
  NYBLER_LOAD_GLES(context, gl_delete_buffers, glDeleteBuffers);
  NYBLER_LOAD_GLES(context, gl_enable_vertex_attrib_array, glEnableVertexAttribArray);
  NYBLER_LOAD_GLES(context, gl_disable_vertex_attrib_array, glDisableVertexAttribArray);
  NYBLER_LOAD_GLES(context, gl_vertex_attrib_pointer, glVertexAttribPointer);
  NYBLER_LOAD_GLES(context, gl_draw_arrays, glDrawArrays);
  NYBLER_LOAD_GLES(context, gl_create_shader, glCreateShader);
  NYBLER_LOAD_GLES(context, gl_shader_source, glShaderSource);
  NYBLER_LOAD_GLES(context, gl_compile_shader, glCompileShader);
  NYBLER_LOAD_GLES(context, gl_get_shaderiv, glGetShaderiv);
  NYBLER_LOAD_GLES(context, gl_get_shader_info_log, glGetShaderInfoLog);
  NYBLER_LOAD_GLES(context, gl_delete_shader, glDeleteShader);
  NYBLER_LOAD_GLES(context, gl_create_program, glCreateProgram);
  NYBLER_LOAD_GLES(context, gl_attach_shader, glAttachShader);
  NYBLER_LOAD_GLES(context, gl_link_program, glLinkProgram);
  NYBLER_LOAD_GLES(context, gl_get_programiv, glGetProgramiv);
  NYBLER_LOAD_GLES(context, gl_get_program_info_log, glGetProgramInfoLog);
  NYBLER_LOAD_GLES(context, gl_use_program, glUseProgram);
  NYBLER_LOAD_GLES(context, gl_delete_program, glDeleteProgram);
  NYBLER_LOAD_GLES(context, gl_finish, glFinish);

  get_platform_display = (NyblerEGLGetPlatformDisplayFn)
    nybler_load_symbol(
      context->egl_library, "eglGetPlatformDisplayEXT");
  if (!get_platform_display && context->egl_get_proc_address) {
    get_platform_display = (NyblerEGLGetPlatformDisplayFn)
      context->egl_get_proc_address("eglGetPlatformDisplayEXT");
  }
  if (get_platform_display) {
    display = get_platform_display(
      NYBLER_EGL_PLATFORM_SURFACELESS_MESA, NULL, NULL);
  }
  if (!display && context->egl_get_display) {
    display = context->egl_get_display(NULL);
  }
  if (!display) goto fail;

  context->display = display;
  if (!context->egl_initialize(display, NULL, NULL)) goto fail;
  if (!context->egl_choose_config(
        display, config_attributes, &config, 1, &config_count) ||
      config_count == 0) {
    goto fail;
  }

  context->context = context->egl_create_context(
    display, config, NULL, context_attributes);
  if (!context->context) goto fail;

  context->surface = context->egl_create_pbuffer_surface(
    display, config, surface_attributes);
  if (!context->egl_make_current(
        display, context->surface, context->surface, context->context)) {
    if (context->surface) {
      context->egl_destroy_surface(display, context->surface);
      context->surface = NULL;
    }
    if (!context->egl_make_current(display, NULL, NULL, context->context)) {
      goto fail;
    }
  }

  context->ready = 1;
  *out_context = context;
  return NYBLER_OK;

fail:
  if (context) {
    if (context->egl_make_current && context->display) {
      context->egl_make_current(context->display, NULL, NULL, NULL);
    }
    if (context->egl_destroy_surface && context->display && context->surface) {
      context->egl_destroy_surface(context->display, context->surface);
    }
    if (context->egl_destroy_context && context->display && context->context) {
      context->egl_destroy_context(context->display, context->context);
    }
    if (context->egl_terminate && context->display) {
      context->egl_terminate(context->display);
    }
    nybler_unload_library(context->gles_library);
    nybler_unload_library(context->egl_library);
    free(context);
  }
  return NYBLER_ERROR_BACKEND_UNAVAILABLE;
}

void nybler_mesa_destroy(NyblerMesaContext *context) {
  if (!context) return;

  if (context->egl_make_current && context->display) {
    context->egl_make_current(context->display, NULL, NULL, NULL);
  }
  if (context->egl_destroy_surface && context->display && context->surface) {
    context->egl_destroy_surface(context->display, context->surface);
  }
  if (context->egl_destroy_context && context->display && context->context) {
    context->egl_destroy_context(context->display, context->context);
  }
  if (context->egl_terminate && context->display) {
    context->egl_terminate(context->display);
  }
  nybler_unload_library(context->gles_library);
  nybler_unload_library(context->egl_library);
  free(context);
}

static int nybler_mesa_make_current(NyblerMesaContext *context) {
  if (!context || !context->ready) return 0;
  return context->egl_make_current(
    context->display, context->surface, context->surface, context->context) != 0;
}

static NyblerMesaContext *nybler_texture_context(NyblerTexture *texture) {
  if (!texture || !texture->device) return NULL;
  return (NyblerMesaContext *)texture->device->backend_context;
}

NyblerResult nybler_mesa_create_texture(
  NyblerMesaContext *context,
  NyblerTexture *texture
) {
  NyblerMesaTexture *state;
  NyblerGLuint framebuffer = 0;
  NyblerGLuint gl_texture = 0;

  if (!texture) return NYBLER_ERROR_INVALID_ARGUMENT;
  if (!context || !context->ready) return NYBLER_OK;

  if (texture->descriptor.format != NYBLER_TEXTURE_FORMAT_RGBA8_UNORM ||
      texture->descriptor.size.depth_or_array_layers != 1 ||
      texture->descriptor.mip_level_count != 1 ||
      texture->descriptor.sample_count != 1) {
    return NYBLER_ERROR_UNSUPPORTED;
  }
  if (!nybler_mesa_make_current(context)) {
    return NYBLER_ERROR_BACKEND_UNAVAILABLE;
  }

  state = (NyblerMesaTexture *)calloc(1, sizeof(*state));
  if (!state) return NYBLER_ERROR_OUT_OF_MEMORY;

  context->gl_gen_textures(1, &gl_texture);
  if (!gl_texture) {
    free(state);
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }
  context->gl_bind_texture(NYBLER_GL_TEXTURE_2D, gl_texture);
  context->gl_tex_parameteri(
    NYBLER_GL_TEXTURE_2D, NYBLER_GL_TEXTURE_MIN_FILTER, NYBLER_GL_NEAREST);
  context->gl_tex_parameteri(
    NYBLER_GL_TEXTURE_2D, NYBLER_GL_TEXTURE_MAG_FILTER, NYBLER_GL_NEAREST);
  context->gl_tex_image_2d(
    NYBLER_GL_TEXTURE_2D,
    0,
    NYBLER_GL_RGBA8,
    (NyblerGLint)texture->descriptor.size.width,
    (NyblerGLint)texture->descriptor.size.height,
    0,
    NYBLER_GL_RGBA,
    NYBLER_GL_UNSIGNED_BYTE,
    NULL);

  context->gl_gen_framebuffers(1, &framebuffer);
  if (!framebuffer) {
    context->gl_delete_textures(1, &gl_texture);
    free(state);
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }
  context->gl_bind_framebuffer(NYBLER_GL_FRAMEBUFFER, framebuffer);
  context->gl_framebuffer_texture_2d(
    NYBLER_GL_FRAMEBUFFER,
    NYBLER_GL_COLOR_ATTACHMENT0,
    NYBLER_GL_TEXTURE_2D,
    gl_texture,
    0);
  if (context->gl_check_framebuffer_status(NYBLER_GL_FRAMEBUFFER) !=
      NYBLER_GL_FRAMEBUFFER_COMPLETE) {
    context->gl_bind_framebuffer(NYBLER_GL_FRAMEBUFFER, 0);
    context->gl_delete_framebuffers(1, &framebuffer);
    context->gl_delete_textures(1, &gl_texture);
    free(state);
    return NYBLER_ERROR_UNSUPPORTED;
  }
  context->gl_bind_framebuffer(NYBLER_GL_FRAMEBUFFER, 0);

  state->texture = gl_texture;
  state->framebuffer = framebuffer;
  state->width = (NyblerGLint)texture->descriptor.size.width;
  state->height = (NyblerGLint)texture->descriptor.size.height;
  texture->backend = state;
  return NYBLER_OK;
}

void nybler_mesa_destroy_texture(NyblerTexture *texture) {
  NyblerMesaContext *context;
  NyblerMesaTexture *state;

  if (!texture || !texture->backend) return;
  context = nybler_texture_context(texture);
  state = (NyblerMesaTexture *)texture->backend;
  if (context && nybler_mesa_make_current(context)) {
    context->gl_delete_framebuffers(1, &state->framebuffer);
    context->gl_delete_textures(1, &state->texture);
  }
  free(state);
  texture->backend = NULL;
}

static NyblerGLuint nybler_compile_shader(
  NyblerMesaContext *context,
  NyblerGLenum type,
  const char *source
) {
  NyblerGLuint shader;
  NyblerGLint compiled = 0;
  char log[1024];

  shader = context->gl_create_shader(type);
  if (!shader) return 0;
  context->gl_shader_source(shader, 1, &source, NULL);
  context->gl_compile_shader(shader);
  context->gl_get_shaderiv(shader, NYBLER_GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    log[0] = '\0';
    context->gl_get_shader_info_log(shader, (NyblerGLsizei)sizeof(log), NULL, log);
    fprintf(stderr, "nybler mesa shader compile failed: %s\n", log);
    context->gl_delete_shader(shader);
    return 0;
  }
  return shader;
}

NyblerResult nybler_mesa_create_pipeline(
  NyblerMesaContext *context,
  NyblerRenderPipeline *pipeline
) {
  static const char *vertex_source =
    "#version 300 es\n"
    "layout(location = 0) in vec4 position;\n"
    "void main() {\n"
    "  gl_Position = position;\n"
    "}\n";
  static const char *fragment_source =
    "#version 300 es\n"
    "precision mediump float;\n"
    "out vec4 out_color;\n"
    "void main() {\n"
    "  out_color = vec4(1.0, 0.0, 1.0, 1.0);\n"
    "}\n";
  static const char *colored_vertex_source =
    "#version 300 es\n"
    "layout(location = 0) in vec2 position;\n"
    "layout(location = 1) in vec4 color;\n"
    "out vec4 fragment_color;\n"
    "void main() {\n"
    "  gl_Position = vec4(position, 0.0, 1.0);\n"
    "  fragment_color = color;\n"
    "}\n";
  static const char *colored_fragment_source =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec4 fragment_color;\n"
    "out vec4 out_color;\n"
    "void main() {\n"
    "  out_color = fragment_color;\n"
    "}\n";
  int use_vertex_color = pipeline && pipeline->vertex_attribute_count >= 2;
  NyblerMesaPipeline *state;
  NyblerGLuint vertex_shader = 0;
  NyblerGLuint fragment_shader = 0;
  NyblerGLint linked = 0;
  char log[1024];

  if (!pipeline) return NYBLER_ERROR_INVALID_ARGUMENT;
  if (!context || !context->ready) return NYBLER_OK;
  if (!pipeline->vertex_module || !pipeline->fragment_module ||
      !pipeline->vertex_module->code_size || !pipeline->fragment_module->code_size) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  if (!nybler_mesa_make_current(context)) {
    return NYBLER_ERROR_BACKEND_UNAVAILABLE;
  }

  vertex_shader = nybler_compile_shader(
    context,
    NYBLER_GL_VERTEX_SHADER,
    use_vertex_color ? colored_vertex_source : vertex_source);
  fragment_shader = nybler_compile_shader(
    context,
    NYBLER_GL_FRAGMENT_SHADER,
    use_vertex_color ? colored_fragment_source : fragment_source);
  if (!vertex_shader || !fragment_shader) goto fail;

  state = (NyblerMesaPipeline *)calloc(1, sizeof(*state));
  if (!state) goto fail;
  state->program = context->gl_create_program();
  if (!state->program) {
    free(state);
    goto fail;
  }
  context->gl_attach_shader(state->program, vertex_shader);
  context->gl_attach_shader(state->program, fragment_shader);
  context->gl_link_program(state->program);
  context->gl_get_programiv(state->program, NYBLER_GL_LINK_STATUS, &linked);
  if (!linked) {
    log[0] = '\0';
    context->gl_get_program_info_log(
      state->program, (NyblerGLsizei)sizeof(log), NULL, log);
    fprintf(stderr, "nybler mesa pipeline link failed: %s\n", log);
    context->gl_delete_program(state->program);
    free(state);
    goto fail;
  }

  context->gl_delete_shader(vertex_shader);
  context->gl_delete_shader(fragment_shader);
  pipeline->backend = state;
  return NYBLER_OK;

fail:
  if (vertex_shader) context->gl_delete_shader(vertex_shader);
  if (fragment_shader) context->gl_delete_shader(fragment_shader);
  return NYBLER_ERROR_UNSUPPORTED;
}

void nybler_mesa_destroy_pipeline(NyblerRenderPipeline *pipeline) {
  NyblerMesaContext *context;
  NyblerMesaPipeline *state;

  if (!pipeline || !pipeline->backend) return;
  context = pipeline->device
    ? (NyblerMesaContext *)pipeline->device->backend_context
    : NULL;
  state = (NyblerMesaPipeline *)pipeline->backend;
  if (context && nybler_mesa_make_current(context)) {
    context->gl_delete_program(state->program);
  }
  free(state);
  pipeline->backend = NULL;
}

static uint32_t nybler_vertex_components(NyblerVertexFormat format) {
  switch (format) {
    case NYBLER_VERTEX_FORMAT_FLOAT32: return 1;
    case NYBLER_VERTEX_FORMAT_FLOAT32X2: return 2;
    case NYBLER_VERTEX_FORMAT_FLOAT32X3: return 3;
    case NYBLER_VERTEX_FORMAT_FLOAT32X4: return 4;
    default: return 0;
  }
}

static NyblerGLenum nybler_primitive_mode(NyblerPrimitiveTopology topology) {
  switch (topology) {
    case NYBLER_PRIMITIVE_TOPOLOGY_POINT_LIST: return NYBLER_GL_POINTS;
    case NYBLER_PRIMITIVE_TOPOLOGY_LINE_LIST: return NYBLER_GL_LINES;
    case NYBLER_PRIMITIVE_TOPOLOGY_LINE_STRIP: return NYBLER_GL_LINE_STRIP;
    case NYBLER_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: return NYBLER_GL_TRIANGLES;
    case NYBLER_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: return NYBLER_GL_TRIANGLE_STRIP;
    default: return 0;
  }
}

static NyblerResult nybler_mesa_copy_texture_to_buffer(
  NyblerMesaContext *context,
  const NyblerCommand *command
) {
  const NyblerImageCopyTexture *source = &command->data.copy_texture_to_buffer.source;
  const NyblerImageCopyBuffer *destination =
    &command->data.copy_texture_to_buffer.destination;
  const NyblerExtent3D *size = &command->data.copy_texture_to_buffer.size;
  NyblerMesaTexture *texture;
  uint64_t bytes_per_pixel = 4;
  uint64_t row_size;
  uint64_t required;
  uint32_t row;

  texture = source->texture ? (NyblerMesaTexture *)source->texture->backend : NULL;
  if (!texture || !destination->buffer) return NYBLER_ERROR_UNSUPPORTED;
  if (source->mip_level != 0 || source->origin.z != 0 ||
      size->depth_or_array_layers != 1 ||
      size->width == 0 || size->height == 0) {
    return NYBLER_ERROR_UNSUPPORTED;
  }
  if ((uint64_t)source->origin.x + size->width > (uint64_t)texture->width ||
      (uint64_t)source->origin.y + size->height > (uint64_t)texture->height) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }

  row_size = (uint64_t)size->width * bytes_per_pixel;
  if (destination->bytes_per_row < row_size) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  required = ((uint64_t)size->height - 1) * destination->bytes_per_row + row_size;
  if (destination->offset > destination->buffer->size ||
      required > destination->buffer->size - destination->offset) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }

  context->gl_bind_framebuffer(NYBLER_GL_FRAMEBUFFER, texture->framebuffer);
  context->gl_pixel_storei(NYBLER_GL_PACK_ALIGNMENT, 1);
  for (row = 0; row < size->height; ++row) {
    NyblerGLint y = texture->height -
      (NyblerGLint)source->origin.y -
      (NyblerGLint)size->height +
      (NyblerGLint)row;
    context->gl_read_pixels(
      (NyblerGLint)source->origin.x,
      y,
      (NyblerGLsizei)size->width,
      1,
      NYBLER_GL_RGBA,
      NYBLER_GL_UNSIGNED_BYTE,
      destination->buffer->data + destination->offset +
        (size_t)row * destination->bytes_per_row);
  }
  context->gl_bind_framebuffer(NYBLER_GL_FRAMEBUFFER, 0);
  return NYBLER_OK;
}

NyblerResult nybler_mesa_execute(
  NyblerMesaContext *context,
  size_t command_count,
  NyblerCommandBuffer *const *command_buffers
) {
  NyblerRenderPipeline *pipeline = NULL;
  NyblerBuffer *vertex_buffer = NULL;
  uint64_t vertex_offset = 0;
  NyblerGLenum primitive = 0;
  size_t buffer_index;
  size_t i;
  NyblerResult result = NYBLER_OK;

  if (!context || !context->ready || !command_buffers) {
    return NYBLER_ERROR_BACKEND_UNAVAILABLE;
  }
  if (!nybler_mesa_make_current(context)) {
    return NYBLER_ERROR_BACKEND_UNAVAILABLE;
  }

  for (buffer_index = 0; buffer_index < command_count; ++buffer_index) {
    const NyblerCommandBuffer *commands = command_buffers[buffer_index];
    if (!commands) return NYBLER_ERROR_INVALID_ARGUMENT;

    for (i = 0; i < commands->command_count; ++i) {
      const NyblerCommand *command = &commands->commands[i];

      switch (command->kind) {
      case NYBLER_COMMAND_BEGIN_RENDER_PASS: {
        const NyblerRenderPassColorAttachment *attachment;
        NyblerMesaTexture *texture;

        if (command->data.render_pass.attachment_count != 1) {
          result = NYBLER_ERROR_UNSUPPORTED;
          goto done;
        }
        attachment = &command->data.render_pass.attachments[0];
        texture = attachment->texture
          ? (NyblerMesaTexture *)attachment->texture->backend
          : NULL;
        if (!texture) {
          result = NYBLER_ERROR_UNSUPPORTED;
          goto done;
        }

        context->gl_bind_framebuffer(
          NYBLER_GL_FRAMEBUFFER, texture->framebuffer);
        context->gl_viewport(0, 0, texture->width, texture->height);
        if (attachment->load_op != NYBLER_LOAD_OP_LOAD ||
            attachment->store_op == NYBLER_STORE_OP_DISCARD) {
          context->gl_clear_color(
            attachment->clear_value.r,
            attachment->clear_value.g,
            attachment->clear_value.b,
            attachment->clear_value.a);
          context->gl_clear(NYBLER_GL_COLOR_BUFFER_BIT);
        }
        pipeline = NULL;
        vertex_buffer = NULL;
        vertex_offset = 0;
        break;
      }

      case NYBLER_COMMAND_SET_PIPELINE:
        pipeline = command->data.set_pipeline.pipeline;
        primitive = pipeline ? nybler_primitive_mode(pipeline->topology) : 0;
        break;

      case NYBLER_COMMAND_SET_VERTEX_BUFFER:
        vertex_buffer = command->data.set_vertex_buffer.buffer;
        vertex_offset = command->data.set_vertex_buffer.offset;
        break;

      case NYBLER_COMMAND_DRAW: {
        NyblerMesaPipeline *state;
        NyblerGLuint vbo = 0;
        NyblerGLuint bound_locations[8];
        size_t bound_count = 0;
        uint64_t stride;
        uint64_t data_size;
        uint32_t instance;
        uint32_t components;
        size_t attribute_index;

        if (!pipeline || !pipeline->backend || !vertex_buffer ||
            !primitive || command->data.draw.first_instance != 0) {
          result = NYBLER_ERROR_UNSUPPORTED;
          goto done;
        }
        state = (NyblerMesaPipeline *)pipeline->backend;
        stride = pipeline->vertex_stride ? pipeline->vertex_stride : 16;
        if (pipeline->vertex_attribute_count > 8) {
          result = NYBLER_ERROR_UNSUPPORTED;
          goto done;
        }
        for (attribute_index = 0;
             attribute_index < pipeline->vertex_attribute_count;
             ++attribute_index) {
          const NyblerVertexAttribute *attribute =
            &pipeline->vertex_attributes[attribute_index];
          if (attribute->shader_location >= 8 ||
              !nybler_vertex_components(attribute->format)) {
            result = NYBLER_ERROR_UNSUPPORTED;
            goto done;
          }
          bound_locations[bound_count++] = attribute->shader_location;
        }
        data_size = (uint64_t)command->data.draw.vertex_count * stride;
        if (vertex_offset > vertex_buffer->size ||
            data_size > vertex_buffer->size - vertex_offset) {
          result = NYBLER_ERROR_INVALID_ARGUMENT;
          goto done;
        }

        context->gl_use_program(state->program);
        context->gl_gen_buffers(1, &vbo);
        context->gl_bind_buffer(NYBLER_GL_ARRAY_BUFFER, vbo);
        context->gl_buffer_data(
          NYBLER_GL_ARRAY_BUFFER,
          (NyblerGLsizeiptr)data_size,
          vertex_buffer->data + vertex_offset,
          NYBLER_GL_STATIC_DRAW);
        if (pipeline->vertex_attribute_count == 0) {
          context->gl_enable_vertex_attrib_array(0);
          context->gl_vertex_attrib_pointer(
            0, 4, NYBLER_GL_FLOAT, NYBLER_GL_FALSE,
            (NyblerGLsizei)stride, NULL);
          bound_locations[bound_count++] = 0;
        } else {
          for (attribute_index = 0;
               attribute_index < pipeline->vertex_attribute_count;
               ++attribute_index) {
            const NyblerVertexAttribute *attribute =
              &pipeline->vertex_attributes[attribute_index];
            components = nybler_vertex_components(attribute->format);
            context->gl_enable_vertex_attrib_array(attribute->shader_location);
            context->gl_vertex_attrib_pointer(
              attribute->shader_location,
              (NyblerGLint)components,
              NYBLER_GL_FLOAT,
              NYBLER_GL_FALSE,
              (NyblerGLsizei)stride,
              (const void *)(uintptr_t)attribute->offset);
          }
        }
        for (instance = 0; instance < command->data.draw.instance_count; ++instance) {
          context->gl_draw_arrays(
            primitive,
            (NyblerGLint)command->data.draw.first_vertex,
            (NyblerGLsizei)command->data.draw.vertex_count);
        }
        for (attribute_index = 0; attribute_index < bound_count; ++attribute_index) {
          context->gl_disable_vertex_attrib_array(bound_locations[attribute_index]);
        }
        context->gl_delete_buffers(1, &vbo);
        break;
      }

      case NYBLER_COMMAND_END_RENDER_PASS:
        context->gl_bind_framebuffer(NYBLER_GL_FRAMEBUFFER, 0);
        pipeline = NULL;
        vertex_buffer = NULL;
        vertex_offset = 0;
        break;

      case NYBLER_COMMAND_COPY_TEXTURE_TO_BUFFER:
        result = nybler_mesa_copy_texture_to_buffer(context, command);
        if (result != NYBLER_OK) goto done;
        break;

      default:
        break;
      }
    }
  }

done:
  context->gl_bind_framebuffer(NYBLER_GL_FRAMEBUFFER, 0);
  context->gl_finish();
  return result;
}
