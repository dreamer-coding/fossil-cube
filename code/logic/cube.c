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
#include "fossil/cube/cube.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* =========================
   Internal state
   ========================= */

typedef struct fc_clip {
    int x, y, w, h;
    bool enabled;
} fc_clip;

typedef struct fc_ctx {
    int w, h;
    int pitch; /* bytes per row */
    uint8_t* pixels; /* RGBA8 */

    fossil_cube_present_fn present;
    void* userdata;

    fc_clip clip;
    bool initialized;
} fc_ctx;

static fc_ctx g_fc = {0};

/* =========================
   Helpers
   ========================= */

static inline uint8_t u8_clampi(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

static inline void blend_rgba_over(uint8_t* dst, uint8_t sr, uint8_t sg, uint8_t sb, uint8_t sa) {
    /* Straight alpha source over straight alpha dst, dst assumed fully opaque or not
       Formula: out = s + d*(1 - sa)
       Operate in 0..255 integer space
    */
    const int da = dst[3];
    const int inv_sa = 255 - sa;

    const int r = (int)sr + ((int)dst[0] * inv_sa + 127) / 255;
    const int g = (int)sg + ((int)dst[1] * inv_sa + 127) / 255;
    const int b = (int)sb + ((int)dst[2] * inv_sa + 127) / 255;
    const int a = sa + ((da * inv_sa + 127) / 255);

    dst[0] = u8_clampi(r);
    dst[1] = u8_clampi(g);
    dst[2] = u8_clampi(b);
    dst[3] = u8_clampi(a);
}

static inline bool in_clip(int x, int y) {
    if (!g_fc.clip.enabled) return true;
    return (x >= g_fc.clip.x) && (y >= g_fc.clip.y) &&
           (x < g_fc.clip.x + g_fc.clip.w) &&
           (y < g_fc.clip.y + g_fc.clip.h);
}

static inline void put_px_nocheck(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    uint8_t* p = g_fc.pixels + (size_t)y * (size_t)g_fc.pitch + (size_t)x * 4u;
    if (a == 255) {
        p[0] = r; p[1] = g; p[2] = b; p[3] = 255;
    } else if (a != 0) {
        blend_rgba_over(p, r, g, b, a);
    }
}

/* =========================
   Public API
   ========================= */

fossil_cube_result fossil_cube_init(
    int width, int height,
    fossil_cube_present_fn present,
    void* userdata)
{
    if (width <= 0 || height <= 0 || present == NULL) return FOSSIL_CUBE_ERR_BADARGS;
    if (g_fc.initialized) fossil_cube_shutdown();

    g_fc.w = width;
    g_fc.h = height;
    g_fc.pitch = width * 4;
    g_fc.present = present;
    g_fc.userdata = userdata;
    g_fc.clip.enabled = false;

    size_t sz = (size_t)g_fc.pitch * (size_t)g_fc.h;
    g_fc.pixels = (uint8_t*)malloc(sz);
    if (!g_fc.pixels) {
        memset(&g_fc, 0, sizeof(g_fc));
        return FOSSIL_CUBE_ERR_OOM;
    }
    memset(g_fc.pixels, 0, sz);

    g_fc.initialized = true;
    return FOSSIL_CUBE_OK;
}

void fossil_cube_shutdown(void) {
    if (!g_fc.initialized) return;
    free(g_fc.pixels);
    memset(&g_fc, 0, sizeof(g_fc));
}

fossil_cube_result fossil_cube_resize(int new_width, int new_height) {
    if (!g_fc.initialized) return FOSSIL_CUBE_ERR_NOTINIT;
    if (new_width <= 0 || new_height <= 0) return FOSSIL_CUBE_ERR_BADARGS;

    int new_pitch = new_width * 4;
    size_t sz = (size_t)new_pitch * (size_t)new_height;
    uint8_t* npix = (uint8_t*)malloc(sz);
    if (!npix) return FOSSIL_CUBE_ERR_OOM;

    free(g_fc.pixels);
    g_fc.pixels = npix;
    g_fc.w = new_width;
    g_fc.h = new_height;
    g_fc.pitch = new_pitch;
    g_fc.clip.enabled = false;
    memset(g_fc.pixels, 0, sz);
    return FOSSIL_CUBE_OK;
}

void fossil_cube_begin_frame(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    fossil_cube_clear(r, g, b, a);
}

void fossil_cube_end_frame(void) {
    if (!g_fc.initialized || !g_fc.present) return;
    g_fc.present(g_fc.pixels, g_fc.w, g_fc.h, g_fc.pitch, g_fc.userdata);
}

void fossil_cube_clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!g_fc.initialized) return;
    /* Fast clear: fill 32-bit pattern */
    uint32_t color = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
    uint32_t* row = (uint32_t*)g_fc.pixels;
    size_t count = (size_t)g_fc.w * (size_t)g_fc.h;
    for (size_t i = 0; i < count; ++i) row[i] = color;
}

void fossil_cube_set_clip(int x, int y, int w, int h) {
    if (!g_fc.initialized) return;
    if (w <= 0 || h <= 0) {
        g_fc.clip.enabled = false;
        return;
    }
    /* clamp to framebuffer */
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w; if (x1 > g_fc.w) x1 = g_fc.w;
    int y1 = y + h; if (y1 > g_fc.h) y1 = g_fc.h;

    g_fc.clip.x = x0;
    g_fc.clip.y = y0;
    g_fc.clip.w = (x1 > x0) ? (x1 - x0) : 0;
    g_fc.clip.h = (y1 > y0) ? (y1 - y0) : 0;
    g_fc.clip.enabled = (g_fc.clip.w > 0 && g_fc.clip.h > 0);
}

void fossil_cube_get_clip(int* x, int* y, int* w, int* h) {
    if (x) *x = g_fc.clip.x;
    if (y) *y = g_fc.clip.y;
    if (w) *w = g_fc.clip.w;
    if (h) *h = g_fc.clip.h;
}

void fossil_cube_put_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!g_fc.initialized) return;
    if ((unsigned)x >= (unsigned)g_fc.w || (unsigned)y >= (unsigned)g_fc.h) return;
    if (!in_clip(x, y)) return;
    put_px_nocheck(x, y, r, g, b, a);
}

void fossil_cube_fill_rect(int x, int y, int w, int h,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!g_fc.initialized) return;
    if (w <= 0 || h <= 0) return;

    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > g_fc.w) x1 = g_fc.w;
    if (y1 > g_fc.h) y1 = g_fc.h;

    for (int yy = y0; yy < y1; ++yy) {
        for (int xx = x0; xx < x1; ++xx) {
            if (!in_clip(xx, yy)) continue;
            put_px_nocheck(xx, yy, r, g, b, a);
        }
    }
}

/* Bresenham line */
void fossil_cube_draw_line(int x0, int y0, int x1, int y1,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!g_fc.initialized) return;

    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 > y0) ? (y0 - y1) : (y1 - y0); /* negative for standard algorithm */
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        fossil_cube_put_pixel(x0, y0, r, g, b, a);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err << 1;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
        if ((unsigned)x0 >= (unsigned)g_fc.w && (unsigned)y0 >= (unsigned)g_fc.h) break;
    }
}

void fossil_cube_blit_rgba(int dst_x, int dst_y,
                           const uint8_t* src, int src_w, int src_h, int src_pitch) {
    if (!g_fc.initialized || !src || src_w <= 0 || src_h <= 0) return;

    int x0 = dst_x < 0 ? 0 : dst_x;
    int y0 = dst_y < 0 ? 0 : dst_y;
    int x1 = dst_x + src_w; if (x1 > g_fc.w) x1 = g_fc.w;
    int y1 = dst_y + src_h; if (y1 > g_fc.h) y1 = g_fc.h;
    if (x0 >= x1 || y0 >= y1) return;

    for (int y = y0; y < y1; ++y) {
        const uint8_t* srow = src + (size_t)(y - dst_y) * (size_t)src_pitch
                                   + (size_t)(x0 - dst_x) * 4u;
        uint8_t* drow = g_fc.pixels + (size_t)y * (size_t)g_fc.pitch + (size_t)x0 * 4u;

        for (int x = x0; x < x1; ++x) {
            if (in_clip(x, y)) {
                const uint8_t sr = srow[0], sg = srow[1], sb = srow[2], sa = srow[3];
                if (sa == 255) {
                    drow[0] = sr; drow[1] = sg; drow[2] = sb; drow[3] = 255;
                } else if (sa != 0) {
                    blend_rgba_over(drow, sr, sg, sb, sa);
                }
            }
            srow += 4;
            drow += 4;
        }
    }
}

uint8_t* fossil_cube_framebuffer(int* out_w, int* out_h, int* out_pitch) {
    if (out_w) *out_w = g_fc.w;
    if (out_h) *out_h = g_fc.h;
    if (out_pitch) *out_pitch = g_fc.pitch;
    return g_fc.pixels;
}

int fossil_cube_width(void)  { return g_fc.w; }
int fossil_cube_height(void) { return g_fc.h; }
