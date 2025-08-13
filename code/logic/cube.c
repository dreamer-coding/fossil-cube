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
#include <math.h>
#include <stdio.h>

/* --------------------------------- Config --------------------------------- */

#ifndef FOSSIL_CUBE_ASSERT
#   include <assert.h>
#   define FOSSIL_CUBE_ASSERT(x) assert(x)
#endif

#ifndef FOSSIL_CUBE_MALLOC
#   define FOSSIL_CUBE_MALLOC(sz) malloc(sz)
#   define FOSSIL_CUBE_FREE(p)    free(p)
#endif

#ifndef FOSSIL_CUBE_PI
#   define FOSSIL_CUBE_PI 3.14159265358979323846
#endif

/* -------------------------------- GL Helpers -------------------------------- */

static int g_has_shaders = 0;

static GLuint _cube_gl_create_shader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, 0);
    glCompileShader(sh);
    GLint ok = 0; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if(!ok){
        char log[1024]; GLsizei n=0; glGetShaderInfoLog(sh, sizeof(log), &n, log);
        /* Optional: print – library stays quiet otherwise */
        /* fprintf(stderr,"Fossil CUBE shader compile error:\n%.*s\n", (int)n, log); */
        glDeleteShader(sh); return 0;
    }
    return sh;
}

static GLuint _cube_gl_create_program(const char* vs, const char* fs, GLint* a_pos, GLint* a_uv, GLint* a_col) {
    GLuint v = _cube_gl_create_shader(GL_VERTEX_SHADER, vs);
    if(!v) return 0;
    GLuint f = _cube_gl_create_shader(GL_FRAGMENT_SHADER, fs);
    if(!f){ glDeleteShader(v); return 0; }

    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glBindAttribLocation(p, 0, "a_pos");
    glBindAttribLocation(p, 1, "a_uv");
    glBindAttribLocation(p, 2, "a_col");
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);

    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if(!ok){ glDeleteProgram(p); return 0; }

    if(a_pos) *a_pos = 0;
    if(a_uv)  *a_uv  = 1;
    if(a_col) *a_col = 2;
    return p;
}

/* ------------------------------- Tiny Math -------------------------------- */

static float clampf(float x, float a, float b){ return (x < a) ? a : (x > b ? b : x); }

/* ------------------------------ Bitmap Font -------------------------------- */

/* 6x8 ASCII font (96 glyphs), public domain-ish mini set.
 * Each glyph is 6x8, stored as 8 bytes (rows), LSB left, 6 bits used.
 * Credit: derived from classic tiny bit fonts. */
static const unsigned char g_font6x8[96][8] = {
/* SP */{0,0,0,0,0,0,0,0},
/* !  */{0x30,0x30,0x30,0x30,0x30,0x00,0x30,0x00},
/* "  */{0x6c,0x6c,0x48,0x00,0x00,0x00,0x00,0x00},
/* #  */{0x48,0x48,0xfc,0x48,0xfc,0x48,0x48,0x00},
/* $  */{0x30,0x7c,0xd0,0x78,0x1c,0xd8,0x70,0x00},
/* %  */{0xc4,0xcc,0x18,0x30,0x60,0xc6,0x86,0x00},
/* &  */{0x30,0x48,0x30,0x72,0x8c,0x8c,0x76,0x00},
/* '  */{0x30,0x30,0x60,0x00,0x00,0x00,0x00,0x00},
/* (  */{0x18,0x30,0x60,0x60,0x60,0x30,0x18,0x00},
/* )  */{0x60,0x30,0x18,0x18,0x18,0x30,0x60,0x00},
/* *  */{0x00,0x48,0x30,0xfc,0x30,0x48,0x00,0x00},
/* +  */{0x00,0x30,0x30,0xfc,0x30,0x30,0x00,0x00},
/* ,  */{0x00,0x00,0x00,0x00,0x30,0x30,0x60,0x00},
/* -  */{0x00,0x00,0x00,0xfc,0x00,0x00,0x00,0x00},
/* .  */{0x00,0x00,0x00,0x00,0x30,0x30,0x00,0x00},
/* /  */{0x04,0x0c,0x18,0x30,0x60,0xc0,0x80,0x00},
/* 0  */{0x78,0xcc,0xdc,0xf4,0xec,0xcc,0x78,0x00},
/* 1  */{0x30,0x70,0x30,0x30,0x30,0x30,0xfc,0x00},
/* 2  */{0x78,0xcc,0x0c,0x38,0x60,0xcc,0xfc,0x00},
/* 3  */{0x78,0xcc,0x0c,0x38,0x0c,0xcc,0x78,0x00},
/* 4  */{0x1c,0x3c,0x6c,0xcc,0xfe,0x0c,0x1e,0x00},
/* 5  */{0xfc,0xc0,0xf8,0x0c,0x0c,0xcc,0x78,0x00},
/* 6  */{0x38,0x60,0xc0,0xf8,0xcc,0xcc,0x78,0x00},
/* 7  */{0xfc,0xcc,0x0c,0x18,0x30,0x30,0x30,0x00},
/* 8  */{0x78,0xcc,0xcc,0x78,0xcc,0xcc,0x78,0x00},
/* 9  */{0x78,0xcc,0xcc,0x7c,0x0c,0x18,0x70,0x00},
/* :  */{0x00,0x30,0x30,0x00,0x00,0x30,0x30,0x00},
/* ;  */{0x00,0x30,0x30,0x00,0x30,0x30,0x60,0x00},
/* <  */{0x0c,0x18,0x30,0x60,0x30,0x18,0x0c,0x00},
/* =  */{0x00,0x00,0xfc,0x00,0xfc,0x00,0x00,0x00},
/* >  */{0x60,0x30,0x18,0x0c,0x18,0x30,0x60,0x00},
/* ?  */{0x78,0xcc,0x0c,0x38,0x30,0x00,0x30,0x00},
/* @  */{0x7c,0xc6,0xde,0xd6,0xde,0xc0,0x7c,0x00},
/* A  */{0x30,0x78,0xcc,0xcc,0xfc,0xcc,0xcc,0x00},
/* B  */{0xf8,0xcc,0xcc,0xf8,0xcc,0xcc,0xf8,0x00},
/* C  */{0x78,0xcc,0xc0,0xc0,0xc0,0xcc,0x78,0x00},
/* D  */{0xf8,0xcc,0xcc,0xcc,0xcc,0xcc,0xf8,0x00},
/* E  */{0xfc,0xc0,0xc0,0xf8,0xc0,0xc0,0xfc,0x00},
/* F  */{0xfc,0xc0,0xc0,0xf8,0xc0,0xc0,0xc0,0x00},
/* G  */{0x78,0xcc,0xc0,0xdc,0xcc,0xcc,0x7c,0x00},
/* H  */{0xcc,0xcc,0xcc,0xfc,0xcc,0xcc,0xcc,0x00},
/* I  */{0x7c,0x30,0x30,0x30,0x30,0x30,0x7c,0x00},
/* J  */{0x1c,0x0c,0x0c,0x0c,0xcc,0xcc,0x78,0x00},
/* K  */{0xcc,0xd8,0xf0,0xe0,0xf0,0xd8,0xcc,0x00},
/* L  */{0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xfc,0x00},
/* M  */{0x84,0xcc,0xfc,0xd4,0xcc,0xcc,0xcc,0x00},
/* N  */{0xcc,0xec,0xfc,0xdc,0xcc,0xcc,0xcc,0x00},
/* O  */{0x78,0xcc,0xcc,0xcc,0xcc,0xcc,0x78,0x00},
/* P  */{0xf8,0xcc,0xcc,0xf8,0xc0,0xc0,0xc0,0x00},
/* Q  */{0x78,0xcc,0xcc,0xcc,0xdc,0xd8,0x7c,0x00},
/* R  */{0xf8,0xcc,0xcc,0xf8,0xd8,0xcc,0xcc,0x00},
/* S  */{0x78,0xcc,0xe0,0x78,0x1c,0xcc,0x78,0x00},
/* T  */{0xfc,0x30,0x30,0x30,0x30,0x30,0x30,0x00},
/* U  */{0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0x78,0x00},
/* V  */{0xcc,0xcc,0xcc,0xcc,0x78,0x30,0x30,0x00},
/* W  */{0xcc,0xcc,0xcc,0xd4,0xfc,0xcc,0x84,0x00},
/* X  */{0xcc,0xcc,0x78,0x30,0x78,0xcc,0xcc,0x00},
/* Y  */{0xcc,0xcc,0x78,0x30,0x30,0x30,0x30,0x00},
/* Z  */{0xfc,0x0c,0x18,0x30,0x60,0xc0,0xfc,0x00},
/* [  */{0x7c,0x60,0x60,0x60,0x60,0x60,0x7c,0x00},
/* \  */{0x80,0xc0,0x60,0x30,0x18,0x0c,0x04,0x00},
/* ]  */{0x7c,0x0c,0x0c,0x0c,0x0c,0x0c,0x7c,0x00},
/* ^  */{0x30,0x78,0xcc,0x00,0x00,0x00,0x00,0x00},
/* _  */{0x00,0x00,0x00,0x00,0x00,0x00,0xfc,0x00},
/* `  */{0x60,0x30,0x18,0x00,0x00,0x00,0x00,0x00},
/* a  */{0x00,0x00,0x78,0x0c,0x7c,0xcc,0x7c,0x00},
/* b  */{0xc0,0xc0,0xf8,0xcc,0xcc,0xcc,0xf8,0x00},
/* c  */{0x00,0x00,0x78,0xcc,0xc0,0xcc,0x78,0x00},
/* d  */{0x0c,0x0c,0x7c,0xcc,0xcc,0xcc,0x7c,0x00},
/* e  */{0x00,0x00,0x78,0xcc,0xfc,0xc0,0x78,0x00},
/* f  */{0x38,0x60,0x60,0xf8,0x60,0x60,0x60,0x00},
/* g  */{0x00,0x00,0x7c,0xcc,0xcc,0x7c,0x0c,0x78},
/* h  */{0xc0,0xc0,0xf8,0xcc,0xcc,0xcc,0xcc,0x00},
/* i  */{0x30,0x00,0x70,0x30,0x30,0x30,0x78,0x00},
/* j  */{0x18,0x00,0x38,0x18,0x18,0x18,0x98,0x70},
/* k  */{0xc0,0xc0,0xd8,0xf0,0xe0,0xf0,0xd8,0x00},
/* l  */{0x70,0x30,0x30,0x30,0x30,0x30,0x78,0x00},
/* m  */{0x00,0x00,0xcc,0xfc,0xd4,0xcc,0xcc,0x00},
/* n  */{0x00,0x00,0xf8,0xcc,0xcc,0xcc,0xcc,0x00},
/* o  */{0x00,0x00,0x78,0xcc,0xcc,0xcc,0x78,0x00},
/* p  */{0x00,0x00,0xf8,0xcc,0xcc,0xf8,0xc0,0xc0},
/* q  */{0x00,0x00,0x7c,0xcc,0xcc,0x7c,0x0c,0x0c},
/* r  */{0x00,0x00,0xd8,0xf0,0xc0,0xc0,0xc0,0x00},
/* s  */{0x00,0x00,0x7c,0xc0,0x78,0x0c,0xf8,0x00},
/* t  */{0x60,0x60,0xf8,0x60,0x60,0x60,0x38,0x00},
/* u  */{0x00,0x00,0xcc,0xcc,0xcc,0xcc,0x7c,0x00},
/* v  */{0x00,0x00,0xcc,0xcc,0xcc,0x78,0x30,0x00},
/* w  */{0x00,0x00,0xcc,0xcc,0xd4,0xfc,0xcc,0x00},
/* x  */{0x00,0x00,0xcc,0x78,0x30,0x78,0xcc,0x00},
/* y  */{0x00,0x00,0xcc,0xcc,0x7c,0x0c,0x78,0x00},
/* z  */{0x00,0x00,0xfc,0x18,0x30,0x60,0xfc,0x00},
/* {  */{0x1c,0x30,0x30,0x60,0x30,0x30,0x1c,0x00},
/* |  */{0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x00},
/* }  */{0x70,0x18,0x18,0x0c,0x18,0x18,0x70,0x00},
/* ~  */{0x00,0x68,0xd8,0x90,0x00,0x00,0x00,0x00},
};

static void font_glyph_uv(int ch, float* u0, float* v0, float* u1, float* v1) {
    /* We rasterize on the fly; no atlas tex. Return UVs for a generated 1x1 white tex. */
    (void)ch; *u0=0; *v0=0; *u1=1; *v1=1;
}

/* ------------------------------ Draw Batching -------------------------------- */

typedef struct {
    float x, y;   /* position */
    float u, v;   /* uv */
    unsigned int abgr; /* color packed */
} Vtx;

typedef struct {
    GLuint vbo, ibo;
    GLuint prog;
    GLint  loc_mvp, loc_tex;
    GLint  a_pos, a_uv, a_col;
} GLPipe;

typedef struct {
    /* dynamic buffers */
    Vtx*   vtx;  int vtx_count;  int vtx_cap;
    unsigned int* idx; int idx_count; int idx_cap;

    /* GL */
    GLPipe gl;

    /* framebuffer */
    int fb_w, fb_h;
    float dpi;

    /* per frame */
    fossil_cube_input input;
    double dt;

    /* style */
    fossil_cube_style style;
    int clear_background;

    /* layout / window state */
    struct {
        int   active;   /* inside a begin/end pair */
        float x, y, w, h;
        float cursor_x, cursor_y;
        float line_height;
        int   id_seed;  /* for widget ids */
        const char* title;
    } panel;

    /* interaction */
    unsigned int hot_id;
    unsigned int active_id;
    int          mouse_down_prev[3];

    /* white 1x1 texture (for colored rects & font) */
    GLuint tex_white;

    /* text scale cache */
    float glyph_w, glyph_h;
} Ctx;

struct fossil_cube_ctx { Ctx s; };

/* ---------------------------- Small Utilities -------------------------------- */

static unsigned int pack_abgr(fossil_cube_color c){
    unsigned int a = (unsigned int)(clampf(c.a,0,1)*255.0f + 0.5f);
    unsigned int b = (unsigned int)(clampf(c.b,0,1)*255.0f + 0.5f);
    unsigned int g = (unsigned int)(clampf(c.g,0,1)*255.0f + 0.5f);
    unsigned int r = (unsigned int)(clampf(c.r,0,1)*255.0f + 0.5f);
    return (a<<24) | (b<<16) | (g<<8) | (r);
}

static void ensure_vtx(Ctx* c, int add_vtx, int add_idx){
    if(c->vtx_count + add_vtx > c->vtx_cap){
        int nc = c->vtx_cap ? c->vtx_cap*2 : 4096;
        while(nc < c->vtx_count + add_vtx) nc*=2;
        c->vtx = (Vtx*)realloc(c->vtx, nc*sizeof(Vtx));
        c->vtx_cap = nc;
    }
    if(c->idx_count + add_idx > c->idx_cap){
        int nc = c->idx_cap ? c->idx_cap*2 : 8192;
        while(nc < c->idx_count + add_idx) nc*=2;
        c->idx = (unsigned int*)realloc(c->idx, nc*sizeof(unsigned int));
        c->idx_cap = nc;
    }
}

static void push_quad(Ctx* c, float x0,float y0,float x1,float y1,
                      float u0,float v0,float u1,float v1, fossil_cube_color col)
{
    ensure_vtx(c, 4, 6);
    unsigned int base = (unsigned int)c->vtx_count;
    unsigned int pc = pack_abgr(col);
    c->vtx[c->vtx_count++] = (Vtx){x0,y0,u0,v0,pc};
    c->vtx[c->vtx_count++] = (Vtx){x1,y0,u1,v0,pc};
    c->vtx[c->vtx_count++] = (Vtx){x1,y1,u1,v1,pc};
    c->vtx[c->vtx_count++] = (Vtx){x0,y1,u0,v1,pc};
    c->idx[c->idx_count++] = base+0;
    c->idx[c->idx_count++] = base+1;
    c->idx[c->idx_count++] = base+2;
    c->idx[c->idx_count++] = base+0;
    c->idx[c->idx_count++] = base+2;
    c->idx[c->idx_count++] = base+3;
}

/* ------------------------------ GL Pipeline ---------------------------------- */

static const char* VS_SRC =
"#ifdef GL_ES\n"
"precision mediump float;\n"
"#endif\n"
"uniform mat4 u_mvp;\n"
"attribute vec2 a_pos;\n"
"attribute vec2 a_uv;\n"
"attribute vec4 a_col;\n"
"varying vec2 v_uv;\n"
"varying vec4 v_col;\n"
"void main(){ v_uv=a_uv; v_col=a_col; gl_Position = u_mvp * vec4(a_pos,0.0,1.0); }\n";

static const char* FS_SRC =
"#ifdef GL_ES\n"
"precision mediump float;\n"
"#endif\n"
"uniform sampler2D u_tex;\n"
"varying vec2 v_uv;\n"
"varying vec4 v_col;\n"
"void main(){ vec4 tex=texture2D(u_tex, v_uv); gl_FragColor = tex * v_col; }\n";

static void ortho(float l,float r,float b,float t,float* m){
    /* column-major 4x4 */
    float n= -1.0f, f=1.0f;
    memset(m,0,16*sizeof(float));
    m[0]= 2.0f/(r-l);
    m[5]= 2.0f/(t-b);
    m[10]= -2.0f/(f-n);
    m[12]= -(r+l)/(r-l);
    m[13]= -(t+b)/(t-b);
    m[14]= -(f+n)/(f-n);
    m[15]= 1.0f;
}

static void glpipe_init(Ctx* c){
    /* white texture */
    glGenTextures(1, &c->tex_white);
    glBindTexture(GL_TEXTURE_2D, c->tex_white);
    unsigned int white = 0xffffffffu;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1,1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    /* Try shaders */
    GLuint prog = _cube_gl_create_program(VS_SRC, FS_SRC, &c->gl.a_pos, &c->gl.a_uv, &c->gl.a_col);
    if(prog){
        c->gl.prog = prog;
        c->gl.loc_mvp = glGetUniformLocation(prog, "u_mvp");
        c->gl.loc_tex = glGetUniformLocation(prog, "u_tex");
        g_has_shaders = 1;
    } else {
        c->gl.prog = 0;
        g_has_shaders = 0; /* Fixed pipeline fallback */
    }

    glGenBuffers(1, &c->gl.vbo);
    glGenBuffers(1, &c->gl.ibo);
}

static void glpipe_shutdown(Ctx* c){
    if(c->gl.vbo) glDeleteBuffers(1, &c->gl.vbo);
    if(c->gl.ibo) glDeleteBuffers(1, &c->gl.ibo);
    if(c->gl.prog) glDeleteProgram(c->gl.prog);
    if(c->tex_white) glDeleteTextures(1, &c->tex_white);
    memset(&c->gl, 0, sizeof(c->gl));
    c->tex_white = 0;
}

static void glpipe_draw(Ctx* c){
    if(c->vtx_count == 0 || c->idx_count == 0) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST); /* could be used later for clipping */

    glBindTexture(GL_TEXTURE_2D, c->tex_white);

    glBindBuffer(GL_ARRAY_BUFFER, c->gl.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(c->vtx_count*sizeof(Vtx)), c->vtx, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, c->gl.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(c->idx_count*sizeof(unsigned int)), c->idx, GL_DYNAMIC_DRAW);

    if(g_has_shaders){
        glUseProgram(c->gl.prog);
        float m[16]; ortho(0,(float)c->fb_w, (float)c->fb_h, 0, m);
        glUniformMatrix4fv(c->gl.loc_mvp, 1, GL_FALSE, m);
        glUniform1i(c->gl.loc_tex, 0);

        glEnableVertexAttribArray(c->gl.a_pos);
        glEnableVertexAttribArray(c->gl.a_uv);
        glEnableVertexAttribArray(c->gl.a_col);

        glVertexAttribPointer(c->gl.a_pos, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (const void*)(0));
        glVertexAttribPointer(c->gl.a_uv,  2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (const void*)(8));
        glVertexAttribPointer(c->gl.a_col, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vtx), (const void*)(16));

        glDrawElements(GL_TRIANGLES, c->idx_count, GL_UNSIGNED_INT, (const void*)0);

        glDisableVertexAttribArray(c->gl.a_pos);
        glDisableVertexAttribArray(c->gl.a_uv);
        glDisableVertexAttribArray(c->gl.a_col);

        glUseProgram(0);
    } else {
        /* Fixed pipeline fallback (GL 1.x) */
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
        glOrtho(0, c->fb_w, c->fb_h, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);

        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

        glVertexPointer(2, GL_FLOAT, sizeof(Vtx), (const void*)(0));
        glTexCoordPointer(2, GL_FLOAT, sizeof(Vtx), (const void*)(8));
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vtx), (const void*)(16));

        glDrawElements(GL_TRIANGLES, c->idx_count, GL_UNSIGNED_INT, (const void*)0);

        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);

        glMatrixMode(GL_MODELVIEW);  glPopMatrix();
        glMatrixMode(GL_PROJECTION); glPopMatrix();
    }

    glDisable(GL_SCISSOR_TEST);

    c->vtx_count = 0;
    c->idx_count = 0;
}

/* ------------------------------ Public API ------------------------------------ */

static void set_default_style(fossil_cube_style* s){
    s->clear_color  = fossil_cube_rgba(0.08f,0.09f,0.10f,1.0f);
    s->panel_bg     = fossil_cube_rgba(0.12f,0.13f,0.15f,0.95f);
    s->panel_border = fossil_cube_rgba(0.05f,0.05f,0.05f,1.0f);
    s->text         = fossil_cube_rgba(0.92f,0.93f,0.95f,1.0f);
    s->button       = fossil_cube_rgba(0.25f,0.27f,0.30f,1.0f);
    s->button_hot   = fossil_cube_rgba(0.34f,0.36f,0.40f,1.0f);
    s->button_active= fossil_cube_rgba(0.18f,0.75f,0.42f,1.0f);
    s->slider_bg    = fossil_cube_rgba(0.20f,0.22f,0.25f,1.0f);
    s->slider_knob  = fossil_cube_rgba(0.80f,0.82f,0.85f,1.0f);
    s->padding      = 8.0f;
    s->item_spacing = 6.0f;
    s->roundness    = 3.0f;
    s->font_px      = 14.0f;
}

fossil_cube_ctx* fossil_cube_create_context(int fb_w, int fb_h, float dpi_scale){
    fossil_cube_ctx* o = (fossil_cube_ctx*)FOSSIL_CUBE_MALLOC(sizeof(*o));
    memset(o, 0, sizeof(*o));
    Ctx* c = &o->s;

    c->fb_w = (fb_w>0?fb_w:640);
    c->fb_h = (fb_h>0?fb_h:480);
    c->dpi  = (dpi_scale>0?dpi_scale:1.0f);

    c->vtx = NULL; c->vtx_cap = 0; c->vtx_count = 0;
    c->idx = NULL; c->idx_cap = 0; c->idx_count = 0;

    set_default_style(&c->style);
    c->clear_background = 0;

    c->panel.active = 0;
    c->panel.line_height = c->style.font_px + c->style.item_spacing;

    c->glyph_w = 6.0f * (c->style.font_px / 8.0f); /* scale 6x8 font to font_px height */
    c->glyph_h = c->style.font_px;

    glpipe_init(c);
    return o;
}

void fossil_cube_destroy_context(fossil_cube_ctx* ctx){
    if(!ctx) return;
    Ctx* c = &ctx->s;
    glpipe_shutdown(c);
    if(c->vtx) FOSSIL_CUBE_FREE(c->vtx);
    if(c->idx) FOSSIL_CUBE_FREE(c->idx);
    FOSSIL_CUBE_FREE(ctx);
}

void fossil_cube_resize(fossil_cube_ctx* ctx, int fb_w, int fb_h, float dpi_scale){
    Ctx* c = &ctx->s;
    if(fb_w>0) c->fb_w = fb_w;
    if(fb_h>0) c->fb_h = fb_h;
    if(dpi_scale>0) c->dpi = dpi_scale;

    c->panel.line_height = c->style.font_px + c->style.item_spacing;
    c->glyph_w = 6.0f * (c->style.font_px / 8.0f);
    c->glyph_h = c->style.font_px;
}

void fossil_cube_style_reset_default(fossil_cube_style* st){ set_default_style(st); }
void fossil_cube_get_style(fossil_cube_ctx* ctx, fossil_cube_style* out_style){
    if(!out_style) return; *out_style = ctx->s.style;
}
void fossil_cube_set_style(fossil_cube_ctx* ctx, const fossil_cube_style* in_style){
    if(!in_style) return; ctx->s.style = *in_style;
    /* recompute dependent metrics */
    Ctx* c = &ctx->s;
    c->panel.line_height = c->style.font_px + c->style.item_spacing;
    c->glyph_w = 6.0f * (c->style.font_px / 8.0f);
    c->glyph_h = c->style.font_px;
}

void fossil_cube_set_clear_background(fossil_cube_ctx* ctx, int enabled){ ctx->s.clear_background = enabled ? 1 : 0; }

/* Frame */
void fossil_cube_new_frame(fossil_cube_ctx* ctx, const fossil_cube_input* in, double dt_seconds){
    Ctx* c = &ctx->s;

    if(in){
        c->input = *in;
        if(in->fb_w>0) c->fb_w = in->fb_w;
        if(in->fb_h>0) c->fb_h = in->fb_h;
        if(in->dpi_scale>0) c->dpi = in->dpi_scale;
    }
    c->dt = dt_seconds;

    c->panel.id_seed = 0;
    c->hot_id = 0;

    /* Compute mouse clicked flags (edge) if not provided */
    for(int i=0;i<3;i++){
        if(!in) break;
        int was = c->mouse_down_prev[i];
        int is  = c->input.mouse_down[i];
        if(!was && is) c->input.mouse_clicked[i] = 1;
        c->mouse_down_prev[i] = is;
    }

    if(c->clear_background){
        glClearColor(c->style.clear_color.r, c->style.clear_color.g, c->style.clear_color.b, c->style.clear_color.a);
        glClear(GL_COLOR_BUFFER_BIT);
    }
}

/* Rendering */
void fossil_cube_render(fossil_cube_ctx* ctx){
    glpipe_draw(&ctx->s);
}

/* ---------------------------- Primitive Drawing ------------------------------- */

void fossil_cube_draw_rect(fossil_cube_ctx* ctx, fossil_cube_rect r, fossil_cube_color c, float round){
    (void)round; /* roundness could be implemented with more verts; keep simple quad for now. */
    push_quad(&ctx->s, r.x, r.y, r.x+r.w, r.y+r.h, 0,0,1,1, c);
}

void fossil_cube_draw_rect_line(fossil_cube_ctx* ctx, fossil_cube_rect r, float t, fossil_cube_color c, float round){
    (void)round;
    fossil_cube_draw_rect(ctx, fossil_cube_rect_xywh(r.x, r.y, r.w, t), c, 0);
    fossil_cube_draw_rect(ctx, fossil_cube_rect_xywh(r.x, r.y+r.h-t, r.w, t), c, 0);
    fossil_cube_draw_rect(ctx, fossil_cube_rect_xywh(r.x, r.y+t, t, r.h-2*t), c, 0);
    fossil_cube_draw_rect(ctx, fossil_cube_rect_xywh(r.x+r.w-t, r.y+t, t, r.h-2*t), c, 0);
}

/* ------------------------------- Text Drawing --------------------------------- */

float fossil_cube_text_height(fossil_cube_ctx* ctx){ return ctx->s.glyph_h; }

float fossil_cube_text_width(fossil_cube_ctx* ctx, const char* text){
    (void)ctx;
    if(!text) return 0.0f;
    size_t n = strlen(text);
    /* proportional-ish: treat narrow chars slightly narrower */
    float w = 0.0f;
    for(size_t i=0;i<n;i++){
        char ch = text[i];
        if(ch=='\t') w += 4.0f*6.0f;
        else if(ch==' ') w += 4.0f;
        else w += 6.0f;
    }
    return w * (ctx->s.style.font_px / 8.0f);
}

static void draw_glyph(Ctx* c, float x, float y, char ch, fossil_cube_color col){
    if(ch < 32 || ch > 126) ch = '?';
    const unsigned char* rows = g_font6x8[(int)ch - 32];
    float sx = (c->style.font_px / 8.0f); /* scale from 8px font height to desired */
    float sy = sx;
    for(int ry=0; ry<8; ++ry){
        unsigned char bits = rows[ry];
        for(int rx=0; rx<6; ++rx){
            if(bits & (1<<rx)){
                float px = x + rx*sx;
                float py = y + ry*sy;
                push_quad(c, px, py, px+sx, py+sy, 0,0,1,1, col);
            }
        }
    }
}

void fossil_cube_draw_text(fossil_cube_ctx* ctx, float x, float y, const char* text, fossil_cube_color col){
    if(!text) return;
    Ctx* c = &ctx->s;
    float cursor = 0.0f;
    for(const char* p=text; *p; ++p){
        if(*p=='\n'){ y += c->glyph_h; cursor = 0.0f; continue; }
        if(*p==' '){ cursor += 4.0f*(c->style.font_px/8.0f); continue; }
        draw_glyph(c, x + cursor, y, *p, col);
        cursor += 6.0f*(c->style.font_px/8.0f);
    }
}

/* ------------------------------ Textures API ---------------------------------- */

fossil_cube_texture fossil_cube_texture_create(const void* rgba8_pixels, int w, int h, int linear_filter){
    fossil_cube_texture t; t.id=0; t.w=w; t.h=h;
    GLuint id=0; glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear_filter?GL_LINEAR:GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear_filter?GL_LINEAR:GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w,h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba8_pixels);
    t.id = id;
    return t;
}

void fossil_cube_texture_destroy(fossil_cube_texture* t){
    if(!t || !t->id) return;
    GLuint id = t->id; glDeleteTextures(1, &id);
    t->id = 0; t->w=0; t->h=0;
}

/* ------------------------------- UI Core -------------------------------------- */

static unsigned int hash_str(const char* s, unsigned int seed){
    unsigned int h = seed ? seed : 2166136261u;
    while(*s){ h ^= (unsigned char)(*s++); h *= 16777619u; }
    if(h==0) h=1;
    return h;
}

static unsigned int next_widget_id(Ctx* c, const char* label){
    return hash_str(label?label:"", (unsigned int)(++c->panel.id_seed));
}

static int mouse_in_rect(const fossil_cube_input* in, fossil_cube_rect r){
    float x=in->mouse_pos.x, y=in->mouse_pos.y;
    return (x>=r.x && x<=r.x+r.w && y>=r.y && y<=r.y+r.h);
}

static void advance_cursor(Ctx* c, float h){ c->panel.cursor_y += h; c->panel.cursor_x = c->panel.x + c->style.padding; }

/* Begin/End window/panel */

int fossil_cube_begin_window(fossil_cube_ctx* ctx, const char* title, float x, float y, float w, float h, int* open_opt){
    Ctx* c = &ctx->s;
    if(open_opt && !*open_opt) { c->panel.active = 0; return 0; }

    c->panel.active = 1;
    c->panel.x = x; c->panel.y = y; c->panel.w = w; c->panel.h = h;
    c->panel.cursor_x = x + c->style.padding;
    c->panel.cursor_y = y + c->style.padding + c->glyph_h + c->style.item_spacing; /* leave room for title bar */
    c->panel.title = title;

    /* Panel body */
    fossil_cube_draw_rect(ctx, fossil_cube_rect_xywh(x, y, w, h), c->style.panel_bg, c->style.roundness);
    fossil_cube_draw_rect_line(ctx, fossil_cube_rect_xywh(x, y, w, h), 1.0f, c->style.panel_border, c->style.roundness);

    /* Title bar */
    fossil_cube_color tb = c->style.button;
    fossil_cube_draw_rect(ctx, fossil_cube_rect_xywh(x, y, w, c->glyph_h + c->style.padding*0.5f), tb, c->style.roundness);
    if(title){
        fossil_cube_draw_text(ctx, x + c->style.padding, y + (c->style.padding*0.25f), title, c->style.text);
    }

    /* (Optional) close box */
    if(open_opt){
        fossil_cube_rect close_r = fossil_cube_rect_xywh(x+w - (c->glyph_h), y+2, c->glyph_h-4, c->glyph_h-4);
        fossil_cube_color cc = c->style.button;
        if(mouse_in_rect(&c->input, close_r)) cc = c->style.button_hot;
        fossil_cube_draw_rect(ctx, close_r, cc, c->style.roundness);
        fossil_cube_draw_text(ctx, close_r.x+4, close_r.y, "x", c->style.text);
        if(mouse_in_rect(&c->input, close_r) && c->input.mouse_clicked[0]) { *open_opt = 0; }
    }
    return 1;
}

void fossil_cube_end_window(fossil_cube_ctx* ctx){
    Ctx* c = &ctx->s;
    c->panel.active = 0;
}

/* Label */
void fossil_cube_label(fossil_cube_ctx* ctx, const char* text){
    Ctx* c = &ctx->s;
    if(!c->panel.active) return;
    fossil_cube_draw_text(ctx, c->panel.cursor_x, c->panel.cursor_y, text?text:"", c->style.text);
    advance_cursor(c, c->panel.line_height);
}

/* Button */
int fossil_cube_button(fossil_cube_ctx* ctx, const char* label){
    Ctx* c = &ctx->s; if(!c->panel.active) return 0;
    unsigned int id = next_widget_id(c, label?label:"button");

    float tw = fossil_cube_text_width(ctx, label?label:"");
    float w = tw + c->style.padding*2.0f;
    float h = c->glyph_h + c->style.padding*0.5f;

    fossil_cube_rect r = fossil_cube_rect_xywh(c->panel.cursor_x, c->panel.cursor_y, w, h);

    int hovered = mouse_in_rect(&c->input, r);
    if(hovered) c->hot_id = id;

    int pressed = 0;
    if(hovered && c->input.mouse_clicked[0]) c->active_id = id;
    if(c->active_id == id && !c->input.mouse_down[0]){ /* released */
        if(hovered) pressed = 1;
        c->active_id = 0;
    }

    fossil_cube_color bc = c->style.button;
    if(c->active_id == id) bc = c->style.button_active;
    else if(hovered) bc = c->style.button_hot;

    fossil_cube_draw_rect(ctx, r, bc, c->style.roundness);
    fossil_cube_draw_rect_line(ctx, r, 1.0f, c->style.panel_border, c->style.roundness);
    fossil_cube_draw_text(ctx, r.x + c->style.padding, r.y + (h - c->glyph_h)*0.5f, label?label:"", c->style.text);

    advance_cursor(c, h + c->style.item_spacing);
    return pressed;
}

/* Slider */
int fossil_cube_slider(fossil_cube_ctx* ctx, const char* label, float* value, float min_v, float max_v, float step){
    Ctx* c = &ctx->s; if(!c->panel.active) return 0;
    unsigned int id = next_widget_id(c, label?label:"slider");

    float bar_w = 160.0f * (c->style.font_px/14.0f);
    float bar_h = c->glyph_h * 0.5f;
    float w = bar_w;
    float h = c->glyph_h + c->style.item_spacing;

    fossil_cube_rect r = fossil_cube_rect_xywh(c->panel.cursor_x, c->panel.cursor_y, w, bar_h);
    fossil_cube_rect knob;
    float t = (*value - min_v) / (max_v - min_v);
    t = clampf(t, 0.0f, 1.0f);
    float kx = r.x + t*(r.w - bar_h);
    knob = fossil_cube_rect_xywh(kx, r.y - (bar_h*0.5f) + (bar_h*0.5f), bar_h, bar_h);

    int hovered = mouse_in_rect(&c->input, r) || mouse_in_rect(&c->input, knob);
    if(hovered) c->hot_id = id;
    int changed = 0;

    if(hovered && c->input.mouse_clicked[0]) c->active_id = id;
    if(c->active_id == id){
        if(c->input.mouse_down[0]){
            float mx = c->input.mouse_pos.x;
            float nt = clampf((mx - r.x) / (r.w - bar_h), 0.0f, 1.0f);
            float nv = min_v + nt*(max_v - min_v);
            if(step>0){ nv = floorf((nv - min_v)/step + 0.5f)*step + min_v; nv = clampf(nv, min_v, max_v); }
            if(nv != *value){ *value = nv; changed = 1; }
        } else {
            c->active_id = 0;
        }
    }

    /* Draw bar */
    fossil_cube_draw_rect(ctx, r, c->style.slider_bg, c->style.roundness);
    /* Draw knob */
    fossil_cube_draw_rect(ctx, knob, c->style.slider_knob, c->style.roundness);
    fossil_cube_draw_rect_line(ctx, knob, 1.0f, c->style.panel_border, c->style.roundness);

    /* Label & value */
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %.3g", label?label:"", *value);
    fossil_cube_draw_text(ctx, r.x + r.w + c->style.padding, r.y - (c->glyph_h - bar_h)*0.5f, buf, c->style.text);

    advance_cursor(c, h);
    return changed;
}

/* Image */
void fossil_cube_image(fossil_cube_ctx* ctx, fossil_cube_texture tex, float w, float h){
    Ctx* c = &ctx->s; if(!c->panel.active) return;
    float x0 = c->panel.cursor_x, y0 = c->panel.cursor_y;
    glBindTexture(GL_TEXTURE_2D, tex.id);
    push_quad(c, x0, y0, x0+w, y0+h, 0,0,1,1, fossil_cube_rgba(1,1,1,1));
    glBindTexture(GL_TEXTURE_2D, c->tex_white);
    advance_cursor(c, h + c->style.item_spacing);
}

/* Layout */
void fossil_cube_same_line(fossil_cube_ctx* ctx){
    Ctx* c = &ctx->s;
    c->panel.cursor_x += 8.0f; /* small gap */
    c->panel.cursor_y -= c->panel.line_height; /* back up one line */
}
void fossil_cube_spacing(fossil_cube_ctx* ctx, float px){
    Ctx* c = &ctx->s;
    c->panel.cursor_y += px;
}

/* Version */
const char* fossil_cube_version(void){ return "Fossil CUBE 0.1.0"; }
