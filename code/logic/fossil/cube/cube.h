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

typedef struct {
    int width;
    int height;
    uint32_t *pixels; // RGBA8888
    void *platform;   // OS-specific data
} fossil_cube_t;

bool fossil_cube_init(fossil_cube_t *cube, int width, int height, const char *title);
void fossil_cube_shutdown(fossil_cube_t *cube);

void fossil_cube_draw_pixel(fossil_cube_t *cube, int x, int y, uint32_t color);
void fossil_cube_clear(fossil_cube_t *cube, uint32_t color);

void fossil_cube_present(fossil_cube_t *cube); // Blit framebuffer to window

bool fossil_cube_poll_event(fossil_cube_t *cube, int *event_type, int *p1, int *p2);

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
