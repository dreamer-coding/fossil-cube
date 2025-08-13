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
#define FOSSIL_CUBE_IMPLEMENTATION
#include "fossil/cube/cube.h"
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------
 * Common internal state and helpers
 * -------------------------------------------------------------- */

struct fossil_cube_window_t {
    int width;
    int height;
    int double_buffer;
    int headless;      /* 1 for macOS CGL pbuffer path */
    int vsync;

#if defined(_WIN32) || defined(_WIN64)
    HWND   hwnd;
    HDC    hdc;
    HGLRC  hglrc;
    int    close_requested;
#elif defined(__APPLE__)
    /* Pure C headless via CGL pbuffer */
    void*  cgl_ctx;        /* CGLContextObj */
    void*  cgl_pbuffer;    /* CGLPBufferObj */
#else
    /* X11 + GLX */
    Display* dpy;
    Window   win;
    GLXContext ctx;
    Colormap cmap;
    Atom     wm_delete;
    int      close_requested;
#endif
};

/* Static flags */
static int g_inited = 0;

/* Error strings */
const char* fossil_cube_strerror(fossil_cube_result_t err) {
    switch (err) {
        case FOSSIL_CUBE_OK: return "OK";
        case FOSSIL_CUBE_ERR_GENERIC: return "Generic error";
        case FOSSIL_CUBE_ERR_PLATFORM: return "Platform error";
        case FOSSIL_CUBE_ERR_NO_DISPLAY: return "No display found";
        case FOSSIL_CUBE_ERR_CREATE_WINDOW: return "Failed to create window";
        case FOSSIL_CUBE_ERR_CREATE_CONTEXT: return "Failed to create GL context";
        case FOSSIL_CUBE_ERR_MAKE_CURRENT: return "Failed to make context current";
        case FOSSIL_CUBE_ERR_GL_LOADER: return "Failed to resolve GL procedure";
        case FOSSIL_CUBE_ERR_HEADLESS_ONLY: return "This platform build is headless-only";
        default: return "Unknown error";
    }
}

fossil_cube_result_t fossil_cube_init(void) {
    if (g_inited) return FOSSIL_CUBE_OK;
#if defined(_WIN32) || defined(_WIN64)
    /* Nothing global needed. */
    g_inited = 1;
    return FOSSIL_CUBE_OK;
#elif defined(__APPLE__)
    /* No global init required for CGL. */
    g_inited = 1;
    return FOSSIL_CUBE_OK;
#else
    /* X11 has per-window Display; no global init needed. */
    g_inited = 1;
    return FOSSIL_CUBE_OK;
#endif
}

void fossil_cube_shutdown(void) {
    g_inited = 0;
}

/* Forward decls per-platform */
static fossil_cube_window_t* cube_create_platform(const fossil_cube_config_t* cfg, fossil_cube_result_t* out_err);
static void cube_destroy_platform(fossil_cube_window_t* win);
static fossil_cube_result_t cube_make_current_platform(fossil_cube_window_t* win);
static void cube_swap_buffers_platform(fossil_cube_window_t* win);
static void cube_poll_events_platform(fossil_cube_window_t* win, fossil_cube_events_t* out_events);
static void cube_set_vsync_platform(fossil_cube_window_t* win, int interval);
static void* cube_get_proc_address_platform(const char* name);

fossil_cube_window_t* fossil_cube_create(const fossil_cube_config_t* cfg, fossil_cube_result_t* out_err) {
    if (!g_inited) fossil_cube_init();
    fossil_cube_result_t dummy;
    if (!out_err) out_err = &dummy;
    if (!cfg) { *out_err = FOSSIL_CUBE_ERR_GENERIC; return NULL; }
    return cube_create_platform(cfg, out_err);
}

void fossil_cube_destroy(fossil_cube_window_t* win) {
    if (!win) return;
    cube_destroy_platform(win);
}

fossil_cube_result_t fossil_cube_make_current(fossil_cube_window_t* win) {
    if (!win) return FOSSIL_CUBE_ERR_GENERIC;
    return cube_make_current_platform(win);
}

void fossil_cube_swap_buffers(fossil_cube_window_t* win) {
    if (!win) return;
    cube_swap_buffers_platform(win);
}

void fossil_cube_poll_events(fossil_cube_window_t* win, fossil_cube_events_t* out_events) {
    if (!out_events) return;
    memset(out_events, 0, sizeof(*out_events));
    if (!win) return;
    cube_poll_events_platform(win, out_events);
}

void fossil_cube_set_vsync(fossil_cube_window_t* win, int interval) {
    if (!win) return;
    cube_set_vsync_platform(win, interval);
}

void* fossil_cube_get_proc_address(const char* name) {
    return cube_get_proc_address_platform(name);
}

int fossil_cube_width(const fossil_cube_window_t* win)  { return win ? win->width  : 0; }
int fossil_cube_height(const fossil_cube_window_t* win) { return win ? win->height : 0; }
int fossil_cube_is_headless(const fossil_cube_window_t* win) { return win ? win->headless : 0; }

/* Minimal helper: poll + swap; returns 0 if close requested */
int fossil_cube_frame(fossil_cube_window_t* win, fossil_cube_events_t* ev) {
    fossil_cube_poll_events(win, ev);
    if (ev && ev->should_close) return 0;
    fossil_cube_swap_buffers(win);
    return 1;
}

/* --------------------------------------------------------------
 * Windows (Win32 + WGL)
 * -------------------------------------------------------------- */
#if defined(_WIN32) || defined(_WIN64)

static LRESULT CALLBACK cube_wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    fossil_cube_window_t* win = (fossil_cube_window_t*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CLOSE:
            if (win) win->headless = 0; /* no-op, but keep compiler happy */
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            if (win) {
                win->width  = LOWORD(lParam);
                win->height = HIWORD(lParam);
            }
            break;
        default:
            break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/* wglSwapIntervalEXT loader */
typedef BOOL (WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int);
static PFNWGLSWAPINTERVALEXTPROC p_wglSwapIntervalEXT = NULL;

static void cube_win_try_load_swapinterval(void) {
    if (!p_wglSwapIntervalEXT) {
        p_wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
    }
}

static fossil_cube_window_t* cube_create_platform(const fossil_cube_config_t* cfg, fossil_cube_result_t* out_err) {
    *out_err = FOSSIL_CUBE_ERR_GENERIC;

    WNDCLASSA wc = {0};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = cube_wndproc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "FossilCubeWndClass";

    if (!RegisterClassA(&wc)) {
        /* Might be already registered; ignore failure if so */
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rect = {0, 0, cfg->width > 0 ? cfg->width : 640, cfg->height > 0 ? cfg->height : 480};
    AdjustWindowRect(&rect, style, FALSE);

    HWND hwnd = CreateWindowA(
        wc.lpszClassName,
        cfg->title ? cfg->title : "Fossil CUBE",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        NULL, NULL, wc.hInstance, NULL
    );
    if (!hwnd) { *out_err = FOSSIL_CUBE_ERR_CREATE_WINDOW; return NULL; }

    HDC hdc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | (cfg->double_buffer ? PFD_DOUBLEBUFFER : 0);
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = (BYTE)(cfg->color_bits ? cfg->color_bits : 24);
    pfd.cDepthBits = (BYTE)(cfg->depth_bits ? cfg->depth_bits : 24);
    pfd.cStencilBits = (BYTE)(cfg->stencil_bits ? cfg->stencil_bits : 8);
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(hdc, &pfd);
    if (pf == 0 || !SetPixelFormat(hdc, pf, &pfd)) {
        DestroyWindow(hwnd);
        *out_err = FOSSIL_CUBE_ERR_CREATE_CONTEXT;
        return NULL;
    }

    HGLRC hglrc = wglCreateContext(hdc);
    if (!hglrc) {
        DestroyWindow(hwnd);
        *out_err = FOSSIL_CUBE_ERR_CREATE_CONTEXT;
        return NULL;
    }

    fossil_cube_window_t* win = (fossil_cube_window_t*)calloc(1, sizeof(*win));
    win->width  = cfg->width  > 0 ? cfg->width  : 640;
    win->height = cfg->height > 0 ? cfg->height : 480;
    win->double_buffer = cfg->double_buffer ? 1 : 0;
    win->headless = 0;
    win->hwnd = hwnd;
    win->hdc = hdc;
    win->hglrc = hglrc;

    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)win);
    ShowWindow(hwnd, SW_SHOW);

    if (wglMakeCurrent(hdc, hglrc) == FALSE) {
        fossil_cube_destroy(win);
        *out_err = FOSSIL_CUBE_ERR_MAKE_CURRENT;
        return NULL;
    }

    cube_win_try_load_swapinterval();
    if (cfg->vsync && p_wglSwapIntervalEXT) p_wglSwapIntervalEXT(1);

    *out_err = FOSSIL_CUBE_OK;
    return win;
}

static void cube_destroy_platform(fossil_cube_window_t* win) {
    if (!win) return;
    if (win->hglrc) { wglMakeCurrent(NULL, NULL); wglDeleteContext(win->hglrc); }
    if (win->hwnd && win->hdc) { ReleaseDC(win->hwnd, win->hdc); }
    if (win->hwnd) DestroyWindow(win->hwnd);
    free(win);
}

static fossil_cube_result_t cube_make_current_platform(fossil_cube_window_t* win) {
    if (!wglMakeCurrent(win->hdc, win->hglrc)) return FOSSIL_CUBE_ERR_MAKE_CURRENT;
    return FOSSIL_CUBE_OK;
}

static void cube_swap_buffers_platform(fossil_cube_window_t* win) {
    if (win->double_buffer && win->hdc) SwapBuffers(win->hdc);
}

static void cube_poll_events_platform(fossil_cube_window_t* win, fossil_cube_events_t* out_events) {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) out_events->should_close = 1;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    /* Update size from current client rect */
    RECT r;
    if (GetClientRect(win->hwnd, &r)) {
        int w = r.right - r.left;
        int h = r.bottom - r.top;
        if (w != win->width || h != win->height) {
            win->width = w; win->height = h;
            out_events->resized = 1;
            out_events->width = w;
            out_events->height = h;
        }
    }
}

static void cube_set_vsync_platform(fossil_cube_window_t* win, int interval) {
    (void)win;
    cube_win_try_load_swapinterval();
    if (p_wglSwapIntervalEXT) p_wglSwapIntervalEXT(interval ? 1 : 0);
}

static void* cube_get_proc_address_platform(const char* name) {
    void* p = (void*)wglGetProcAddress(name);
    if (!p) {
        HMODULE lib = LoadLibraryA("opengl32.dll");
        if (lib) p = (void*)GetProcAddress(lib, name);
    }
    return p;
}

/* --------------------------------------------------------------
 * macOS (CGL headless pbuffer)
 * -------------------------------------------------------------- */
#elif defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/OpenGL.h>

static fossil_cube_window_t* cube_create_platform(const fossil_cube_config_t* cfg, fossil_cube_result_t* out_err) {
    *out_err = FOSSIL_CUBE_ERR_GENERIC;

    CGLPixelFormatAttribute attrs[] = {
        kCGLPFAAccelerated,
        kCGLPFADoubleBuffer,        /* honored only for onscreen; harmless here */
        kCGLPFAColorSize, (CGLPixelFormatAttribute)(cfg->color_bits ? cfg->color_bits : 24),
        kCGLPFADepthSize, (CGLPixelFormatAttribute)(cfg->depth_bits ? cfg->depth_bits : 24),
        kCGLPFAStencilSize, (CGLPixelFormatAttribute)(cfg->stencil_bits ? cfg->stencil_bits : 8),
        (CGLPixelFormatAttribute)0
    };

    CGLPixelFormatObj pf = NULL;
    GLint npf = 0;
    CGLError e = CGLChoosePixelFormat(attrs, &pf, &npf);
    if (e != kCGLNoError || !pf) { *out_err = FOSSIL_CUBE_ERR_CREATE_CONTEXT; return NULL; }

    CGLContextObj ctx = NULL;
    e = CGLCreateContext(pf, NULL, &ctx);
    CGLReleasePixelFormat(pf);
    if (e != kCGLNoError || !ctx) { *out_err = FOSSIL_CUBE_ERR_CREATE_CONTEXT; return NULL; }

    /* Create a pbuffer for offscreen rendering */
    GLint w = cfg->width  > 0 ? cfg->width  : 640;
    GLint h = cfg->height > 0 ? cfg->height : 480;
    CGLPBufferObj pb = NULL;
    e = CGLCreatePBuffer(w, h, GL_TEXTURE_2D, GL_RGBA, 0, &pb);
    if (e != kCGLNoError || !pb) {
        CGLDestroyContext(ctx);
        *out_err = FOSSIL_CUBE_ERR_CREATE_CONTEXT;
        return NULL;
    }

    e = CGLSetPBuffer(ctx, pb, 0, 0, 0);
    if (e != kCGLNoError) {
        CGLDestroyPBuffer(pb);
        CGLDestroyContext(ctx);
        *out_err = FOSSIL_CUBE_ERR_CREATE_CONTEXT;
        return NULL;
    }

    fossil_cube_window_t* win = (fossil_cube_window_t*)calloc(1, sizeof(*win));
    win->width = w;
    win->height = h;
    win->double_buffer = cfg->double_buffer ? 1 : 0; /* mostly irrelevant offscreen */
    win->headless = 1;
    win->cgl_ctx = ctx;
    win->cgl_pbuffer = pb;

    if (CGLSetCurrentContext(ctx) != kCGLNoError) {
        fossil_cube_destroy(win);
        *out_err = FOSSIL_CUBE_ERR_MAKE_CURRENT;
        return NULL;
    }

    *out_err = FOSSIL_CUBE_OK;
    return win;
}

static void cube_destroy_platform(fossil_cube_window_t* win) {
    if (!win) return;
    if (win->cgl_ctx) {
        CGLSetCurrentContext(NULL);
        CGLDestroyContext((CGLContextObj)win->cgl_ctx);
    }
    if (win->cgl_pbuffer) {
        CGLDestroyPBuffer((CGLPBufferObj)win->cgl_pbuffer);
    }
    free(win);
}

static fossil_cube_result_t cube_make_current_platform(fossil_cube_window_t* win) {
    return (CGLSetCurrentContext((CGLContextObj)win->cgl_ctx) == kCGLNoError) ?
           FOSSIL_CUBE_OK : FOSSIL_CUBE_ERR_MAKE_CURRENT;
}

static void cube_swap_buffers_platform(fossil_cube_window_t* win) {
    (void)win; /* Offscreen; nothing to swap. */
}

static void cube_poll_events_platform(fossil_cube_window_t* win, fossil_cube_events_t* out_events) {
    (void)win;
    (void)out_events;
    /* Headless: no events. */
}

static void cube_set_vsync_platform(fossil_cube_window_t* win, int interval) {
    (void)win; (void)interval; /* Offscreen; ignored. */
}

static void* cube_get_proc_address_platform(const char* name) {
    /* CGL doesn’t have a general GetProc; core GL 2.1 is available. Return NULL for extensions. */
    (void)name;
    return NULL;
}

/* --------------------------------------------------------------
 * Linux/*BSD (X11 + GLX)
 * -------------------------------------------------------------- */
#else

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <GL/glx.h>

typedef int (*PFNGLXSWAPINTERVALEXTPROC)(Display*, GLXDrawable, int);
typedef int (*PFNGLXSWAPINTERVALSGIPROC)(int);
static PFNGLXSWAPINTERVALEXTPROC p_glXSwapIntervalEXT = NULL;
static PFNGLXSWAPINTERVALSGIPROC p_glXSwapIntervalSGI = NULL;

static void cube_glx_try_load_swapinterval(Display* dpy) {
    if (!p_glXSwapIntervalEXT) {
        p_glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalEXT");
    }
    if (!p_glXSwapIntervalSGI) {
        p_glXSwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalSGI");
    }
    (void)dpy;
}

static fossil_cube_window_t* cube_create_platform(const fossil_cube_config_t* cfg, fossil_cube_result_t* out_err) {
    *out_err = FOSSIL_CUBE_ERR_GENERIC;

    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) { *out_err = FOSSIL_CUBE_ERR_NO_DISPLAY; return NULL; }

    int screen = DefaultScreen(dpy);

    int att[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_RED_SIZE,    8,
        GLX_GREEN_SIZE,  8,
        GLX_BLUE_SIZE,   8,
        GLX_DEPTH_SIZE,  cfg->depth_bits ? cfg->depth_bits : 24,
        GLX_STENCIL_SIZE,cfg->stencil_bits ? cfg->stencil_bits : 8,
        None
    };

    XVisualInfo* vi = glXChooseVisual(dpy, screen, att);
    if (!vi) {
        XCloseDisplay(dpy);
        *out_err = FOSSIL_CUBE_ERR_CREATE_CONTEXT;
        return NULL;
    }

    Colormap cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);
    XSetWindowAttributes swa;
    memset(&swa, 0, sizeof(swa));
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask;

    int w = cfg->width > 0 ? cfg->width : 640;
    int h = cfg->height > 0 ? cfg->height : 480;

    Window win = XCreateWindow(
        dpy, RootWindow(dpy, vi->screen),
        0, 0, (unsigned)w, (unsigned)h, 0,
        vi->depth, InputOutput, vi->visual,
        CWColormap | CWEventMask, &swa
    );

    if (!win) {
        XFreeColormap(dpy, cmap);
        XFree(vi);
        XCloseDisplay(dpy);
        *out_err = FOSSIL_CUBE_ERR_CREATE_WINDOW;
        return NULL;
    }

    XStoreName(dpy, win, cfg->title ? cfg->title : "Fossil CUBE");
    XMapWindow(dpy, win);

    GLXContext ctx = glXCreateContext(dpy, vi, 0, True);
    XFree(vi);
    if (!ctx) {
        XDestroyWindow(dpy, win);
        XFreeColormap(dpy, cmap);
        XCloseDisplay(dpy);
        *out_err = FOSSIL_CUBE_ERR_CREATE_CONTEXT;
        return NULL;
    }

    if (!glXMakeCurrent(dpy, win, ctx)) {
        glXDestroyContext(dpy, ctx);
        XDestroyWindow(dpy, win);
        XFreeColormap(dpy, cmap);
        XCloseDisplay(dpy);
        *out_err = FOSSIL_CUBE_ERR_MAKE_CURRENT;
        return NULL;
    }

    fossil_cube_window_t* cw = (fossil_cube_window_t*)calloc(1, sizeof(*cw));
    cw->dpy = dpy;
    cw->win = win;
    cw->ctx = ctx;
    cw->cmap = cmap;
    cw->width = w;
    cw->height = h;
    cw->double_buffer = 1;
    cw->headless = 0;

    cw->wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &cw->wm_delete, 1);

    cube_glx_try_load_swapinterval(dpy);
    if (cfg->vsync) {
        if (p_glXSwapIntervalEXT) {
            p_glXSwapIntervalEXT(dpy, glXGetCurrentDrawable(), 1);
        } else if (p_glXSwapIntervalSGI) {
            p_glXSwapIntervalSGI(1);
        }
    }

    *out_err = FOSSIL_CUBE_OK;
    return cw;
}

static void cube_destroy_platform(fossil_cube_window_t* win) {
    if (!win) return;
    if (win->dpy) {
        glXMakeCurrent(win->dpy, None, NULL);
        if (win->ctx) glXDestroyContext(win->dpy, win->ctx);
        if (win->win) XDestroyWindow(win->dpy, win->win);
        if (win->cmap) XFreeColormap(win->dpy, win->cmap);
        XCloseDisplay(win->dpy);
    }
    free(win);
}

static fossil_cube_result_t cube_make_current_platform(fossil_cube_window_t* win) {
    return glXMakeCurrent(win->dpy, win->win, win->ctx) ? FOSSIL_CUBE_OK : FOSSIL_CUBE_ERR_MAKE_CURRENT;
}

static void cube_swap_buffers_platform(fossil_cube_window_t* win) {
    if (win->dpy && win->win) glXSwapBuffers(win->dpy, win->win);
}

static void cube_poll_events_platform(fossil_cube_window_t* win, fossil_cube_events_t* out_events) {
    while (XPending(win->dpy)) {
        XEvent e;
        XNextEvent(win->dpy, &e);
        switch (e.type) {
            case ClientMessage:
                if ((Atom)e.xclient.data.l[0] == win->wm_delete) {
                    out_events->should_close = 1;
                }
                break;
            case ConfigureNotify: {
                int nw = e.xconfigure.width;
                int nh = e.xconfigure.height;
                if (nw != win->width || nh != win->height) {
                    win->width = nw; win->height = nh;
                    out_events->resized = 1;
                    out_events->width = nw;
                    out_events->height = nh;
                }
                break;
            }
            default:
                break;
        }
    }
}

static void cube_set_vsync_platform(fossil_cube_window_t* win, int interval) {
    (void)win;
    if (p_glXSwapIntervalEXT) {
        p_glXSwapIntervalEXT(glXGetCurrentDisplay(), glXGetCurrentDrawable(), interval ? 1 : 0);
    } else if (p_glXSwapIntervalSGI) {
        p_glXSwapIntervalSGI(interval ? 1 : 0);
    }
}

static void* cube_get_proc_address_platform(const char* name) {
    return (void*)glXGetProcAddressARB((const GLubyte*)name);
}

#endif /* Platforms */
