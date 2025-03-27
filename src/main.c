#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#define SDL_USE_BUILTIN_OPENGL_DEFINITIONS
#include <SDL3/SDL_opengles2.h>

int main(int argc, char** argv) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL init failed: %s\n", SDL_GetError());
        return 1;
    }

    // The OpenGL ES renderer backend in TGUI requires at least OpenGL ES 2.0
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_Window *sdl_window = SDL_CreateWindow("window", 640, 480, SDL_WINDOW_OPENGL);
    if (sdl_window == NULL) {
        SDL_Log("Window init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GLContext gl = SDL_GL_CreateContext(sdl_window);

    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0)
            if (event.type == SDL_EVENT_QUIT)
                goto quit;

        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        SDL_GL_SwapWindow(sdl_window);
    }
    quit:

    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(sdl_window);
    return 0;
}
