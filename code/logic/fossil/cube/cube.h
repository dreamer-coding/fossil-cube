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

/* Version */
#define FOSSIL_CUBE_VERSION_MAJOR 0
#define FOSSIL_CUBE_VERSION_MINOR 1
#define FOSSIL_CUBE_VERSION_PATCH 0

/* Public result codes */
typedef enum fossil_cube_result_e {
    FOSSIL_CUBE_OK = 0,
    FOSSIL_CUBE_ERR_GENERIC = -1,
    FOSSIL_CUBE_ERR_PLATFORM = -2,
    FOSSIL_CUBE_ERR_NO_DISPLAY = -3,
    FOSSIL_CUBE_ERR_CREATE_WINDOW = -4,
    FOSSIL_CUBE_ERR_CREATE_CONTEXT = -5,
    FOSSIL_CUBE_ERR_MAKE_CURRENT = -6,
    FOSSIL_CUBE_ERR_GL_LOADER = -7,
    FOSSIL_CUBE_ERR_HEADLESS_ONLY = -8,
} fossil_cube_result_t;

/* Opaque handle for a GL context + (optionally) a native window */
typedef struct fossil_cube_window_t fossil_cube_window_t;

/* Basic configuration for window/context creation */
typedef struct fossil_cube_config_t {
    int width;               /* Desired width  (ignored for macOS headless; used as pbuffer size) */
    int height;              /* Desired height (ignored for macOS headless; used as pbuffer size) */
    int color_bits;          /* Usually 24 or 32 */
    int depth_bits;          /* Usually 24 */
    int stencil_bits;        /* Usually 8  */
    int double_buffer;       /* 1 = double buffer, 0 = single */
    int vsync;               /* 1 = request vsync, 0 = no vsync (best-effort) */
    const char* title;       /* Window title (ignored for macOS headless) */
} fossil_cube_config_t;

/* Simple event mask (future-proof). For now we only track close + resize. */
typedef struct fossil_cube_events_t {
    int should_close;        /* Set when the user requests close (Win/X11). */
    int resized;             /* Set when the surface resized (Win/X11).     */
    int width;               /* New width  if resized != 0                  */
    int height;              /* New height if resized != 0                  */
} fossil_cube_events_t;

/* ---- API ---- */

/* Initialize internal state (optional on most platforms, idempotent). */
fossil_cube_result_t fossil_cube_init(void);

/* Terminate/cleanup any global state. Safe to call multiple times. */
void fossil_cube_shutdown(void);

/* Create a window + GL context (windowed on Win/X11, headless pbuffer on macOS). */
fossil_cube_window_t* fossil_cube_create(const fossil_cube_config_t* cfg, fossil_cube_result_t* out_err);

/* Destroy a window/context. */
void fossil_cube_destroy(fossil_cube_window_t* win);

/* Make the context current on the calling thread. */
fossil_cube_result_t fossil_cube_make_current(fossil_cube_window_t* win);

/* Swap buffers if double-buffered; no-op for single-buffer or headless. */
void fossil_cube_swap_buffers(fossil_cube_window_t* win);

/* Poll native events; fills out simple event struct. Non-blocking. */
void fossil_cube_poll_events(fossil_cube_window_t* win, fossil_cube_events_t* out_events);

/* Set swap interval (vsync): 0 = off, 1 = on (best-effort). */
void fossil_cube_set_vsync(fossil_cube_window_t* win, int interval);

/* Get native GL procedure address for extension loading. */
void* fossil_cube_get_proc_address(const char* name);

/* Accessors */
int  fossil_cube_width(const fossil_cube_window_t* win);
int  fossil_cube_height(const fossil_cube_window_t* win);
int  fossil_cube_is_headless(const fossil_cube_window_t* win); /* 1 if offscreen (macOS), 0 otherwise */

/* Utility: human-readable error string (static text). */
const char* fossil_cube_strerror(fossil_cube_result_t err);

/* Minimal spin helper: returns 0 if a close was requested. */
int fossil_cube_frame(fossil_cube_window_t* win, fossil_cube_events_t* ev); /* poll events + swap if double-buffered */

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
