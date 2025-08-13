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

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>   /* must come before gl.h */
    #include <GL/gl.h>
#elif defined(__APPLE__)
    #include <OpenGL/gl.h>
#else
    #include <GL/gl.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------- Public Types --------------------------------- */

typedef struct { float x, y; } fossil_cube_v2;
typedef struct { float r, g, b, a; } fossil_cube_color;
typedef struct { float x, y, w, h; } fossil_cube_rect;

typedef struct {
    /* Mouse */
    int mouse_down[3];     /* 0:Left 1:Right 2:Middle */
    int mouse_clicked[3];  /* set true on transition down this frame (optional helper) */
    fossil_cube_v2 mouse_pos;  /* in framebuffer pixels */
    fossil_cube_v2 mouse_delta;

    /* Keyboard (minimal immediate-mode flags; extend as you like) */
    int key_ctrl;
    int key_shift;
    int key_alt;
    int key_super;

    /* Wheel (positive up) */
    float wheel_y;

    /* Viewport info (optional overrides – otherwise context values used) */
    int fb_w, fb_h;        /* framebuffer size */
    float dpi_scale;       /* 1.0 = 96 DPI */
} fossil_cube_input;

typedef struct fossil_cube_ctx fossil_cube_ctx;

typedef struct {
    GLuint id;
    int w, h;
} fossil_cube_texture;

/* ------------------------------- Public Constants ------------------------------- */

enum {
    FOSSIL_CUBE_MAX_CMD    = 16384,
    FOSSIL_CUBE_MAX_VTX    = 131072,
    FOSSIL_CUBE_MAX_IDX    = 262144
};

/* --------------------------------- Public API ---------------------------------- */

/* Lifecycle */
fossil_cube_ctx* fossil_cube_create_context(int fb_w, int fb_h, float dpi_scale);
void             fossil_cube_destroy_context(fossil_cube_ctx* ctx);
void             fossil_cube_resize(fossil_cube_ctx* ctx, int fb_w, int fb_h, float dpi_scale);

/* Per-frame */
void fossil_cube_new_frame(fossil_cube_ctx* ctx, const fossil_cube_input* in, double dt_seconds);
void fossil_cube_render(fossil_cube_ctx* ctx);

/* Style */
typedef struct {
    fossil_cube_color clear_color;     /* background (used if you want ui to clear)      */
    fossil_cube_color panel_bg;        /* window bg                                      */
    fossil_cube_color panel_border;
    fossil_cube_color text;
    fossil_cube_color button;
    fossil_cube_color button_hot;
    fossil_cube_color button_active;
    fossil_cube_color slider_bg;
    fossil_cube_color slider_knob;
    float padding;                     /* panel inner padding                             */
    float item_spacing;                /* between widgets                                 */
    float roundness;                   /* corners (0 = sharp)                             */
    float font_px;                     /* base font size (pixels)                         */
} fossil_cube_style;

void fossil_cube_get_style(fossil_cube_ctx* ctx, fossil_cube_style* out_style);
void fossil_cube_set_style(fossil_cube_ctx* ctx, const fossil_cube_style* in_style);
void fossil_cube_style_reset_default(fossil_cube_style* st);

/* Text & basic draw (public helpers if you need them) */
float fossil_cube_text_width(fossil_cube_ctx* ctx, const char* text);
float fossil_cube_text_height(fossil_cube_ctx* ctx); /* equals style.font_px */
void  fossil_cube_draw_text(fossil_cube_ctx* ctx, float x, float y, const char* text, fossil_cube_color c);
void  fossil_cube_draw_rect(fossil_cube_ctx* ctx, fossil_cube_rect r, fossil_cube_color c, float round);
void  fossil_cube_draw_rect_line(fossil_cube_ctx* ctx, fossil_cube_rect r, float thickness, fossil_cube_color c, float round);

/* Textures */
fossil_cube_texture fossil_cube_texture_create(const void* rgba8_pixels, int w, int h, int linear_filter);
void                fossil_cube_texture_destroy(fossil_cube_texture* t);

/* Immediate-mode UI */
int  fossil_cube_begin_window(fossil_cube_ctx* ctx, const char* title, float x, float y, float w, float h, int* open_opt);
void fossil_cube_end_window(fossil_cube_ctx* ctx);

void fossil_cube_label(fossil_cube_ctx* ctx, const char* text);

/* returns 1 if clicked this frame */
int  fossil_cube_button(fossil_cube_ctx* ctx, const char* label);

/* returns 1 if value changed */
int  fossil_cube_slider(fossil_cube_ctx* ctx, const char* label, float* value, float min_v, float max_v, float step);

/* Image widget (draws an RGBA texture) */
void fossil_cube_image(fossil_cube_ctx* ctx, fossil_cube_texture tex, float w, float h);

/* Layout helpers */
void fossil_cube_same_line(fossil_cube_ctx* ctx);      /* place next item on same line */
void fossil_cube_spacing(fossil_cube_ctx* ctx, float px); /* vertical spacing push */

/* Optional: tell UI to clear background using style.clear_color (default off) */
void fossil_cube_set_clear_background(fossil_cube_ctx* ctx, int enabled);

/* Version */
const char* fossil_cube_version(void); /* "Fossil CUBE x.y.z" */

/* --------------------------------- Utilities ----------------------------------- */
static inline fossil_cube_color fossil_cube_rgba(float r,float g,float b,float a){ fossil_cube_color c={r,g,b,a}; return c; }
static inline fossil_cube_rect  fossil_cube_rect_xywh(float x,float y,float w,float h){ fossil_cube_rect r={x,y,w,h}; return r; }

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
