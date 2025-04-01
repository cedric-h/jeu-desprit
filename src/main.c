// vim: sw=2 ts=2 expandtab smartindent

#include <stdlib.h>

#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include "gl_include/gles3.h"

#include "font.h"

#define jx_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

#define ANTIALIAS_NONE    0
#define ANTIALIAS_LINEAR  1
#define ANTIALIAS_FXAA    2
#define ANTIALIAS_2xSSAA  3
#define ANTIALIAS_4xSSAA  4
#define CURRENT_ALIASING ANTIALIAS_4xSSAA

static struct {
  struct {
    SDL_Window    *window;
    SDL_GLContext  gl_ctx;
  } sdl;

  size_t window_size_x, window_size_y;

  struct {
    struct {
      GLuint pp; /* postprocessing */
      GLint pp_u_win_size;

      GLuint text;
      GLint text_u_texsize;
      GLint text_u_buffer;
      GLint text_u_gamma;
      GLint text_a_pos;

      GLuint geo;
    } shader;
    GLuint fullscreen_vtx;
    GLuint geo_vtx;

    GLuint text_vtx;
    GLuint text_tex;
    float text_scale;

    float fb_scale;

    /* resources inside here need to be recreated
     * when the application window is resized. */
    struct {
      /* postprocessing framebuffer (anti-aliasing and other fx) */
      GLuint pp_tex, pp_fb;
    } screen;
  } gl;
} jeux = {
  .window_size_x = 800,
  .window_size_y = 450,
  .gl = {
#if   CURRENT_ALIASING == ANTIALIAS_4xSSAA
    .fb_scale = 4.0f,
#elif CURRENT_ALIASING == ANTIALIAS_2xSSAA
    .fb_scale = 2.0f,
#else
    .fb_scale = 1.0f,
#endif

    .text_scale = 1.0f,
  }
};

static SDL_AppResult gl_init(void);
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {

  /* sdl init */
  {
    SDL_SetHint(SDL_HINT_APP_NAME, "jeu desprit");

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

    jeux.sdl.window = SDL_CreateWindow(
      "jeu desprit",
      jeux.window_size_x,
      jeux.window_size_y,
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

  jeux.window_size_x *= SDL_GetWindowPixelDensity(jeux.sdl.window);
  jeux.window_size_y *= SDL_GetWindowPixelDensity(jeux.sdl.window);

  return gl_init();
}

SDL_AppResult SDL_AppIterate(void *appstate) {

  /* render */
  {

    // {
    //   /* switch to the fb that gets postprocessing applied later */
    //   glViewport(0, 0, jeux.window_size_x*jeux.gl.fb_scale, jeux.window_size_y*jeux.gl.fb_scale);
    //   glBindFramebuffer(GL_FRAMEBUFFER, jeux.gl.screen.pp_fb);

    //   /* clear color */
    //   glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
    //   glClear(GL_COLOR_BUFFER_BIT);

    //   // glBindTexture(GL_TEXTURE_2D, jeux.gl.tex);
    //   glUseProgram(jeux.gl.shader.geo);
    //   glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.geo_vtx);
    //   glDrawArrays(GL_TRIANGLES, 0, 3);
    // }

    // /* stop writing to the framebuffer, start writing to the screen */
    glViewport(0, 0, jeux.window_size_x, jeux.window_size_y);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // /* draw the contents of the framebuffer with postprocessing/aa applied */
    // {
    //   glUseProgram(jeux.gl.shader.pp);
    //   glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.fullscreen_vtx);
    //   glBindTexture(GL_TEXTURE_2D, jeux.gl.screen.pp_tex);
    //   glUniform2f(jeux.gl.shader.pp_u_win_size, jeux.window_size_x, jeux.window_size_y);
    //   glDrawArrays(GL_TRIANGLES, 0, 3);
    // }

    /* draw text (after pp because it has its own AA) */
    {
      glUseProgram(jeux.gl.shader.text);

      glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.text_vtx);
      glEnableVertexAttribArray(jeux.gl.shader.text_a_pos);
      glVertexAttribPointer(jeux.gl.shader.text_a_pos, 4, GL_FLOAT, GL_FALSE, 0, 0);

      glBindTexture(GL_TEXTURE_2D, jeux.gl.text_tex);
      glUniform2f(jeux.gl.shader.text_u_texsize, 1.0, 1.0);
      glUniform1f(jeux.gl.shader.text_u_buffer, 0.55);

      float gamma = 2.0;
      glUniform1f(jeux.gl.shader.text_u_gamma,  gamma * 1.4142 / jeux.gl.text_scale);
      glDrawArrays(GL_TRIANGLES, 0, 3);
    }
  }

  SDL_GL_SwapWindow(jeux.sdl.window);
  return SDL_APP_CONTINUE;
}

static void gl_resize(void);
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
  if (event->type == SDL_EVENT_WINDOW_RESIZED) {
    jeux.window_size_x = event->window.data1*SDL_GetWindowPixelDensity(jeux.sdl.window);
    jeux.window_size_y = event->window.data2*SDL_GetWindowPixelDensity(jeux.sdl.window);
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
        .dst = &jeux.gl.shader.geo,
        .debug_name = "geo",
        .vs =
          "attribute vec4 a_pos;                      \n"
          "varying vec3 v_color;                      \n"
          "void main() {                              \n"
          "  gl_Position = vec4(a_pos.xyz, 1.0);      \n"
          "  v_color = gl_Position.xyz + vec3(0.5);   \n"
          "}                                          \n"
        ,
        .fs =
          "precision mediump float;               \n"
          "varying vec3 v_color;                  \n"
          "uniform sampler2D u_tex;               \n"
          "void main() {                          \n"
          "  gl_FragColor = vec4(v_color, 1.0);   \n"
          "}                                      \n"
      },
      {
        .dst = &jeux.gl.shader.text,
        .debug_name = "text",
        .vs =
          "attribute vec4 a_pos;\n"
          "\n"
          "uniform vec2 u_texsize;\n"
          "\n"
          "varying vec2 v_uv;\n"
          "\n"
          "void main() {\n"
          "  gl_Position = vec4(a_pos.xy, 0.0, 1.0);\n"
          "  v_uv = a_pos.zw / u_texsize;\n"
          "}\n"
        ,
        .fs =
          "precision mediump float;\n"
          "\n"
          "varying vec2 v_uv;\n"
          "\n"
          "uniform sampler2D u_tex;\n"
          "uniform float u_buffer;\n"
          "uniform float u_gamma;\n"
          "\n"
          "void main() {\n"
          "  float dist = texture2D(u_tex, v_uv).r;\n"
          "  float alpha = smoothstep(u_buffer - u_gamma, u_buffer + u_gamma, dist);\n"
          "  gl_FragColor = vec4(alpha);\n"
          "}\n"
      },
      {
        .dst = &jeux.gl.shader.pp,
        .debug_name = "pp",
        .vs =
          "attribute vec4 a_pos;                         \n"
          "varying vec2 v_uv;                            \n"
          "void main() {                                 \n"
          "  gl_Position = vec4(a_pos.xyz, 1.0);         \n"
          "  v_uv = gl_Position.xy*0.5 + vec2(0.5);      \n"
          "}                                             \n"
        ,
        .fs =

// No AA
#if CURRENT_ALIASING == ANTIALIAS_NONE || CURRENT_ALIASING == ANTIALIAS_LINEAR
          "precision mediump float;                                                                \n"
          "varying vec2 v_uv;                                                                      \n"
          "uniform sampler2D u_tex;                                                                \n"
          "uniform vec2 u_win_size;                                                                \n"
          "void main() {                                                                           \n"
          "  gl_FragColor = texture2D(u_tex, v_uv);\n"
          "}                                                                                       \n"
#elif CURRENT_ALIASING == ANTIALIAS_2xSSAA
          "precision mediump float;                                                                \n"
          "varying vec2 v_uv;                                                                      \n"
          "uniform sampler2D u_tex;                                                                \n"
          "uniform vec2 u_win_size;                                                                \n"
          "void main() {                                                                           \n"
          "  vec2 inv_vp = 1.0 / u_win_size;\n"
          "  gl_FragColor = vec4(0, 0, 0, 1);\n"
          "  gl_FragColor.xyz += 0.25*texture2D(u_tex, v_uv + (vec2(-0.25, -0.25) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.25*texture2D(u_tex, v_uv + (vec2(+0.25, -0.25) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.25*texture2D(u_tex, v_uv + (vec2(-0.25, +0.25) * inv_vp)).xyz;\n"
          "  gl_FragColor.xyz += 0.25*texture2D(u_tex, v_uv + (vec2(+0.25, +0.25) * inv_vp)).xyz;\n"
          "}\n"
#elif CURRENT_ALIASING == ANTIALIAS_4xSSAA
          "precision mediump float;                                                                  \n"
          "varying vec2 v_uv;                                                                        \n"
          "uniform sampler2D u_tex;                                                                  \n"
          "uniform vec2 u_win_size;                                                                  \n"
          "void main() {                                                                             \n"
          "  vec2 inv_vp = 1.0 / u_win_size;                                                         \n"
          "  gl_FragColor = vec4(0, 0, 0, 1);                                                        \n"
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
          "}\n"
#elif CURRENT_ALIASING == ANTIALIAS_FXAA

          /* TODO: fix this https://github.com/LiveMirror/NVIDIA-Direct3D-SDK-11/blob/a2d3cc46179364c9faa3e218eff230883badcd79/FXAA/FxaaShader.h#L1 */

          "precision mediump float;                                                                \n"
          "varying vec2 v_uv;                                                                      \n"
          "uniform sampler2D u_tex;                                                                \n"
          "uniform vec2 u_win_size;                                                                \n"
          "void main() {                                                                           \n"
            "float FXAA_SPAN_MAX = 8.0;                                                            \n"
            "float FXAA_REDUCE_MUL = 1.0/8.0;                                                      \n"
            "float FXAA_REDUCE_MIN = (1.0/128.0);                                                  \n"

            "vec2 inv_vp = 1.0 / u_win_size;\n"
            "vec3 rgbNW = texture2D(u_tex, v_uv + (vec2(-0.5, -0.5) * inv_vp)).xyz;\n"
            "vec3 rgbNE = texture2D(u_tex, v_uv + (vec2(+0.5, -0.5) * inv_vp)).xyz;\n"
            "vec3 rgbSW = texture2D(u_tex, v_uv + (vec2(-0.5, +0.5) * inv_vp)).xyz;\n"
            "vec3 rgbSE = texture2D(u_tex, v_uv + (vec2(+0.5, +0.5) * inv_vp)).xyz;\n"
            "vec3 rgbM  = texture2D(u_tex, v_uv).xyz;                                 \n"

            "vec3 luma = vec3(0.299, 0.587, 0.114);                                                \n"
            "float lumaNW = dot(rgbNW, luma);                                                      \n"
            "float lumaNE = dot(rgbNE, luma);                                                      \n"
            "float lumaSW = dot(rgbSW, luma);                                                      \n"
            "float lumaSE = dot(rgbSE, luma);                                                      \n"
            "float lumaM  = dot( rgbM, luma);                                                      \n"
          
            "float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));            \n"
            "float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));            \n"
          
            "vec2 dir;                                                                             \n"
            "dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));                                     \n"
            "dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));                                     \n"
          
            "float dirReduce = max(                                                                \n"
            "  (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),                     \n"
            "  FXAA_REDUCE_MIN                                                                     \n"
            ");                                                                                    \n"

            "float rcpDirMin = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);                      \n"
          
            "dir = min(\n"
            "  vec2(FXAA_SPAN_MAX,  FXAA_SPAN_MAX),                                        \n"
            "  max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX), dir * rcpDirMin)\n"
            ") * inv_vp;       \n"

            "vec3 rgbA = (1.0/2.0) * (                                                             \n"
            "  texture2D(u_tex, v_uv + dir * (1.0/3.0 - 0.5)).xyz +                   \n"
            "  texture2D(u_tex, v_uv + dir * (2.0/3.0 - 0.5)).xyz                     \n"
            ");                                                                                    \n"
            "vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (                                          \n"
            "  texture2D(u_tex, v_uv + dir * (0.0/3.0 - 0.5)).xyz +                   \n"
            "  texture2D(u_tex, v_uv + dir * (3.0/3.0 - 0.5)).xyz                     \n"
            ");                                                                                    \n"
            "float lumaB = dot(rgbB, luma);                                                        \n"

            "if((lumaB < lumaMin) || (lumaB > lumaMax)){                                           \n"
            "    gl_FragColor.xyz=rgbA;                                                            \n"
            "} else {                                                                              \n"
            "    gl_FragColor.xyz=rgbB;                                                            \n"
            "}                                                                                     \n"
            "gl_FragColor.a = 1.0;                                                                 \n"
          "}                                                                                       \n"
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
    glGenBuffers(1, &jeux.gl.fullscreen_vtx);
    glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.fullscreen_vtx);
    GLfloat vertices[] = {
      -1.0f,  3.0f, 0.0f,
      -1.0f, -1.0f, 0.0f,
       3.0f, -1.0f, 0.0f
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    jeux.gl.shader.pp_u_win_size = glGetUniformLocation(jeux.gl.shader.pp, "u_win_size");

    GLint attr_pos = glGetAttribLocation(jeux.gl.shader.pp, "a_pos");
    glEnableVertexAttribArray(attr_pos);
    glVertexAttribPointer(attr_pos, 3, GL_FLOAT, GL_FALSE, 0, 0);
  }

  /* dynamic text buffer */
  {
    /* create vbo, fill it */
    glGenBuffers(1, &jeux.gl.text_vtx);
    glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.text_vtx);
    GLfloat vertices[] = {
      -1.0f,  3.0f, 0.0f, 2.0f,
      -1.0f, -1.0f, 0.0f, 0.0f,
       3.0f, -1.0f, 2.0f, 0.0f
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    jeux.gl.shader.text_u_buffer  = glGetUniformLocation(jeux.gl.shader.text, "u_buffer" );
    jeux.gl.shader.text_u_gamma   = glGetUniformLocation(jeux.gl.shader.text, "u_gamma"  );
    jeux.gl.shader.text_u_texsize = glGetUniformLocation(jeux.gl.shader.text, "u_texsize");
    jeux.gl.shader.text_a_pos     = glGetAttribLocation( jeux.gl.shader.text, "a_pos"    );
  }

  /* dynamic geometry buffer */
  {
    glGenBuffers(1, &jeux.gl.geo_vtx);
    glBindBuffer(GL_ARRAY_BUFFER, jeux.gl.geo_vtx);
    GLfloat vertices[] = {
         0.0f,  0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    /* shader data layout */
    GLint attr_pos = glGetAttribLocation(jeux.gl.shader.geo, "a_pos");
    glEnableVertexAttribArray(attr_pos);
    glVertexAttribPointer(attr_pos, 3, GL_FLOAT, GL_FALSE, 0, 0);
  }

  gl_resize();

  /* create texture - writes to jeux.gl.tex */
  {
     glGenTextures(1, &jeux.gl.text_tex);
     glBindTexture(GL_TEXTURE_2D, jeux.gl.text_tex);
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
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
           y = font_TEX_SIZE_Y - 1 - y; /* flip! */
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

  return SDL_APP_CONTINUE;
}

/* recreates jeux.gl.screen resources to match new jeux.window_size */
static void gl_resize(void) {
  /* passing in zero is ignored here, so this doesn't throw an error if screen has never inited */
  glDeleteFramebuffers(1, &jeux.gl.screen.pp_fb);
  glDeleteTextures(1, &jeux.gl.screen.pp_tex);

  /* create postprocessing framebuffer - writes to jeux.gl.screen.pp_tex, jeux.gl.screen.pp_fb */
  {
    glGenFramebuffers(1, &jeux.gl.screen.pp_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, jeux.gl.screen.pp_fb);

    glGenTextures(1, &jeux.gl.screen.pp_tex);
    glBindTexture(GL_TEXTURE_2D, jeux.gl.screen.pp_tex);
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
      /* GLsizei width          */ jeux.window_size_x*jeux.gl.fb_scale,
      /* GLsizei height         */ jeux.window_size_y*jeux.gl.fb_scale,
      /* GLint   border         */ 0,
      /* GLenum  format         */ GL_RGBA,
      /* GLenum  type           */ GL_UNSIGNED_BYTE,
      /* const void *data       */ 0
    );

    glFramebufferTexture2D(
      GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0,
      GL_TEXTURE_2D,
      jeux.gl.screen.pp_tex,
      0
    );

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      SDL_Log("couldn't make render buffer: n%xn", status);
    }
  }
}
