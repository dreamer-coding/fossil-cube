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

/* ---------- Version & Options ---------- */
#define FOSSIL_CUBE_VERSION_MAJOR 1
#define FOSSIL_CUBE_VERSION_MINOR 0
#define FOSSIL_CUBE_VERSION_PATCH 0

typedef void* (*fossil_cube_proc_loader)(const char* name);

/* Optional: pass a hint for core/compat desired profile (not enforced). */
typedef enum fossil_cube_profile {
    FOSSIL_CUBE_PROFILE_ANY = 0,
    FOSSIL_CUBE_PROFILE_CORE = 1,
    FOSSIL_CUBE_PROFILE_COMPAT = 2
} fossil_cube_profile;

/* Initialization options */
typedef struct fossil_cube_config {
    fossil_cube_proc_loader custom_loader; /* Optional. If NULL, built-in loader is used. */
    fossil_cube_profile     profile_hint;  /* Optional hint; informational. */
    int                     require_gl_version_major; /* 0 = don't check */
    int                     require_gl_version_minor; /* 0 = don't check */
} fossil_cube_config;

/* Capability snapshot (queried at init) */
typedef struct fossil_cube_caps {
    int major, minor;             /* GL_VERSION parsed */
    GLint max_vertex_attribs;
    GLint max_uniform_block_size;
    GLint uniform_buffer_offset_alignment;
    GLint max_combined_texture_units;
    GLint max_texture_size;
    GLint max_renderbuffer_size;
    GLint max_color_attachments;
    GLboolean has_vertex_array_obj;
    GLboolean has_instancing;
} fossil_cube_caps;

/* Error codes */
typedef enum fossil_cube_result {
    FOSSIL_CUBE_OK = 0,
    FOSSIL_CUBE_ERR_NO_CONTEXT = -1,
    FOSSIL_CUBE_ERR_LOAD_FUNC   = -2,
    FOSSIL_CUBE_ERR_VERSION     = -3,
    FOSSIL_CUBE_ERR_GL_ERROR    = -4,
    FOSSIL_CUBE_ERR_BAD_ARG     = -5
} fossil_cube_result;

/* ---------- Lifecycle ---------- */

/* Initialize: load modern GL entry points and cache capabilities. */
fossil_cube_result fossil_cube_init(const fossil_cube_config* cfg);

/* Shutdown: release internal state (does NOT destroy your GL context). */
void fossil_cube_shutdown(void);

/* Get last error string from fossil CUBE (sticky per-thread/simple). */
const char* fossil_cube_last_error(void);

/* Capabilities snapshot */
const fossil_cube_caps* fossil_cube_get_caps(void);

/* ---------- Utility ---------- */

/* Clear color/depth/stencil with masks (depth<0 to skip, stencil<0 to skip). */
void fossil_cube_clear(float r, float g, float b, float a, float depth, int stencil);

/* Check glGetError() and store human message; returns 1 if error, 0 otherwise. */
int fossil_cube_check_gl_error(const char* where);

/* ---------- Shaders / Programs ---------- */

typedef struct fossil_cube_shader {
    GLuint id;     /* gl shader object */
    GLenum type;   /* GL_VERTEX_SHADER / GL_FRAGMENT_SHADER / ... */
} fossil_cube_shader;

typedef struct fossil_cube_program {
    GLuint id;     /* linked program */
} fossil_cube_program;

/* Create & compile a shader from source. info_log may be NULL. */
fossil_cube_result fossil_cube_shader_create(GLenum type,
                                             const char* src,
                                             fossil_cube_shader* out_shader,
                                             char* info_log,
                                             int info_log_size);

/* Destroy shader (safe with 0). */
void fossil_cube_shader_destroy(fossil_cube_shader* sh);

/* Link program from array of shaders. */
fossil_cube_result fossil_cube_program_link(const fossil_cube_shader* shaders,
                                            int count,
                                            fossil_cube_program* out_prog,
                                            char* info_log,
                                            int info_log_size);

/* Destroy program (safe with 0). */
void fossil_cube_program_destroy(fossil_cube_program* prg);

/* Bind program for use (pass 0 to unbind). */
void fossil_cube_program_use(fossil_cube_program prg);

/* Get uniform location (cached by user if desired). */
GLint fossil_cube_program_uniform(fossil_cube_program prg, const char* name);

/* ---------- Buffers / VAOs ---------- */

typedef struct fossil_cube_vbo { GLuint id; } fossil_cube_vbo;
typedef struct fossil_cube_ebo { GLuint id; } fossil_cube_ebo;
typedef struct fossil_cube_vao { GLuint id; } fossil_cube_vao;

/* Create buffer objects */
fossil_cube_result fossil_cube_vbo_create(GLsizeiptr size, const void* data, GLenum usage, fossil_cube_vbo* out);
fossil_cube_result fossil_cube_ebo_create(GLsizeiptr size, const void* data, GLenum usage, fossil_cube_ebo* out);

/* Update buffer subrange */
fossil_cube_result fossil_cube_buffer_subdata(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);

/* Destroy buffers */
void fossil_cube_vbo_destroy(fossil_cube_vbo* v);
void fossil_cube_ebo_destroy(fossil_cube_ebo* e);

/* Vertex Array Object */
fossil_cube_result fossil_cube_vao_create(fossil_cube_vao* out);
void fossil_cube_vao_destroy(fossil_cube_vao* vao);

/* Attribute setup (float) */
void fossil_cube_vertex_attrib(GLuint index, GLint size, GLenum type, GLboolean normalized,
                               GLsizei stride, const void* offset);

/* Attribute setup (integer) */
void fossil_cube_vertex_attrib_i(GLuint index, GLint size, GLenum type,
                                 GLsizei stride, const void* offset);

/* Enable/disable attrib */
void fossil_cube_enable_attrib(GLuint index);
void fossil_cube_disable_attrib(GLuint index);

/* Instancing */
void fossil_cube_vertex_divisor(GLuint index, GLuint divisor);

/* ---------- Textures ---------- */

typedef struct fossil_cube_tex2d { GLuint id; } fossil_cube_tex2d;

/* Create RGBA8 texture (data may be NULL), level=0 only. */
fossil_cube_result fossil_cube_tex2d_create(int width, int height, const void* rgba8_pixels, fossil_cube_tex2d* out);

/* Set params */
void fossil_cube_tex2d_params(GLenum min_filter, GLenum mag_filter, GLenum wrap_s, GLenum wrap_t);

/* Update sub-image */
void fossil_cube_tex2d_subimage(int x, int y, int w, int h, const void* rgba8_pixels);

/* Generate mipmaps */
void fossil_cube_tex2d_gen_mips(void);

/* Destroy */
void fossil_cube_tex2d_destroy(fossil_cube_tex2d* t);

/* ---------- Framebuffers ---------- */

typedef struct fossil_cube_fbo { GLuint id; } fossil_cube_fbo;

/* Create & attach color texture (2D) to COLOR_ATTACHMENT0.
   If depth_renderbuffer_bits>0, create a depth renderbuffer and attach. */
fossil_cube_result fossil_cube_fbo_create_color_tex(fossil_cube_tex2d color_tex,
                                                    int depth_renderbuffer_bits,
                                                    fossil_cube_fbo* out);

/* Bind FBO for drawing (0 to bind default). */
void fossil_cube_fbo_bind(fossil_cube_fbo fbo);

/* Destroy */
void fossil_cube_fbo_destroy(fossil_cube_fbo* f);

/* ---------- Drawing ---------- */

/* Bind helpers (explicit binding keeps flow modern & clear) */
void fossil_cube_bind_vao(fossil_cube_vao vao);
void fossil_cube_bind_vbo(GLenum target, GLuint id); /* GL_ARRAY_BUFFER / GL_ELEMENT_ARRAY_BUFFER */

/* Issue draws */
void fossil_cube_draw_arrays(GLenum mode, GLint first, GLsizei count);
void fossil_cube_draw_elements(GLenum mode, GLsizei count, GLenum type, const void* indices);

/* ---------- Uniform helpers ---------- */
void fossil_cube_uniform_mat4(GLint loc, const float* m16_row_major);
void fossil_cube_uniform_vec4(GLint loc, float x, float y, float z, float w);
void fossil_cube_uniform_vec3(GLint loc, float x, float y, float z);
void fossil_cube_uniform_vec2(GLint loc, float x, float y);
void fossil_cube_uniform_float(GLint loc, float v);
void fossil_cube_uniform_int(GLint loc, int v);

/* Active texture & bind 2D */
void fossil_cube_active_texture_unit(GLenum unit);         /* e.g., GL_TEXTURE0 */
void fossil_cube_bind_tex2d(fossil_cube_tex2d tex);

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
