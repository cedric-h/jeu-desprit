// vim: sw=2 ts=2 expandtab smartindent

#include <stdlib.h>
#include <stddef.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include "gl_include/gles3.h"

#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "clay_demo.h"
#include "font.h"

#define GAME_DEBUG true

#define BREAKPOINT() __builtin_debugtrap()
#define jx_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
typedef struct { float x, y; } f2;
typedef struct { float x, y, z; } f3;
typedef union { float arr[4]; struct { float x, y, z, w; } p; f3 xyz; } f4;
typedef union { float arr[4][4]; f4 rows[4]; float floats[16]; } f4x4;

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

typedef struct { float x, y, u, v, size; } gl_text_Vtx;
typedef struct { uint8_t r, g, b, a; } Color;
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
  gl_Model_COUNT,
} gl_Model;

#include "../models/include/head.h"
#include "../models/include/HornedHelmet.h"
#include "../models/include/IntroGravestoneTerrain.h"
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
};

typedef struct {
  gl_Model model;
  /* just a model matrix, (view and projection get applied for you) */
  f4x4 matrix;
} gl_ModelDraw;

static struct {
  struct {
    SDL_Window    *window;
    SDL_GLContext  gl_ctx;
  } sdl;

  /* input */
  size_t win_size_x, win_size_y;
  float mouse_screen_x, mouse_screen_y;
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
      gl_geo_Vtx vtx[999];
      gl_geo_Vtx *vtx_wtr;

      gl_Tri idx[999];
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
      GLint shader_a_pos;

      /* resources inside here need to be recreated
       * when the application window is resized. */
      struct {
        /* postprocessing framebuffer (anti-aliasing and other fx) */
        GLuint pp_tex_color, pp_tex_depth, pp_fb;
      } screen;
    } pp;

    struct {
      gl_text_Vtx vtx[999999];
      gl_text_Vtx *vtx_wtr;

      gl_Tri idx[999999];
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
    } text;

  } gl;

  struct {
    ClayVideoDemo_Data demo_data;
  } gui;

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
static void gui_handle_errors(Clay_ErrorData error_data) { SDL_Log("%s\n", error_data.errorText.chars); }
static Clay_Dimensions gui_measure_text(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
  float size = config->fontSize;
  float scale = (size / font_BASE_CHAR_SIZE);

  float size_x = 0;
  float size_y = 0;
  for (int i = 0; i < text.length; i++) {
    char c = text.chars[i] | (1 << 5); /* this is a caps-only font, so atlas only has lowercase */
    font_LetterRegion *l = &font_letter_regions[(size_t)(c)];

    size_x += l->advance * scale;
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

    jeux.gui.demo_data = ClayVideoDemo_Initialize();
  }

  return gl_init();
}

static void gl_resize(void);
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;

  if (event->type == SDL_EVENT_MOUSE_MOTION) {
    jeux.mouse_screen_x = event->motion.x;
    jeux.mouse_screen_y = jeux.win_size_y - event->motion.y;
  }

  if (event->type == SDL_EVENT_WINDOW_RESIZED) {
    jeux.win_size_x = event->window.data1;
    jeux.win_size_y = event->window.data2;
    gl_resize();
  }

  if (event->type == SDL_EVENT_KEY_UP) {
#if GAME_DEBUG
    if (event->key.key == SDLK_ESCAPE) {
      return SDL_APP_SUCCESS;
    }
#endif
  }

  /* clay event handling */
  switch (event->type) {

    case SDL_EVENT_WINDOW_RESIZED: {
      Clay_SetLayoutDimensions(
        (Clay_Dimensions) { (float) event->window.data1, (float) event->window.data2 }
      );
    } break;

    case SDL_EVENT_MOUSE_MOTION: {
      Clay_SetPointerState(
        (Clay_Vector2) { event->motion.x, event->motion.y },
        event->motion.state & SDL_BUTTON_LMASK
      );
    } break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
      Clay_SetPointerState(
        (Clay_Vector2) { event->button.x, event->button.y },
        event->button.button == SDL_BUTTON_LEFT
      );
    } break;

    case SDL_EVENT_MOUSE_WHEEL: {
      Clay_UpdateScrollContainers(true, (Clay_Vector2) { event->wheel.x, event->wheel.y }, 0.01f);
    } break;

    default: {
    } break;

  };

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

          "  vec3 light_dir = normalize(vec3(4.0, 1.5, 5.9));\n"
          "  float diffuse = max(dot(v_normal, light_dir), 0.0);\n"
          "  float ramp = 0.0;\n"
          "       if (diffuse > 0.923) ramp = 1.00;\n"
          "  else if (diffuse > 0.477) ramp = 0.50;\n"
          "  vec3 desaturated = vec3(dot(v_color.xyz, vec3(0.2126, 0.7152, 0.0722)));\n"
          "  vec3 color = mix(desaturated, v_color.xyz, min(1.0, 0.2 + ramp));\n"
          "  gl_FragColor = vec4(color * mix(0.8, 1.6, ramp), v_color.a);\n"
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
          "uniform mat4 u_mvp;\n"
          "uniform vec2 u_tex_size;\n"
          "uniform float u_gamma;\n"
          "\n"
          "varying vec2 v_uv;\n"
          "varying float v_gamma;\n"
          "\n"
          "void main() {\n"
          "  gl_Position = u_mvp * vec4(a_pos, 0.0, 1.0);\n"
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
    *vtx_wtr++ = (gl_text_Vtx) { x + size_x,  y         , l->x + l->size_x, l->y            , size };
    *vtx_wtr++ = (gl_text_Vtx) { x + size_x,  y + size_y, l->x + l->size_x, l->y + l->size_y, size };
    *vtx_wtr++ = (gl_text_Vtx) { x         ,  y + size_y, l->x            , l->y + l->size_y, size };
    *vtx_wtr++ = (gl_text_Vtx) { x         ,  y         , l->x            , l->y            , size };

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
  float screen_x,
  float screen_y,
  float size,
  Box2 clip,
  Color color
) {
  gl_text_Vtx *vtx_wtr = jeux.gl.text.vtx_wtr;
  gl_Tri      *idx_wtr = jeux.gl.text.idx_wtr;

  float scale = (size / font_BASE_CHAR_SIZE);

  float pen_x = screen_x;
  float pen_y = screen_y + size * 0.75f; /* little adjustment */
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

    *vtx_wtr++ = (gl_text_Vtx) { max_x, min_y, max_u, min_v, size };
    *vtx_wtr++ = (gl_text_Vtx) { max_x, max_y, max_u, max_v, size };
    *vtx_wtr++ = (gl_text_Vtx) { min_x, max_y, min_u, max_v, size };
    *vtx_wtr++ = (gl_text_Vtx) { min_x, min_y, min_u, min_v, size };

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

static void gl_geo_circle(size_t detail, f3 center, float radius, Color color) {

  uint16_t start = jeux.gl.geo.vtx_wtr - jeux.gl.geo.vtx;

  /* center of the triangle fan */
  *jeux.gl.geo.vtx_wtr++ = (gl_geo_Vtx) { .pos = center, .color = color };

  for (int i = 0; i <= detail; i++) {
    float t = (float)i / (float)detail * M_PI * 2.0f;
    float x = center.x + cosf(t) * radius;
    float y = center.y + sinf(t) * radius;
    *jeux.gl.geo.vtx_wtr++ = (gl_geo_Vtx) { { x, y, center.z }, color };

    if (i > 0) *jeux.gl.geo.idx_wtr++ = (gl_Tri) { start, start + i, start + i + 1 };
  }
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

static f3 jeux_world_to_screen(f3 p) {
  p = f4x4_transform_f3(jeux.camera, p);
  /* p is in -1 .. 1 now */
  p = f4x4_transform_f3(f4x4_invert(jeux.screen), p);
  return p;
}

static f3 jeux_screen_to_world(f3 p) {
  /* p is in -1 .. 1 now */
  p = f4x4_transform_f3(jeux.screen, p);

  p = f4x4_transform_f3(f4x4_invert(jeux.camera), p);
  return p;
}

static void gl_geo_ring(size_t detail, f3 center, float radius, float thickness, Color color) {
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

static void gl_geo_box_outline(f3 center, f3 scale, float thickness, Color color) {

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

static void gl_draw_clay_commands(Clay_RenderCommandArray *rcommands) {
  Box2 clip = BOX2_UNCONSTRAINED;

  for (size_t i = 0; i < rcommands->length; i++) {
    Clay_RenderCommand *rcmd = Clay_RenderCommandArray_Get(rcommands, i);
    Clay_BoundingBox rect = rcmd->boundingBox;

    switch (rcmd->commandType) {

      case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
        Clay_RectangleRenderData *config = &rcmd->renderData.rectangle;

        gl_geo_box(
          (f3) { rect.x             , rect.y              , 0.5f },
          (f3) { rect.x + rect.width, rect.y + rect.height, 0.5f },
          (Color) {
            config->backgroundColor.r,
            config->backgroundColor.g,
            config->backgroundColor.b,
            config->backgroundColor.a
          }
        );

        /* NOTE: we're ignoring
         * config->cornerRadius.topLeft */
      } break;

      case CLAY_RENDER_COMMAND_TYPE_TEXT: {
        Clay_TextRenderData *config = &rcmd->renderData.text;
        gl_text_draw_ex(
          config->stringContents.chars,
          config->stringContents.length,
          rect.x,
          rect.y,
          config->fontSize,
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
        // Clay_BorderRenderData *config = &rcmd->renderData.border;

        // const float minRadius = SDL_min(rect.w, rect.h) / 2.0f;
        // const Clay_CornerRadius clampedRadii = {
        //   .topLeft = SDL_min(config->cornerRadius.topLeft, minRadius),
        //   .topRight = SDL_min(config->cornerRadius.topRight, minRadius),
        //   .bottomLeft = SDL_min(config->cornerRadius.bottomLeft, minRadius),
        //   .bottomRight = SDL_min(config->cornerRadius.bottomRight, minRadius)
        // };
        // //edges
        // SDL_SetRenderDrawColor(rendererData->renderer, config->color.r, config->color.g, config->color.b, config->color.a);
        // if (config->width.left > 0) {
        //   const float starting_y = rect.y + clampedRadii.topLeft;
        //   const float length = rect.h - clampedRadii.topLeft - clampedRadii.bottomLeft;
        //   SDL_FRect line = { rect.x, starting_y, config->width.left, length };
        //   SDL_RenderFillRect(rendererData->renderer, &line);
        // }
        // if (config->width.right > 0) {
        //   const float starting_x = rect.x + rect.w - (float)config->width.right;
        //   const float starting_y = rect.y + clampedRadii.topRight;
        //   const float length = rect.h - clampedRadii.topRight - clampedRadii.bottomRight;
        //   SDL_FRect line = { starting_x, starting_y, config->width.right, length };
        //   SDL_RenderFillRect(rendererData->renderer, &line);
        // }
        // if (config->width.top > 0) {
        //   const float starting_x = rect.x + clampedRadii.topLeft;
        //   const float length = rect.w - clampedRadii.topLeft - clampedRadii.topRight;
        //   SDL_FRect line = { starting_x, rect.y, length, config->width.top };
        //   SDL_RenderFillRect(rendererData->renderer, &line);
        // }
        // if (config->width.bottom > 0) {
        //   const float starting_x = rect.x + clampedRadii.bottomLeft;
        //   const float starting_y = rect.y + rect.h - (float)config->width.bottom;
        //   const float length = rect.w - clampedRadii.bottomLeft - clampedRadii.bottomRight;
        //   SDL_FRect line = { starting_x, starting_y, length, config->width.bottom };
        //   SDL_SetRenderDrawColor(rendererData->renderer, config->color.r, config->color.g, config->color.b, config->color.a);
        //   SDL_RenderFillRect(rendererData->renderer, &line);
        // }
        // //corners
        // if (config->cornerRadius.topLeft > 0) {
        //   const float centerX = rect.x + clampedRadii.topLeft -1;
        //   const float centerY = rect.y + clampedRadii.topLeft;
        //   SDL_Clay_RenderArc(rendererData, (SDL_FPoint){centerX, centerY}, clampedRadii.topLeft,
        //     180.0f, 270.0f, config->width.top, config->color);
        // }
        // if (config->cornerRadius.topRight > 0) {
        //   const float centerX = rect.x + rect.w - clampedRadii.topRight -1;
        //   const float centerY = rect.y + clampedRadii.topRight;
        //   SDL_Clay_RenderArc(rendererData, (SDL_FPoint){centerX, centerY}, clampedRadii.topRight,
        //     270.0f, 360.0f, config->width.top, config->color);
        // }
        // if (config->cornerRadius.bottomLeft > 0) {
        //   const float centerX = rect.x + clampedRadii.bottomLeft -1;
        //   const float centerY = rect.y + rect.h - clampedRadii.bottomLeft -1;
        //   SDL_Clay_RenderArc(rendererData, (SDL_FPoint){centerX, centerY}, clampedRadii.bottomLeft,
        //     90.0f, 180.0f, config->width.bottom, config->color);
        // }
        // if (config->cornerRadius.bottomRight > 0) {
        //   const float centerX = rect.x + rect.w - clampedRadii.bottomRight -1; //TODO: why need to -1 in all calculations???
        //   const float centerY = rect.y + rect.h - clampedRadii.bottomRight -1;
        //   SDL_Clay_RenderArc(rendererData, (SDL_FPoint){centerX, centerY}, clampedRadii.bottomRight,
        //     0.0f, 90.0f, config->width.bottom, config->color);
        // }

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
        SDL_Log("does anyone use this?\n");
        // SDL_Surface *image = (SDL_Surface *)rcmd->renderData.image.imageData;
        // SDL_Texture *texture = SDL_CreateTextureFromSurface(rendererData->renderer, image);
        // const SDL_FRect dest = { rect.x, rect.y, rect.w, rect.h };

        // SDL_RenderTexture(rendererData->renderer, texture, NULL, &dest);
        // SDL_DestroyTexture(texture);
      } break;

      default:
        SDL_Log("Unknown render command type: %d", rcmd->commandType);
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
      Clay_RenderCommandArray cmds = ClayVideoDemo_CreateLayout(&jeux.gui.demo_data);
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
        gl_geo_ring(
          32,
          jeux.mouse_ground,
          0.2f,
          debug_thickness,
          (Color) { 255, 0, 0, 255 }
        );
      }

      /* lil ring around the player,
       * useful for debugging physics */
      gl_geo_ring(
        32,
        (f3) { 0.0f, 0.0f, -0.01f },
        0.5f,
        debug_thickness,
        (Color) { 200, 100, 20, 255 }
      );

#if 0
      /* draw player-sized box around (0, 0, 0) */
      gl_geo_box_outline(
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

      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_LEQUAL);

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

          f4x4 mvp = jeux.camera;
          mvp = f4x4_mul_f4x4(mvp, draw->matrix);
          glUniformMatrix4fv(jeux.gl.geo.shader_u_mvp, 1, 0, mvp.floats);

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
      glUniform2f(       jeux.gl.text.shader_u_tex_size, font_TEX_SIZE_X, font_TEX_SIZE_Y);
      glUniformMatrix4fv(jeux.gl.text.shader_u_mvp, 1, 0, jeux.screen.floats);
      glUniform1f(       jeux.gl.text.shader_u_buffer, 0.725);

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
