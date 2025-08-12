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

#ifdef __cplusplus
extern "C"
{
#endif

 Basic types */
typedef uint32_t fc_color_t; /* 0xAARRGGBB */
typedef struct { float x,y; } fc_vec2;
typedef struct { float x,y,z; } fc_vec3;

/* Opaque context */
typedef struct fossil_cube_ctx fossil_cube_ctx;

/* Event types forwarded by platform backend */
typedef enum {
    FC_EVENT_NONE = 0,
    FC_EVENT_KEY_DOWN,
    FC_EVENT_KEY_UP,
    FC_EVENT_MOUSE_MOVE,
    FC_EVENT_MOUSE_BUTTON_DOWN,
    FC_EVENT_MOUSE_BUTTON_UP,
    FC_EVENT_SCROLL,
    FC_EVENT_WINDOW_RESIZE,
} fossil_cube_event_type;

typedef struct {
    fossil_cube_event_type type;
    union {
        struct { int key; int mods; } key;
        struct { int button; int mods; } mouse_button;
        struct { double x; double y; } mouse_move;
        struct { double offset_x; double offset_y; } scroll;
        struct { int width; int height; } resize;
    } d;
} fossil_cube_event;

/* Callbacks user can set */
typedef void (*fossil_cube_render_cb)(fossil_cube_ctx *ctx, void *user);
typedef void (*fossil_cube_event_cb)(fossil_cube_ctx *ctx, const fossil_cube_event *ev, void *user);

/* Initialization flags (future) */
enum { FC_INIT_NONE = 0 };

// *****************************************************************************
// Function prototypes
// *****************************************************************************

/* Create/destroy context
   platform_user_data: opaque pointer supplied by platform backend for callbacks,
   width/height: initial framebuffer size in pixels
*/
fossil_cube_ctx *fossil_cube_create(void *platform_user_data, int width, int height, int flags);
void fossil_cube_destroy(fossil_cube_ctx *c);

/* Set callbacks */
void fossil_cube_set_render_callback(fossil_cube_ctx *c, fossil_cube_render_cb cb, void *user);
void fossil_cube_set_event_callback(fossil_cube_ctx *c, fossil_cube_event_cb ecb, void *user);

/* Frame lifecycle - called by platform backend once per frame */
void fossil_cube_frame_begin(fossil_cube_ctx *c, double dt_seconds);
void fossil_cube_frame_end(fossil_cube_ctx *c);

/* Feed events into the core */
void fossil_cube_push_event(fossil_cube_ctx *c, const fossil_cube_event *ev);

/* Basic drawing primitives (immediate mode) */

/* State: color (applies to untextured rects/lines) */
void fossil_cube_set_color(fossil_cube_ctx *c, fc_color_t color);

/* Rect: x,y origin with width/height (x,y are in screen pixels with origin top-left) */
void fossil_cube_draw_rect(fossil_cube_ctx *c, float x, float y, float w, float h);

/* Filled circle (approx) */
void fossil_cube_draw_circle(fossil_cube_ctx *c, float cx, float cy, float r, int segments);

/* Textured quad: user supplies an OpenGL texture id */
void fossil_cube_draw_textured_quad(fossil_cube_ctx *c, unsigned int gl_texture, 
                                    float x, float y, float w, float h, float u0, float v0, float u1, float v1);

/* Simple GPU resource helpers */
unsigned int fossil_cube_create_texture_from_rgba8(fossil_cube_ctx *c, const void *rgba, int width, int height);
void fossil_cube_destroy_texture(fossil_cube_ctx *c, unsigned int tex);

/* Utility: convert 8-bit RGBA components to fc_color_t */
static inline fc_color_t fossil_cube_rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)a<<24) | ((uint32_t)r<<16) | ((uint32_t)g<<8) | (uint32_t)b;
}

/* Simple debug drawing toggle */
void fossil_cube_enable_debug_draw(fossil_cube_ctx *c, int enable);

/* Version */
const char *fossil_cube_version(void);

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
