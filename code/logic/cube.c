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
#include <stdio.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    #include <GL/gl.h>
    /* WGL extension pointer types (minimal set we use) */
    typedef const char* (WINAPI *PFNWGLGETEXTENSIONSSTRINGARBPROC)(HDC);
    typedef BOOL (WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int);
    typedef int  (WINAPI *PFNWGLGETSWAPINTERVALEXTPROC)(void);
    static PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB_;
    static PFNWGLSWAPINTERVALEXTPROC        wglSwapIntervalEXT_;
    static PFNWGLGETSWAPINTERVALEXTPROC     wglGetSwapIntervalEXT_;
#elif defined(__APPLE__)
    #include <dlfcn.h>
    #include <sys/time.h>
    /* OpenGL is provided by the system; context is expected via attach API. */
#else
    /* Linux / X11 + GLX */
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <X11/Xatom.h>
    #include <GL/glx.h>
    typedef GLXContext (*PFNGLXCREATECONTEXTATTRIBSARBPROC)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
    typedef void* (*PFNGLXGETPROCADDRESSARBPROC)(const GLubyte*);
    static PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB_;
    static PFNGLXGETPROCADDRESSARBPROC       glXGetProcAddressARB_;
#endif

/* --- Minimal modern GL function pointers (loaded at runtime where required) --- */
/* We keep this tiny: shaders, program, buffers, vertex arrays, attribs, textures, draw. */
#define GL_FUNC(ret, name, args) typedef ret (APIENTRY *PFN_##name) args; static PFN_##name name;
#if defined(_WIN32) || defined(_WIN64)
    #define GL_LOAD(name) name = (PFN_##name)wglGetProcAddress(#name); if(!(name)) name = (PFN_##name)GetProcAddress(GetModuleHandleA("opengl32.dll"), #name)
#elif defined(__APPLE__)
    #define GL_LOAD(name) name = (PFN_##name)dlsym(RTLD_DEFAULT, #name)
#else
    #define GL_LOAD(name) name = (PFN_##name)glXGetProcAddressARB_((const GLubyte*)#name)
#endif

/* Core we use (available since GL 2.0+, VAO 3.0 or ARB ext) */
GL_FUNC(GLuint, glCreateShader, (GLenum))
GL_FUNC(void,   glShaderSource, (GLuint, GLsizei, const GLchar* const*, const GLint*))
GL_FUNC(void,   glCompileShader, (GLuint))
GL_FUNC(void,   glGetShaderiv, (GLuint, GLenum, GLint*))
GL_FUNC(void,   glGetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*))
GL_FUNC(GLuint, glCreateProgram, (void))
GL_FUNC(void,   glAttachShader, (GLuint, GLuint))
GL_FUNC(void,   glLinkProgram, (GLuint))
GL_FUNC(void,   glGetProgramiv, (GLuint, GLenum, GLint*))
GL_FUNC(void,   glGetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*))
GL_FUNC(void,   glDeleteShader, (GLuint))
GL_FUNC(void,   glDeleteProgram, (GLuint))
GL_FUNC(void,   glUseProgram, (GLuint))
GL_FUNC(void,   glGenBuffers, (GLsizei, GLuint*))
GL_FUNC(void,   glBindBuffer, (GLenum, GLuint))
GL_FUNC(void,   glBufferData, (GLenum, ptrdiff_t, const void*, GLenum))
GL_FUNC(void,   glEnableVertexAttribArray, (GLuint))
GL_FUNC(void,   glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*))
GL_FUNC(void,   glDisableVertexAttribArray, (GLuint))
/* VAOs (may be missing on GL 2.1; we handle gracefully) */
GL_FUNC(void,   glGenVertexArrays, (GLsizei, GLuint*))
GL_FUNC(void,   glBindVertexArray, (GLuint))
GL_FUNC(void,   glDeleteVertexArrays, (GLsizei, const GLuint*))
/* Textures (needed by many apps; still non-deprecated) */
GL_FUNC(void,   glActiveTexture, (GLenum))
GL_FUNC(void,   glGenTextures, (GLsizei, GLuint*))
GL_FUNC(void,   glBindTexture, (GLenum, GLuint))
GL_FUNC(void,   glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*))
GL_FUNC(void,   glTexParameteri, (GLenum, GLenum, GLint))
GL_FUNC(void,   glGenerateMipmap, (GLenum))
GL_FUNC(void,   glDeleteTextures, (GLsizei, const GLuint*))
/* Draw */
GL_FUNC(void,   glDrawArrays, (GLenum, GLint, GLsizei))
GL_FUNC(void,   glDrawElements, (GLenum, GLsizei, GLenum, const void*))
/* Uniforms */
GL_FUNC(GLint,  glGetUniformLocation, (GLuint, const GLchar*))
GL_FUNC(void,   glUniform1i, (GLint, GLint))
GL_FUNC(void,   glUniform1f, (GLint, GLfloat))
GL_FUNC(void,   glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat))
GL_FUNC(void,   glUniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat*))

/* Internal: try to load above pointers; return 1 on success (partial OK). */
static int cube_load_gl_procs(void) {
    int ok = 1;
    GL_LOAD(glCreateShader);
    GL_LOAD(glShaderSource);
    GL_LOAD(glCompileShader);
    GL_LOAD(glGetShaderiv);
    GL_LOAD(glGetShaderInfoLog);
    GL_LOAD(glCreateProgram);
    GL_LOAD(glAttachShader);
    GL_LOAD(glLinkProgram);
    GL_LOAD(glGetProgramiv);
    GL_LOAD(glGetProgramInfoLog);
    GL_LOAD(glDeleteShader);
    GL_LOAD(glDeleteProgram);
    GL_LOAD(glUseProgram);
    GL_LOAD(glGenBuffers);
    GL_LOAD(glBindBuffer);
    GL_LOAD(glBufferData);
    GL_LOAD(glEnableVertexAttribArray);
    GL_LOAD(glVertexAttribPointer);
    GL_LOAD(glDisableVertexAttribArray);

    /* VAO may be NULL on GL 2.1 (OK). */
    GL_LOAD(glGenVertexArrays);
    GL_LOAD(glBindVertexArray);
    GL_LOAD(glDeleteVertexArrays);

    GL_LOAD(glActiveTexture);
    GL_LOAD(glGenTextures);
    GL_LOAD(glBindTexture);
    GL_LOAD(glTexImage2D);
    GL_LOAD(glTexParameteri);
    GL_LOAD(glGenerateMipmap);
    GL_LOAD(glDeleteTextures);

    GL_LOAD(glDrawArrays);
    GL_LOAD(glDrawElements);
    GL_LOAD(glGetUniformLocation);
    GL_LOAD(glUniform1i);
    GL_LOAD(glUniform1f);
    GL_LOAD(glUniform4f);
    GL_LOAD(glUniformMatrix4fv);

    /* A few critical ones must exist (shaders/program/buffers) */
    ok = ok && glCreateShader && glCreateProgram && glGenBuffers && glVertexAttribPointer;
    return ok ? 1 : 0;
}

/* --- Internal state --- */
struct fossil_cube_t {
    int width, height;
    int should_close;
    int vsync;

#if defined(_WIN32) || defined(_WIN64)
    HINSTANCE   hinst;
    HWND        hwnd;
    HDC         hdc;
    HGLRC       hglrc;
    LARGE_INTEGER perf_freq;
    LARGE_INTEGER start_qpc;
#elif defined(__APPLE__)
    void*       ns_window;
    void*       ns_view;
    void*       ns_glctx; /* NSOpenGLContext* */
    double      start_time;
#else
    Display*    dpy;
    int         screen;
    Window      win;
    GLXContext  ctx;
    Atom        wm_delete;
    struct timespec start_ts;
#endif
};

/* --- Timing --- */
static double cube_now_seconds_win(LARGE_INTEGER freq) {
#if defined(_WIN32) || defined(_WIN64)
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return (double)(q.QuadPart) / (double)freq.QuadPart;
#else
    (void)freq; return 0.0;
#endif
}

static double cube_now_seconds_posix(void) {
#if defined(__APPLE__)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1.0e-6;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1.0e-9;
#endif
}

/* --- Platform: Windows (WGL) --- */
#if defined(_WIN32) || defined(_WIN64)

static LRESULT CALLBACK cube_wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    fossil_cube_t* cube = (fossil_cube_t*)GetWindowLongPtr(h, GWLP_USERDATA);
    switch (m) {
    case WM_CLOSE:
        if (cube) cube->should_close = 1;
        return 0;
    case WM_SIZE:
        if (cube) {
            RECT rc; GetClientRect(h, &rc);
            cube->width = (int)(rc.right - rc.left);
            cube->height = (int)(rc.bottom - rc.top);
        }
        break;
    }
    return DefWindowProc(h, m, w, l);
}

static int cube_wgl_init_extensions(HDC hdc) {
    wglGetExtensionsStringARB_ = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");
    if (wglGetExtensionsStringARB_) {
        const char* ext = wglGetExtensionsStringARB_(hdc);
        (void)ext;
    }
    wglSwapIntervalEXT_    = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
    wglGetSwapIntervalEXT_ = (PFNWGLGETSWAPINTERVALEXTPROC)wglGetProcAddress("wglGetSwapIntervalEXT");
    return 1;
}

static int cube_win_create(const fossil_cube_config* cfg, fossil_cube_t** out) {
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = cube_wndproc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "FossilCube_wnd";
    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return FOSSIL_CUBE_ERR_PLATFORM;
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT wr = {0,0,cfg->width,cfg->height};
    AdjustWindowRect(&wr, style, FALSE);

    HWND hwnd = CreateWindowA(
        wc.lpszClassName,
        cfg->title ? cfg->title : "Fossil CUBE",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) return FOSSIL_CUBE_ERR_PLATFORM;

    HDC hdc = GetDC(hwnd);

    /* Basic pixel format (32-bit color, 24/8 depth/stencil, double-buffer). */
    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(hdc, &pfd);
    if (!pf || !SetPixelFormat(hdc, pf, &pfd)) {
        DestroyWindow(hwnd);
        return FOSSIL_CUBE_ERR_PLATFORM;
    }

    HGLRC hglrc = wglCreateContext(hdc);
    if (!hglrc || !wglMakeCurrent(hdc, hglrc)) {
        if (hglrc) wglDeleteContext(hglrc);
        DestroyWindow(hwnd);
        return FOSSIL_CUBE_ERR_GL;
    }

    /* Load WGL + GL procs */
    cube_wgl_init_extensions(hdc);
    cube_load_gl_procs();

    /* Show the window */
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    /* Instance */
    fossil_cube_t* cube = (fossil_cube_t*)calloc(1, sizeof(*cube));
    if (!cube) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hglrc);
        DestroyWindow(hwnd);
        return FOSSIL_CUBE_ERR_ALLOC;
    }

    cube->hinst = wc.hInstance;
    cube->hwnd = hwnd;
    cube->hdc = hdc;
    cube->hglrc = hglrc;

    RECT rc; GetClientRect(hwnd, &rc);
    cube->width = (int)(rc.right - rc.left);
    cube->height = (int)(rc.bottom - rc.top);

    QueryPerformanceFrequency(&cube->perf_freq);
    QueryPerformanceCounter(&cube->start_qpc);

    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cube);

    /* V-sync */
    cube->vsync = cfg->vsync ? 1 : 0;
    if (wglSwapIntervalEXT_) wglSwapIntervalEXT_(cube->vsync);

    *out = cube;
    return FOSSIL_CUBE_OK;
}

#endif /* Windows */

/* --- Platform: Linux (X11 + GLX) --- */
#if !defined(_WIN32) && !defined(_WIN64) && !defined(__APPLE__)

static int cube_x11_create(const fossil_cube_config* cfg, fossil_cube_t** out) {
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) return FOSSIL_CUBE_ERR_PLATFORM;

    int screen = DefaultScreen(dpy);

    static int attr[] = {
        GLX_RGBA, GLX_DOUBLEBUFFER,
        GLX_RED_SIZE,   8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE,  8,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        None
    };

    XVisualInfo* vi = glXChooseVisual(dpy, screen, attr);
    if (!vi) { XCloseDisplay(dpy); return FOSSIL_CUBE_ERR_PLATFORM; }

    Colormap cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);
    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                     ButtonPressMask | ButtonReleaseMask | StructureNotifyMask;

    Window win = XCreateWindow(dpy, RootWindow(dpy, vi->screen),
                               0, 0, (unsigned)cfg->width, (unsigned)cfg->height, 0,
                               vi->depth, InputOutput, vi->visual,
                               CWColormap | CWEventMask, &swa);

    XStoreName(dpy, win, cfg->title ? cfg->title : "Fossil CUBE");

    /* WM_DELETE_WINDOW protocol */
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);

    XMapWindow(dpy, win);

    /* GLX context */
    GLXContext ctx = glXCreateContext(dpy, vi, NULL, True);
    if (!ctx) {
        XDestroyWindow(dpy, win);
        XFreeColormap(dpy, cmap);
        XCloseDisplay(dpy);
        return FOSSIL_CUBE_ERR_GL;
    }
    glXMakeCurrent(dpy, win, ctx);

    /* Load glXGetProcAddress for GL function loading */
    glXGetProcAddressARB_ = (PFNGLXGETPROCADDRESSARBPROC)glXGetProcAddressARB((const GLubyte*)"glXGetProcAddressARB");

    /* Load GL procs */
    cube_load_gl_procs();

    /* Instance */
    fossil_cube_t* cube = (fossil_cube_t*)calloc(1, sizeof(*cube));
    if (!cube) {
        glXDestroyContext(dpy, ctx);
        XDestroyWindow(dpy, win);
        XFreeColormap(dpy, cmap);
        XCloseDisplay(dpy);
        return FOSSIL_CUBE_ERR_ALLOC;
    }

    cube->dpy = dpy;
    cube->screen = screen;
    cube->win = win;
    cube->ctx = ctx;
    cube->wm_delete = wm_delete;

    /* Initial size */
    cube->width = cfg->width;
    cube->height = cfg->height;

    clock_gettime(CLOCK_MONOTONIC, &cube->start_ts);

    /* V-sync hint (GLX_EXT_swap_control etc. not guaranteed; ignore if unavailable) */
    (void)cfg; /* we keep cfg->vsync best-effort; left as future improvement */

    *out = cube;
    return FOSSIL_CUBE_OK;
}

#endif /* Linux */

/* --- Platform: macOS (attach-only in this pure-C build) --- */

#if defined(__APPLE__)
int fossil_cube_attach_existing_context(void* ns_window,
                                        void* ns_view,
                                        void* ns_glctx,
                                        fossil_cube_t** out)
{
    if (!ns_glctx || !out) return FOSSIL_CUBE_ERR_PARAM;

    fossil_cube_t* cube = (fossil_cube_t*)calloc(1, sizeof(*cube));
    if (!cube) return FOSSIL_CUBE_ERR_ALLOC;

    cube->ns_window = ns_window;
    cube->ns_view = ns_view;
    cube->ns_glctx = ns_glctx;
    cube->start_time = cube_now_seconds_posix();

    /* GL procs are provided by system lib; just try to fetch pointers. */
    cube_load_gl_procs();

    /* We cannot know size without querying Cocoa; default to 800x600. */
    cube->width = 800;
    cube->height = 600;

    *out = cube;
    return FOSSIL_CUBE_OK;
}
#else
int fossil_cube_attach_existing_context(void* a, void* b, void* c, fossil_cube_t** out) {
    (void)a; (void)b; (void)c; (void)out;
    return FOSSIL_CUBE_ERR_PLATFORM;
}
#endif

/* --- Public API: create/destroy --- */
int fossil_cube_create(const fossil_cube_config* cfg, fossil_cube_t** out) {
    if (!cfg || !out || cfg->width <= 0 || cfg->height <= 0) return FOSSIL_CUBE_ERR_PARAM;

#if defined(_WIN32) || defined(_WIN64)
    return cube_win_create(cfg, out);
#elif defined(__APPLE__)
    (void)cfg; (void)out;
    /* On macOS, pure-C window creation would require Obj-C runtime. Use attach API. */
    return FOSSIL_CUBE_ERR_PLATFORM;
#else
    return cube_x11_create(cfg, out);
#endif
}

void fossil_cube_destroy(fossil_cube_t* cube) {
    if (!cube) return;

#if defined(_WIN32) || defined(_WIN64)
    wglMakeCurrent(NULL, NULL);
    if (cube->hglrc) wglDeleteContext(cube->hglrc);
    if (cube->hwnd && cube->hdc) ReleaseDC(cube->hwnd, cube->hdc);
    if (cube->hwnd) DestroyWindow(cube->hwnd);
#elif defined(__APPLE__)
    /* Context and window are owned by the host app when attached; do nothing. */
    (void)cube;
#else
    if (cube->dpy) {
        glXMakeCurrent(cube->dpy, None, NULL);
        if (cube->ctx) glXDestroyContext(cube->dpy, cube->ctx);
        if (cube->win) XDestroyWindow(cube->dpy, cube->win);
        XCloseDisplay(cube->dpy);
    }
#endif
    free(cube);
}

/* --- Events / swap / size --- */
void fossil_cube_poll_events(fossil_cube_t* cube) {
    if (!cube) return;

#if defined(_WIN32) || defined(_WIN64)
    MSG msg;
    while (PeekMessageA(&msg, cube->hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
#elif defined(__APPLE__)
    /* Host app should pump NSApp events; nothing to do in pure C here. */
    (void)cube;
#else
    while (XPending(cube->dpy)) {
        XEvent e;
        XNextEvent(cube->dpy, &e);
        switch (e.type) {
            case ClientMessage:
                if ((Atom)e.xclient.data.l[0] == cube->wm_delete)
                    cube->should_close = 1;
                break;
            case ConfigureNotify:
                cube->width = e.xconfigure.width;
                cube->height = e.xconfigure.height;
                break;
            default: break;
        }
    }
#endif
}

void fossil_cube_swap_buffers(fossil_cube_t* cube) {
    if (!cube) return;
#if defined(_WIN32) || defined(_WIN64)
    SwapBuffers(cube->hdc);
#elif defined(__APPLE__)
    /* Host should call -[NSOpenGLContext flushBuffer] */
    (void)cube;
#else
    glXSwapBuffers(cube->dpy, cube->win);
#endif
}

int fossil_cube_should_close(const fossil_cube_t* cube) {
    return cube ? cube->should_close : 1;
}

void fossil_cube_set_should_close(fossil_cube_t* cube, int should_close) {
    if (cube) cube->should_close = should_close ? 1 : 0;
}

void fossil_cube_get_size(const fossil_cube_t* cube, int* w, int* h) {
    if (!cube) return;
    if (w) *w = cube->width;
    if (h) *h = cube->height;
}

double fossil_cube_get_time(void) {
#if defined(_WIN32) || defined(_WIN64)
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return cube_now_seconds_win(freq);
#elif defined(__APPLE__)
    return cube_now_seconds_posix();
#else
    return cube_now_seconds_posix();
#endif
}

void* fossil_cube_get_proc(fossil_cube_t* cube, const char* name) {
    (void)cube;
    if (!name) return NULL;
#if defined(_WIN32) || defined(_WIN64)
    void* p = (void*)wglGetProcAddress(name);
    if (!p) p = (void*)GetProcAddress(GetModuleHandleA("opengl32.dll"), name);
    return p;
#elif defined(__APPLE__)
    return dlsym(RTLD_DEFAULT, name);
#else
    if (!glXGetProcAddressARB_) glXGetProcAddressARB_ =
        (PFNGLXGETPROCADDRESSARBPROC)glXGetProcAddressARB((const GLubyte*)"glXGetProcAddressARB");
    return glXGetProcAddressARB_ ? glXGetProcAddressARB_((const GLubyte*)name) : NULL;
#endif
}

/* --- Modern-GL helper implementations --- */
GLuint fossil_cube_compile_shader(GLenum type, const char* src, char* log_out, size_t* log_len) {
    if (!src) return 0;
    if (!glCreateShader || !glShaderSource || !glCompileShader) return 0;
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    if (log_out && log_len && *log_len) {
        GLsizei got = 0;
        glGetShaderInfoLog(sh, (GLsizei)(*log_len), &got, log_out);
        /* ensure NUL termination */
        if ((size_t)got >= *log_len) log_out[*log_len - 1] = '\0';
    }
    if (!ok) { glDeleteShader(sh); return 0; }
    return sh;
}

GLuint fossil_cube_link_program(GLuint vs, GLuint fs, char* log_out, size_t* log_len) {
    if (!glCreateProgram || !glAttachShader || !glLinkProgram) return 0;
    GLuint prog = glCreateProgram();
    if (vs) glAttachShader(prog, vs);
    if (fs) glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (log_out && log_len && *log_len) {
        GLsizei got = 0;
        glGetProgramInfoLog(prog, (GLsizei)(*log_len), &got, log_out);
        if ((size_t)got >= *log_len) log_out[*log_len - 1] = '\0';
    }
    if (!ok) { glDeleteProgram(prog); return 0; }
    return prog;
}

int fossil_cube_create_vao_vbo(GLuint* vao_out, GLuint* vbo_out, const void* data, size_t bytes) {
    if (!glGenBuffers || !glBindBuffer || !glBufferData) return 0;

    GLuint vbo = 0, vao = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (ptrdiff_t)bytes, data, GL_STATIC_DRAW);

    if (glGenVertexArrays && glBindVertexArray) {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
    }

    if (vbo_out) *vbo_out = vbo;
    if (vao_out) *vao_out = vao; /* may be 0 if VAO unsupported */
    return 1;
}

void fossil_cube_destroy_vao_vbo(GLuint vao, GLuint vbo) {
    if (glBindVertexArray && vao) {
        glBindVertexArray(0);
        glDeleteVertexArrays(1, &vao);
    }
    if (vbo) {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDeleteBuffers(1, &vbo);
    }
}

void fossil_cube_set_vsync(fossil_cube_t* cube, int vsync) {
    if (!cube) return;
    cube->vsync = vsync ? 1 : 0;
#if defined(_WIN32) || defined(_WIN64)
    if (wglSwapIntervalEXT_) wglSwapIntervalEXT_(cube->vsync);
#else
    (void)vsync; /* Best-effort on other platforms; extension-specific. */
#endif
}

void fossil_cube_set_viewport(int x, int y, int w, int h) {
    glViewport(x, y, w, h);
}

void fossil_cube_set_clear_color(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
}

void fossil_cube_clear(GLbitfield mask) {
    glClear(mask);
}

void fossil_cube_get_native(const fossil_cube_t* cube, fossil_cube_native* out) {
    if (!cube || !out) return;
    memset(out, 0, sizeof(*out));
#if defined(_WIN32) || defined(_WIN64)
    out->hInstance = (void*)cube->hinst;
    out->hwnd = (void*)cube->hwnd;
    out->hdc = (void*)cube->hdc;
    out->hglrc = (void*)cube->hglrc;
#elif defined(__APPLE__)
    out->ns_window = cube->ns_window;
    out->ns_view   = cube->ns_view;
    out->ns_glctx  = cube->ns_glctx;
#else
    out->display = (void*)cube->dpy;
    out->window  = (uintptr_t)cube->win;
    out->glx_ctx = (void*)cube->ctx;
#endif
}
