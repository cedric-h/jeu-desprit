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

  SDL_Window *sdl_window = SDL_CreateWindow("window", 640, 480, SDL_WINDOW_OPENGL);
  if (sdl_window == NULL) {
    SDL_Log("Window init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_GLContext gl = SDL_GL_CreateContext(sdl_window);

  /* shader */
  GLuint shaderProgram = glCreateProgram();
  {
    // Vertex shader
    const GLchar* vertexSource =
      "attribute vec4 position;                      \n"
      "varying vec3 color;                           \n"
      "void main()                                   \n"
      "{                                             \n"
      "    gl_Position = vec4(position.xyz, 1.0);    \n"
      "    color = gl_Position.xyz + vec3(0.5);      \n"
      "}                                             \n";

    // Fragment/pixel shader
    const GLchar* fragmentSource =
      "precision mediump float;                     \n"
      "varying vec3 color;                          \n"
      "void main()                                  \n"
      "{                                            \n"
      "    gl_FragColor = vec4 ( color, 1.0 );      \n"
      "}                                            \n";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glUseProgram(shaderProgram);
  }

  /* geometry */
  {
    // Create vertex buffer object and copy vertex data into it
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    GLfloat vertices[] = 
    {
       0.0f,  0.5f, 0.0f,
      -0.5f, -0.5f, 0.0f,
       0.5f, -0.5f, 0.0f
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Specify the layout of the shader vertex data (positions only, 3 floats)
    GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
  }

  glViewport(0, 0, 640, 480);

  while (true) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0)
      if (event.type == SDL_EVENT_QUIT)
        goto quit;

    glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw the vertex buffer
    glDrawArrays(GL_TRIANGLES, 0, 3);

    SDL_GL_SwapWindow(sdl_window);
  }
quit:

  SDL_GL_DestroyContext(gl);
  SDL_DestroyWindow(sdl_window);
  return 0;
}
