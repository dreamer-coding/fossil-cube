/*
 * -----------------------------------------------------------------------------
 * Project: Fossil Logic
 *
 * This file is part of the Fossil Logic project, which aims to develop high-
 * performance, cross-platform applications and libraries. The code contained
 * herein is subject to the terms and conditions defined in the project license.
 *
 * Author: Michael Gene Brockus (Dreamer)
 *
 * Copyright (C) 2024 Fossil Logic. All rights reserved.
 * -----------------------------------------------------------------------------
 */
#ifndef FOSSIL_OPENCUBE_CORE_H
#define FOSSIL_OPENCUBE_CORE_H

#include <stddef.h>
#include <stdint.h>

/*
 * Fossil CUBE — ultra-light OpenGL wrapper (pure C, no external deps)
 * ---------------------------------------------------------------
 * Platform backends:
 *   - Windows  : Win32 + WGL (windowed)
 *   - Linux/BSD: X11 + GLX (windowed)
 *   - macOS    : CGL pbuffer (OFFSCREEN ONLY, pure C)
 *
 * Notes:
 *   - You supply your own OpenGL headers (see includes below).
 *   - This wrapper exposes a tiny “window/context + loop” facade.
 *   - macOS windowed rendering needs Cocoa/NSOpenGL (Objective-C) and is
 *     intentionally not included to keep this pair purely C.
 *
 * Build (examples):
 *   Windows: cl /O2 /DNDEBUG cube.c user32.lib gdi32.lib opengl32.lib
 *   Linux  : cc -O2 -DNDEBUG cube.c -lX11 -lGL
 *   macOS  : cc -O2 -DNDEBUG cube.c -framework OpenGL -framework CoreFoundation
 */

#if defined(_WIN32) || defined(_WIN64)
#  include <windows.h>   /* Must be included before gl.h on Windows */
#  include <GL/gl.h>
#elif defined(__APPLE__)
#  include <OpenGL/gl.h>
#else
#  include <GL/gl.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Forward decl */
typedef struct fossil_cube_t fossil_cube_t;

/* Basic configuration for a windowed GL context. */
typedef struct fossil_cube_config {
    int width;            /* client area width */
    int height;           /* client area height */
    const char* title;    /* window title (UTF-8) */
    int gl_major;         /* requested GL major version (hint) */
    int gl_minor;         /* requested GL minor version (hint) */
    int vsync;            /* 0 = off, 1 = on if possible */
    int resizable;        /* 0 or 1 (ignored on some platforms) */
} fossil_cube_config;

/* Opaque platform-native handles if you need them (optional). */
typedef struct fossil_cube_native {
#if defined(_WIN32) || defined(_WIN64)
    void* hInstance;      /* HINSTANCE */
    void* hwnd;           /* HWND */
    void* hdc;            /* HDC */
    void* hglrc;          /* HGLRC */
#elif defined(__APPLE__)
    void* ns_window;      /* NSWindow* (if available) */
    void* ns_view;        /* NSView*   (if available) */
    void* ns_glctx;       /* NSOpenGLContext* */
#else
    void* display;        /* Display* */
    uintptr_t window;     /* Window (XID) */
    void* glx_ctx;        /* GLXContext */
#endif
} fossil_cube_native;

/* Error codes (return 0 = success; negative = failure). */
enum {
    FOSSIL_CUBE_OK = 0,
    FOSSIL_CUBE_ERR_PLATFORM = -1,
    FOSSIL_CUBE_ERR_GL       = -2,
    FOSSIL_CUBE_ERR_ALLOC    = -3,
    FOSSIL_CUBE_ERR_PARAM    = -4
};

/* Creation / destruction */
int  fossil_cube_create(const fossil_cube_config* cfg, fossil_cube_t** out);
void fossil_cube_destroy(fossil_cube_t* cube);

/* macOS helper: attach an already-created NSOpenGLContext (pure C build friendly).
 * Pass your NSWindow/NSView/NSOpenGLContext* as void* (may pass NULL for window/view).
 */
int  fossil_cube_attach_existing_context(void* ns_window,
                                         void* ns_view,
                                         void* ns_opengl_context,
                                         fossil_cube_t** out);

/* Main loop utilities */
void fossil_cube_poll_events(fossil_cube_t* cube);
void fossil_cube_swap_buffers(fossil_cube_t* cube);
int  fossil_cube_should_close(const fossil_cube_t* cube);
void fossil_cube_set_should_close(fossil_cube_t* cube, int should_close);
void fossil_cube_get_size(const fossil_cube_t* cube, int* w, int* h);
double fossil_cube_get_time(void); /* monotonic seconds since library init */

/* Raw GL function address (for loading extensions manually if desired). */
void* fossil_cube_get_proc(fossil_cube_t* cube, const char* name);

/* Minimal modern-GL helper set (no deprecated pipeline). Great for quick bring-up. */

/* Compile a shader (GL_VERTEX_SHADER or GL_FRAGMENT_SHADER). Returns shader id or 0 on error.
 * If log_out != NULL and *log_len != 0, writes infolog (NUL-terminated) up to *log_len bytes.
 * On return, *log_len contains written length (including NUL).
 */
GLuint fossil_cube_compile_shader(GLenum type,
                                  const char* source,
                                  char* log_out,
                                  size_t* log_len);

/* Link a program from the given shader objects (which you can glDeleteShader after).
 * Returns program id or 0 on error; optional info log like compile helper.
 */
GLuint fossil_cube_link_program(GLuint vs, GLuint fs, char* log_out, size_t* log_len);

/* Create a VAO (if available) and a VBO, upload data (GL_STATIC_DRAW).
 * Returns 1 on success; vao_out may be 0 if VAO not supported (then just bind VBO and set attribs).
 */
int fossil_cube_create_vao_vbo(GLuint* vao_out, GLuint* vbo_out,
                               const void* data, size_t bytes);

/* Destroy VAO/VBO (safe to pass 0). */
void fossil_cube_destroy_vao_vbo(GLuint vao, GLuint vbo);

/* Convenience GL helpers (thin wrappers). */
void fossil_cube_set_vsync(fossil_cube_t* cube, int vsync);
void fossil_cube_set_viewport(int x, int y, int w, int h);
void fossil_cube_set_clear_color(float r, float g, float b, float a);
void fossil_cube_clear(GLbitfield mask);

/* query native handles (for advanced users). */
void fossil_cube_get_native(const fossil_cube_t* cube, fossil_cube_native* out);

#ifdef __cplusplus
}
#include <stdexcept>
#include <vector>
#include <string>

namespace fossil {

namespace cube {



} // namespace cube

} // namespace fossil

#endif

#endif /* FOSSIL_OPENCUBE_CORE_H */
