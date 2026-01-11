// vim: sw=2 ts=2 expandtab smartindent

#include <stdlib.h>
#include <stddef.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include "gl_include/gles3.h"

#define UNUSED_FN __attribute__((unused))
#define BREAKPOINT() __builtin_debugtrap()
#define jx_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

#include "math.h"

#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "font.h"

/* enables extra things in the options, escape to quit, etc. */
#define GAME_DEBUG true

/* key actions, abstracted away from scancodes so we can support bindings */
typedef enum {
  KeyAction_Up,
  KeyAction_Down,
  KeyAction_Left,
  KeyAction_Right,
  KeyAction_COUNT
} KeyAction;

#include "gl.h"
#include "gui.h"
#include "cad.h"

#include "geometry_assets.h"
#include "anim.h"

static struct {
  struct {
    SDL_Window    *window;
    SDL_GLContext  gl_ctx;
    SDL_Cursor    *cursor_move, *cursor_default, *cursor_doable, *cursor_sideways;
    /* dis one special, it's what I set the cursor to at the end of
     * the frame because I'm paranoid about calling SDL_SetCursor
     * more than once a frame */
    SDL_Cursor *cursor_next;
  } sdl;

  /* gui */
  gui_State gui;

  /* input */
  size_t win_size_x, win_size_y;
  KeyAction key_actions[KeyAction_COUNT];
  float mouse_screen_x, mouse_screen_y;
  /* raw_mouse_lmb_down may get captured by the UI and not go to mouse_lmb_down */
  bool raw_mouse_lmb_down, mouse_lmb_down;
  /* this is different than mouse_screen_x because dynamic ui scale;
   * screen x/y is in raw screen coordinates */
  float mouse_ui_x, mouse_ui_y, mouse_ui_lmb_down_x, mouse_ui_lmb_down_y;
  /* mouse projected onto the ground plane at z=0
   * used for knowing where to build things */
  f3 mouse_ground;

  /* timekeeping */
  uint64_t ts_last_frame, ts_first;
  double elapsed;

  /* camera goes from 3D world -> 2D in -1 .. 1
   * useful for ... knowing where to put a 3D thing on the screen
   *
   * screen goes from 2D in 0 ... window_size_x/window_size_y to -1 ... 1,
   * useful for laying things out in pixels, mouse picking etc.
   */
  f4x4 camera, screen, ui_transform;
  float gui_scale;

  /* sim, short for "simulation," stores things related to the
   * gameplay, physics and combat. */
  struct {
    struct {
      f2 pos;

      float heading_from_rads, heading_to_rads;
      double heading_from_ts, heading_to_ts;
    } player;

  } sim;

  /* cad stores the state for the construction mode, where
   * you build, tweak and upgrade walls, turrets and traps */
  cad_State cad;

  /* renderer ("gl") */
  gl_State gl;

} jeux = {
  .win_size_x = 800,
  .win_size_y = 450,
  .gui_scale = 0.7f,

  .gl.pp.current_aa = gl_AntiAliasingApproach_4XSSAA,
  .gl.camera.fov = 100.0f,
  .gl.camera.dist = 5.0f,
  .gl.camera.perspective = true,
  .gl.camera.angle = 0.0f,
  .gl.camera.height = 1.0f,

  .gl.light.angle = 0.35f,
  .gl.light.height = 1.36f,

  .gui = {
    // .options.window.open = true,
    .options.window.x = 142,
    .options.window.y = 68,
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
static f3 jeux_screen_to_ui(f3 p) {
  p = f4x4_transform_f3(jeux.screen, p);
  p = f4x4_transform_f3(f4x4_invert(jeux.ui_transform), p);
  return p;
}
static f3 jeux_ui_to_viewport(f3 p) {
  float size_x = jeux.gl.pp.phys_win_size_x*jeux.gl.pp.fb_scale;
  float size_y = jeux.gl.pp.phys_win_size_y*jeux.gl.pp.fb_scale;
  f4x4 viewport = f4x4_ortho(
     0.0f, size_x,
     0.0f, size_y,
    -1.0f,  1.0f
  );
  p = f4x4_transform_f3(jeux.ui_transform, p);
  p = f4x4_transform_f3(f4x4_invert(viewport), p);
  return p;
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

    /* TODO: do custom assets for these https://wiki.libsdl.org/SDL3/SDL_CreateColorCursor */
    jeux.sdl.cursor_move     = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
    jeux.sdl.cursor_default  = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    jeux.sdl.cursor_doable   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    jeux.sdl.cursor_sideways = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);

    /* TODO: SDL_SYSTEM_CURSOR_NOT_ALLOWED exists, could be useful sometimes */;
  }

  jeux.ts_last_frame = SDL_GetPerformanceCounter();
  jeux.ts_first = SDL_GetPerformanceCounter();

  {
    SDL_AppResult gl_init_res = gl_init();
    if (gl_init_res != SDL_APP_CONTINUE) return gl_init_res;
  }

  /* gui init - need to init gl first so that the
   * ui matrix thingy is initialized */
  gui_init();

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  /* window lifecycle */
  {
    if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;

    if (event->type == SDL_EVENT_WINDOW_RESIZED) {
      jeux.win_size_x = event->window.data1;
      jeux.win_size_y = event->window.data2;
      gl_resize();

      {
        float x = (float) event->window.data1;
        float y = (float) event->window.data2;
        f3 ui = jeux_screen_to_ui((f3) { x, y, 0 });

        Clay_SetLayoutDimensions((Clay_Dimensions) { ui.x, ui.y });
      }
    }
  }

  /* mouse/keyboard input */
  {
    if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
      jeux.raw_mouse_lmb_down = !(event->button.button == SDL_BUTTON_LEFT);
    }
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
      jeux.raw_mouse_lmb_down = event->button.button == SDL_BUTTON_LEFT;

      f3 ui = jeux_screen_to_ui((f3) { event->button.x, event->button.y, 0 });
      jeux.mouse_ui_lmb_down_x = ui.x;
      jeux.mouse_ui_lmb_down_y = ui.y;
    }
    if (event->type == SDL_EVENT_MOUSE_MOTION) {
      jeux.mouse_screen_x = event->motion.x;
      jeux.mouse_screen_y = event->motion.y;

      f3 ui = jeux_screen_to_ui((f3) { event->motion.x, event->motion.y, 0 });
      jeux.mouse_ui_x = ui.x;
      jeux.mouse_ui_y = ui.y;
    }
    if (event->type == SDL_EVENT_MOUSE_WHEEL) {
      Clay_UpdateScrollContainers(true, (Clay_Vector2) { event->wheel.x, event->wheel.y }, 0.01f);
    }

    /* https://wiki.libsdl.org/SDL3/BestKeyboardPractices */
    if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) {
      bool down = event->type == SDL_EVENT_KEY_DOWN;
      if (event->key.scancode == SDL_SCANCODE_W) jeux.key_actions[KeyAction_Up   ] = down;
      if (event->key.scancode == SDL_SCANCODE_S) jeux.key_actions[KeyAction_Down ] = down;
      if (event->key.scancode == SDL_SCANCODE_A) jeux.key_actions[KeyAction_Left ] = down;
      if (event->key.scancode == SDL_SCANCODE_D) jeux.key_actions[KeyAction_Right] = down;

      if (event->key.scancode == SDL_SCANCODE_UP   ) jeux.key_actions[KeyAction_Up   ] = down;
      if (event->key.scancode == SDL_SCANCODE_DOWN ) jeux.key_actions[KeyAction_Down ] = down;
      if (event->key.scancode == SDL_SCANCODE_LEFT ) jeux.key_actions[KeyAction_Left ] = down;
      if (event->key.scancode == SDL_SCANCODE_RIGHT) jeux.key_actions[KeyAction_Right] = down;
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

SDL_AppResult SDL_AppIterate(void *appstate) {
  /* timekeeping */
  {
    uint64_t ts_now = SDL_GetPerformanceCounter();
    // double delta_time = (double)(ts_now - jeux.ts_last_frame) / (double)SDL_GetPerformanceFrequency();
    jeux.ts_last_frame = ts_now;

    jeux.elapsed = (double)(ts_now - jeux.ts_first) / (double)SDL_GetPerformanceFrequency();
  }

  {
    /* this gives us effectively an immediate mode API
     * for cursor setting; if you don't want the cursor
     * to be the default, set it that way every frame. */
    jeux.sdl.cursor_next = jeux.sdl.cursor_default;
  }

  /* construct the jeux.camera for this frame */
  {
    /* camera */
    jeux.camera = (f4x4) {0};

    {
      float dist = jeux.gl.camera.dist;
      float height = jeux.gl.camera.height;

      float x = cosf(jeux.gl.camera.angle) * dist;
      float y = sinf(jeux.gl.camera.angle) * dist;
      jeux.gl.camera.eye = (f3) { x, y, height * dist };
    }

    {
      float ar = (float)(jeux.win_size_y) / (float)(jeux.win_size_x);
      f4x4 projection;
      if (jeux.gl.camera.perspective)
        projection = f4x4_perspective(
          jeux.gl.camera.fov * M_PI / 180.0f,
          ar,
          0.1f, 30.0f
        );
      else
        projection = f4x4_ortho(
           -7.0f     ,  7.0f     ,
           -7.0f * ar,  7.0f * ar,
          -20.0f     , 20.0f
        );

      f4x4 orbit = f4x4_target_to(
        jeux.gl.camera.eye,
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

    /* MARK: draw ui */
    {
      Clay_RenderCommandArray cmds;

      /* Because of my proclivity towards using Clay_Hovered() instead of
       * Clay_OnHover (because I think the callback is ugly), we have
       * to layout 3 times/frame to prevent there from being any instability */
      for (int i = 0; i < 3; i++) {
        Clay_SetPointerState(
          (Clay_Vector2) { jeux.mouse_ui_x, jeux.mouse_ui_y },
          jeux.raw_mouse_lmb_down
        );

        Clay_BeginLayout();

        gui_frame();

        cmds = Clay_EndLayout();
      }

      jeux.mouse_lmb_down = false;
      SDL_Log("mouse_capture = %d", (int)jeux.gui.capture_mouse);
      if (!jeux.gui.capture_mouse) {
        jeux.mouse_lmb_down = jeux.raw_mouse_lmb_down;
      }

      gl_draw_clay_commands(&cmds);
    }

    /* debug */
    {
      float debug_thickness = jeux.win_size_x * 0.00225f;

      /* debug where we think the mouse is */
      if (1) {

        /* draw an X at the mouse's 2D position */
        if (0) {
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

    /* draw player-constructed geometry */
    cad_frame();

    /* draw figure */
    if (1) {
      animdata_Frame *turn_frames;
      size_t turn_frame_count;
      float turn_anim_duration;

      bool left = rads_distance(jeux.sim.player.heading_from_rads, jeux.sim.player.heading_to_rads) < 0;

      if (left) {
        turn_frames = animdata_turn90_left_frames;
        turn_frame_count = jx_COUNT(animdata_turn90_left_frames);
        turn_anim_duration = animdata_turn90_left_duration;
      } else {
        turn_frames = animdata_turn90_right_frames;
        turn_frame_count = jx_COUNT(animdata_turn90_right_frames);
        turn_anim_duration = animdata_turn90_right_duration;
      }

      animdata_Frame *walk_frames = animdata_walk_frames;
      size_t walk_frame_count = jx_COUNT(animdata_walk_frames);
      float walk_anim_duration = animdata_walk_duration;

      f4x4 model = f4x4_scale(1.0f);

      /* here we figure out progress for the turn animation,
       * and fade out either end of it via return_anim_t */
      float turn_anim_t, return_anim_t;
      {
        float turn_t = clamp(0, 1, inv_lerp(
          jeux.sim.player.heading_from_ts,
          jeux.sim.player.heading_to_ts,
          jeux.elapsed
        ));

        /* as you go from (1 - ((n - 1)/n)) going to 1 it starts to wrap back around to the first frame */
        float loopless_duration = turn_anim_duration * (((float)turn_frame_count - 1.0f) / (float)turn_frame_count);
        turn_anim_t = turn_t * loopless_duration;
        float turn_anim_begin_t  = clamp(0, 1, inv_lerp(loopless_duration*0.3,                 0, turn_anim_t));
        float turn_anim_finish_t = clamp(0, 1, inv_lerp(loopless_duration*0.7, loopless_duration, turn_anim_t));
        return_anim_t = fmaxf(turn_anim_begin_t, turn_anim_finish_t);
        float return_t = clamp(0, 1, return_anim_t / loopless_duration);

        model = f4x4_mul_f4x4(model, f4x4_turn(-rads_lerp(
            jeux.sim.player.heading_from_rads,
            jeux.sim.player.heading_to_rads,
            turn_t
        ) + M_PI*0.5f*turn_t*(left ? -1 : 1)*(1.0f - return_t) ));
      }

      /* figure out the positions of all the joints for this frame */
      f3 joint_pos[animdata_JointKey_COUNT] = {0};
      {
        for (int joint_i = 0; joint_i < animdata_JointKey_COUNT; joint_i++) {
          float walk_anim_t = fmod(jeux.elapsed, (double)walk_anim_duration);
          f3 walk = animdata_sample(walk_frames, walk_frame_count, walk_anim_duration, joint_i, walk_anim_t);
          f3 turn = animdata_sample(turn_frames, turn_frame_count, turn_anim_duration, joint_i, turn_anim_t);
          joint_pos[joint_i] = f3_lerp(
            turn,
            walk,
            return_anim_t
          );
        }
      }

      /* draw lines between connected joints */
      for (int i = 0; i < jx_COUNT(animdata_limb_connections); i++) {
        animdata_JointKey from = animdata_limb_connections[i].from,
                            to = animdata_limb_connections[i].to;
        f3 a = jeux_world_to_screen(f4x4_transform_f3(model, joint_pos[from]));
        f3 b = jeux_world_to_screen(f4x4_transform_f3(model, joint_pos[  to]));

        float thickness = jeux.win_size_x * 0.006f;

        Color color = { 1, 1, 1, 255 };
        gl_geo_line(a, b, thickness, color);

        gl_geo_circle(8, a, thickness * 0.5f, color);
        gl_geo_circle(8, b, thickness * 0.5f, color);
      }

      /* draw head */
      {
        f3 head = joint_pos[animdata_JointKey_Head];

        /* head assets are 2x2x2 centered around (0, 0, 0) */
        float radius = 0.175f;

        /* if this is 0.8f, a perfectly (0, 0, 1) aligned neck will penetrate 20% */
        head.z += radius*0.8f;

        f4x4 matrix = model;
        matrix = f4x4_mul_f4x4(matrix, f4x4_move(head));
        /* the animations seem to be exported with the negative X axis as "forward," so ... */
        matrix = f4x4_mul_f4x4(matrix, f4x4_turn(-M_PI * 0.5f));
        matrix = f4x4_mul_f4x4(matrix, f4x4_scale(radius));

        *jeux.gl.geo.model_draws_wtr++ = (gl_ModelDraw) { .model = gl_Model_Head, .matrix = matrix };
        *jeux.gl.geo.model_draws_wtr++ = (gl_ModelDraw) { .model = gl_Model_HornedHelmet, .matrix = matrix };
      }

    }

    if (0) gl_text_draw(
      "hi! i'm ced?",
      jeux.win_size_x * 0.5,
      jeux.win_size_y * 0.5,
      24.0f
    );
  }

  /* render */
  gl_render();

  SDL_GL_SwapWindow(jeux.sdl.window);

  SDL_SetCursor(jeux.sdl.cursor_next);
  return SDL_APP_CONTINUE;
}

#define gui_IMPLEMENTATION
#include "gui.h"

#define cad_IMPLEMENTATION
#include "cad.h"

#define gl_IMPLEMENTATION
#include "gl.h"
