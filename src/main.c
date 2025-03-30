// vim: sw=2 ts=2 expandtab smartindent

#include <stdlib.h>

#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include "gl_include/gles3.h"

#define jx_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

static struct {
  struct {
    SDL_Window    *window;
    SDL_GLContext  gl_ctx;
  } sdl;

  size_t window_size_x, window_size_y;

  struct {
    struct { GLuint pp, geo; } shader;

    // GLuint tex;

    /* resources inside here need to be recreated
     * when the application window is resized. */
    struct {
      /* postprocessing framebuffer (anti-aliasing and other fx) */
      GLuint pp_tex, pp_fb;
    } screen;
  } gl;
} jeux = {
  .window_size_x = 640,
  .window_size_y = 480
};

static void gl_init(void);
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
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
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

  gl_init();

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {

  /* render */
  {
    glViewport(0, 0, jeux.window_size_x, jeux.window_size_y);

    {
      glBindFramebuffer(GL_FRAMEBUFFER, jeux.gl.screen.pp_fb);

      glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      // glBindTexture(GL_TEXTURE_2D, jeux.gl.tex);
      glUseProgram(jeux.gl.shader.geo);
      glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      glUseProgram(jeux.gl.shader.pp);
      glBindTexture(GL_TEXTURE_2D, jeux.gl.screen.pp_tex);
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
    jeux.window_size_x = event->window.data1;
    jeux.window_size_y = event->window.data2;
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
static void gl_init(void) {
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
          "void main()                                \n"
          "{                                          \n"
          "    gl_Position = vec4(a_pos.xyz, 1.0);    \n"
          "    v_color = gl_Position.xyz + vec3(0.5); \n"
          "}                                          \n"
        ,
        .fs =
          "precision mediump float;               \n"
          "varying vec3 v_color;                  \n"
          "uniform sampler2D u_tex;               \n"
          "void main() {                          \n"
          "    gl_FragColor = vec4(v_color, 1.0); \n"
          "}                                      \n"
      },
      {
        .dst = &jeux.gl.shader.pp,
        .debug_name = "pp",
        .vs =
          "attribute vec4 a_pos;                         \n"
          "varying vec2 v_uv;                            \n"
          "void main() {                                 \n"
          "    gl_Position = vec4(a_pos.xyz, 1.0);       \n"
          "    v_uv = gl_Position.xy*0.5 + vec2(0.5);    \n"
          "}                                             \n"
        ,
        .fs =
          "precision mediump float;             \n"
          "varying vec2 v_uv;                   \n"
          "uniform sampler2D u_tex;             \n"
          "void main() {                        \n"
          "    vec4 p = texture2D(u_tex, v_uv); \n"
          "    gl_FragColor = p;                \n"
          "}                                    \n"
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
            SDL_Log("%s %s compilation failed: %s\n", shaders[shader_index].debug_name, s_name, log);
            free(log);
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

  /* geometry */
  {
    /* create vbo, fill it */
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    GLfloat vertices[] = 
    {
      -1.0f,  3.0f, 0.0f,
      -1.0f, -1.0f, 0.0f,
       3.0f, -1.0f, 0.0f
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    /* shader data layout */
    GLint attr_pos = glGetAttribLocation(jeux.gl.shader.geo, "a_pos");
    glEnableVertexAttribArray(attr_pos);
    glVertexAttribPointer(attr_pos, 3, GL_FLOAT, GL_FALSE, 0, 0);
  }

  gl_resize();

  /* create texture - writes to jeux.gl.tex */
  // {
  //    glGenTextures(1, &jeux.gl.tex);
  //    glBindTexture(GL_TEXTURE_2D, jeux.gl.tex);
  //    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  //    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  //    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  //    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  //    uint8_t data[4 * 4 * 4] = {0};
  //    {
  //      int i = 0;
  //      for (int x = 0; x < 4; x++)
  //        for (int y = 0; y < 4; y++)
  //          data[i++] = ((x ^ y)%2) ? 0 : 255,
  //          data[i++] = ((x ^ y)%2) ? 0 : 255,
  //          data[i++] = ((x ^ y)%2) ? 0 : 255,
  //          data[i++] = ((x ^ y)%2) ? 0 : 255;
  //    }

  //    glTexImage2D(
  //      /* GLenum  target         */ GL_TEXTURE_2D,
  //      /* GLint   level          */ 0,
  //      /* GLint   internalFormat */ GL_RGBA,
  //      /* GLsizei width          */ 4,
  //      /* GLsizei height         */ 4,
  //      /* GLint   border         */ 0,
  //      /* GLenum  format         */ GL_RGBA,
  //      /* GLenum  type           */ GL_UNSIGNED_BYTE,
  //      /* const void *data       */ data
  //    );
  // }
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
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);                             
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(
      /* GLenum  target         */ GL_TEXTURE_2D,
      /* GLint   level          */ 0,
      /* GLint   internalFormat */ GL_RGBA,
      /* GLsizei width          */ jeux.window_size_x,
      /* GLsizei height         */ jeux.window_size_y,
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
