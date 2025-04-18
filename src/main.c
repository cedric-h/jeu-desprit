// vim: sw=2 ts=2 expandtab smartindent

#include <stdlib.h>
#include <stddef.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include "gl_include/gles3.h"

typedef struct { float x, y; } f2;
typedef struct { float x, y, z; } f3;
typedef union { float arr[4]; struct { float x, y, z, w; } p; f3 xyz; } f4;
typedef union { float arr[4][4]; f4 rows[4]; float floats[16]; } f4x4;

#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "font.h"

#define GAME_DEBUG true

#define BREAKPOINT() __builtin_debugtrap()
#define jx_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef struct { f2 min, max; } Box2;
#define BOX2_UNCONSTRAINED (Box2) { { -INFINITY, -INFINITY }, {  INFINITY,  INFINITY } }
#define BOX2_CLOSED        (Box2) { {  INFINITY,  INFINITY }, { -INFINITY, -INFINITY } }

static float lerp(float v0, float v1, float t) { return (1.0f - t) * v0 + t * v1; }
static float clamp(float min, float max, float t) { return fminf(max, fmaxf(min, t)); }
static float inv_lerp(float min, float max, float p) { return (p - min) / (max - min); }

static f3 lerp3(f3 a, f3 b, float t) {
  return (f3) {
    .x = lerp(a.x, b.x, t),
    .y = lerp(a.y, b.y, t),
    .z = lerp(a.z, b.z, t)
  };
}

static f3 ray_hit_plane(f3 ray_origin, f3 ray_vector, f3 plane_origin, f3 plane_vector) {
  float delta_x = plane_origin.x - ray_origin.x;
  float delta_y = plane_origin.y - ray_origin.y;
  float delta_z = plane_origin.z - ray_origin.z;

  float ldot = delta_x*plane_vector.x +
               delta_y*plane_vector.y +
               delta_z*plane_vector.z ;

  float rdot = ray_vector.x*plane_vector.x +
               ray_vector.y*plane_vector.y +
               ray_vector.z*plane_vector.z ;

  float d = ldot / rdot;
  return (f3) {
    ray_origin.x + ray_vector.x * d,
    ray_origin.y + ray_vector.y * d,
    ray_origin.z + ray_vector.z * d,
  };
}

static f4x4 f4x4_ortho(float left, float right, float bottom, float top, float near, float far) {
    f4x4 res = {0};

    res.arr[0][0] = 2.0f / (right - left);
    res.arr[1][1] = 2.0f / (top - bottom);
    res.arr[2][2] = 2.0f / (far - near);
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

static f4x4 f4x4_mul_f4x4(f4x4 a, f4x4 b) {
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
  f4 res = {0};
  for (int x = 0; x < 4; x++) {
    float sum = 0;
    for (int y = 0; y < 4; y++)
      sum += m.arr[y][x] * v.arr[y];

    res.arr[x] = sum;
  }
  return res;
}

static f3 f4x4_transform_f3(f4x4 m, f3 v) {
  f4 res = { .xyz = v };
  res.p.w = 1.0f;
  res = f4x4_mul_f4(m, res);
  res.p.x /= res.p.w;
  res.p.y /= res.p.w;
  res.p.z /= res.p.w;
  return res.xyz;
}

static f4x4 f4x4_scale(float scale) {
  f4x4 res = {0};
  res.arr[0][0] = scale;
  res.arr[1][1] = scale;
  res.arr[2][2] = scale;
  res.arr[3][3] = 1.0f;
  return res;
}

static f4x4 f4x4_scale3(f3 scale) {
  f4x4 res = {0};
  res.arr[0][0] = scale.x;
  res.arr[1][1] = scale.y;
  res.arr[2][2] = scale.z;
  res.arr[3][3] = 1.0f;
  return res;
}

/* 2D rotation around the Z axis */
static f4x4 f4x4_turn(float radians) {
  f4x4 res = {0};

  res.arr[0][0] = cosf(radians);
  res.arr[0][1] = sinf(radians);

  res.arr[1][0] = -res.arr[0][1];
  res.arr[1][1] =  res.arr[0][0];

  res.arr[2][2] = 1.0f;
  res.arr[3][3] = 1.0f;
  return res;
}

static f4x4 f4x4_move(f3 pos) {
  f4x4 res = {0};
  res.arr[0][0] = 1.0f;
  res.arr[1][1] = 1.0f;
  res.arr[2][2] = 1.0f;
  res.arr[3][0] = pos.x;
  res.arr[3][1] = pos.y;
  res.arr[3][2] = pos.z;
  res.arr[3][3] = 1.0f;
  return res;
}


#define ANTIALIAS_NONE    0
#define ANTIALIAS_LINEAR  1
#define ANTIALIAS_FXAA    2
#define ANTIALIAS_2xSSAA  3
#define ANTIALIAS_4xSSAA  4
#define CURRENT_ALIASING ANTIALIAS_4xSSAA
#define SRGB

typedef struct { uint8_t r, g, b, a; } Color;
typedef struct { float x, y, z, u, v, size; Color color;} gl_text_Vtx;
typedef struct {
  f3 pos;
  Color color;
  f3 normal;
} gl_geo_Vtx;
typedef struct { uint16_t a, b, c; } gl_Tri;

typedef enum {
  gl_Model_Head,
  gl_Model_HornedHelmet,
  gl_Model_IntroGravestoneTerrain,

  gl_Model_UiOptions,
  gl_Model_UiArrowButton,
  gl_Model_UiCheck,
  gl_Model_UiCheckBox,
  gl_Model_UiEcksButton,
  gl_Model_UiSliderBody,
  gl_Model_UiSliderHandle,
  gl_Model_UiWindowCorner,
  gl_Model_UiWindowTop,
  gl_Model_UiWindowBorder,
  gl_Model_UiWindowBg,

  gl_Model_COUNT,
} gl_Model;

#include "../models/include/Head.h"
#include "../models/include/HornedHelmet.h"
#include "../models/include/IntroGravestoneTerrain.h"
#include "../svg/include/UiOptions.svg.h"
#include "../svg/include/UiArrowButton.svg.h"
#include "../svg/include/UiCheck.svg.h"
#include "../svg/include/UiCheckBox.svg.h"
#include "../svg/include/UiEcksButton.svg.h"
#include "../svg/include/UiSliderBody.svg.h"
#include "../svg/include/UiSliderHandle.svg.h"
#include "../svg/include/UiWindowCorner.svg.h"
#include "../svg/include/UiWindowTop.svg.h"
#include "../svg/include/UiWindowBorder.svg.h"
#include "../svg/include/UiWindowBg.svg.h"
#include "walk.h"

struct {
  gl_geo_Vtx *vtx;
  gl_Tri *tri;
  size_t vtx_count;
  size_t tri_count;
} gl_modeldata[gl_Model_COUNT] = {
  [gl_Model_Head] = {
      .vtx = model_vtx_Head, .vtx_count = jx_COUNT(model_vtx_Head),
      .tri = model_tri_Head, .tri_count = jx_COUNT(model_tri_Head),
  },
  [gl_Model_HornedHelmet] = {
      .vtx = model_vtx_HornedHelmet, .vtx_count = jx_COUNT(model_vtx_HornedHelmet),
      .tri = model_tri_HornedHelmet, .tri_count = jx_COUNT(model_tri_HornedHelmet),
  },
  [gl_Model_IntroGravestoneTerrain] = {
      .vtx = model_vtx_IntroGravestoneTerrain, .vtx_count = jx_COUNT(model_vtx_IntroGravestoneTerrain),
      .tri = model_tri_IntroGravestoneTerrain, .tri_count = jx_COUNT(model_tri_IntroGravestoneTerrain),
  },

  /* TODO: In retrospect, the asset cookers really should output these sized fat pointer structs. You live and you learn. */
  [gl_Model_UiOptions     ] = { .vtx = model_vtx_UiOptions     , .vtx_count = jx_COUNT(model_vtx_UiOptions     ), .tri = model_tri_UiOptions     , .tri_count = jx_COUNT(model_tri_UiOptions     ) },
  [gl_Model_UiArrowButton ] = { .vtx = model_vtx_UiArrowButton , .vtx_count = jx_COUNT(model_vtx_UiArrowButton ), .tri = model_tri_UiArrowButton , .tri_count = jx_COUNT(model_tri_UiArrowButton ) },
  [gl_Model_UiCheck       ] = { .vtx = model_vtx_UiCheck       , .vtx_count = jx_COUNT(model_vtx_UiCheck       ), .tri = model_tri_UiCheck       , .tri_count = jx_COUNT(model_tri_UiCheck       ) },
  [gl_Model_UiCheckBox    ] = { .vtx = model_vtx_UiCheckBox    , .vtx_count = jx_COUNT(model_vtx_UiCheckBox    ), .tri = model_tri_UiCheckBox    , .tri_count = jx_COUNT(model_tri_UiCheckBox    ) },
  [gl_Model_UiEcksButton  ] = { .vtx = model_vtx_UiEcksButton  , .vtx_count = jx_COUNT(model_vtx_UiEcksButton  ), .tri = model_tri_UiEcksButton  , .tri_count = jx_COUNT(model_tri_UiEcksButton  ) },
  [gl_Model_UiSliderBody  ] = { .vtx = model_vtx_UiSliderBody  , .vtx_count = jx_COUNT(model_vtx_UiSliderBody  ), .tri = model_tri_UiSliderBody  , .tri_count = jx_COUNT(model_tri_UiSliderBody  ) },
  [gl_Model_UiSliderHandle] = { .vtx = model_vtx_UiSliderHandle, .vtx_count = jx_COUNT(model_vtx_UiSliderHandle), .tri = model_tri_UiSliderHandle, .tri_count = jx_COUNT(model_tri_UiSliderHandle) },
  [gl_Model_UiWindowCorner] = { .vtx = model_vtx_UiWindowCorner, .vtx_count = jx_COUNT(model_vtx_UiWindowCorner), .tri = model_tri_UiWindowCorner, .tri_count = jx_COUNT(model_tri_UiWindowCorner) },
  [gl_Model_UiWindowTop   ] = { .vtx = model_vtx_UiWindowTop   , .vtx_count = jx_COUNT(model_vtx_UiWindowTop   ), .tri = model_tri_UiWindowTop   , .tri_count = jx_COUNT(model_tri_UiWindowTop   ) },
  [gl_Model_UiWindowBorder] = { .vtx = model_vtx_UiWindowBorder, .vtx_count = jx_COUNT(model_vtx_UiWindowBorder), .tri = model_tri_UiWindowBorder, .tri_count = jx_COUNT(model_tri_UiWindowBorder) },
  [gl_Model_UiWindowBg    ] = { .vtx = model_vtx_UiWindowBg    , .vtx_count = jx_COUNT(model_vtx_UiWindowBg    ), .tri = model_tri_UiWindowBg    , .tri_count = jx_COUNT(model_tri_UiWindowBg    ) },
};

typedef struct {
  gl_Model model;
  /* just a model matrix, (view and projection get applied for you) */
  f4x4 matrix;
  /* doesn't premultiply in camera matrix for you */
  bool two_dee;
} gl_ModelDraw;

static struct {
  struct {
    SDL_Window    *window;
    SDL_GLContext  gl_ctx;
  } sdl;

  /* input */
  size_t win_size_x, win_size_y;
  float mouse_screen_x, mouse_screen_y, mouse_lmb_down;
  /* mouse projected onto the ground plane at z=0
   * used for aiming weapons, picking up/dropping things from inventory etc. */
  f3 mouse_ground;

  /* timekeeping */
  uint64_t ts_last_frame, ts_first;
  double elapsed;

  /* camera goes from 3D world -> 2D in -1 .. 1
   *
   * screen goes from 2D in 0 ... window_size_x/window_size_y to -1 ... 1,
   * useful for laying things out in pixels, mouse picking etc.
   */
  f4x4 camera, screen;
  f3 camera_eye;

  struct {
    struct {
      gl_geo_Vtx vtx[9999];
      gl_geo_Vtx *vtx_wtr;

      gl_Tri idx[9999];
      gl_Tri *idx_wtr;

      GLuint buf_vtx;
      GLuint buf_idx;

      /* Dynamic, per-frame geometry like lines and such are simply written
       * directly into their buffers and subsequently rendered in a single call.
       *
       * Static geometry (models made in Blender), rather than being drawn directly,
       * are requested to be drawn through the model_draws queue. This allows us to
       * handle instancing, sorting etc. in a pass immediately before rendering. */
      gl_ModelDraw  model_draws[999];
      gl_ModelDraw* model_draws_wtr;
      struct {
        GLuint buf_vtx;
        GLuint buf_idx;
        size_t tri_count;
      } static_models[gl_Model_COUNT];

      GLuint shader;
      GLint shader_a_pos;
      GLint shader_a_color;
      GLint shader_a_normal;
      GLint shader_u_mvp;
    } geo;

    /* pp is "post processing" - used for AA and FX */
    struct {
      float fb_scale;

      /* jeux.win_size * SDL_GetWindowPixelDensity() */
      float phys_win_size_x, phys_win_size_y;

      GLuint buf_vtx;

      GLuint shader; /* postprocessing */
      GLint shader_u_win_size;
      GLint shader_u_tex_color;
      GLint shader_u_tex_depth;
      GLint shader_a_pos;

      /* resources inside here need to be recreated
       * when the application window is resized. */
      struct {
        /* postprocessing framebuffer (anti-aliasing and other fx) */
        GLuint pp_tex_color, pp_tex_depth, pp_fb;
      } screen;
    } pp;

    struct {
      gl_text_Vtx vtx[9999];
      gl_text_Vtx *vtx_wtr;

      gl_Tri idx[9999];
      gl_Tri *idx_wtr;

      GLuint buf_vtx;
      GLuint buf_idx;

      GLuint tex;

      GLuint shader;
      GLint shader_u_tex_size;
      GLint shader_u_mvp;
      GLint shader_u_buffer;
      GLint shader_u_gamma;
      GLint shader_a_pos;
      GLint shader_a_uv;
      GLint shader_a_size;
      GLint shader_a_color;
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

/* these are useful for rendering, picking etc. */
static f3 jeux_world_to_screen(f3 p) {
  p = f4x4_transform_f3(jeux.camera, p);
  p = f4x4_transform_f3(f4x4_invert(jeux.screen), p);
  return p;
}
static f3 jeux_screen_to_world(f3 p) {
  p = f4x4_transform_f3(jeux.screen, p);
  p = f4x4_transform_f3(f4x4_invert(jeux.camera), p);
  return p;
}

static SDL_AppResult gl_init(void);
static void gui_handle_errors(Clay_ErrorData error_data) { SDL_Log("%s\n", error_data.errorText.chars); }
static Clay_Dimensions gui_measure_text(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
  float size = config->fontSize;
  float scale = (size / font_BASE_CHAR_SIZE);

  float size_x = 0;
  float size_y = 0;
  for (int i = 0; i < text.length; i++) {
    char c = text.chars[i] | (1 << 5); /* this is a caps-only font, so atlas only has lowercase */
    font_LetterRegion *l = &font_letter_regions[(size_t)(c)];

    /* for the last character, don't add its advance - we aren't writing more after it */
    size_x += ((i == (text.length-1)) ? l->size_x : l->advance) * scale;
    size_y = fmaxf(size_y, l->size_y * scale);
  }

  return (Clay_Dimensions) { size_x, size_y };
}
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

  /* gui init */
  {
    Clay_Initialize(
      (Clay_Arena) {
        .memory = SDL_malloc(Clay_MinMemorySize()),
        .capacity = Clay_MinMemorySize()
      },
      (Clay_Dimensions) { jeux.win_size_x, jeux.win_size_y },
      (Clay_ErrorHandler) { gui_handle_errors }
    );
    Clay_SetMeasureTextFunction(gui_measure_text, NULL);
  }

  return gl_init();
}

static void gl_resize(void);
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  /* window lifecycle */
  {
    if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;

    if (event->type == SDL_EVENT_WINDOW_RESIZED) {
      jeux.win_size_x = event->window.data1;
      jeux.win_size_y = event->window.data2;
      gl_resize();

      Clay_SetLayoutDimensions(
        (Clay_Dimensions) {
          (float) event->window.data1,
          (float) event->window.data2
        }
      );
    }
  }

  /* mouse/keyboard input */
  {
    if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
      jeux.mouse_lmb_down = !(event->button.button == SDL_BUTTON_LEFT);
    }
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
      jeux.mouse_lmb_down = event->button.button == SDL_BUTTON_LEFT;
      // jeux.mouse_lmb_down_x = event->button.x;
      // jeux.mouse_lmb_down_y = event->button.y;
    }
    if (event->type == SDL_EVENT_MOUSE_MOTION) {
      jeux.mouse_screen_x = event->motion.x;
      jeux.mouse_screen_y = event->motion.y;
    }
    if (event->type == SDL_EVENT_MOUSE_WHEEL) {
      Clay_UpdateScrollContainers(true, (Clay_Vector2) { event->wheel.x, event->wheel.y }, 0.01f);
    }
    if (event->type == SDL_EVENT_KEY_UP) {
#if GAME_DEBUG
      if (event->key.key == SDLK_ESCAPE) {
        return SDL_APP_SUCCESS;
      }
#endif
    }
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
          "varying vec4 v_color;\n"
          "varying vec3 v_normal;\n"
          "\n"
          "void main() {\n"
          "  gl_Position = u_mvp * vec4(a_pos.xyz, 1.0);\n"
          "  v_color  = a_color;\n"
          "  v_normal = a_normal;\n"
          "}\n"
        ,
        .fs =
          "precision mediump float;\n"
          "\n"
          "varying vec4 v_color;\n"
          "varying vec3 v_normal;\n"
          "\n"
          "uniform sampler2D u_tex;\n"
          "\n"
          "void main() {\n"
          /* debug normals */
          // "  gl_FragColor = vec4(mix(vec3(1), v_normal, 0.5), 1.0);\n"

          // use unchanged color if no normals at all */
          "  if (abs(dot(v_normal, vec3(1))) < 0.00001) { gl_FragColor = v_color; return; }\n"

          "  vec3 light_dir = normalize(vec3(4.0, 1.5, 5.9));\n"
          "  float diffuse = max(dot(v_normal, light_dir), 0.0);\n"
          "  float ramp = 0.0;\n"
          "       if (diffuse > 0.923) ramp = 1.00;\n"
          "  else if (diffuse > 0.477) ramp = 0.50;\n"
          "  gl_FragColor = vec4(v_color.xyz * mix(0.8, 1.6, ramp), v_color.a);\n"
          "}\n"
      },
      {
        .dst = &jeux.gl.text.shader,
        .debug_name = "text",
        .vs =
          "attribute vec3 a_pos;\n"
          "attribute vec2 a_uv;\n"
          "attribute float a_size;\n"
          "attribute vec4 a_color;\n"
          "\n"
          "uniform mat4 u_mvp;\n"
          "uniform vec2 u_tex_size;\n"
          "uniform float u_gamma;\n"
          "\n"
          "varying vec2 v_uv;\n"
          "varying float v_gamma;\n"
          "varying vec4 v_color;\n"
          "\n"
          "void main() {\n"
          "  gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
          "  v_color = a_color;\n"
          "  v_uv = a_uv / u_tex_size;\n"
          "  v_gamma = u_gamma / a_size;\n"
          "}\n"
        ,
        .fs =
          "precision mediump float;\n"
          "\n"
          "varying vec2 v_uv;\n"
          "varying float v_gamma;\n"
          "varying vec4 v_color;\n"
          "\n"
          "uniform sampler2D u_tex;\n"
          "uniform float u_buffer;\n"
          "\n"
          "void main() {\n"
          "  float dist = texture2D(u_tex, v_uv).r;\n"
          "  float alpha = smoothstep(u_buffer - v_gamma, u_buffer + v_gamma, dist);\n"
          "  gl_FragColor = v_color * v_color.a * alpha;\n"
          "}\n"
      },
      {
        .dst = &jeux.gl.pp.shader,
        .debug_name = "pp",
        .vs =
          "#version 300 es\n"
          "in vec4 a_pos;\n"
          "out vec2 v_uv;\n"
          "void main() {\n"
          "  gl_Position = vec4(a_pos.xy, 0.0, 1.0);\n"
          "  v_uv = gl_Position.xy*0.5 + vec2(0.5);\n"
          "}\n"
        ,
        .fs =

// No AA
          "#version 300 es\n"
          "precision mediump float;\n"
          "in vec2 v_uv;\n"
          "uniform sampler2D u_tex;\n"
          "uniform sampler2D u_tex_depth;\n"
          "uniform vec2 u_win_size;\n"
          "out vec4 frag_color;\n"
          "void main() {\n"
          "  gl_FragDepth = texture(u_tex_depth, v_uv).r;\n"
#if CURRENT_ALIASING == ANTIALIAS_NONE || CURRENT_ALIASING == ANTIALIAS_LINEAR
          "  frag_color = texture(u_tex, v_uv);\n"
#ifdef SRGB
          "  frag_color = vec4(pow(abs(frag_color.xyz), vec3(1.0 / 2.2)), 1);\n"
#endif

#elif CURRENT_ALIASING == ANTIALIAS_2xSSAA
          "  vec2 inv_vp = 1.0 / u_win_size;\n"
          "  frag_color = vec4(0, 0, 0, 1);\n"
          "  frag_color.xyz += 0.25*texture(u_tex, v_uv + (vec2(-0.25, -0.25) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.25*texture(u_tex, v_uv + (vec2(+0.25, -0.25) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.25*texture(u_tex, v_uv + (vec2(-0.25, +0.25) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.25*texture(u_tex, v_uv + (vec2(+0.25, +0.25) * inv_vp)).xyz;\n"
#ifdef SRGB
          "  frag_color = vec4(pow(abs(frag_color.xyz), vec3(1.0 / 2.2)), 1);\n"
#endif

#elif CURRENT_ALIASING == ANTIALIAS_4xSSAA
          "  vec2 inv_vp = 1.0 / u_win_size;\n"
          "  frag_color = vec4(0, 0, 0, 1);\n"
          "  frag_color.xyz += 0.0625*texture(u_tex, v_uv + (vec2(-0.375, -0.375) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.0625*texture(u_tex, v_uv + (vec2(-0.375, -0.125) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.0625*texture(u_tex, v_uv + (vec2(-0.375,  0.125) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.0625*texture(u_tex, v_uv + (vec2(-0.375,  0.375) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.0625*texture(u_tex, v_uv + (vec2(-0.125, -0.375) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.0625*texture(u_tex, v_uv + (vec2(-0.125, -0.125) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.0625*texture(u_tex, v_uv + (vec2(-0.125,  0.125) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.0625*texture(u_tex, v_uv + (vec2(-0.125,  0.375) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.0625*texture(u_tex, v_uv + (vec2( 0.125, -0.375) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.0625*texture(u_tex, v_uv + (vec2( 0.125, -0.125) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.0625*texture(u_tex, v_uv + (vec2( 0.125,  0.125) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.0625*texture(u_tex, v_uv + (vec2( 0.125,  0.375) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.0625*texture(u_tex, v_uv + (vec2( 0.375, -0.375) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.0625*texture(u_tex, v_uv + (vec2( 0.375, -0.125) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.0625*texture(u_tex, v_uv + (vec2( 0.375,  0.125) * inv_vp)).xyz;\n"
          "  frag_color.xyz += 0.0625*texture(u_tex, v_uv + (vec2( 0.375,  0.375) * inv_vp)).xyz;\n"
#ifdef SRGB
          "  frag_color = vec4(pow(abs(frag_color.xyz), vec3(1.0 / 2.2)), 1);\n"
#endif

#elif CURRENT_ALIASING == ANTIALIAS_FXAA
#define SRGB_SAMPLE(x) "pow(abs(texture(u_tex, " x ").xyz), vec3(1.0 / 2.2))"

            /* https://github.com/LiveMirror/NVIDIA-Direct3D-SDK-11/blob/a2d3cc46179364c9faa3e218eff230883badcd79/FXAA/FxaaShader.h#L1 */

            "float FXAA_SPAN_MAX = 8.0;\n"
            "float FXAA_REDUCE_MUL = 1.0/8.0;\n"
            "float FXAA_REDUCE_MIN = (1.0/128.0);\n"

            "vec2 inv_vp = 1.0 / u_win_size;\n"
            "vec3 rgbNW = " SRGB_SAMPLE("v_uv + (vec2(-0.5, -0.5) * inv_vp)") ";\n"
            "vec3 rgbNE = " SRGB_SAMPLE("v_uv + (vec2(+0.5, -0.5) * inv_vp)") ";\n"
            "vec3 rgbSW = " SRGB_SAMPLE("v_uv + (vec2(-0.5, +0.5) * inv_vp)") ";\n"
            "vec3 rgbSE = " SRGB_SAMPLE("v_uv + (vec2(+0.5, +0.5) * inv_vp)") ";\n"
            "vec3 rgbM  = " SRGB_SAMPLE("v_uv"                              ) ";\n"

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
            "  " SRGB_SAMPLE("v_uv + dir * (1.0/3.0 - 0.5)") " +\n"
            "  " SRGB_SAMPLE("v_uv + dir * (2.0/3.0 - 0.5)") "  \n"
            ");\n"
            "vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (\n"
            "  " SRGB_SAMPLE("v_uv + dir * (0.0/3.0 - 0.5)") " +\n"
            "  " SRGB_SAMPLE("v_uv + dir * (3.0/3.0 - 0.5)") "  \n"
            ");\n"
            "float lumaB = dot(rgbB, luma);\n"

            "if((lumaB < lumaMin) || (lumaB > lumaMax)){\n"
            "    frag_color.xyz=rgbA;\n"
            "} else {\n"
            "    frag_color.xyz=rgbB;\n"
            "}\n"
            "frag_color.a = 1.0;\n"
#else
#error "no such aliasing!"
#endif
          "}\n"
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
            char *log = SDL_malloc(log_length);
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

    jeux.gl.pp.shader_u_win_size  = glGetUniformLocation(jeux.gl.pp.shader, "u_win_size");
    jeux.gl.pp.shader_u_tex_color = glGetUniformLocation(jeux.gl.pp.shader, "u_tex_color");
    jeux.gl.pp.shader_u_tex_depth = glGetUniformLocation(jeux.gl.pp.shader, "u_tex_depth");
    jeux.gl.pp.shader_a_pos       = glGetAttribLocation( jeux.gl.pp.shader, "a_pos");
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

    for (size_t i = 0; i < gl_Model_COUNT; i++) {
      size_t vtx_count = gl_modeldata[i].vtx_count;
      size_t tri_count = gl_modeldata[i].tri_count;
      gl_geo_Vtx *vtx  = gl_modeldata[i].vtx;
      gl_Tri     *tri  = gl_modeldata[i].tri;

      jeux.gl.geo.static_models[i].tri_count = tri_count;

      /* right now this gives slightly different results to what comes out of Blender,
       * but if we could get it working perfectly, we could cut down on asset size tremendously
       *
       * EDIT: this might be fine to use; the bad normals were an asset authoring issue */
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

      /* normalalize the normals */
      for (int vtx_i = 0; vtx_i < vtx_count; vtx_i++) {
        gl_geo_Vtx *v = vtx + vtx_i;
        v->color.a = 255;

        if (i == gl_Model_IntroGravestoneTerrain) {
          float desaturation = 0.4; /* 0 .. 1 */

          float r = (float)v->color.r / 255.0f;
          float g = (float)v->color.g / 255.0f;
          float b = (float)v->color.b / 255.0f;
          float luma = (r * 0.2126) + (g * 0.7152) + (b * 0.0722);
          r = lerp(r, luma, desaturation) * 255.0f;
          g = lerp(g, luma, desaturation) * 255.0f;
          b = lerp(b, luma, desaturation) * 255.0f;
          v->color = (Color) { r, g, b, 255 };
        }
      }

      glGenBuffers(1, &jeux.gl.geo.static_models[i].buf_vtx);
      glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.geo.static_models[i].buf_vtx);
      glBufferData(GL_ARRAY_BUFFER, vtx_count * sizeof(gl_geo_Vtx), vtx, GL_STATIC_DRAW);

      glGenBuffers(1, &jeux.gl.geo.static_models[i].buf_idx);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, jeux.gl.geo.static_models[i].buf_idx);
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
      jeux.gl.text.shader_u_mvp      = glGetUniformLocation(jeux.gl.text.shader, "u_mvp"     );
      jeux.gl.text.shader_a_pos      = glGetAttribLocation( jeux.gl.text.shader, "a_pos"     );
      jeux.gl.text.shader_a_uv       = glGetAttribLocation( jeux.gl.text.shader, "a_uv"      );
      jeux.gl.text.shader_a_size     = glGetAttribLocation( jeux.gl.text.shader, "a_size"    );
      jeux.gl.text.shader_a_color    = glGetAttribLocation( jeux.gl.text.shader, "a_color"   );
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
  /* Flip the y! */
  jeux.screen = f4x4_ortho(
     0.0f,             jeux.win_size_x,
     jeux.win_size_y,  0.0f,
    -1.0f,             1.0f
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

      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

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

/* easy text drawing, for e.g. debug text! */
static void gl_text_draw(const char *msg, float screen_x, float screen_y, float size) {
  gl_text_Vtx *vtx_wtr = jeux.gl.text.vtx_wtr;
  gl_Tri      *idx_wtr = jeux.gl.text.idx_wtr;

  float scale = (size / font_BASE_CHAR_SIZE);

  float pen_x = screen_x;
  float pen_y = screen_y + size * 0.75f; /* little adjustment */
  do {
    size_t c = *msg | (1 << 5); /* this is a caps-only font, so atlas only has lowercase */
    font_LetterRegion *l = &font_letter_regions[c];

    uint16_t start = vtx_wtr - jeux.gl.text.vtx;

    float x = pen_x;
    float y = pen_y - (l->top) * scale;

    float size_x = l->size_x * scale;
    float size_y = l->size_y * scale;
    Color color = { 255, 255, 255, 255 };
    *vtx_wtr++ = (gl_text_Vtx) { x + size_x,  y         , 0.999f, l->x + l->size_x, l->y            , size, color };
    *vtx_wtr++ = (gl_text_Vtx) { x + size_x,  y + size_y, 0.999f, l->x + l->size_x, l->y + l->size_y, size, color };
    *vtx_wtr++ = (gl_text_Vtx) { x         ,  y + size_y, 0.999f, l->x            , l->y + l->size_y, size, color };
    *vtx_wtr++ = (gl_text_Vtx) { x         ,  y         , 0.999f, l->x            , l->y            , size, color };

    *idx_wtr++ = (gl_Tri) { start + 0, start + 1, start + 2 };
    *idx_wtr++ = (gl_Tri) { start + 2, start + 3, start + 0 };

    pen_x += l->advance * scale;
  } while (*msg++);

  jeux.gl.text.vtx_wtr = vtx_wtr;
  jeux.gl.text.idx_wtr = idx_wtr;
}

/* probably only UI renderers need this level of configuration */
static void gl_text_draw_ex(
  const char *msg,
  size_t msg_len,
  f3 pos,
  float size,
  Box2 clip,
  Color color
) {
  gl_text_Vtx *vtx_wtr = jeux.gl.text.vtx_wtr;
  gl_Tri      *idx_wtr = jeux.gl.text.idx_wtr;

  float scale = (size / font_BASE_CHAR_SIZE);

  float pen_x = pos.x;
  float pen_y = pos.y + size * 0.75f; /* little adjustment */
  for (int i = 0; i < msg_len; i++) {
    size_t c = msg[i] | (1 << 5); /* this is a caps-only font, so atlas only has lowercase */
    font_LetterRegion *l = &font_letter_regions[c];

    uint16_t start = vtx_wtr - jeux.gl.text.vtx;

    float min_x = pen_x;
    float min_y = pen_y - (l->top) * scale;
    float max_x = min_x + l->size_x * scale;
    float max_y = min_y + l->size_y * scale;

    float min_u = l->x;
    float min_v = l->y;
    float max_u = l->x + l->size_x;
    float max_v = l->y + l->size_y;

    /* reproject UVs so that clip doesn't exceed X without leaking */
    {
      /* no need to clamp X so far :shrug: */

      /* enforce clamp max y */
      {
        float cut_max_y_t = clamp(1.0, 0.0, inv_lerp(min_y, max_y, clip.max.y));
        float cut_min_y_t = clamp(0.0, 1.0, inv_lerp(max_y, min_y, clip.max.y));

        float og_min_y = min_y, og_max_y = max_y;
        min_y = lerp(og_min_y, og_max_y, cut_max_y_t);
        max_y = lerp(og_max_y, og_min_y, cut_min_y_t);

        float og_min_v = min_v, og_max_v = max_v;
        min_v = lerp(og_min_v, og_max_v, cut_max_y_t);
        max_v = lerp(og_max_v, og_min_v, cut_min_y_t);
      }

      /* enforce clamp min y */
      {
        float cut_max_y_t = clamp(0.0, 1.0, inv_lerp(min_y, max_y, clip.min.y));
        float cut_min_y_t = clamp(1.0, 0.0, inv_lerp(max_y, min_y, clip.min.y));

        float og_min_y = min_y, og_max_y = max_y;
        min_y = lerp(og_min_y, og_max_y, cut_max_y_t);
        max_y = lerp(og_max_y, og_min_y, cut_min_y_t);

        float og_min_v = min_v, og_max_v = max_v;
        min_v = lerp(og_min_v, og_max_v, cut_max_y_t);
        max_v = lerp(og_max_v, og_min_v, cut_min_y_t);
      }
    }

    *vtx_wtr++ = (gl_text_Vtx) { max_x, min_y, pos.z, max_u, min_v, size, color };
    *vtx_wtr++ = (gl_text_Vtx) { max_x, max_y, pos.z, max_u, max_v, size, color };
    *vtx_wtr++ = (gl_text_Vtx) { min_x, max_y, pos.z, min_u, max_v, size, color };
    *vtx_wtr++ = (gl_text_Vtx) { min_x, min_y, pos.z, min_u, min_v, size, color };

    *idx_wtr++ = (gl_Tri) { start + 0, start + 1, start + 2 };
    *idx_wtr++ = (gl_Tri) { start + 2, start + 3, start + 0 };

    pen_x += l->advance * scale;
  }

  jeux.gl.text.vtx_wtr = vtx_wtr;
  jeux.gl.text.idx_wtr = idx_wtr;
}

static void gl_geo_reset(void) {
  jeux.gl.geo.vtx_wtr = jeux.gl.geo.vtx;
  jeux.gl.geo.idx_wtr = jeux.gl.geo.idx;
  jeux.gl.geo.model_draws_wtr = jeux.gl.geo.model_draws;
}

static void gl_geo_arc(
  float radians_from,
  float radians_to,
  size_t detail,
  f3 center,
  float radius,
  Color color
) {
  uint16_t start = jeux.gl.geo.vtx_wtr - jeux.gl.geo.vtx;

  /* center of the triangle fan */
  *jeux.gl.geo.vtx_wtr++ = (gl_geo_Vtx) { .pos = center, .color = color };

  for (int i = 0; i <= detail; i++) {
    float t = lerp(radians_from, radians_to, (float)i / (float)detail);
    float x = center.x + cosf(t) * radius;
    float y = center.y + sinf(t) * radius;
    *jeux.gl.geo.vtx_wtr++ = (gl_geo_Vtx) { { x, y, center.z }, color };

    if (i > 0) *jeux.gl.geo.idx_wtr++ = (gl_Tri) { start, start + i, start + i + 1 };
  }
}

static void gl_geo_circle(size_t detail, f3 center, float radius, Color color) {
  gl_geo_arc(
    0.0f,
    M_PI * 2.0f,
    detail,
    center,
    radius,
    color
  );
}

static void gl_geo_line(f3 a, f3 b, float thickness, Color color) {
  float dx = a.x - b.x;
  float dy = a.y - b.y;
  float dlen = dx*dx + dy*dy;
  if (dlen <= 0) return;
  dlen = sqrtf(dlen);
  float nx = -dy / dlen * thickness*0.5;
  float ny =  dx / dlen * thickness*0.5;

  uint16_t start = jeux.gl.geo.vtx_wtr - jeux.gl.geo.vtx;

  *jeux.gl.geo.vtx_wtr++ = (gl_geo_Vtx) { { a.x + nx, a.y + ny, a.z }, color };
  *jeux.gl.geo.vtx_wtr++ = (gl_geo_Vtx) { { a.x - nx, a.y - ny, a.z }, color };
  *jeux.gl.geo.vtx_wtr++ = (gl_geo_Vtx) { { b.x + nx, b.y + ny, b.z }, color };
  *jeux.gl.geo.vtx_wtr++ = (gl_geo_Vtx) { { b.x - nx, b.y - ny, b.z }, color };

  *jeux.gl.geo.idx_wtr++ = (gl_Tri) { start + 0, start + 1, start + 2 };
  *jeux.gl.geo.idx_wtr++ = (gl_Tri) { start + 2, start + 1, start + 3 };
}

static void gl_geo_box(f3 min, f3 max, Color color) {
  uint16_t start = jeux.gl.geo.vtx_wtr - jeux.gl.geo.vtx;

  *jeux.gl.geo.vtx_wtr++ = (gl_geo_Vtx) { { min.x, min.y, min.z }, color };
  *jeux.gl.geo.vtx_wtr++ = (gl_geo_Vtx) { { min.x, max.y, min.z }, color };
  *jeux.gl.geo.vtx_wtr++ = (gl_geo_Vtx) { { max.x, max.y, min.z }, color };
  *jeux.gl.geo.vtx_wtr++ = (gl_geo_Vtx) { { max.x, min.y, min.z }, color };

  *jeux.gl.geo.idx_wtr++ = (gl_Tri) { start + 0, start + 1, start + 2 };
  *jeux.gl.geo.idx_wtr++ = (gl_Tri) { start + 3, start + 2, start + 0 };
}

static void gl_geo_box2_outline(f3 min, f3 max, float thickness, Color color) {
  float t = thickness;
  float h = thickness * 0.5;
  Color c = color;
  gl_geo_line((f3) { min.x - h, min.y, min.z }, (f3) { max.x + h, min.y, min.z }, t, c);
  gl_geo_line((f3) { min.x    , max.y, min.z }, (f3) { min.x    , min.y, min.z }, t, c);
  gl_geo_line((f3) { max.x + h, max.y, min.z }, (f3) { min.x - h, max.y, min.z }, t, c);
  gl_geo_line((f3) { max.x    , min.y, min.z }, (f3) { max.x    , max.y, min.z }, t, c);
}

static void gl_geo_ring2_arc(
  float radians_from,
  float radians_to,
  size_t detail,
  f3 center,
  float radius,
  float thickness,
  Color color
) {
  for (int i = 0; i < detail; i++) {
    float t0 = lerp(radians_from, radians_to, (float)i / (float)detail);
    float x0 = center.x + cosf(t0) * radius;
    float y0 = center.y + sinf(t0) * radius;

    float t1 = lerp(radians_from, radians_to, (float)(i + 1) / (float)detail);
    float x1 = center.x + cosf(t1) * radius;
    float y1 = center.y + sinf(t1) * radius;

    gl_geo_line(
      (f3) { x0, y0, center.z },
      (f3) { x1, y1, center.z },
      thickness,
      color
    );
  }
}

static void gl_geo_ring2(
  size_t detail,
  f3 center,
  float radius,
  float thickness,
  Color color
) {
  gl_geo_ring2_arc(
    0.0f,
    M_PI * 2.0f,
    detail,
    center,
    radius,
    thickness,
    color
  );
}

static void gl_geo_ring3(
  size_t detail,
  f3 center,
  float radius,
  float thickness,
  Color color
) {
  for (int i = 0; i < detail; i++) {
    float t0 = (float)i / (float)detail * M_PI * 2.0f;
    float x0 = center.x + cosf(t0) * radius;
    float y0 = center.y + sinf(t0) * radius;

    float t1 = (float)(i + 1) / (float)detail * M_PI * 2.0f;
    float x1 = center.x + cosf(t1) * radius;
    float y1 = center.y + sinf(t1) * radius;

    gl_geo_line(
      jeux_world_to_screen((f3) { x0, y0, center.z }),
      jeux_world_to_screen((f3) { x1, y1, center.z }),
      thickness,
      color
    );
  }
}

static void gl_geo_box3_outline(f3 center, f3 scale, float thickness, Color color) {

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

        a.p.x *=  scale.x; a.p.y *=  scale.y; a.p.z *=  scale.z;
        b.p.x *=  scale.x; b.p.y *=  scale.y; b.p.z *=  scale.z;
        a.p.x += center.x; a.p.y += center.y; a.p.z += center.z;
        b.p.x += center.x; b.p.y += center.y; b.p.z += center.z;

        gl_geo_line(jeux_world_to_screen(a.xyz), jeux_world_to_screen(b.xyz), thickness, color);
      }
    }
  }
}

/**
 * WARNING: This Clay renderer has "character":
 *
 *  [x] We only take into account cornerRadius.topLeft. Other corners? Fuck 'em.
 *
 *  [x] 2 draw calls, one for all UI, one afterwards for text.
 *      (Text has its own AA, so it happens after postprocessing.)
 *
 *  [x] We reproject UVs instead of using scissor. (this allows just 2 draw calls)
 *
 *  [x] The first pass that draws shapes uses the same geometry buffers and shaders
 *      as things in the 3D scene, so we can easily draw the character in your inventory.
 *
 *  [x] UI is at z=0.99, draw over that to draw over the UI.

 *  [x] clay.h's "imageData" has been changed from "void *" to a "size_t",
 *      which is interpreted as a gl_Model.
 *
 *  [x] Even though our text is arbitrarily resizable, Clay's fontSize is a uint16_t, so
 *      the text ends up getting truncated. This is trivial to tweak in clay.h, but the
 *      layouting still treats the text as if it were uint16_t (meaning your text scales
 *      up gracefully, but the layout around it increases and decreases in clear 1px chunks)
 */
static void gl_draw_clay_commands(Clay_RenderCommandArray *rcommands) {
  Box2 clip = BOX2_UNCONSTRAINED;
  float ui_z = 0.99f;
  float ui_z_bump = 0.00001f;

  for (size_t i = 0; i < rcommands->length; i++) {
    Clay_RenderCommand *rcmd = Clay_RenderCommandArray_Get(rcommands, i);
    Clay_BoundingBox rect = rcmd->boundingBox;

    switch (rcmd->commandType) {

      case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
        Clay_RectangleRenderData *config = &rcmd->renderData.rectangle;

        f3 min = { rect.x             , rect.y              , ui_z };
        f3 max = { rect.x + rect.width, rect.y + rect.height, ui_z };
        Color color = {
          config->backgroundColor.r,
          config->backgroundColor.g,
          config->backgroundColor.b,
          config->backgroundColor.a
        };

        if (config->cornerRadius.topLeft == 0.0f) {
          gl_geo_box(
            (f3) { min.x, min.y, ui_z },
            (f3) { max.x, max.y, ui_z },
            color
          );
        } else {
          float r = config->cornerRadius.topLeft;

          /* corner arcs */
          {
            float q = M_PI * 0.5f;
            gl_geo_arc(2*q, 3*q, 16, (f3) { min.x + r, min.y + r, ui_z }, r, color);
            gl_geo_arc(1*q, 2*q, 16, (f3) { min.x + r, max.y - r, ui_z }, r, color);
            gl_geo_arc(3*q, 4*q, 16, (f3) { max.x - r, min.y + r, ui_z }, r, color);
            gl_geo_arc(0*q, 1*q, 16, (f3) { max.x - r, max.y - r, ui_z }, r, color);
          }

          /* min and max inset by radius */
          f3 rmin = { min.x + r, min.y + r, ui_z };
          f3 rmax = { max.x - r, max.y - r, ui_z };
          gl_geo_box(rmin, rmax, color);

          float h = r * 0.5f;
          rmin = (f3) { min.x + h, min.y + h, ui_z };
          rmax = (f3) { max.x - h, max.y - h, ui_z };
          gl_geo_line((f3) { rmin.x+h, rmin.y  , ui_z }, (f3) { rmax.x-h, rmin.y  , ui_z }, r, color);
          gl_geo_line((f3) { rmin.x  , rmax.y-h, ui_z }, (f3) { rmin.x  , rmin.y+h, ui_z }, r, color);
          gl_geo_line((f3) { rmax.x-h, rmax.y  , ui_z }, (f3) { rmin.x+h, rmax.y  , ui_z }, r, color);
          gl_geo_line((f3) { rmax.x  , rmin.y+h, ui_z }, (f3) { rmax.x  , rmax.y-h, ui_z }, r, color);
        }

        ui_z += ui_z_bump;
      } break;

      case CLAY_RENDER_COMMAND_TYPE_TEXT: {
        Clay_TextRenderData *config = &rcmd->renderData.text;
        gl_text_draw_ex(
          config->stringContents.chars,
          config->stringContents.length,
          (f3) { rect.x, rect.y, ui_z },
          roundf(config->fontSize),
          clip,
          (Color) {
            config->textColor.r,
            config->textColor.g,
            config->textColor.b,
            config->textColor.a
          }
        );
      } break;

      case CLAY_RENDER_COMMAND_TYPE_BORDER: {
        Clay_BorderRenderData *config = &rcmd->renderData.border;
        Color color = {
          config->color.r,
          config->color.g,
          config->color.b,
          config->color.a
        };

        f3 min = { rect.x             , rect.y              , ui_z };
        f3 max = { rect.x + rect.width, rect.y + rect.height, ui_z };
        float r = config->cornerRadius.topLeft;

        {
          float t;
          t = config->width.top;
          gl_geo_line(
            (f3){ min.x - t*0.5f + r, min.y, ui_z },
            (f3){ max.x + t*0.5f - r, min.y, ui_z },
            t,
            color
          );

          t = config->width.left;
          gl_geo_line(
            (f3){ min.x, max.y + t*0.5f - r, ui_z },
            (f3){ min.x, min.y - t*0.5f + r, ui_z },
            t,
            color
          );

          t = config->width.bottom;
          gl_geo_line(
            (f3){ max.x + t*0.5f - r, max.y, ui_z },
            (f3){ min.x - t*0.5f + r, max.y, ui_z },
            t,
            color
          );

          t = config->width.right;
          gl_geo_line(
            (f3){ max.x, min.y - t*0.5f + r, ui_z },
            (f3){ max.x, max.y + t*0.5f - r, ui_z },
            t,
            color
          );
        }

        {
          /* probably fine just to use the top as the width of all of these */
          float t = config->width.top;
          float q = M_PI * 0.5f;
          gl_geo_ring2_arc(2*q, 3*q, 16, (f3) { min.x + r, min.y + r, ui_z }, r, t, color);
          gl_geo_ring2_arc(1*q, 2*q, 16, (f3) { min.x + r, max.y - r, ui_z }, r, t, color);
          gl_geo_ring2_arc(3*q, 4*q, 16, (f3) { max.x - r, min.y + r, ui_z }, r, t, color);
          gl_geo_ring2_arc(0*q, 1*q, 16, (f3) { max.x - r, max.y - r, ui_z }, r, t, color);
        }

      } break;

      case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
        Clay_BoundingBox bbox = rcmd->boundingBox;
        clip.min.x = bbox.x;
        clip.min.y = bbox.y;
        clip.max.x = bbox.x + bbox.width;
        clip.max.y = bbox.y + bbox.height;
      } break;

      case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
        clip = BOX2_UNCONSTRAINED;
      } break;

      case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
        Clay_BoundingBox bbox = rcmd->boundingBox;

        f4x4 mvp = f4x4_move((f3) { bbox.x, bbox.y, ui_z });
        mvp = f4x4_mul_f4x4(mvp, f4x4_scale3((f3) { bbox.width, bbox.height, 1.0f }));
        mvp = f4x4_mul_f4x4(mvp, rcmd->renderData.image.transform);

        *jeux.gl.geo.model_draws_wtr++ = (gl_ModelDraw) {
          .model = rcmd->renderData.image.imageData,
          .matrix = mvp,
          .two_dee = true,
        };
      } break;

      default:
        SDL_Log("Unknown render command type: %d", rcmd->commandType);
    }
  }
}

static void ui_main(void);
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
      float x = cosf(M_PI * -0.6f);
      float y = sinf(M_PI * -0.6f);
      jeux.camera_eye = (f3) { x, y, 1.8f };
    }

    {
      float ar = (float)(jeux.win_size_y) / (float)(jeux.win_size_x);
      f4x4 projection = f4x4_ortho(
         -7.0f     ,  7.0f     ,
         -7.0f * ar,  7.0f * ar,
        -20.0f     , 20.0f
      );

      f4x4 orbit = f4x4_target_to(
        jeux.camera_eye,
        (f3) { 0.0f, 0.0f, 1.0f },
        (f3) { 0.0f, 0.0f, 1.0f }
      );
      jeux.camera = f4x4_mul_f4x4(projection, f4x4_invert(orbit));
    }


    /* cast a ray to the ground to find jeux.mouse_ground */
    {
      f3 origin = jeux_screen_to_world((f3) { jeux.mouse_screen_x, jeux.mouse_screen_y,  1.0f });
      f3 target = jeux_screen_to_world((f3) { jeux.mouse_screen_x, jeux.mouse_screen_y, -1.0f });
      f3 ray_vector = { target.x - origin.x, target.y - origin.y, target.z - origin.z };

      jeux.mouse_ground = ray_hit_plane(origin, ray_vector, (f3) { 0 }, (f3) { 0, 0, 1 });
    }

  }

  /* fill dynamic geometry buffers */
  {
    gl_geo_reset();
    gl_text_reset();

    /* draw terrain! */
    *jeux.gl.geo.model_draws_wtr++ = (gl_ModelDraw) {
      .model = gl_Model_IntroGravestoneTerrain,
      .matrix = f4x4_scale(1)
    };

    /* ui */
    {
      Clay_RenderCommandArray cmds;

      {
        Clay_SetPointerState(
          (Clay_Vector2) { jeux.mouse_screen_x, jeux.mouse_screen_y },
          jeux.mouse_lmb_down
        );

        Clay_BeginLayout();

        ui_main();


        cmds = Clay_EndLayout();
      }

      gl_draw_clay_commands(&cmds);
    }

    /* debug */
    {
      float debug_thickness = jeux.win_size_x * 0.00225f;

      /* debug where we think the mouse is */
      if (0) {

        /* draw an X at the mouse's 2D position */
        {
          gl_geo_line(
            (f3) { jeux.mouse_screen_x - 10.0f, jeux.mouse_screen_y + 10.0f, 0.99f },
            (f3) { jeux.mouse_screen_x + 10.0f, jeux.mouse_screen_y - 10.0f, 0.99f },
            debug_thickness,
            (Color) { 255, 0, 0, 255 }
          );

          gl_geo_line(
            (f3) { jeux.mouse_screen_x + 10.0f, jeux.mouse_screen_y + 10.0f, 0.99f },
            (f3) { jeux.mouse_screen_x - 10.0f, jeux.mouse_screen_y - 10.0f, 0.99f },
            debug_thickness,
            (Color) { 255, 0, 0, 255 }
          );
        }

        /* draw a ring where we think the mouse is in 3D space
         * (useful to compare to its 2D position) */
        gl_geo_ring3(
          32,
          jeux.mouse_ground,
          0.2f,
          debug_thickness,
          (Color) { 255, 0, 0, 255 }
        );
      }

      /* lil ring around the player,
       * useful for debugging physics */
      gl_geo_ring3(
        32,
        (f3) { 0.0f, 0.0f, -0.01f },
        0.5f,
        debug_thickness,
        (Color) { 200, 100, 20, 255 }
      );

#if 0
      /* draw player-sized box around (0, 0, 0) */
      gl_geo_box3_outline(
        (f3) { 0.0f, 0.0f, 1.0f },
        (f3) { 0.3f, 0.3f, 1.0f },
        debug_thickness,
        (Color) { 200, 80, 20, 255 }
      );
#endif

#if 0
      /* draw a red line down the X axis, and a green line down the Y axis */
      gl_geo_line(
        jeux_world_to_screen((f3) { 0, 0, 0 }),
        jeux_world_to_screen((f3) { 1, 0, 0 }),
        debug_thickness,
        (Color) { 255, 0, 0, 255 }
      );

      gl_geo_line(
        jeux_world_to_screen((f3) { 0, 0, 0 }),
        jeux_world_to_screen((f3) { 0, 1, 0 }),
        debug_thickness,
        (Color) { 0, 255, 0, 255 }
      );
#endif
    }

    /* draw figure */
    {
      f4x4 model = f4x4_scale(1.0f);

      /* rotate towards mouse */
      {
          f3 mouse = jeux.mouse_ground;
          model = f4x4_mul_f4x4(model, f4x4_turn(atan2f(mouse.y - 0, mouse.x - 0) + (M_PI * 0.5f)));
      }

      float t = fmodf(jeux.elapsed, animdata_duration);

      size_t rhs_frame = -1;
      for (size_t i = 0; i < jx_COUNT(animdata_frames); i++)
        if (animdata_frames[i].time > t) { rhs_frame = i; break; }
      bool last_frame = rhs_frame == -1;

      size_t lhs_frame = (last_frame ? jx_COUNT(animdata_frames) : rhs_frame) - 1;
      rhs_frame = (lhs_frame + 1) % jx_COUNT(animdata_frames);
      float next_frame_t = last_frame ? animdata_duration : animdata_frames[rhs_frame].time;
      float this_frame_t = animdata_frames[lhs_frame].time;
      float tween_t = (t - this_frame_t) / (next_frame_t - this_frame_t);

#define joint_pos(joint) lerp3(\
  animdata_frames[lhs_frame].joint_pos[joint], \
  animdata_frames[rhs_frame].joint_pos[joint], \
  tween_t \
)

      for (int i = 0; i < jx_COUNT(animdata_limb_connections); i++) {
        animdata_JointKey_t from = animdata_limb_connections[i].from,
                              to = animdata_limb_connections[i].to;
        f3 a = jeux_world_to_screen(f4x4_transform_f3(model, joint_pos(from)));
        f3 b = jeux_world_to_screen(f4x4_transform_f3(model, joint_pos(  to)));

        float thickness = jeux.win_size_x * 0.005f;

        Color color = { 1, 1, 1, 255 };
        gl_geo_line(a, b, thickness, color);

        gl_geo_circle(8, a, thickness * 0.5f, color);
        gl_geo_circle(8, b, thickness * 0.5f, color);
      }

      /* draw head */
      {
        f3 head = joint_pos(animdata_JointKey_Head);

        /* head assets are 2x2x2 centered around (0, 0, 0) */
        float radius = 0.175f;

        /* if this is 0.8f, a perfectly (0, 0, 1) aligned neck will penetrate 20% */
        head.z += radius*0.8f;

        f4x4 matrix = model;
        matrix = f4x4_mul_f4x4(matrix, f4x4_move(head));
        /* the animations seem to be exported with the negative X axis as "forward," so ... */
        matrix = f4x4_mul_f4x4(matrix, f4x4_turn(M_PI * -0.5f));
        matrix = f4x4_mul_f4x4(matrix, f4x4_scale(radius));

        *jeux.gl.geo.model_draws_wtr++ = (gl_ModelDraw) { .model = gl_Model_Head, .matrix = matrix };
        *jeux.gl.geo.model_draws_wtr++ = (gl_ModelDraw) { .model = gl_Model_HornedHelmet, .matrix = matrix };
      }

#undef joint_pos
    }

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
      glClearColor(0.027f, 0.027f, 0.047f, 1.0f);
      glClearDepthf(0.0f);

      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_GEQUAL);

      /* set up premultiplied alpha */
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      glEnable(GL_BLEND);

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
        size_t models_to_draw = jeux.gl.geo.model_draws_wtr - jeux.gl.geo.model_draws;
        for (int i = 0; i < models_to_draw; i++) {
          gl_ModelDraw *draw = jeux.gl.geo.model_draws + i;

          glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.geo.static_models[draw->model].buf_vtx);
          glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, jeux.gl.geo.static_models[draw->model].buf_idx);
          size_t tri_count = jeux.gl.geo.static_models[draw->model].tri_count;

          GEO_VTX_BIND_LAYOUT;

          if (draw->two_dee) {
            f4x4 mvp = jeux.screen;
            mvp = f4x4_mul_f4x4(mvp, draw->matrix);
            glUniformMatrix4fv(jeux.gl.geo.shader_u_mvp, 1, 0, mvp.floats);
          } else {
            f4x4 mvp = jeux.camera;
            mvp = f4x4_mul_f4x4(mvp, draw->matrix);
            glUniformMatrix4fv(jeux.gl.geo.shader_u_mvp, 1, 0, mvp.floats);
          }

          glDrawElements(GL_TRIANGLES, 3 * tri_count, GL_UNSIGNED_SHORT, 0);
        }

      }

      /* note: if you run the postprocessing with depth enabled,
       * nothing renders, but only on Windows! */
      glDisable(GL_DEPTH_TEST);

      glDisable(GL_BLEND);
    }

    /* stop writing to the framebuffer, start writing to the screen */
    glViewport(0, 0, jeux.gl.pp.phys_win_size_x, jeux.gl.pp.phys_win_size_y);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    /* draw the contents of the framebuffer with postprocessing/aa applied */
    {
      glDepthFunc(GL_GEQUAL);
      glEnable(GL_DEPTH_TEST);

      glClearDepthf(0.0f);
      glClear(GL_DEPTH_BUFFER_BIT);

      glUseProgram(jeux.gl.pp.shader);
      glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.pp.buf_vtx);
      glEnableVertexAttribArray(jeux.gl.pp.shader_a_pos);
      glVertexAttribPointer(jeux.gl.pp.shader_a_pos, 3, GL_FLOAT, GL_FALSE, 0, 0);

      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, jeux.gl.pp.screen.pp_tex_depth);
      glUniform1i(jeux.gl.pp.shader_u_tex_depth, 1);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, jeux.gl.pp.screen.pp_tex_color);
      glUniform1i(jeux.gl.pp.shader_u_tex_color, 0);

      glUniform2f(jeux.gl.pp.shader_u_win_size, jeux.gl.pp.phys_win_size_x, jeux.gl.pp.phys_win_size_y);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glDisable(GL_DEPTH_TEST);
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
        glVertexAttribPointer(jeux.gl.text.shader_a_pos, 3, GL_FLOAT, GL_FALSE, size, (void *)offsetof(gl_text_Vtx, x));

        glEnableVertexAttribArray(jeux.gl.text.shader_a_uv);
        glVertexAttribPointer(jeux.gl.text.shader_a_uv, 2, GL_FLOAT, GL_FALSE, size, (void *)offsetof(gl_text_Vtx, u));

        glEnableVertexAttribArray(jeux.gl.text.shader_a_size);
        glVertexAttribPointer(jeux.gl.text.shader_a_size, 1, GL_FLOAT, GL_FALSE, size, (void *)offsetof(gl_text_Vtx, size));

        glEnableVertexAttribArray(jeux.gl.text.shader_a_color);
        glVertexAttribPointer(jeux.gl.text.shader_a_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, size, (void *)offsetof(gl_text_Vtx, color));
      }

      glBindTexture(GL_TEXTURE_2D, jeux.gl.text.tex);
      glUniform2f(       jeux.gl.text.shader_u_tex_size, font_TEX_SIZE_X, font_TEX_SIZE_Y);
      glUniformMatrix4fv(jeux.gl.text.shader_u_mvp, 1, 0, jeux.screen.floats);
      glUniform1f(       jeux.gl.text.shader_u_buffer, 0.725);

      /* set up premultiplied alpha */
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      glEnable(GL_BLEND);

      glDepthFunc(GL_GEQUAL);
      glEnable(GL_DEPTH_TEST);

      float gamma = 2.0;
      glUniform1f(jeux.gl.text.shader_u_gamma, gamma * 1.4142 / SDL_GetWindowPixelDensity(jeux.sdl.window));
      glDrawElements(GL_TRIANGLES, 3 * (jeux.gl.text.idx_wtr - jeux.gl.text.idx), GL_UNSIGNED_SHORT, 0);

      glDisable(GL_BLEND);
      glDisable(GL_DEPTH_TEST);
    }
  }

  SDL_GL_SwapWindow(jeux.sdl.window);
  return SDL_APP_CONTINUE;
}

/* UI */

Clay_Color paper       = { 150, 131, 107, 255 };
Clay_Color paper_hover = { 128, 101,  77, 255 };
Clay_Color wood        = {  27,  15,   7, 255 };
Clay_Color ink         = {   0,   0,   0, 255 };

uint16_t text_body = 24;
uint16_t text_title = 24;

typedef enum {
  gl_AntiAliasingApproach_None,
  gl_AntiAliasingApproach_Linear,
  gl_AntiAliasingApproach_FXAA,
  gl_AntiAliasingApproach_2XSSAA,
  gl_AntiAliasingApproach_4XSSAA,
  gl_AntiAliasingApproach_COUNT
} gl_AntiAliasingApproach;
static struct {
  struct {
    bool open;
    bool perspective;
    float fov;
    uint8_t antialiasing;
  } options;

  /* the element who owns the current mouse down action */
  Clay_ElementId lmb_click_el;
  bool lmb_click;
} gui = {
  .options.open = true
};


static void ui_icon(gl_Model model, size_t size) {
  CLAY({
    .layout.sizing = { CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size) },
    .image = {
      .sourceDimensions = { size, size },
      .imageData = model,
      .transform = f4x4_scale(1)
    }
  });
}

static void ui_icon_f4x4(gl_Model model, size_t size, f4x4 mat) {
  CLAY({
    .layout.sizing = { CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size) },
    .image = {
      .sourceDimensions = { size, size },
      .imageData = model,
      .transform = mat
    }
  });
}

static void ui_checkbox(bool *state) {

  CLAY({
    .layout.sizing = { CLAY_SIZING_FIXED(14), CLAY_SIZING_FIXED(14) },
  }) {

    Clay_ElementDeclaration floating = {
      .floating.attachTo = CLAY_ATTACH_TO_PARENT,
      .floating.attachPoints.element = CLAY_ATTACH_POINT_CENTER_CENTER,
      .floating.attachPoints.parent = CLAY_ATTACH_POINT_CENTER_CENTER,
      .floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH
    };

    CLAY(floating) {
      size_t icon_size = 18;
      if (Clay_Hovered()) {
        icon_size = 20;
        if (gui.lmb_click) *state ^= 1;
      }

      ui_icon(gl_Model_UiCheckBox, icon_size);

      floating.floating.offset.x =  4;
      floating.floating.offset.y = -3;
      if (*state) CLAY(floating) { ui_icon(gl_Model_UiCheck, icon_size); }
    }
  };
}

static void ui_slider(Clay_ElementId id, float *state) {
  Clay_ElementId handle_id = CLAY_IDI("SliderHandle", id.id);

  CLAY({
    .layout.sizing.width  = CLAY_SIZING_GROW(0),
    .layout.sizing.height = CLAY_SIZING_GROW(0),
    .layout.layoutDirection = CLAY_TOP_TO_BOTTOM,
    .id = id,
  }) {
    CLAY({ .layout.sizing.height = CLAY_SIZING_GROW(0) });
    CLAY({
      .layout.sizing.width  = CLAY_SIZING_GROW(0),
      .layout.sizing.height  = CLAY_SIZING_FIXED(2),
    });
    CLAY({ .layout.sizing.height = CLAY_SIZING_GROW(0) });

    Clay_BoundingBox bbox = Clay_GetElementData(id).boundingBox;

    CLAY({
      .floating.attachTo = CLAY_ATTACH_TO_PARENT,
      .floating.attachPoints.element = CLAY_ATTACH_POINT_CENTER_CENTER,
      .floating.attachPoints.parent = CLAY_ATTACH_POINT_CENTER_CENTER,
      .floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH
    }) {
      ui_icon(gl_Model_UiSliderBody, bbox.width);
    }

    CLAY({
      .floating.attachTo = CLAY_ATTACH_TO_PARENT,
      .floating.attachPoints.element = CLAY_ATTACH_POINT_CENTER_CENTER,
      .floating.attachPoints.parent = CLAY_ATTACH_POINT_LEFT_CENTER,
      .floating.offset = { *state * bbox.width, 0 },
      .id = handle_id,
    }) {
      size_t icon_size = 15;

      bool held = gui.lmb_click_el.id == handle_id.id;

      if (Clay_Hovered() || held) {
        icon_size = 17;

        if (gui.lmb_click) {
          gui.lmb_click_el = handle_id;
        }
      }

      if (held) {
        float progress_px = jeux.mouse_screen_x - bbox.x;
        *state = clamp(0, 1, progress_px / bbox.width);
      }

      ui_icon(gl_Model_UiSliderHandle, icon_size);
    }
  }
}

static bool ui_arrow_button(bool left) {

  bool click = false;

  CLAY({
    .layout.sizing = { CLAY_SIZING_FIXED(18), CLAY_SIZING_GROW(0) },
  }) {

    size_t icon_size = 14;

    CLAY({
      .floating.attachTo = CLAY_ATTACH_TO_PARENT,
      .floating.attachPoints.element = CLAY_ATTACH_POINT_CENTER_CENTER,
      .floating.attachPoints.parent = CLAY_ATTACH_POINT_CENTER_CENTER,
    }) {
      if (Clay_Hovered()) {
        icon_size = 17;
        if (gui.lmb_click) click = true;
      }

      f4x4 mvp = f4x4_move((f3) { 0.5, 0.5, 0 });
      /* you can also use f4x4_turn() here to rotate components around their center */
      mvp = f4x4_mul_f4x4(mvp, f4x4_scale3((f3) { left ? 1 : -1, 1, 1 }));
      mvp = f4x4_mul_f4x4(mvp, f4x4_move((f3) { -0.5f, -0.5f, 0 }));
      ui_icon_f4x4(gl_Model_UiArrowButton, icon_size, mvp);
    }
  };

  return click;
}

static void ui_picker(uint8_t *state, uint8_t option_count, Clay_String *labels) {
  Clay_TextElementConfig text_conf = { .fontSize = text_body, .textColor = ink };

  CLAY({
    .layout.sizing.width  = CLAY_SIZING_GROW(0),
    .layout.sizing.height = CLAY_SIZING_GROW(0),
  }) {
    if (ui_arrow_button(true)) *state = (*state == 0 ? option_count : *state) - 1;
    CLAY({ .layout.sizing.width  = CLAY_SIZING_GROW(0) });
    CLAY_TEXT(labels[*state], CLAY_TEXT_CONFIG(text_conf));
    CLAY({ .layout.sizing.width  = CLAY_SIZING_GROW(0) });
    if (ui_arrow_button(false)) *state = (*state + 1) % option_count;
  }
}

static void ui_main(void) {
  bool mouse_up = Clay_GetCurrentContext()->pointerInfo.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME;
  if (mouse_up) gui.lmb_click_el = (Clay_ElementId) { 0 };
  gui.lmb_click = Clay_GetCurrentContext()->pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME;

  /* HUD */
  {
    CLAY({
      .id = CLAY_ID("HeaderBar"),
      .layout = {
        .sizing = { .width = CLAY_SIZING_GROW(0) },
        .padding = CLAY_PADDING_ALL(4),
        .childGap = 16,
        .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }
      },
    }) {
      CLAY({
        .id = CLAY_ID("OptionsToggle"),
        .border = { .color = wood, .width = { 2, 2, 2, 2 }},
        .backgroundColor = Clay_Hovered() ? paper_hover : paper,
        .layout.sizing = { CLAY_SIZING_FIXED(32), CLAY_SIZING_FIXED(32) },
        .cornerRadius = CLAY_CORNER_RADIUS(16),
      }) {
        if (Clay_Hovered() && gui.lmb_click) {
          gui.options.open ^= 1;
        }

        /* icon here */
        CLAY({
          .layout.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
          .layout.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
        }) {
          ui_icon(gl_Model_UiOptions, 22);
        }
      }
    }
  }

  /* options window */
  if (gui.options.open) {
    CLAY({
      .id = CLAY_ID("OptionsWindow"),
      .floating.attachTo = CLAY_ATTACH_TO_ROOT,
      .layout.layoutDirection = CLAY_LEFT_TO_RIGHT,
      .floating.offset = { 200, 150 },
    }) {

      Clay_ElementDeclaration floating = {
        .floating.attachTo = CLAY_ATTACH_TO_PARENT,
        .floating.attachPoints.element = CLAY_ATTACH_POINT_CENTER_CENTER,
        .floating.attachPoints.parent = CLAY_ATTACH_POINT_CENTER_CENTER,
        .floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH
      };

      Clay_ElementDeclaration bg = floating;
      bg.floating.attachPoints.element = CLAY_ATTACH_POINT_LEFT_TOP;
      bg.floating.offset.x = -53;
      bg.floating.offset.y = -94;
      CLAY(bg) {
        float ar = (float)270 * (float)model_UiWindowBg_size_x / (float)model_UiWindowBg_size_y;
        ui_icon(gl_Model_UiWindowBg, ar);
      }

      Clay_ElementDeclaration corner = floating;
      corner.floating.offset.x = -20;
      corner.floating.offset.y =   5;
      CLAY(corner) {
        float ar = (float)75 * (float)model_UiWindowCorner_size_x / (float)model_UiWindowCorner_size_y;
        ui_icon(gl_Model_UiWindowCorner, ar);
        CLAY(floating) { ui_icon(gl_Model_UiOptions, 50); }
      }

      Clay_ElementDeclaration top = floating;
      top.floating.attachPoints.element = CLAY_ATTACH_POINT_LEFT_CENTER;
      CLAY(top) {
        float ar = (float)50 * (float)model_UiWindowTop_size_x / (float)model_UiWindowTop_size_y;
        ui_icon(gl_Model_UiWindowTop, ar);

        Clay_ElementDeclaration top_content = floating;
        top_content.floating.offset.x = 26;
        top_content.floating.offset.y = 2;
        top_content.layout.sizing.width = CLAY_SIZING_FIXED(250);
        CLAY(top_content) {
          CLAY_TEXT(CLAY_STRING("OPTIONS"), CLAY_TEXT_CONFIG({ .fontSize = 30, .textColor = ink }));

          CLAY({ .layout.sizing = { CLAY_SIZING_GROW(0) } });

          CLAY({ .layout.padding.top = 4 }) {
            size_t icon_size = 20;
            if (Clay_Hovered()) {
              icon_size = 22;
              if (gui.lmb_click) gui.options.open ^= 1;
            }
            ui_icon(gl_Model_UiEcksButton, icon_size);
          }; 
        }
      }

      Clay_ElementDeclaration border = floating;
      border.floating.attachPoints.element = CLAY_ATTACH_POINT_LEFT_TOP;
      border.floating.offset.x = -50;
      border.floating.offset.y = -66;
      CLAY(border) {
        float ar = (float)215 * (float)model_UiWindowBorder_size_x / (float)model_UiWindowBorder_size_y;
        ui_icon(gl_Model_UiWindowBorder, ar);

        CLAY({
          .floating.attachTo = CLAY_ATTACH_TO_PARENT,
          .floating.attachPoints.element = CLAY_ATTACH_POINT_LEFT_TOP,
          .floating.attachPoints.parent = CLAY_ATTACH_POINT_LEFT_TOP,
          .floating.offset.y = 80,
          .floating.offset.x = 5,
          .floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
          .layout.sizing = { CLAY_SIZING_FIXED(382), CLAY_SIZING_FIXED(220) },
          .layout.padding = { 24, 24, 24, 24 },
          // .border = { .color = wood, .width = { 3, 3, 3, 3 }},
        }) {

          CLAY({ .layout.sizing.height = CLAY_SIZING_FIXED(30) });

          CLAY({
              .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM },
              .layout.sizing.width = CLAY_SIZING_GROW(0)
          }) {

            Clay_ElementDeclaration pair = {
              .layout.sizing.width = CLAY_SIZING_GROW(0),
              .layout.padding.top = 20,
              .layout.padding.bottom = 20,
            };

            Clay_ElementDeclaration pair_inner = {
              .layout.sizing.width = CLAY_SIZING_GROW(0),
              .layout.sizing.height = CLAY_SIZING_GROW(0),
            };

            Clay_TextElementConfig label = {
              .fontSize = text_body,
              .textColor = ink
            };

            CLAY(pair) {

              CLAY(pair_inner) {
                CLAY_TEXT(CLAY_STRING("PERSPECTIVE"), CLAY_TEXT_CONFIG(label));
              }

              CLAY({ .layout.sizing = { .width = CLAY_SIZING_GROW(0) } }) {
                CLAY({ .layout.sizing.width = CLAY_SIZING_GROW(0) });
                ui_checkbox(&gui.options.perspective);
                CLAY({ .layout.sizing.width = CLAY_SIZING_GROW(0) });
              }

            }

            CLAY(pair) {
              CLAY(pair_inner) { CLAY_TEXT(CLAY_STRING("FOV"), CLAY_TEXT_CONFIG(label)); }
              CLAY(pair_inner) { ui_slider(CLAY_ID("FOV_SLIDER"), &gui.options.fov); }
            }

            CLAY(pair) {
              CLAY(pair_inner) { CLAY_TEXT(CLAY_STRING("ANTIALIASING"), CLAY_TEXT_CONFIG(label)); }; 
              Clay_String labels[] = {
                [gl_AntiAliasingApproach_None  ] = CLAY_STRING("NONE"),
                [gl_AntiAliasingApproach_Linear] = CLAY_STRING("Linear"),
                [gl_AntiAliasingApproach_FXAA  ] = CLAY_STRING("FXAA"),
                [gl_AntiAliasingApproach_2XSSAA] = CLAY_STRING("2x SSAA"),
                [gl_AntiAliasingApproach_4XSSAA] = CLAY_STRING("4x SSAA"),
              };
              CLAY(pair_inner) { ui_picker(&gui.options.antialiasing, gl_AntiAliasingApproach_COUNT, labels); }; 
            }
          }
        };
      }
    }
  }
}
