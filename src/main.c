// vim: sw=2 ts=2 expandtab smartindent

#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include "gl_include/gles3.h"

#include "font.h"

#define BREAKPOINT() __builtin_debugtrap()
#define jx_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
typedef struct { float x, y; } f2;
typedef struct { float x, y, z; } f3;
typedef union { float arr[4]; struct { float x, y, z, w; } p; f3 p3; } f4;
typedef union { float arr[4][4]; f4 rows[4]; float floats[16]; } f4x4;
static f4x4 f4x4_ortho(float left, float right, float bottom, float top, float near, float far) {
    f4x4 res = {0};

    res.arr[0][0] = 2.0f / (right - left);
    res.arr[1][1] = 2.0f / (top - bottom);
    res.arr[2][2] = 2.0f / (near - far);
    res.arr[3][3] = 1.0f;

    res.arr[3][0] = (left + right) / (left - right);
    res.arr[3][1] = (bottom + top) / (bottom - top);
    res.arr[3][2] = (far + near) / (near - far);

    return res;
}

static f4x4 f4x4_target_to(f3 eye, f3 target, f3 up) {
  float z0 = eye.x - target.x,
        z1 = eye.y - target.y,
        z2 = eye.z - target.z;
  float len = z0 * z0 + z1 * z1 + z2 * z2;
  if (len > 0) {
      len = 1.0f / sqrtf(len);
      z0 *= len;
      z1 *= len;
      z2 *= len;
  }
  float x0 = up.y * z2 - up.z * z1,
        x1 = up.z * z0 - up.x * z2,
        x2 = up.x * z1 - up.y * z0;
  len = x0 * x0 + x1 * x1 + x2 * x2;
  if (len > 0) {
      len = 1 / sqrtf(len);
      x0 *= len;
      x1 *= len;
      x2 *= len;
  }

  return (f4x4) { .arr = {
    { x0, x1, x2, 0 },
    {
      z1 * x2 - z2 * x1,
      z2 * x0 - z0 * x2,
      z0 * x1 - z1 * x0,
      0,
    },
    { z0, z1, z2, 0 },
    { eye.x, eye.y, eye.z, 1 },
  } };
}

static f4x4 f4x4_invert(f4x4 a) {
    float b00 = a.arr[0][0] * a.arr[1][1] - a.arr[0][1] * a.arr[1][0];
    float b01 = a.arr[0][0] * a.arr[1][2] - a.arr[0][2] * a.arr[1][0];
    float b02 = a.arr[0][0] * a.arr[1][3] - a.arr[0][3] * a.arr[1][0];
    float b03 = a.arr[0][1] * a.arr[1][2] - a.arr[0][2] * a.arr[1][1];
    float b04 = a.arr[0][1] * a.arr[1][3] - a.arr[0][3] * a.arr[1][1];
    float b05 = a.arr[0][2] * a.arr[1][3] - a.arr[0][3] * a.arr[1][2];
    float b06 = a.arr[2][0] * a.arr[3][1] - a.arr[2][1] * a.arr[3][0];
    float b07 = a.arr[2][0] * a.arr[3][2] - a.arr[2][2] * a.arr[3][0];
    float b08 = a.arr[2][0] * a.arr[3][3] - a.arr[2][3] * a.arr[3][0];
    float b09 = a.arr[2][1] * a.arr[3][2] - a.arr[2][2] * a.arr[3][1];
    float b10 = a.arr[2][1] * a.arr[3][3] - a.arr[2][3] * a.arr[3][1];
    float b11 = a.arr[2][2] * a.arr[3][3] - a.arr[2][3] * a.arr[3][2];

    /* Calculate the determinant */
    float det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
    if (det == 0.0f) {
      SDL_Log("Couldn't invert matrix!\n");
      return (f4x4) {0};
    }

    det = 1.0 / det;
    return (f4x4) { .floats = {
      (a.arr[1][1] * b11 - a.arr[1][2] * b10 + a.arr[1][3] * b09) * det,
      (a.arr[0][2] * b10 - a.arr[0][1] * b11 - a.arr[0][3] * b09) * det,
      (a.arr[3][1] * b05 - a.arr[3][2] * b04 + a.arr[3][3] * b03) * det,
      (a.arr[2][2] * b04 - a.arr[2][1] * b05 - a.arr[2][3] * b03) * det,
      (a.arr[1][2] * b08 - a.arr[1][0] * b11 - a.arr[1][3] * b07) * det,
      (a.arr[0][0] * b11 - a.arr[0][2] * b08 + a.arr[0][3] * b07) * det,
      (a.arr[3][2] * b02 - a.arr[3][0] * b05 - a.arr[3][3] * b01) * det,
      (a.arr[2][0] * b05 - a.arr[2][2] * b02 + a.arr[2][3] * b01) * det,
      (a.arr[1][0] * b10 - a.arr[1][1] * b08 + a.arr[1][3] * b06) * det,
      (a.arr[0][1] * b08 - a.arr[0][0] * b10 - a.arr[0][3] * b06) * det,
      (a.arr[3][0] * b04 - a.arr[3][1] * b02 + a.arr[3][3] * b00) * det,
      (a.arr[2][1] * b02 - a.arr[2][0] * b04 - a.arr[2][3] * b00) * det,
      (a.arr[1][1] * b07 - a.arr[1][0] * b09 - a.arr[1][2] * b06) * det,
      (a.arr[0][0] * b09 - a.arr[0][1] * b07 + a.arr[0][2] * b06) * det,
      (a.arr[3][1] * b01 - a.arr[3][0] * b03 - a.arr[3][2] * b00) * det,
      (a.arr[2][0] * b03 - a.arr[2][1] * b01 + a.arr[2][2] * b00) * det,
    } };
}

static f4x4 f4x4_mulf4x4(f4x4 a, f4x4 b) {
  f4x4 out = {0};

  /* cache only the current line of the second matrix */
  f4 bb = b.rows[0];
  out.arr[0][0] = bb.arr[0] * a.arr[0][0] + bb.arr[1] * a.arr[1][0] + bb.arr[2] * a.arr[2][0] + bb.arr[3] * a.arr[3][0];
  out.arr[0][1] = bb.arr[0] * a.arr[0][1] + bb.arr[1] * a.arr[1][1] + bb.arr[2] * a.arr[2][1] + bb.arr[3] * a.arr[3][1];
  out.arr[0][2] = bb.arr[0] * a.arr[0][2] + bb.arr[1] * a.arr[1][2] + bb.arr[2] * a.arr[2][2] + bb.arr[3] * a.arr[3][2];
  out.arr[0][3] = bb.arr[0] * a.arr[0][3] + bb.arr[1] * a.arr[1][3] + bb.arr[2] * a.arr[2][3] + bb.arr[3] * a.arr[3][3];
  bb = b.rows[1];
  out.arr[1][0] = bb.arr[0] * a.arr[0][0] + bb.arr[1] * a.arr[1][0] + bb.arr[2] * a.arr[2][0] + bb.arr[3] * a.arr[3][0];
  out.arr[1][1] = bb.arr[0] * a.arr[0][1] + bb.arr[1] * a.arr[1][1] + bb.arr[2] * a.arr[2][1] + bb.arr[3] * a.arr[3][1];
  out.arr[1][2] = bb.arr[0] * a.arr[0][2] + bb.arr[1] * a.arr[1][2] + bb.arr[2] * a.arr[2][2] + bb.arr[3] * a.arr[3][2];
  out.arr[1][3] = bb.arr[0] * a.arr[0][3] + bb.arr[1] * a.arr[1][3] + bb.arr[2] * a.arr[2][3] + bb.arr[3] * a.arr[3][3];
  bb = b.rows[2];
  out.arr[2][0] = bb.arr[0] * a.arr[0][0] + bb.arr[1] * a.arr[1][0] + bb.arr[2] * a.arr[2][0] + bb.arr[3] * a.arr[3][0];
  out.arr[2][1] = bb.arr[0] * a.arr[0][1] + bb.arr[1] * a.arr[1][1] + bb.arr[2] * a.arr[2][1] + bb.arr[3] * a.arr[3][1];
  out.arr[2][2] = bb.arr[0] * a.arr[0][2] + bb.arr[1] * a.arr[1][2] + bb.arr[2] * a.arr[2][2] + bb.arr[3] * a.arr[3][2];
  out.arr[2][3] = bb.arr[0] * a.arr[0][3] + bb.arr[1] * a.arr[1][3] + bb.arr[2] * a.arr[2][3] + bb.arr[3] * a.arr[3][3];
  bb = b.rows[3];
  out.arr[3][0] = bb.arr[0] * a.arr[0][0] + bb.arr[1] * a.arr[1][0] + bb.arr[2] * a.arr[2][0] + bb.arr[3] * a.arr[3][0];
  out.arr[3][1] = bb.arr[0] * a.arr[0][1] + bb.arr[1] * a.arr[1][1] + bb.arr[2] * a.arr[2][1] + bb.arr[3] * a.arr[3][1];
  out.arr[3][2] = bb.arr[0] * a.arr[0][2] + bb.arr[1] * a.arr[1][2] + bb.arr[2] * a.arr[2][2] + bb.arr[3] * a.arr[3][2];
  out.arr[3][3] = bb.arr[0] * a.arr[0][3] + bb.arr[1] * a.arr[1][3] + bb.arr[2] * a.arr[2][3] + bb.arr[3] * a.arr[3][3];

  return out;
}

static f4 f4x4_mul_f4(f4x4 m, f4 v) {
    f4 res;
    for (int x = 0; x < 4; x++) {
        float sum = 0;
        for (int y = 0; y < 4; y++)
            sum += m.arr[y][x] * v.arr[y];

        res.arr[x] = sum;
    }
    return res;
}


#define ANTIALIAS_NONE    0
#define ANTIALIAS_LINEAR  1
#define ANTIALIAS_FXAA    2
#define ANTIALIAS_2xSSAA  3
#define ANTIALIAS_4xSSAA  4
#define CURRENT_ALIASING ANTIALIAS_4xSSAA
#define SRGB

typedef struct { float x, y, u, v, size; } gl_text_Vtx;
typedef struct {
  struct { float x, y, z; } pos;
  struct { uint8_t r, g, b, a; } color;
  struct { float x, y, z; } normal;
} gl_geo_Vtx;
typedef struct { uint16_t a, b, c; } gl_Tri;

#include "../models/include/helmet.h"

static struct {
  struct {
    SDL_Window    *window;
    SDL_GLContext  gl_ctx;
  } sdl;

  /* input */
  size_t win_size_x, win_size_y;

  /* timekeeping */
  uint64_t ts_last_frame, ts_first;
  double elapsed;

  /* camera */
  f4x4 camera, screen;

  struct {
    struct {
      gl_geo_Vtx vtx[999];
      gl_geo_Vtx *vtx_wtr;

      gl_Tri idx[999];
      gl_Tri *idx_wtr;

      GLuint buf_vtx;
      GLuint buf_idx;

      size_t koob_tri_count;
      GLuint koob_buf_vtx;
      GLuint koob_buf_idx;

      GLuint shader;
      GLint shader_a_pos;
      GLint shader_a_color;
      GLint shader_a_normal;
      GLint shader_u_mvp;
    } geo;

    struct {
      float fb_scale;

      /* jeux.win_size * SDL_GetWindowPixelDensity() */
      float phys_win_size_x, phys_win_size_y;

      GLuint buf_vtx;

      GLuint shader; /* postprocessing */
      GLint shader_u_win_size;
      GLint shader_a_pos;

      /* resources inside here need to be recreated
       * when the application window is resized. */
      struct {
        /* postprocessing framebuffer (anti-aliasing and other fx) */
        GLuint pp_tex_color, pp_tex_depth, pp_fb;
      } screen;
    } pp;

    struct {
      gl_text_Vtx vtx[999];
      gl_text_Vtx *vtx_wtr;

      gl_Tri idx[999];
      gl_Tri *idx_wtr;

      GLuint buf_vtx;
      GLuint buf_idx;

      GLuint tex;

      GLuint shader;
      GLint shader_u_tex_size;
      GLint shader_u_win_size;
      GLint shader_u_buffer;
      GLint shader_u_gamma;
      GLint shader_a_pos;
      GLint shader_a_uv;
      GLint shader_a_size;
    } text;

  } gl;
} jeux = {
  .win_size_x = 800,
  .win_size_y = 450,
  .gl.pp = {
#if   CURRENT_ALIASING == ANTIALIAS_4xSSAA
    .fb_scale = 4.0f,
#elif CURRENT_ALIASING == ANTIALIAS_2xSSAA
    .fb_scale = 2.0f,
#else
    .fb_scale = 1.0f,
#endif
  }
};

static SDL_AppResult gl_init(void);
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {

  /* sdl init */
  {
    SDL_SetHint(SDL_HINT_APP_NAME, "jeu desprit");
    /* rationale: Obviously ANGLE's GLES is tested the most against the EGL it provides */
    SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "1");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
      SDL_Log("SDL init failed: %s\n", SDL_GetError());
      return 1;
    }

#ifdef _WIN32
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    /* if I try to force this, it fails */
    // SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

    jeux.sdl.window = SDL_CreateWindow(
      "jeu desprit",
      jeux.win_size_x,
      jeux.win_size_y,
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    if (jeux.sdl.window == NULL) {
      SDL_Log("Window init failed: %s\n", SDL_GetError());
      return 1;
    }

    jeux.sdl.gl_ctx = SDL_GL_CreateContext(jeux.sdl.window);
    if (jeux.sdl.window == NULL) {
      SDL_Log("GL init failed: %s\n", SDL_GetError());
      return 1;
    }
  }

  jeux.ts_last_frame = SDL_GetPerformanceCounter();
  jeux.ts_first = SDL_GetPerformanceCounter();

  return gl_init();
}
static void gl_resize(void);
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
  if (event->type == SDL_EVENT_WINDOW_RESIZED) {
    jeux.win_size_x = event->window.data1;
    jeux.win_size_y = event->window.data2;
    gl_resize();
  }

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  SDL_GL_DestroyContext(jeux.sdl.gl_ctx);
  SDL_DestroyWindow(jeux.sdl.window);
}

/* gl renderer init - expects jeux.sdl.gl to be initialized */
static void gl_resize(void);
static SDL_AppResult gl_init(void) {
  /* shader */
  {

    struct {
      GLuint *dst;
      const char *debug_name;
      const GLchar *vs, *fs;
    } shaders[] = {
      {
        /* MARK: geo shader definition */
        .dst = &jeux.gl.geo.shader,
        .debug_name = "geo",
        .vs =
          "attribute vec4 a_pos;\n"
          "attribute vec4 a_color;\n"
          "attribute vec3 a_normal;\n"
          "\n"
          "uniform mat4 u_mvp;\n"
          "\n"
          "varying vec3 v_color;\n"
          "varying vec3 v_normal;\n"
          "\n"
          "void main() {\n"
          "  gl_Position = u_mvp * vec4(a_pos.xyz, 1.0);\n"
          "  v_color  = a_color.xyz;\n"
          "  v_normal = a_normal;\n"
          "}\n"
        ,
        .fs =
          "precision mediump float;\n"
          "\n"
          "varying vec3 v_color;\n"
          "varying vec3 v_normal;\n"
          "\n"
          "uniform sampler2D u_tex;\n"
          "\n"
          "void main() {\n"
          /* debug normals */
          // "  gl_FragColor = vec4(mix(vec3(1), v_normal, 0.5), 1.0);\n"

          "  vec3 light_dir = normalize(vec3(4.0, 1.0, 5.9));\n"
          "  float diffuse = max(dot(v_normal, light_dir), 0.0);\n"
          "  float ramp = 0.0;\n"
          "       if (diffuse > 0.923) ramp = 1.00;\n"
          "  else if (diffuse > 0.477) ramp = 0.50;\n"
          "  gl_FragColor = vec4(v_color * mix(0.8, 1.6, ramp), 1.0);\n"
          "}\n"
      },
      {
        .dst = &jeux.gl.text.shader,
        .debug_name = "text",
        .vs =
          "attribute vec2 a_pos;\n"
          "attribute vec2 a_uv;\n"
          "attribute float a_size;\n"
          "\n"
          "uniform vec2 u_tex_size;\n"
          "uniform vec2 u_win_size;\n"
          "uniform float u_gamma;\n"
          "\n"
          "varying vec2 v_uv;\n"
          "varying float v_gamma;\n"
          "\n"
          "void main() {\n"
          "  gl_Position = vec4(a_pos / u_win_size, 0.0, 1.0);\n"
          "  gl_Position.xy = gl_Position.xy*2.0 - 1.0;\n"
          "  v_uv = a_uv / u_tex_size;\n"
          "  v_gamma = u_gamma / a_size;\n"
          "}\n"
        ,
        .fs =
          "precision mediump float;\n"
          "\n"
          "varying vec2 v_uv;\n"
          "varying float v_gamma;\n"
          "\n"
          "uniform sampler2D u_tex;\n"
          "uniform float u_buffer;\n"
          "\n"
          "void main() {\n"
          "  float dist = texture2D(u_tex, v_uv).r;\n"
          "  float alpha = smoothstep(u_buffer - v_gamma, u_buffer + v_gamma, dist);\n"
          "  gl_FragColor = vec4(alpha);\n"
          "}\n"
      },
      {
        .dst = &jeux.gl.pp.shader,
        .debug_name = "pp",
        .vs =
          "attribute vec4 a_pos;\n"
          "varying vec2 v_uv;\n"
          "void main() {\n"
          "  gl_Position = vec4(a_pos.xyz, 1.0);\n"
          "  v_uv = gl_Position.xy*0.5 + vec2(0.5);\n"
          "}\n"
        ,
        .fs =

// No AA
#if CURRENT_ALIASING == ANTIALIAS_NONE || CURRENT_ALIASING == ANTIALIAS_LINEAR
          "precision mediump float;\n"
          "varying vec2 v_uv;\n"
          "uniform sampler2D u_tex;\n"
          "uniform vec2 u_win_size;\n"
          "void main() {\n"
          "  gl_FragColor = texture2D(u_tex, v_uv);\n"
#ifdef SRGB
          "  gl_FragColor = vec4(pow(abs(gl_FragColor.xyz), vec3(1.0 / 2.2)), 1);\n"
#endif
          "}\n"
#elif CURRENT_ALIASING == ANTIALIAS_2xSSAA
          "precision mediump float;\n"
          "varying vec2 v_uv;\n"
          "uniform sampler2D u_tex;\n"
          "uniform vec2 u_win_size;\n"
          "void main() {\n"
          "  vec2 inv_vp = 1.0 / u_win_size;\n"
          "  gl_FragColor = vec4(0, 0, 0, 1);\n"
          "  gl_FragColor.xyz += 0.25*texture2D(u_tex, v_uv + (vec2(-0.25, -0.25) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.25*texture2D(u_tex, v_uv + (vec2(+0.25, -0.25) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.25*texture2D(u_tex, v_uv + (vec2(-0.25, +0.25) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.25*texture2D(u_tex, v_uv + (vec2(+0.25, +0.25) * inv_vp)).xyz;\n"
#ifdef SRGB
          "  gl_FragColor = vec4(pow(abs(gl_FragColor.xyz), vec3(1.0 / 2.2)), 1);\n"
#endif
          "}\n"
#elif CURRENT_ALIASING == ANTIALIAS_4xSSAA
          "precision mediump float;\n"
          "varying vec2 v_uv;\n"
          "uniform sampler2D u_tex;\n"
          "uniform vec2 u_win_size;\n"
          "void main() {\n"
          "  vec2 inv_vp = 1.0 / u_win_size;\n"
          "  gl_FragColor = vec4(0, 0, 0, 1);\n"
          "  gl_FragColor.xyz += 0.0625*texture2D(u_tex, v_uv + (vec2(-0.375, -0.375) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.0625*texture2D(u_tex, v_uv + (vec2(-0.375, -0.125) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.0625*texture2D(u_tex, v_uv + (vec2(-0.375,  0.125) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.0625*texture2D(u_tex, v_uv + (vec2(-0.375,  0.375) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.0625*texture2D(u_tex, v_uv + (vec2(-0.125, -0.375) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.0625*texture2D(u_tex, v_uv + (vec2(-0.125, -0.125) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.0625*texture2D(u_tex, v_uv + (vec2(-0.125,  0.125) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.0625*texture2D(u_tex, v_uv + (vec2(-0.125,  0.375) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.0625*texture2D(u_tex, v_uv + (vec2( 0.125, -0.375) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.0625*texture2D(u_tex, v_uv + (vec2( 0.125, -0.125) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.0625*texture2D(u_tex, v_uv + (vec2( 0.125,  0.125) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.0625*texture2D(u_tex, v_uv + (vec2( 0.125,  0.375) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.0625*texture2D(u_tex, v_uv + (vec2( 0.375, -0.375) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.0625*texture2D(u_tex, v_uv + (vec2( 0.375, -0.125) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.0625*texture2D(u_tex, v_uv + (vec2( 0.375,  0.125) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.0625*texture2D(u_tex, v_uv + (vec2( 0.375,  0.375) * inv_vp)).xyz;\n"
#ifdef SRGB
          "  gl_FragColor = vec4(pow(abs(gl_FragColor.xyz), vec3(1.0 / 2.2)), 1);\n"
#endif
          "}\n"
#elif CURRENT_ALIASING == ANTIALIAS_FXAA

          /* TODO: fix this https://github.com/LiveMirror/NVIDIA-Direct3D-SDK-11/blob/a2d3cc46179364c9faa3e218eff230883badcd79/FXAA/FxaaShader.h#L1 */

          "precision mediump float;\n"
          "varying vec2 v_uv;\n"
          "uniform sampler2D u_tex;\n"
          "uniform vec2 u_win_size;\n"
          "void main() {\n"
            "float FXAA_SPAN_MAX = 8.0;\n"
            "float FXAA_REDUCE_MUL = 1.0/8.0;\n"
            "float FXAA_REDUCE_MIN = (1.0/128.0);\n"

            "vec2 inv_vp = 1.0 / u_win_size;\n"
            "vec3 rgbNW = texture2D(u_tex, v_uv + (vec2(-0.5, -0.5) * inv_vp)).xyz;\n"
            "vec3 rgbNE = texture2D(u_tex, v_uv + (vec2(+0.5, -0.5) * inv_vp)).xyz;\n"
            "vec3 rgbSW = texture2D(u_tex, v_uv + (vec2(-0.5, +0.5) * inv_vp)).xyz;\n"
            "vec3 rgbSE = texture2D(u_tex, v_uv + (vec2(+0.5, +0.5) * inv_vp)).xyz;\n"
            "vec3 rgbM  = texture2D(u_tex, v_uv).xyz;\n"

            "vec3 luma = vec3(0.299, 0.587, 0.114);\n"
            "float lumaNW = dot(rgbNW, luma);\n"
            "float lumaNE = dot(rgbNE, luma);\n"
            "float lumaSW = dot(rgbSW, luma);\n"
            "float lumaSE = dot(rgbSE, luma);\n"
            "float lumaM  = dot( rgbM, luma);\n"
          
            "float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));\n"
            "float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));\n"
          
            "vec2 dir;\n"
            "dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));\n"
            "dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));\n"
          
            "float dirReduce = max(\n"
            "  (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),\n"
            "  FXAA_REDUCE_MIN\n"
            ");\n"

            "float rcpDirMin = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);\n"
          
            "dir = min(\n"
            "  vec2(FXAA_SPAN_MAX,  FXAA_SPAN_MAX),\n"
            "  max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX), dir * rcpDirMin)\n"
            ") * inv_vp;\n"

            "vec3 rgbA = (1.0/2.0) * (\n"
            "  texture2D(u_tex, v_uv + dir * (1.0/3.0 - 0.5)).xyz +\n"
            "  texture2D(u_tex, v_uv + dir * (2.0/3.0 - 0.5)).xyz\n"
            ");\n"
            "vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (\n"
            "  texture2D(u_tex, v_uv + dir * (0.0/3.0 - 0.5)).xyz +\n"
            "  texture2D(u_tex, v_uv + dir * (3.0/3.0 - 0.5)).xyz\n"
            ");\n"
            "float lumaB = dot(rgbB, luma);\n"

            "if((lumaB < lumaMin) || (lumaB > lumaMax)){\n"
            "    gl_FragColor.xyz=rgbA;\n"
            "} else {\n"
            "    gl_FragColor.xyz=rgbB;\n"
            "}\n"
            "gl_FragColor.a = 1.0;\n"
          "}\n"
#else
#error "no such aliasing!"
#endif
      }
    };

    for (int shader_index = 0; shader_index < jx_COUNT(shaders); shader_index++) {

      *shaders[shader_index].dst = glCreateProgram();

      GLuint vs_shader = glCreateShader(GL_VERTEX_SHADER);
      glShaderSource(vs_shader, 1, &shaders[shader_index].vs, NULL);
      glCompileShader(vs_shader);

      GLuint fs_shader = glCreateShader(GL_FRAGMENT_SHADER);
      glShaderSource(fs_shader, 1, &shaders[shader_index].fs, NULL);
      glCompileShader(fs_shader);

      for (int i = 0; i < 2; i++) {
        GLuint shader = (i == 0) ? vs_shader : fs_shader;
        char  *s_name = (i == 0) ? "vs"      : "fs";
        GLint compiled;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (compiled != GL_TRUE) {
          GLint log_length;
          glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
          if (log_length > 0) {
            char *log = malloc(log_length);
            glGetShaderInfoLog(shader, log_length, &log_length, log);
            SDL_Log(
              "\n\n%s %s compilation failed:\n\n%s\n",
              shaders[shader_index].debug_name,
              s_name,
              log
            );
            free(log);

            return SDL_APP_FAILURE;
          }
        }
      }

      GLuint dst = *shaders[shader_index].dst;
      glAttachShader(dst, vs_shader);
      glAttachShader(dst, fs_shader);
      glLinkProgram(dst);
      glUseProgram(dst);
    }
  }

  /* fullscreen tri for post-processing */
  {
    /* create vbo, fill it */
    glGenBuffers(1, &jeux.gl.pp.buf_vtx);
    glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.pp.buf_vtx);
    GLfloat vtx[] = {
      -1.0f,  3.0f, 0.0f,
      -1.0f, -1.0f, 0.0f,
       3.0f, -1.0f, 0.0f
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vtx), vtx, GL_STATIC_DRAW);

    jeux.gl.pp.shader_u_win_size = glGetUniformLocation(jeux.gl.pp.shader, "u_win_size");
    jeux.gl.pp.shader_a_pos      = glGetAttribLocation( jeux.gl.pp.shader, "a_pos");
  }

  /* dynamic geometry buffer */
  {
    {
      glGenBuffers(1, &jeux.gl.geo.buf_vtx);
      glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.geo.buf_vtx);
      glBufferData(GL_ARRAY_BUFFER, sizeof(jeux.gl.geo.vtx), NULL, GL_DYNAMIC_DRAW);

      glGenBuffers(1, &jeux.gl.geo.buf_idx);
      glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.geo.buf_idx);
      glBufferData(GL_ARRAY_BUFFER, sizeof(jeux.gl.geo.idx), NULL, GL_DYNAMIC_DRAW);
    }

    {
      size_t vtx_count = jx_COUNT(model_vtx_Horned_Helmet);
      size_t tri_count = jx_COUNT(model_tri_Horned_Helmet);
      gl_geo_Vtx *vtx  =          model_vtx_Horned_Helmet;
      gl_Tri     *tri  =          model_tri_Horned_Helmet;

      jeux.gl.geo.koob_tri_count = tri_count;

      /* right now this gives slightly different results to what comes out of Blender,
       * but if we could get it working perfectly, we could cut down on asset size tremendously */
#if 0
      /* make smooth normals for asset */
      {
        /* add the normalals of each triangle to each vert's normal */
        for (int tri_i = 0; tri_i < tri_count; tri_i++) {
          gl_geo_Vtx *a = vtx + tri[tri_i].a;
          gl_geo_Vtx *b = vtx + tri[tri_i].b;
          gl_geo_Vtx *c = vtx + tri[tri_i].c;
          float edge0_x = b->pos.x - a->pos.x;
          float edge0_y = b->pos.y - a->pos.y;
          float edge0_z = b->pos.z - a->pos.z;
          float edge1_x = c->pos.x - a->pos.x;
          float edge1_y = c->pos.y - a->pos.y;
          float edge1_z = c->pos.z - a->pos.z;
          float normal_x = (edge0_y * edge1_z) - (edge0_z * edge1_y);
          float normal_y = (edge0_z * edge1_x) - (edge0_x * edge1_z);
          float normal_z = (edge0_x * edge1_y) - (edge0_y * edge1_x);
          a->normal.x += normal_x;
          a->normal.y += normal_y;
          a->normal.z += normal_z;
          b->normal.x += normal_x;
          b->normal.y += normal_y;
          b->normal.z += normal_z;
          c->normal.x += normal_x;
          c->normal.y += normal_y;
          c->normal.z += normal_z;
        }

        /* normalalize the normals */
        for (int vtx_i = 0; vtx_i < vtx_count; vtx_i++) {
          gl_geo_Vtx *v = vtx + vtx_i;
          float len = sqrtf(v->normal.x*v->normal.x +
                            v->normal.y*v->normal.y +
                            v->normal.z*v->normal.z);
          v->normal.x /= len;
          v->normal.y /= len;
          v->normal.z /= len;
        }
      }
#endif

      glGenBuffers(1, &jeux.gl.geo.koob_buf_vtx);
      glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.geo.koob_buf_vtx);
      glBufferData(GL_ARRAY_BUFFER, vtx_count * sizeof(gl_geo_Vtx), vtx, GL_STATIC_DRAW);

      glGenBuffers(1, &jeux.gl.geo.koob_buf_idx);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, jeux.gl.geo.koob_buf_idx);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, tri_count * sizeof(gl_Tri), tri, GL_STATIC_DRAW);
    }

    /* shader data layout */
    jeux.gl.geo.shader_u_mvp    = glGetUniformLocation(jeux.gl.geo.shader, "u_mvp"  );
    jeux.gl.geo.shader_a_pos    = glGetAttribLocation (jeux.gl.geo.shader, "a_pos"  );
    jeux.gl.geo.shader_a_color  = glGetAttribLocation (jeux.gl.geo.shader, "a_color");
    jeux.gl.geo.shader_a_normal = glGetAttribLocation (jeux.gl.geo.shader, "a_normal");

#define GEO_VTX_BIND_LAYOUT { \
      glEnableVertexAttribArray(jeux.gl.geo.shader_a_pos); \
      glVertexAttribPointer( \
        jeux.gl.geo.shader_a_pos, \
        3, \
        GL_FLOAT, \
        GL_FALSE, \
        sizeof(gl_geo_Vtx), \
        (void *)offsetof(gl_geo_Vtx, pos) \
      ); \
      glEnableVertexAttribArray(jeux.gl.geo.shader_a_color); \
      glVertexAttribPointer( \
        jeux.gl.geo.shader_a_color, \
        4, \
        GL_UNSIGNED_BYTE, \
        GL_TRUE, \
        sizeof(gl_geo_Vtx), \
        (void *)offsetof(gl_geo_Vtx, color) \
      ); \
      glEnableVertexAttribArray(jeux.gl.geo.shader_a_normal); \
      glVertexAttribPointer( \
        jeux.gl.geo.shader_a_normal, \
        3, \
        GL_FLOAT, \
        GL_FALSE, \
        sizeof(gl_geo_Vtx), \
        (void *)offsetof(gl_geo_Vtx, normal) \
      ); \
    }

  }

  gl_resize();

  /* initialize text rendering */
  {

    /* dynamic text buffer */
    {
      /* create vbo, filled later dynamically */
      {
        glGenBuffers(1, &jeux.gl.text.buf_vtx);
        glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.text.buf_vtx);
        glBufferData(GL_ARRAY_BUFFER, sizeof(jeux.gl.text.vtx), NULL, GL_DYNAMIC_DRAW);
      }

      {
        glGenBuffers(1, &jeux.gl.text.buf_idx);
        glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.text.buf_idx);
        glBufferData(GL_ARRAY_BUFFER, sizeof(jeux.gl.text.idx), NULL, GL_DYNAMIC_DRAW);
      }

      jeux.gl.text.shader_u_buffer   = glGetUniformLocation(jeux.gl.text.shader, "u_buffer"  );
      jeux.gl.text.shader_u_gamma    = glGetUniformLocation(jeux.gl.text.shader, "u_gamma"   );
      jeux.gl.text.shader_u_tex_size = glGetUniformLocation(jeux.gl.text.shader, "u_tex_size");
      jeux.gl.text.shader_u_win_size = glGetUniformLocation(jeux.gl.text.shader, "u_win_size");
      jeux.gl.text.shader_a_pos      = glGetAttribLocation( jeux.gl.text.shader, "a_pos"     );
      jeux.gl.text.shader_a_uv       = glGetAttribLocation( jeux.gl.text.shader, "a_uv"      );
      jeux.gl.text.shader_a_size     = glGetAttribLocation( jeux.gl.text.shader, "a_size"    );
    }

    /* create texture - writes to jeux.gl.tex */
    {
       glGenTextures(1, &jeux.gl.text.tex);
       glBindTexture(GL_TEXTURE_2D, jeux.gl.text.tex);
       glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
       glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
       glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
       glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

       uint8_t data[font_TEX_SIZE_X * font_TEX_SIZE_Y * 4] = {0};
       for (int letter_idx = 0; letter_idx < jx_COUNT(font_letter_regions); letter_idx++) {
         font_LetterRegion *lr = font_letter_regions + letter_idx;

         size_t i = lr->data_start;
         for (int pixel_y = 0; pixel_y < lr->size_y; pixel_y++)
           for (int pixel_x = 0; pixel_x < lr->size_x; pixel_x++) {
             size_t x = lr->x + pixel_x;
             size_t y = lr->y + pixel_y;
             data[font_TEX_SIZE_X*y + x] = font_tex_bytes[i++];
           }
       }

       glTexImage2D(
         /* GLenum  target         */ GL_TEXTURE_2D,
         /* GLint   level          */ 0,
         /* GLint   internalFormat */ GL_R8,
         /* GLsizei width          */ font_TEX_SIZE_X,
         /* GLsizei height         */ font_TEX_SIZE_Y,
         /* GLint   border         */ 0,
         /* GLenum  format         */ GL_RED,
         /* GLenum  type           */ GL_UNSIGNED_BYTE,
         /* const void *data       */ data
       );
    }
  }

  return SDL_APP_CONTINUE;
}

/* recreates jeux.gl.pp.screen resources to match new jeux.win_size */
static void gl_resize(void) {
  jeux.screen = f4x4_ortho(
       0.0f, jeux.win_size_x,
       0.0f, jeux.win_size_y,
      -1.0f, 1.0f
  );

  jeux.gl.pp.phys_win_size_x = jeux.win_size_x*SDL_GetWindowPixelDensity(jeux.sdl.window);
  jeux.gl.pp.phys_win_size_y = jeux.win_size_y*SDL_GetWindowPixelDensity(jeux.sdl.window);

  /* passing in zero is ignored here, so this doesn't throw an error if screen has never inited */
  glDeleteFramebuffers(1, &jeux.gl.pp.screen.pp_fb);
  glDeleteTextures(1, &jeux.gl.pp.screen.pp_tex_color);

  /* create postprocessing framebuffer - writes to jeux.gl.pp.screen.pp_tex_color, jeux.gl.pp.screen.pp_fb */
  {
    glGenFramebuffers(1, &jeux.gl.pp.screen.pp_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, jeux.gl.pp.screen.pp_fb);

    /* create pp_tex_color */
    {
      glGenTextures(1, &jeux.gl.pp.screen.pp_tex_color);
      glBindTexture(GL_TEXTURE_2D, jeux.gl.pp.screen.pp_tex_color);
#if CURRENT_ALIASING == ANTIALIAS_LINEAR || CURRENT_ALIASING == ANTIALIAS_FXAA
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#else
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
#endif
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

      glTexImage2D(
        /* GLenum  target         */ GL_TEXTURE_2D,
        /* GLint   level          */ 0,
        /* GLint   internalFormat */ GL_RGBA,
        /* GLsizei width          */ jeux.gl.pp.phys_win_size_x*jeux.gl.pp.fb_scale,
        /* GLsizei height         */ jeux.gl.pp.phys_win_size_y*jeux.gl.pp.fb_scale,
        /* GLint   border         */ 0,
        /* GLenum  format         */ GL_RGBA,
        /* GLenum  type           */ GL_UNSIGNED_BYTE,
        /* const void *data       */ 0
      );

      glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        jeux.gl.pp.screen.pp_tex_color,
        0
      );
    }

    /* create pp_tex_depth */
    {
      glGenTextures(1, &jeux.gl.pp.screen.pp_tex_depth);
      glBindTexture(GL_TEXTURE_2D, jeux.gl.pp.screen.pp_tex_depth);

      glTexImage2D(
        /* GLenum  target         */ GL_TEXTURE_2D,
        /* GLint   level          */ 0,
        /* GLint   internalFormat */ GL_DEPTH_COMPONENT24,
        /* GLsizei width          */ jeux.gl.pp.phys_win_size_x*jeux.gl.pp.fb_scale,
        /* GLsizei height         */ jeux.gl.pp.phys_win_size_y*jeux.gl.pp.fb_scale,
        /* GLint   border         */ 0,
        /* GLenum  format         */ GL_DEPTH_COMPONENT,
        /* GLenum  type           */ GL_UNSIGNED_INT,
        /* const void *data       */ 0
      );

      glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D,
        jeux.gl.pp.screen.pp_tex_depth,
        0
      );

    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      SDL_Log("couldn't make render buffer: %x", status);
    }
  }
}

static void gl_text_reset(void) {
  jeux.gl.text.vtx_wtr = jeux.gl.text.vtx;
  jeux.gl.text.idx_wtr = jeux.gl.text.idx;
}

static void gl_text_draw(const char *msg, float screen_x, float screen_y, float size) {
  gl_text_Vtx *vtx_wtr = jeux.gl.text.vtx_wtr;
  gl_Tri      *idx_wtr = jeux.gl.text.idx_wtr;

  size *= SDL_GetWindowPixelDensity(jeux.sdl.window);
  float scale = (size / font_BASE_CHAR_SIZE);

  float pen_x = screen_x;
  float pen_y = screen_y;
  do {
    font_LetterRegion *l = &font_letter_regions[(size_t)(*msg)];

    uint16_t start = vtx_wtr - jeux.gl.text.vtx;

    float x = pen_x;
    float y = pen_y + l->top * scale;
    float size_x = l->size_x * scale;
    float size_y = l->size_y * scale;
    *vtx_wtr++ = (gl_text_Vtx) { x + size_x,  y - size_y, l->x + l->size_x, l->y + l->size_y, size };
    *vtx_wtr++ = (gl_text_Vtx) { x + size_x,  y         , l->x + l->size_x, l->y            , size };
    *vtx_wtr++ = (gl_text_Vtx) { x         ,  y         , l->x            , l->y            , size };
    *vtx_wtr++ = (gl_text_Vtx) { x         ,  y - size_y, l->x            , l->y + l->size_y, size };

    *idx_wtr++ = (gl_Tri) { start + 0, start + 1, start + 2 };
    *idx_wtr++ = (gl_Tri) { start + 2, start + 3, start + 0 };

    pen_x += l->advance * scale;
  } while (*msg++);

  jeux.gl.text.vtx_wtr = vtx_wtr;
  jeux.gl.text.idx_wtr = idx_wtr;
}

static void gl_geo_reset(void) {
  jeux.gl.geo.vtx_wtr = jeux.gl.geo.vtx;
  jeux.gl.geo.idx_wtr = jeux.gl.geo.idx;
}

static void gl_geo_line(f3 a, f3 b, float thickness) {
  float dx = a.x - b.x;
  float dy = a.y - b.y;
  float dlen = dx*dx + dy*dy;
  if (dlen <= 0) return;
  dlen = sqrtf(dlen);
  float nx = -dy / dlen * thickness*0.5;
  float ny =  dx / dlen * thickness*0.5;

  uint16_t start = jeux.gl.geo.vtx_wtr - jeux.gl.geo.vtx;

  *jeux.gl.geo.vtx_wtr++ = (gl_geo_Vtx) { { a.x + nx, a.y + ny, a.z }, { 200, 80, 20 } };
  *jeux.gl.geo.vtx_wtr++ = (gl_geo_Vtx) { { a.x - nx, a.y - ny, a.z }, { 200, 80, 20 } };
  *jeux.gl.geo.vtx_wtr++ = (gl_geo_Vtx) { { b.x + nx, b.y + ny, b.z }, { 200, 80, 20 } };
  *jeux.gl.geo.vtx_wtr++ = (gl_geo_Vtx) { { b.x - nx, b.y - ny, b.z }, { 200, 80, 20 } };

  *jeux.gl.geo.idx_wtr++ = (gl_Tri) { start + 0, start + 1, start + 2 };
  *jeux.gl.geo.idx_wtr++ = (gl_Tri) { start + 2, start + 1, start + 3 };
}

static f3 jeux_world_to_screen(f3 pt) {
  f4 p = { .p = { pt.x, pt.y, pt.z, 1.0f } };

  p = f4x4_mul_f4(jeux.camera, p);
  /* a is in -1 .. 1 now */
  p = f4x4_mul_f4(f4x4_invert(jeux.screen), p);

  return p.p3;
}

static void gl_geo_box_outline(f3 p, f3 scale, float thickness) {

  for (float dir_x = 0; dir_x < 2; dir_x++) {
    for (float dir_y = -1; dir_y <= 1; dir_y += 2) {
      for (int axis_i = 0; axis_i < 3; axis_i++) {
        f4 a = {0}, b = {0};
        a.arr[axis_i] = -1;
        b.arr[axis_i] =  1;

        for (int i = 0; i < 3; i++) {
          if (i == axis_i) continue;
          a.arr[i] = dir_y;
          b.arr[i] = dir_y;
        }
        if (dir_x) a.arr[(axis_i+1)%3] *= -1;
        if (dir_x) b.arr[(axis_i+1)%3] *= -1;

        gl_geo_line(jeux_world_to_screen(a.p3), jeux_world_to_screen(b.p3), 2.0f);
      }
    }
  }
}

SDL_AppResult SDL_AppIterate(void *appstate) {
  /* timekeeping */
  {
    uint64_t ts_now = SDL_GetPerformanceCounter();
    // double delta_time = (double)(ts_now - jeux.ts_last_frame) / (double)SDL_GetPerformanceFrequency();
    jeux.ts_last_frame = ts_now;

    jeux.elapsed = (double)(ts_now - jeux.ts_first) / (double)SDL_GetPerformanceFrequency();
  }
  
  /* construct the jeux.camera for this frame */
  {
    /* camera */
    jeux.camera = (f4x4) {0};
    {
      float ar = (float)(jeux.win_size_y) / (float)(jeux.win_size_x);
      f4x4 projection = f4x4_ortho(
         -5.0f     ,  5.0f     ,
         -5.0f * ar,  5.0f * ar,
        -20.0f     , 20.0f
      );

      float x = cosf(jeux.elapsed * 0.5f);
      float y = sinf(jeux.elapsed * 0.5f);
      f4x4 orbit = f4x4_target_to(
        (f3) {    x,    y, 1.2f },
        (f3) { 0.0f, 0.0f, 0.5f },
        (f3) { 0.0f, 0.0f, 1.0f }
      );
      jeux.camera = f4x4_mulf4x4(projection, f4x4_invert(orbit));
    }
  }

  /* fill dynamic geometry buffers */
  {
    gl_geo_reset();
    gl_text_reset();

    gl_geo_box_outline((f3) { 0, 0, 0 },
                       (f3) { 1, 1, 1 },
                       2.0f);

    if (0) gl_text_draw(
      "hi! i'm ced?",
      jeux.win_size_x * 0.5,
      jeux.win_size_y * 0.5,
      24.0f
    );
  }

  /* render */
  {

    {
      /* switch to the fb that gets postprocessing applied later */
      glViewport(0, 0, jeux.gl.pp.phys_win_size_x*jeux.gl.pp.fb_scale, jeux.gl.pp.phys_win_size_y*jeux.gl.pp.fb_scale);
      glBindFramebuffer(GL_FRAMEBUFFER, jeux.gl.pp.screen.pp_fb);

      /* clear color */
      glClearColor(0.047f, 0.047f, 0.047f, 1.0f);

      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_LEQUAL);

      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      {
        glUseProgram(jeux.gl.geo.shader);

        /* draw dynamic, generated per-frame geo content */
        {
          /* upload data into dynamic buffers */
          {
            gl_geo_Vtx *vtx = jeux.gl.geo.vtx;
            gl_Tri     *idx = jeux.gl.geo.idx;

            {
              glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.geo.buf_vtx);
              size_t len = jeux.gl.geo.vtx_wtr - vtx;
              glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vtx[0]) * len, vtx);
            }

            {
              glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, jeux.gl.geo.buf_idx);
              size_t len = jeux.gl.geo.idx_wtr - idx;
              glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(idx[0]) * len, idx);
            }
          }

          GEO_VTX_BIND_LAYOUT;

          glUniformMatrix4fv(jeux.gl.geo.shader_u_mvp, 1, 0, jeux.screen.floats);

          glDrawElements(GL_TRIANGLES, 3*(jeux.gl.geo.idx_wtr - jeux.gl.geo.idx), GL_UNSIGNED_SHORT, 0);
        }

        /* draw static geo content */
        {
          glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.geo.koob_buf_vtx);
          glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, jeux.gl.geo.koob_buf_idx);

          GEO_VTX_BIND_LAYOUT;

          glUniformMatrix4fv(jeux.gl.geo.shader_u_mvp, 1, 0, jeux.camera.floats);

          glDrawElements(GL_TRIANGLES, 3 * jeux.gl.geo.koob_tri_count, GL_UNSIGNED_SHORT, 0);
        }

      }

    }

    /* stop writing to the framebuffer, start writing to the screen */
    glViewport(0, 0, jeux.gl.pp.phys_win_size_x, jeux.gl.pp.phys_win_size_y);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    /* draw the contents of the framebuffer with postprocessing/aa applied */
    {
      glUseProgram(jeux.gl.pp.shader);
      glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.pp.buf_vtx);
      glEnableVertexAttribArray(jeux.gl.pp.shader_a_pos);
      glVertexAttribPointer(jeux.gl.pp.shader_a_pos, 3, GL_FLOAT, GL_FALSE, 0, 0);

      glBindTexture(GL_TEXTURE_2D, jeux.gl.pp.screen.pp_tex_color);
      glUniform2f(jeux.gl.pp.shader_u_win_size, jeux.gl.pp.phys_win_size_x, jeux.gl.pp.phys_win_size_y);
      glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    /* draw text (after pp because it has its own AA) */
    {
      glUseProgram(jeux.gl.text.shader);

      /* update VBO contents */
      {
        gl_text_Vtx *vtx = jeux.gl.text.vtx;
        gl_Tri      *idx = jeux.gl.text.idx;

        {
          glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.text.buf_vtx);
          size_t len = jeux.gl.text.vtx_wtr - vtx;
          glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vtx[0]) * len, vtx);
        }

        {
          glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, jeux.gl.text.buf_idx);
          size_t len = jeux.gl.text.idx_wtr - idx;
          glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(idx[0]) * len, idx);
        }
      }

      {
        size_t size = sizeof(gl_text_Vtx);

        glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.text.buf_vtx);
        glEnableVertexAttribArray(jeux.gl.text.shader_a_pos);
        glVertexAttribPointer(jeux.gl.text.shader_a_pos, 2, GL_FLOAT, GL_FALSE, size, (void *)offsetof(gl_text_Vtx, x));

        glEnableVertexAttribArray(jeux.gl.text.shader_a_uv);
        glVertexAttribPointer(jeux.gl.text.shader_a_uv, 2, GL_FLOAT, GL_FALSE, size, (void *)offsetof(gl_text_Vtx, u));

        glEnableVertexAttribArray(jeux.gl.text.shader_a_size);
        glVertexAttribPointer(jeux.gl.text.shader_a_size, 1, GL_FLOAT, GL_FALSE, size, (void *)offsetof(gl_text_Vtx, size));
      }

      glBindTexture(GL_TEXTURE_2D, jeux.gl.text.tex);
      glUniform2f(jeux.gl.text.shader_u_tex_size, font_TEX_SIZE_X, font_TEX_SIZE_Y);
      glUniform2f(jeux.gl.text.shader_u_win_size, jeux.gl.pp.phys_win_size_x, jeux.gl.pp.phys_win_size_y);
      glUniform1f(jeux.gl.text.shader_u_buffer, 0.725);

      /* set up premultiplied alpha */
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      glEnable(GL_BLEND);

      float gamma = 2.0;
      glUniform1f(jeux.gl.text.shader_u_gamma, gamma * 1.4142);
      glDrawElements(GL_TRIANGLES, 3 * (jeux.gl.text.idx_wtr - jeux.gl.text.idx), GL_UNSIGNED_SHORT, 0);

      glDisable(GL_BLEND);
    }
  }

  SDL_GL_SwapWindow(jeux.sdl.window);
  return SDL_APP_CONTINUE;
}
