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

// cube.c
#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>   // Must be included before gl.h on Windows
    #include <GL/gl.h>
#elif defined(__APPLE__)
    #include <OpenGL/gl.h>
#else
    #include <GL/gl.h>
#endif

struct fossil_cube_ctx {
    void *platform_user_data;
    int fb_width, fb_height;
    float clear_r, clear_g, clear_b, clear_a;

    /* Immediate-mode state */
    fc_color_t current_color;

    /* callbacks */
    fossil_cube_render_cb user_render;
    void *user_render_ptr;
    fossil_cube_event_cb user_event;
    void *user_event_ptr;

    /* GL resources */
    unsigned int shader_program;
    unsigned int vbo, vao;
    unsigned int ibo;

    int debug_draw;
};

/* forward */
static unsigned int fc_create_basic_shader(void);
static void fc_destroy_shader(unsigned int prog);

/* Create/destroy */
fossil_cube_ctx *fossil_cube_create(void *platform_user_data, int width, int height, int flags) {
    (void)flags;
    fossil_cube_ctx *c = (fossil_cube_ctx*)calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->platform_user_data = platform_user_data;
    c->fb_width = width;
    c->fb_height = height;
    c->clear_r = c->clear_g = c->clear_b = 0.08f;
    c->clear_a = 1.0f;
    c->current_color = fossil_cube_rgba8(0xEE,0xEE,0xEE,0xFF);
    c->shader_program = fc_create_basic_shader();
    c->vbo = c->vao = c->ibo = 0;

    /* Setup minimal GL buffers (dynamic usage) */
    glGenVertexArrays(1, &c->vao);
    glBindVertexArray(c->vao);
    glGenBuffers(1, &c->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, c->vbo);
    glBufferData(GL_ARRAY_BUFFER, 1024*64, NULL, GL_DYNAMIC_DRAW); /* resize as needed */
    glGenBuffers(1, &c->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, c->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 1024*64, NULL, GL_DYNAMIC_DRAW);

    /* basic vertex attrib layout: pos(2f), uv(2f), color(4ub normalized) */
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float)*8, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float)*8, (void*)(sizeof(float)*2));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(float)*8, (void*)(sizeof(float)*4));

    glBindVertexArray(0);

    return c;
}

void fossil_cube_destroy(fossil_cube_ctx *c) {
    if (!c) return;
    if (c->shader_program) fc_destroy_shader(c->shader_program);
    if (c->vbo) glDeleteBuffers(1, &c->vbo);
    if (c->ibo) glDeleteBuffers(1, &c->ibo);
    if (c->vao) glDeleteVertexArrays(1, &c->vao);
    free(c);
}

/* Callbacks */
void fossil_cube_set_render_callback(fossil_cube_ctx *c, fossil_cube_render_cb cb, void *user) {
    c->user_render = cb; c->user_render_ptr = user;
}
void fossil_cube_set_event_callback(fossil_cube_ctx *c, fossil_cube_event_cb ecb, void *user) {
    c->user_event = ecb; c->user_event_ptr = user;
}

/* Events */
void fossil_cube_push_event(fossil_cube_ctx *c, const fossil_cube_event *ev) {
    if (c->user_event) c->user_event(c, ev, c->user_event_ptr);
    /* Built-in events */
    if (ev->type == FC_EVENT_WINDOW_RESIZE) {
        c->fb_width = ev->d.resize.width;
        c->fb_height = ev->d.resize.height;
        glViewport(0,0,c->fb_width,c->fb_height);
    }
}

/* Frame lifecycle */
void fossil_cube_frame_begin(fossil_cube_ctx *c, double dt_seconds) {
    (void)dt_seconds;
    glViewport(0,0,c->fb_width,c->fb_height);
    glClearColor(c->clear_r, c->clear_g, c->clear_b, c->clear_a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Bind shared resources */
    glUseProgram(c->shader_program);
    glBindVertexArray(c->vao);

    /* orthographic projection uniform (pixel coords) */
    int loc = glGetUniformLocation(c->shader_program, "u_proj");
    if (loc >= 0) {
        float proj[16] = {
            2.0f/c->fb_width, 0,0,0,
            0, -2.0f/c->fb_height,0,0,
            0,0,1,0,
            -1,1,0,1
        };
        glUniformMatrix4fv(loc, 1, GL_FALSE, proj);
    }

    /* call user render */
    if (c->user_render) c->user_render(c, c->user_render_ptr);
}

void fossil_cube_frame_end(fossil_cube_ctx *c) {
    /* flush / unbind */
    glBindVertexArray(0);
    glUseProgram(0);
}

/* drawing simple rect (untextured) - immediate */
void fossil_cube_set_color(fossil_cube_ctx *c, fc_color_t color) {
    c->current_color = color;
}

void fossil_cube_draw_rect(fossil_cube_ctx *c, float x, float y, float w, float h) {
    /* We will use textured quad path but with white 1x1 texture assumed bound.
       For simplicity: build 6 verts and issue a draw call immediately.
       For performance, a real implementation would batch many draws.
    */
    float x0 = x, y0 = y;
    float x1 = x + w, y1 = y + h;
    /* pos(2) uv(2) packed color(4ub) -> 8 floats equivalent space (we pack color into 4 bytes) */
    struct V {
        float px, py;
        float u, v;
        unsigned char r,g,b,a;
    } v[6];

    unsigned char r = (c->current_color>>16)&0xFF;
    unsigned char g = (c->current_color>>8)&0xFF;
    unsigned char b = (c->current_color)&0xFF;
    unsigned char a = (c->current_color>>24)&0xFF;

    v[0] = (struct V){x0,y0, 0.0f,0.0f, r,g,b,a};
    v[1] = (struct V){x1,y0, 1.0f,0.0f, r,g,b,a};
    v[2] = (struct V){x1,y1, 1.0f,1.0f, r,g,b,a};
    v[3] = (struct V){x0,y0, 0.0f,0.0f, r,g,b,a};
    v[4] = (struct V){x1,y1, 1.0f,1.0f, r,g,b,a};
    v[5] = (struct V){x0,y1, 0.0f,1.0f, r,g,b,a};

    /* upload and draw */
    glBindBuffer(GL_ARRAY_BUFFER, c->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

/* circle approximation */
void fossil_cube_draw_circle(fossil_cube_ctx *c, float cx, float cy, float r, int segments) {
    if (segments < 8) segments = 8;
    /* simple GL_TRIANGLE_FAN */
    int verts = segments + 2;
    /* allocate on stack if small */
    float *buf = (float*)alloca(verts * (2+2+4) * sizeof(float)); /* pos+uv+color packed as floats */
    /* We'll do a naive approach: not packing color into bytes for simplicity */
    int idx = 0;
    unsigned char rr = (c->current_color>>16)&0xFF;
    unsigned char gg = (c->current_color>>8)&0xFF;
    unsigned char bb = (c->current_color)&0xFF;
    unsigned char aa = (c->current_color>>24)&0xFF;
    /* center */
    buf[idx++] = cx; buf[idx++] = cy; buf[idx++] = 0.5f; buf[idx++] = 0.5f;
    buf[idx++] = rr/255.0f; buf[idx++] = gg/255.0f; buf[idx++] = bb/255.0f; buf[idx++] = aa/255.0f;
    for (int i=0;i<=segments;i++) {
        float a = (float)i/(float)segments * 3.14159265f*2.0f;
        float sx = cx + cosf(a)*r;
        float sy = cy + sinf(a)*r;
        buf[idx++] = sx; buf[idx++] = sy; buf[idx++] = 0.5f; buf[idx++] = 0.5f;
        buf[idx++] = rr/255.0f; buf[idx++] = gg/255.0f; buf[idx++] = bb/255.0f; buf[idx++] = aa/255.0f;
    }
    glBindBuffer(GL_ARRAY_BUFFER, c->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, idx * sizeof(float), buf);
    glDrawArrays(GL_TRIANGLE_FAN, 0, verts);
}

/* textured quad */
void fossil_cube_draw_textured_quad(fossil_cube_ctx *c, unsigned int gl_texture,
                                    float x, float y, float w, float h,
                                    float u0, float v0, float u1, float v1) {
    (void)u0; (void)v0; (void)u1; (void)v1;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    fossil_cube_draw_rect(c, x, y, w, h);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/* texture helpers */
unsigned int fossil_cube_create_texture_from_rgba8(fossil_cube_ctx *c, const void *rgba, int width, int height) {
    (void)c;
    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}
void fossil_cube_destroy_texture(fossil_cube_ctx *c, unsigned int tex) {
    (void)c;
    glDeleteTextures(1, &tex);
}

/* debug */
void fossil_cube_enable_debug_draw(fossil_cube_ctx *c, int enable) { c->debug_draw = enable; }
const char *fossil_cube_version(void) { return "0.1.0"; }

/* Simple shader creation (vertex + fragment) */
static unsigned int fc_create_basic_shader(void) {
    const char *vs =
        "#version 110\n"
        "attribute vec2 a_pos;\n"
        "attribute vec2 a_uv;\n"
        "attribute vec4 a_col;\n"
        "uniform mat4 u_proj;\n"
        "varying vec2 v_uv;\n"
        "varying vec4 v_col;\n"
        "void main() { v_uv = a_uv; v_col = a_col; gl_Position = u_proj * vec4(a_pos,0.0,1.0); }\n";
    const char *fs =
        "#version 110\n"
        "varying vec2 v_uv;\n"
        "varying vec4 v_col;\n"
        "uniform sampler2D u_tex;\n"
        "void main(){ vec4 tex = texture2D(u_tex, v_uv); gl_FragColor = tex * v_col; }\n";

    unsigned int vsid = glCreateShader(GL_VERTEX_SHADER);
    unsigned int fsid = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vsid, 1, &vs, NULL);
    glCompileShader(vsid);
    glShaderSource(fsid, 1, &fs, NULL);
    glCompileShader(fsid);

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vsid);
    glAttachShader(prog, fsid);
    glBindAttribLocation(prog, 0, "a_pos");
    glBindAttribLocation(prog, 1, "a_uv");
    glBindAttribLocation(prog, 2, "a_col");
    glLinkProgram(prog);

    glDeleteShader(vsid);
    glDeleteShader(fsid);
    return prog;
}

static void fc_destroy_shader(unsigned int prog) {
    if (!prog) return;
    glDeleteProgram(prog);
}
