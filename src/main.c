// vim: sw=2 ts=2 expandtab smartindent ft=javascript

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#define SDL_USE_BUILTIN_OPENGL_DEFINITIONS
#include <SDL3/SDL_opengles2.h>

int main(int argc, char** argv) {
  SDL_SetHint(SDL_HINT_APP_NAME, "jeu desprit");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL init failed: %s\n", SDL_GetError());
    return 1;
  }

#ifdef _WIN32
  SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
#endif
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

  size_t window_size_x = 640;
  size_t window_size_y = 480;
  SDL_Window *sdl_window = SDL_CreateWindow(
    "jeu desprit",
    window_size_x,
    window_size_y,
    SDL_WINDOW_OPENGL
  );
  if (sdl_window == NULL) {
    SDL_Log("Window init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_GLContext gl = SDL_GL_CreateContext(sdl_window);

  /* shader */
  GLuint geo_shader = glCreateProgram();
  {
    const GLchar* vs_geo =
      "attribute vec4 a_pos;                         \n"
      "varying vec2 v_uv;                            \n"
      "void main()                                   \n"
      "{                                             \n"
      "    gl_Position = vec4(a_pos.xyz, 1.0);       \n"
      "    v_uv = gl_Position.xy*0.5 + vec2(0.5);    \n"
      "}                                             \n";

    const GLchar* fs_geo =
      "precision mediump float;                            \n"
      "varying vec2 v_uv;                                  \n"
      "uniform sampler2D u_tex;                            \n"
      "void main()                                         \n"
      "{                                                   \n"
      "    vec3 p = texture2D(u_tex, v_uv).rgb;            \n"
      "    gl_FragColor = vec4(p, 1.0);                    \n"
      "}                                                   \n";

    GLuint vs_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs_shader, 1, &vs_geo, NULL);
    glCompileShader(vs_shader);

    GLuint fs_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs_shader, 1, &fs_geo, NULL);
    glCompileShader(fs_shader);

    for (int i = 0; i < 2; i++) {
      GLuint shader = (i == 0) ? vs_shader : fs_shader;
      char    *name = (i == 0) ? "vs"      : "fs";
      GLint compiled;
      glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
      if (compiled != GL_TRUE) {
        GLint log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        if (log_length > 0) {
          char* log = malloc(log_length);
          glGetShaderInfoLog(shader, log_length, &log_length, log);
          SDL_Log("%s compilation failed: %s\n", name, log);
          free(log);
        }
      }
    }

    glAttachShader(geo_shader, vs_shader);
    glAttachShader(geo_shader, fs_shader);
    glLinkProgram(geo_shader);
    glUseProgram(geo_shader);
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
    GLint attr_pos = glGetAttribLocation(geo_shader, "a_pos");
    glEnableVertexAttribArray(attr_pos);
    glVertexAttribPointer(attr_pos, 3, GL_FLOAT, GL_FALSE, 0, 0);
  }

  /* create framebuffer */
  GLuint rb, rb_tex, rb_fb;
  {
    glGenFramebuffers(1, &rb_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, rb_fb);

    glGenTextures(1, &rb_tex);
    glBindTexture(GL_TEXTURE_2D, rb_tex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);                             
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(
      /* GLenum  target         */ GL_TEXTURE_2D,
      /* GLint   level          */ 0,
      /* GLint   internalFormat */ GL_RGB,
      /* GLsizei width          */ window_size_x,
      /* GLsizei height         */ window_size_y,
      /* GLint   border         */ 0,
      /* GLenum  format         */ GL_RGB,
      /* GLenum  type           */ GL_UNSIGNED_BYTE,
      /* const void *data       */ 0
    );

     glFramebufferTexture2D(
       GL_FRAMEBUFFER,
       GL_COLOR_ATTACHMENT0,
       GL_TEXTURE_2D,
       rb_tex,
       0
     );

     glGenRenderbuffers(1, &rb);
     glBindRenderbuffer(GL_RENDERBUFFER, rb);

     glRenderbufferStorage(
       GL_RENDERBUFFER,
       GL_RGB565,
       window_size_x,
       window_size_y
     );
     glFramebufferRenderbuffer(
       GL_FRAMEBUFFER,
       GL_COLOR_ATTACHMENT0,
       GL_RENDERBUFFER,
       rb
     );

     GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
     if (status != GL_FRAMEBUFFER_COMPLETE) {
       SDL_Log("couldn't make render buffer: n%xn", status);
     }
  }

  /* create texture */
  GLuint tex;
  {
     glGenTextures(1, &tex);
     glBindTexture(GL_TEXTURE_2D, tex);
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);                             
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

     uint8_t data[4 * 4 * 4] = {0};
     {
       int i = 0;
       for (int x = 0; x < 4; x++)
         for (int y = 0; y < 4; y++)
           data[i++] = ((x ^ y)%2) ? 0 : 255,
           data[i++] = ((x ^ y)%2) ? 0 : 255,
           data[i++] = ((x ^ y)%2) ? 0 : 255,
           data[i++] = ((x ^ y)%2) ? 0 : 255;
     }

     glTexImage2D(
       /* GLenum  target         */ GL_TEXTURE_2D,
       /* GLint   level          */ 0,
       /* GLint   internalFormat */ GL_RGBA,
       /* GLsizei width          */ 4,
       /* GLsizei height         */ 4,
       /* GLint   border         */ 0,
       /* GLenum  format         */ GL_RGBA,
       /* GLenum  type           */ GL_UNSIGNED_BYTE,
       /* const void *data       */ data
     );
  }

  glViewport(0, 0, window_size_x, window_size_y);

  while (true) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0)
      if (event.type == SDL_EVENT_QUIT)
        goto quit;

    {
      glBindRenderbuffer(GL_RENDERBUFFER, rb);
      glBindFramebuffer(GL_FRAMEBUFFER, rb_fb);

      glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      glBindTexture(GL_TEXTURE_2D, tex);
      glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    {
      glBindRenderbuffer(GL_RENDERBUFFER, 0);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      glBindTexture(GL_TEXTURE_2D, rb_tex);
      glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    SDL_GL_SwapWindow(sdl_window);
  }
quit:

  SDL_GL_DestroyContext(gl);
  SDL_DestroyWindow(sdl_window);
  return 0;
}
