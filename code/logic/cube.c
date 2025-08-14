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

#if defined(_WIN32)
    #include <windows.h>
#elif defined(__APPLE__)
    #include <ApplicationServices/ApplicationServices.h>
    #include <CoreGraphics/CoreGraphics.h>
    #include <Carbon/Carbon.h>
#else
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
#endif

// Platform-specific storage
typedef struct {
#if defined(_WIN32)
    HWND hwnd;
    HDC hdc;
    BITMAPINFO bmi;
#elif defined(__APPLE__)
    CGColorSpaceRef colorSpace;
    CGContextRef context;
#else
    Display *dpy;
    Window win;
    GC gc;
    XImage *image;
#endif
} cube_platform_t;

// ----------------- PUBLIC API -----------------

bool fossil_cube_init(fossil_cube_t *cube, int width, int height, const char *title) {
    if (!cube) return false;
    cube->width = width;
    cube->height = height;
    cube->pixels = calloc(width * height, sizeof(uint32_t));
    if (!cube->pixels) return false;

    cube_platform_t *plat = calloc(1, sizeof(cube_platform_t));
    cube->platform = plat;

#if defined(_WIN32)
    WNDCLASS wc = {0};
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "FossilCube";
    RegisterClass(&wc);

    plat->hwnd = CreateWindow("FossilCube", title, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                              CW_USEDEFAULT, CW_USEDEFAULT, width, height,
                              NULL, NULL, wc.hInstance, NULL);
    plat->hdc = GetDC(plat->hwnd);

    plat->bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    plat->bmi.bmiHeader.biWidth = width;
    plat->bmi.bmiHeader.biHeight = -height;
    plat->bmi.bmiHeader.biPlanes = 1;
    plat->bmi.bmiHeader.biBitCount = 32;
    plat->bmi.bmiHeader.biCompression = BI_RGB;

#elif defined(__APPLE__)
    plat->colorSpace = CGColorSpaceCreateDeviceRGB();
#else
    plat->dpy = XOpenDisplay(NULL);
    int scr = DefaultScreen(plat->dpy);
    plat->win = XCreateSimpleWindow(plat->dpy, RootWindow(plat->dpy, scr),
                                    0, 0, width, height, 0,
                                    BlackPixel(plat->dpy, scr),
                                    WhitePixel(plat->dpy, scr));
    XStoreName(plat->dpy, plat->win, title);
    XMapWindow(plat->dpy, plat->win);
    plat->gc = XCreateGC(plat->dpy, plat->win, 0, NULL);

    plat->image = XCreateImage(plat->dpy, DefaultVisual(plat->dpy, scr), 24,
                               ZPixmap, 0, (char*)cube->pixels,
                               width, height, 32, 0);
#endif

    return true;
}

void fossil_cube_shutdown(fossil_cube_t *cube) {
    if (!cube) return;
    cube_platform_t *plat = cube->platform;
#if defined(_WIN32)
    ReleaseDC(plat->hwnd, plat->hdc);
    DestroyWindow(plat->hwnd);
#elif defined(__APPLE__)
    CGColorSpaceRelease(plat->colorSpace);
#else
    XDestroyImage(plat->image);
    XCloseDisplay(plat->dpy);
#endif
    free(plat);
    free(cube->pixels);
}

void fossil_cube_draw_pixel(fossil_cube_t *cube, int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= cube->width || y >= cube->height) return;
    cube->pixels[y * cube->width + x] = color;
}

void fossil_cube_clear(fossil_cube_t *cube, uint32_t color) {
    for (int i = 0; i < cube->width * cube->height; i++)
        cube->pixels[i] = color;
}

void fossil_cube_present(fossil_cube_t *cube) {
    cube_platform_t *plat = cube->platform;
#if defined(_WIN32)
    StretchDIBits(plat->hdc, 0, 0, cube->width, cube->height,
                  0, 0, cube->width, cube->height, cube->pixels,
                  &plat->bmi, DIB_RGB_COLORS, SRCCOPY);
#elif defined(__APPLE__)
    // macOS needs a real window binding, omitted for brevity
#else
    XPutImage(plat->dpy, plat->win, plat->gc, plat->image,
              0, 0, 0, 0, cube->width, cube->height);
    XFlush(plat->dpy);
#endif
}

bool fossil_cube_poll_event(fossil_cube_t *cube, int *event_type, int *p1, int *p2) {
    if (!cube) return false;
    cube_platform_t *plat = cube->platform;

#if defined(_WIN32)
    MSG msg;
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        // Map some basic events
        if (event_type) *event_type = msg.message;
        if (p1) *p1 = (int)msg.wParam;
        if (p2) *p2 = (int)msg.lParam;

        return true;
    }

#elif defined(__APPLE__)
    // macOS event handling omitted
#else
    if (XPending(plat->dpy)) {
        XEvent e;
        XNextEvent(plat->dpy, &e);

        if (event_type) *event_type = e.type;

        switch (e.type) {
            case KeyPress:
            case KeyRelease:
                if (p1) *p1 = e.xkey.keycode;   // keycode
                if (p2) *p2 = e.xkey.state;     // modifier mask
                break;
            case ButtonPress:
            case ButtonRelease:
                if (p1) *p1 = e.xbutton.button; // mouse button
                if (p2) *p2 = e.xbutton.state;  // modifier mask
                break;
            case MotionNotify:
                if (p1) *p1 = e.xmotion.x;
                if (p2) *p2 = e.xmotion.y;
                break;
            default:
                if (p1) *p1 = 0;
                if (p2) *p2 = 0;
                break;
        }
        return true;
    }
#endif

    // No event
    if (event_type) *event_type = 0;
    if (p1) *p1 = 0;
    if (p2) *p2 = 0;
    return false;
}
