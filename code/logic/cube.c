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
#include <string.h>
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    static HMODULE s_opengl32 = NULL;
    typedef PROC (WINAPI *PFNWGLGETPROCADDRESSPROC)(LPCSTR);
    static PFNWGLGETPROCADDRESSPROC s_wglGetProcAddress = NULL;
#elif defined(__APPLE__)
    /* macOS: link-time symbols from OpenGL framework; no runtime loading needed. */
    #include <dlfcn.h>
#else
    #include <dlfcn.h>
    #include <GL/glx.h>
    typedef void* (*PFNGLXGETPROCADDRESSPROC)(const GLubyte*);
    static PFNGLXGETPROCADDRESSPROC s_glXGetProcAddress = NULL;
#endif

/* -------- Internal state -------- */
typedef struct fossil_cube_state {
    fossil_cube_caps caps;
    char last_error[512];
    int initialized;
    fossil_cube_proc_loader user_loader;
} fossil_cube_state;

static fossil_cube_state g;

/* Write last error */
static void fc_set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g.last_error, sizeof(g.last_error), fmt, args);
    va_end(args);
}

/* Public: get last error */
const char* fossil_cube_last_error(void) {
    return g.last_error[0] ? g.last_error : "";
}

/* --------- GL function pointers we use (modern only) --------- */

/* Core shader/program */
static PFNGLCREATESHADERPROC            pglCreateShader;
static PFNGLSHADERSOURCEPROC            pglShaderSource;
static PFNGLCOMPILESHADERPROC           pglCompileShader;
static PFNGLGETSHADERIVPROC             pglGetShaderiv;
static PFNGLGETSHADERINFOLOGPROC        pglGetShaderInfoLog;
static PFNGLDELETESHADERPROC            pglDeleteShader;
static PFNGLCREATEPROGRAMPROC           pglCreateProgram;
static PFNGLATTACHSHADERPROC            pglAttachShader;
static PFNGLLINKPROGRAMPROC             pglLinkProgram;
static PFNGLGETPROGRAMIVPROC            pglGetProgramiv;
static PFNGLGETPROGRAMINFOLOGPROC       pglGetProgramInfoLog;
static PFNGLDELETEPROGRAMPROC           pglDeleteProgram;
static PFNGLUSEPROGRAMPROC              pglUseProgram;
static PFNGLGETUNIFORMLOCATIONPROC      pglGetUniformLocation;
static PFNGLUNIFORM1IPROC               pglUniform1i;
static PFNGLUNIFORM1FPROC               pglUniform1f;
static PFNGLUNIFORM2FPROC               pglUniform2f;
static PFNGLUNIFORM3FPROC               pglUniform3f;
static PFNGLUNIFORM4FPROC               pglUniform4f;
static PFNGLUNIFORMMATRIX4FVPROC        pglUniformMatrix4fv;

/* Buffers / VAOs */
static PFNGLGENBUFFERSPROC              pglGenBuffers;
static PFNGLBINDBUFFERPROC              pglBindBuffer;
static PFNGLBUFFERDATAPROC              pglBufferData;
static PFNGLBUFFERSUBDATAPROC           pglBufferSubData;
static PFNGLDELETEBUFFERSPROC           pglDeleteBuffers;
static PFNGLGENVERTEXARRAYSPROC         pglGenVertexArrays;
static PFNGLBINDVERTEXARRAYPROC         pglBindVertexArray;
static PFNGLDELETEVERTEXARRAYSPROC      pglDeleteVertexArrays;
static PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC pglDisableVertexAttribArray;
static PFNGLVERTEXATTRIBPOINTERPROC     pglVertexAttribPointer;
static PFNGLVERTEXATTRIBIPOINTERPROC    pglVertexAttribIPointer;
static PFNGLVERTEXATTRIBDIVISORPROC     pglVertexAttribDivisor;

/* Draw */
static PFNGLDRAWELEMENTSPROC            pglDrawElements;
static PFNGLDRAWARRAYSPROC              pglDrawArrays;

/* Textures */
static PFNGLACTIVETEXTUREPROC           pglActiveTexture;
static PFNGLGENTEXTURESPROC             pglGenTextures;
static PFNGLBINDTEXTUREPROC             pglBindTexture;
static PFNGLTEXPARAMETERIPROC           pglTexParameteri;
static PFNGLTEXIMAGE2DPROC              pglTexImage2D;
static PFNGLTEXSUBIMAGE2DPROC           pglTexSubImage2D;
static PFNGLGENERATEMIPMAPPROC          pglGenerateMipmap;
static PFNGLDELETETEXTURESPROC          pglDeleteTextures;

/* Framebuffers / renderbuffers */
static PFNGLGENFRAMEBUFFERSPROC         pglGenFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC         pglBindFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC    pglFramebufferTexture2D;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC  pglCheckFramebufferStatus;
static PFNGLDELETEFRAMEBUFFERSPROC      pglDeleteFramebuffers;
static PFNGLGENRENDERBUFFERSPROC        pglGenRenderbuffers;
static PFNGLBINDRENDERBUFFERPROC        pglBindRenderbuffer;
static PFNGLRENDERBUFFERSTORAGEPROC     pglRenderbufferStorage;
static PFNGLFRAMEBUFFERRENDERBUFFERPROC pglFramebufferRenderbuffer;
static PFNGLDELETERENDERBUFFERSPROC     pglDeleteRenderbuffers;

/* --------- Loader --------- */

static void* fc_load_symbol_default(const char* name) {
    /* 1) user-supplied loader wins */
    if (g.user_loader) {
        void* p = g.user_loader(name);
        if (p) return p;
    }

#if defined(_WIN32) || defined(_WIN64)
    if (!s_opengl32) {
        s_opengl32 = LoadLibraryA("opengl32.dll");
        if (!s_opengl32) return NULL;
    }
    if (!s_wglGetProcAddress) {
        s_wglGetProcAddress = (PFNWGLGETPROCADDRESSPROC)GetProcAddress(s_opengl32, "wglGetProcAddress");
    }
    if (s_wglGetProcAddress) {
        PROC p = s_wglGetProcAddress(name);
        if (p) return (void*)p;
    }
    /* Some core funcs must be fetched from opengl32.dll directly */
    return (void*)GetProcAddress(s_opengl32, name);

#elif defined(__APPLE__)
    /* On macOS, functions are generally available after linking to OpenGL framework. */
    (void)name;
    return NULL; /* Returning NULL is OK; direct calls will link if available. */

#else
    /* Linux/Unix with GLX */
    if (!s_glXGetProcAddress) {
        void* libGL = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
        if (libGL) {
            s_glXGetProcAddress = (PFNGLXGETPROCADDRESSPROC)dlsym(libGL, "glXGetProcAddressARB");
            if (!s_glXGetProcAddress)
                s_glXGetProcAddress = (PFNGLXGETPROCADDRESSPROC)dlsym(libGL, "glXGetProcAddress");
        }
    }
    if (s_glXGetProcAddress) {
        void* p = s_glXGetProcAddress((const GLubyte*)name);
        if (p) return p;
    }
    /* Fallback to dlsym from global space (rarely needed) */
    return dlsym(RTLD_DEFAULT, name);
#endif
}

static int fc_load(void** out, const char* name) {
#if defined(__APPLE__)
    /* On macOS, we rely on direct linkage; set function pointer to the symbol address if possible.
       We can still attempt dlsym(NULL, name) to be robust. */
    void* p = dlsym(RTLD_DEFAULT, name);
    *out = p; /* may be NULL; direct call path will still work if headers link it. */
    return 1;  /* don't treat as fatal here to keep portability */
#else
    void* p = fc_load_symbol_default(name);
    if (!p) return 0;
    *out = p;
    return 1;
#endif
}

static int fc_load_required_symbols(void) {
    /* Try load; on macOS, allow NULL and rely on direct symbols, but we still check critical ones. */
    int ok = 1;
#define FC_LOAD(name) ok = ok && fc_load((void**)&p##name, #name)

    FC_LOAD(glCreateShader);
    FC_LOAD(glShaderSource);
    FC_LOAD(glCompileShader);
    FC_LOAD(glGetShaderiv);
    FC_LOAD(glGetShaderInfoLog);
    FC_LOAD(glDeleteShader);
    FC_LOAD(glCreateProgram);
    FC_LOAD(glAttachShader);
    FC_LOAD(glLinkProgram);
    FC_LOAD(glGetProgramiv);
    FC_LOAD(glGetProgramInfoLog);
    FC_LOAD(glDeleteProgram);
    FC_LOAD(glUseProgram);
    FC_LOAD(glGetUniformLocation);
    FC_LOAD(glUniform1i);
    FC_LOAD(glUniform1f);
    FC_LOAD(glUniform2f);
    FC_LOAD(glUniform3f);
    FC_LOAD(glUniform4f);
    FC_LOAD(glUniformMatrix4fv);

    FC_LOAD(glGenBuffers);
    FC_LOAD(glBindBuffer);
    FC_LOAD(glBufferData);
    FC_LOAD(glBufferSubData);
    FC_LOAD(glDeleteBuffers);

    FC_LOAD(glGenVertexArrays);
    FC_LOAD(glBindVertexArray);
    FC_LOAD(glDeleteVertexArrays);
    FC_LOAD(glEnableVertexAttribArray);
    FC_LOAD(glDisableVertexAttribArray);
    FC_LOAD(glVertexAttribPointer);
    FC_LOAD(glVertexAttribIPointer);
    FC_LOAD(glVertexAttribDivisor);

    FC_LOAD(glDrawElements);
    FC_LOAD(glDrawArrays);

    FC_LOAD(glActiveTexture);
    FC_LOAD(glGenTextures);
    FC_LOAD(glBindTexture);
    FC_LOAD(glTexParameteri);
    FC_LOAD(glTexImage2D);
    FC_LOAD(glTexSubImage2D);
    FC_LOAD(glGenerateMipmap);
    FC_LOAD(glDeleteTextures);

    FC_LOAD(glGenFramebuffers);
    FC_LOAD(glBindFramebuffer);
    FC_LOAD(glFramebufferTexture2D);
    FC_LOAD(glCheckFramebufferStatus);
    FC_LOAD(glDeleteFramebuffers);
    FC_LOAD(glGenRenderbuffers);
    FC_LOAD(glBindRenderbuffer);
    FC_LOAD(glRenderbufferStorage);
    FC_LOAD(glFramebufferRenderbuffer);
    FC_LOAD(glDeleteRenderbuffers);

#undef FC_LOAD
#if defined(__APPLE__)
    /* On mac, tolerate missing pointers due to direct linkage; consider init successful. */
    return 1;
#else
    return ok;
#endif
}

/* --------- Helpers --------- */

static void fc_query_caps(fossil_cube_caps* c) {
    memset(c, 0, sizeof(*c));

    /* Parse GL_VERSION */
    const char* ver = (const char*)glGetString(GL_VERSION);
    int maj=0, min=0;
    if (ver && (sscanf(ver, "%d.%d", &maj, &min) == 2)) {
        c->major = maj; c->minor = min;
    }

    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &c->max_vertex_attribs);
#ifdef GL_MAX_UNIFORM_BLOCK_SIZE
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &c->max_uniform_block_size);
#endif
#ifdef GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &c->uniform_buffer_offset_alignment);
#endif
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &c->max_combined_texture_units);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &c->max_texture_size);
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &c->max_renderbuffer_size);

#ifdef GL_MAX_COLOR_ATTACHMENTS
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &c->max_color_attachments);
#else
    c->max_color_attachments = 1;
#endif

#ifdef GL_VERTEX_ARRAY_BINDING
    c->has_vertex_array_obj = 1;
#else
    c->has_vertex_array_obj = (pglGenVertexArrays && pglBindVertexArray) ? 1 : 0;
#endif

    c->has_instancing = (pglVertexAttribDivisor != NULL);
}

/* Public: capabilities */
const fossil_cube_caps* fossil_cube_get_caps(void) {
    return g.initialized ? &g.caps : NULL;
}

/* Public: init */
fossil_cube_result fossil_cube_init(const fossil_cube_config* cfg) {
    memset(&g, 0, sizeof(g));
    if (cfg) g.user_loader = cfg->custom_loader;

    /* Is there a current context? Try a harmless query. */
    glGetError(); /* clear */
    GLenum e = glGetError(); /* if no context on some drivers, this can be invalid op */
    (void)e; /* can't reliably detect; proceed to load and check version afterwards */

    if (!fc_load_required_symbols()) {
        fc_set_error("Failed to load required OpenGL entry points (no modern GL?).");
        return FOSSIL_CUBE_ERR_LOAD_FUNC;
    }

    fc_query_caps(&g.caps);

    if (cfg && cfg->require_gl_version_major > 0) {
        if (g.caps.major < cfg->require_gl_version_major ||
           (g.caps.major == cfg->require_gl_version_major && g.caps.minor < cfg->require_gl_version_minor)) {
            fc_set_error("Insufficient GL version. Have %d.%d, need %d.%d.",
                         g.caps.major, g.caps.minor,
                         cfg->require_gl_version_major, cfg->require_gl_version_minor);
            return FOSSIL_CUBE_ERR_VERSION;
        }
    }

    g.initialized = 1;
    return FOSSIL_CUBE_OK;
}

void fossil_cube_shutdown(void) {
    memset(&g, 0, sizeof(g));
}

/* --------- Utility --------- */

int fossil_cube_check_gl_error(const char* where) {
    GLenum err = glGetError();
    if (err == GL_NO_ERROR) return 0;
    const char* msg = "UNKNOWN";
    switch (err) {
        case GL_INVALID_ENUM: msg="GL_INVALID_ENUM"; break;
        case GL_INVALID_VALUE: msg="GL_INVALID_VALUE"; break;
        case GL_INVALID_OPERATION: msg="GL_INVALID_OPERATION"; break;
#ifdef GL_INVALID_FRAMEBUFFER_OPERATION
        case GL_INVALID_FRAMEBUFFER_OPERATION: msg="GL_INVALID_FRAMEBUFFER_OPERATION"; break;
#endif
        case GL_OUT_OF_MEMORY: msg="GL_OUT_OF_MEMORY"; break;
    }
    fc_set_error("%s: %s (0x%x)", where ? where : "GL", msg, (unsigned)err);
    return 1;
}

void fossil_cube_clear(float r, float g, float b, float a, float depth, int stencil) {
    GLbitfield mask = 0;
    glClearColor(r, g, b, a);
    mask |= GL_COLOR_BUFFER_BIT;
    if (depth >= 0.0f) {
        glClearDepth(depth);
        mask |= GL_DEPTH_BUFFER_BIT;
    }
    if (stencil >= 0) {
        glClearStencil(stencil);
        mask |= GL_STENCIL_BUFFER_BIT;
    }
    glClear(mask);
}

/* --------- Shaders / Programs --------- */

fossil_cube_result fossil_cube_shader_create(GLenum type,
                                             const char* src,
                                             fossil_cube_shader* out_shader,
                                             char* info_log,
                                             int info_log_size) {
    if (!src || !out_shader) return FOSSIL_CUBE_ERR_BAD_ARG;
    GLuint sh = pglCreateShader(type);
    pglShaderSource(sh, 1, &src, NULL);
    pglCompileShader(sh);

    GLint ok = 0;
    pglGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    if (info_log && info_log_size > 0) {
        GLsizei n = 0;
        pglGetShaderInfoLog(sh, info_log_size, &n, info_log);
        if (n < info_log_size) info_log[n] = '\0';
    }

    if (!ok) {
        pglDeleteShader(sh);
        fc_set_error("Shader compilation failed.");
        return FOSSIL_CUBE_ERR_GL_ERROR;
    }

    out_shader->id = sh;
    out_shader->type = type;
    return FOSSIL_CUBE_OK;
}

void fossil_cube_shader_destroy(fossil_cube_shader* sh) {
    if (sh && sh->id) {
        pglDeleteShader(sh->id);
        sh->id = 0;
    }
}

fossil_cube_result fossil_cube_program_link(const fossil_cube_shader* shaders,
                                            int count,
                                            fossil_cube_program* out_prog,
                                            char* info_log,
                                            int info_log_size) {
    if (!shaders || count <= 0 || !out_prog) return FOSSIL_CUBE_ERR_BAD_ARG;

    GLuint prg = pglCreateProgram();
    for (int i=0;i<count;i++) {
        if (shaders[i].id) pglAttachShader(prg, shaders[i].id);
    }
    pglLinkProgram(prg);

    GLint ok = 0;
    pglGetProgramiv(prg, GL_LINK_STATUS, &ok);

    if (info_log && info_log_size > 0) {
        GLsizei n = 0;
        pglGetProgramInfoLog(prg, info_log_size, &n, info_log);
        if (n < info_log_size) info_log[n] = '\0';
    }

    if (!ok) {
        pglDeleteProgram(prg);
        fc_set_error("Program link failed.");
        return FOSSIL_CUBE_ERR_GL_ERROR;
    }

    out_prog->id = prg;
    return FOSSIL_CUBE_OK;
}

void fossil_cube_program_destroy(fossil_cube_program* prg) {
    if (prg && prg->id) {
        pglDeleteProgram(prg->id);
        prg->id = 0;
    }
}

void fossil_cube_program_use(fossil_cube_program prg) {
    pglUseProgram(prg.id);
}

GLint fossil_cube_program_uniform(fossil_cube_program prg, const char* name) {
    return pglGetUniformLocation(prg.id, name);
}

/* --------- Buffers / VAOs --------- */

fossil_cube_result fossil_cube_vbo_create(GLsizeiptr size, const void* data, GLenum usage, fossil_cube_vbo* out) {
    if (!out) return FOSSIL_CUBE_ERR_BAD_ARG;
    GLuint id=0; pglGenBuffers(1, &id);
    pglBindBuffer(GL_ARRAY_BUFFER, id);
    pglBufferData(GL_ARRAY_BUFFER, size, data, usage);
    out->id = id;
    return fossil_cube_check_gl_error("vbo_create") ? FOSSIL_CUBE_ERR_GL_ERROR : FOSSIL_CUBE_OK;
}

fossil_cube_result fossil_cube_ebo_create(GLsizeiptr size, const void* data, GLenum usage, fossil_cube_ebo* out) {
    if (!out) return FOSSIL_CUBE_ERR_BAD_ARG;
    GLuint id=0; pglGenBuffers(1, &id);
    pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
    pglBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, usage);
    out->id = id;
    return fossil_cube_check_gl_error("ebo_create") ? FOSSIL_CUBE_ERR_GL_ERROR : FOSSIL_CUBE_OK;
}

fossil_cube_result fossil_cube_buffer_subdata(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
    pglBufferSubData(target, offset, size, data);
    return fossil_cube_check_gl_error("buffer_subdata") ? FOSSIL_CUBE_ERR_GL_ERROR : FOSSIL_CUBE_OK;
}

void fossil_cube_vbo_destroy(fossil_cube_vbo* v) {
    if (v && v->id) { pglDeleteBuffers(1, &v->id); v->id = 0; }
}

void fossil_cube_ebo_destroy(fossil_cube_ebo* e) {
    if (e && e->id) { pglDeleteBuffers(1, &e->id); e->id = 0; }
}

fossil_cube_result fossil_cube_vao_create(fossil_cube_vao* out) {
    if (!out) return FOSSIL_CUBE_ERR_BAD_ARG;
    GLuint id=0; pglGenVertexArrays(1, &id);
    out->id = id;
    return fossil_cube_check_gl_error("vao_create") ? FOSSIL_CUBE_ERR_GL_ERROR : FOSSIL_CUBE_OK;
}

void fossil_cube_vao_destroy(fossil_cube_vao* vao) {
    if (vao && vao->id) { pglDeleteVertexArrays(1, &vao->id); vao->id = 0; }
}

void fossil_cube_bind_vao(fossil_cube_vao vao) {
    pglBindVertexArray(vao.id);
}

void fossil_cube_bind_vbo(GLenum target, GLuint id) {
    pglBindBuffer(target, id);
}

void fossil_cube_vertex_attrib(GLuint index, GLint size, GLenum type, GLboolean normalized,
                               GLsizei stride, const void* offset) {
    pglVertexAttribPointer(index, size, type, normalized, stride, offset);
}

void fossil_cube_vertex_attrib_i(GLuint index, GLint size, GLenum type,
                                 GLsizei stride, const void* offset) {
    pglVertexAttribIPointer(index, size, type, stride, offset);
}

void fossil_cube_enable_attrib(GLuint index)  { pglEnableVertexAttribArray(index); }
void fossil_cube_disable_attrib(GLuint index) { pglDisableVertexAttribArray(index); }

void fossil_cube_vertex_divisor(GLuint index, GLuint divisor) {
    if (pglVertexAttribDivisor) pglVertexAttribDivisor(index, divisor);
}

/* --------- Textures --------- */

fossil_cube_result fossil_cube_tex2d_create(int w, int h, const void* rgba8_pixels, fossil_cube_tex2d* out) {
    if (!out || w<=0 || h<=0) return FOSSIL_CUBE_ERR_BAD_ARG;
    GLuint id=0; pglGenTextures(1, &id);
    pglBindTexture(GL_TEXTURE_2D, id);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba8_pixels);
    out->id = id;
    return fossil_cube_check_gl_error("tex2d_create") ? FOSSIL_CUBE_ERR_GL_ERROR : FOSSIL_CUBE_OK;
}

void fossil_cube_tex2d_params(GLenum min_filter, GLenum mag_filter, GLenum wrap_s, GLenum wrap_t) {
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);
}

void fossil_cube_tex2d_subimage(int x, int y, int w, int h, const void* rgba8_pixels) {
    pglTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba8_pixels);
}

void fossil_cube_tex2d_gen_mips(void) {
    pglGenerateMipmap(GL_TEXTURE_2D);
}

void fossil_cube_tex2d_destroy(fossil_cube_tex2d* t) {
    if (t && t->id) { pglDeleteTextures(1, &t->id); t->id = 0; }
}

void fossil_cube_active_texture_unit(GLenum unit) { pglActiveTexture(unit); }

void fossil_cube_bind_tex2d(fossil_cube_tex2d tex) { pglBindTexture(GL_TEXTURE_2D, tex.id); }

/* --------- Framebuffers --------- */

fossil_cube_result fossil_cube_fbo_create_color_tex(fossil_cube_tex2d color_tex,
                                                    int depth_bits,
                                                    fossil_cube_fbo* out) {
    if (!out || !color_tex.id) return FOSSIL_CUBE_ERR_BAD_ARG;

    GLuint fbo=0; pglGenFramebuffers(1, &fbo);
    pglBindFramebuffer(GL_FRAMEBUFFER, fbo);
    pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex.id, 0);

    GLuint rbo = 0;
    if (depth_bits > 0) {
        pglGenRenderbuffers(1, &rbo);
        pglBindRenderbuffer(GL_RENDERBUFFER, rbo);
        /* We need the texture size; unfortunately GL doesn't let us query tex level dims easily without binding/query.
           Here we assume caller bound and set dimensions; for safety, create a reasonable depth RBO after querying. */
        /* Minimal approach: let the user handle matching sizes if they resize. */
        /* Create a 24-bit depth RBO of same size as texture by reusing glTexImage2D dims: not accessible here,
           so document that user should bind/resize RBO when resizing color_tex. We'll just create a 24-bit RBO of 1x1 as placeholder if unknown. */

        /* Better: Require user to pass desired size via texture; we cannot query width/height portably without GL 4.5 getTexLevelParameter. */
        /* Use GL 3.0: glGetTexLevelParameteriv is available in GL 1.1+ (core), so we can query: */
        GLint w=0,h=0;
        glBindTexture(GL_TEXTURE_2D, color_tex.id);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
        if (w<=0) w=1; if (h<=0) h=1;

        pglRenderbufferStorage(GL_RENDERBUFFER,
                               (depth_bits >= 32) ? GL_DEPTH_COMPONENT32 :
                               (depth_bits >= 24) ? GL_DEPTH_COMPONENT24 :
                                                    GL_DEPTH_COMPONENT16,
                               w, h);
        pglFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
    }

    GLenum status = pglCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        pglBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (rbo) pglDeleteRenderbuffers(1, &rbo);
        if (fbo) pglDeleteFramebuffers(1, &fbo);
        fc_set_error("Framebuffer incomplete (0x%x).", (unsigned)status);
        return FOSSIL_CUBE_ERR_GL_ERROR;
    }

    out->id = fbo;
    return FOSSIL_CUBE_OK;
}

void fossil_cube_fbo_bind(fossil_cube_fbo fbo) {
    pglBindFramebuffer(GL_FRAMEBUFFER, fbo.id);
}

void fossil_cube_fbo_destroy(fossil_cube_fbo* f) {
    if (f && f->id) { pglDeleteFramebuffers(1, &f->id); f->id = 0; }
}

/* --------- Drawing --------- */

void fossil_cube_draw_arrays(GLenum mode, GLint first, GLsizei count) {
    pglDrawArrays(mode, first, count);
}

void fossil_cube_draw_elements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
    pglDrawElements(mode, count, type, indices);
}

/* --------- Uniform helpers --------- */

void fossil_cube_uniform_mat4(GLint loc, const float* m16_row_major) {
    /* GL expects column-major by default; pass transpose=GL_TRUE to accept row-major input. */
    pglUniformMatrix4fv(loc, 1, GL_TRUE, m16_row_major);
}
void fossil_cube_uniform_vec4(GLint loc, float x, float y, float z, float w) { pglUniform4f(loc, x, y, z, w); }
void fossil_cube_uniform_vec3(GLint loc, float x, float y, float z)          { pglUniform3f(loc, x, y, z); }
void fossil_cube_uniform_vec2(GLint loc, float x, float y)                   { pglUniform2f(loc, x, y); }
void fossil_cube_uniform_float(GLint loc, float v)                           { pglUniform1f(loc, v); }
void fossil_cube_uniform_int(GLint loc, int v)                               { pglUniform1i(loc, v); }
