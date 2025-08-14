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

/* Fossil CUBE: minimal, portable, software 2D core (no GL, no OS deps)
   - Single instance, single-threaded core
   - Your app provides a 'present' callback to display the RGBA buffer
*/

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Result codes */
typedef enum fossil_cube_result {
    FOSSIL_CUBE_OK = 0,
    FOSSIL_CUBE_ERR_BADARGS = -1,
    FOSSIL_CUBE_ERR_OOM = -2,
    FOSSIL_CUBE_ERR_NOTINIT = -3
} fossil_cube_result;

/* Present callback:
   - pixels: pointer to tightly-packed RGBA8 buffer (row-major)
   - width/height: size in pixels
   - pitch: bytes per row (always width*4 in this core)
   - userdata: passthrough pointer you supplied at init
*/
typedef void (*fossil_cube_present_fn)(
    const uint8_t* pixels, int width, int height, int pitch, void* userdata);

/* Init / Shutdown */
fossil_cube_result fossil_cube_init(
    int width, int height,
    fossil_cube_present_fn present,
    void* userdata);

void fossil_cube_shutdown(void);

/* Resize the internal framebuffer (contents are undefined after) */
fossil_cube_result fossil_cube_resize(int new_width, int new_height);

/* Begin/End frame workflow */
void fossil_cube_begin_frame(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void fossil_cube_end_frame(void); /* calls your present() */

/* Immediate 2D drawing (software) */
void fossil_cube_clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void fossil_cube_set_clip(int x, int y, int w, int h); /* set clip rect; w/h<=0 disables clipping */
void fossil_cube_get_clip(int* x, int* y, int* w, int* h);

void fossil_cube_put_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/* Filled rectangle (alpha-blended) */
void fossil_cube_fill_rect(int x, int y, int w, int h,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/* 1px line (alpha-blended) using Bresenham */
void fossil_cube_draw_line(int x0, int y0, int x1, int y1,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/* Blit a source RGBA buffer (premultiplied or straight? -> straight alpha expected) */
void fossil_cube_blit_rgba(int dst_x, int dst_y,
                           const uint8_t* src, int src_w, int src_h, int src_pitch);

/* Access to the raw framebuffer if the app wants to do custom drawing */
uint8_t* fossil_cube_framebuffer(int* out_w, int* out_h, int* out_pitch);

/* Utilities */
int fossil_cube_width(void);
int fossil_cube_height(void);

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
