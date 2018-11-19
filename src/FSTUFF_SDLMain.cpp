
#include "FSTUFF.h"
#include "FSTUFF_OpenGLES.h"
#include <SDL.h>
#include <cstdio>


struct FSTUFF_SDLGLRenderer : public FSTUFF_GLESRenderer {
    SDL_Window * window = nullptr;
    SDL_GLContext gl = nullptr;

    FSTUFF_ViewSize GetViewSize() override
    {
        FSTUFF_ViewSize vs;
        SDL_GetWindowSize(window, &vs.widthOS, &vs.heightOS);
        SDL_GL_GetDrawableSize(window, &vs.widthPixels, &vs.heightPixels);
        const float osToMMApproximate = 1.f/8.f;
        vs.widthMM = vs.widthOS * osToMMApproximate;
        vs.heightMM = vs.heightOS * osToMMApproximate;

        // float widthMM;
        // float heightMM;
        // int widthPixels;
        // int heightPixels;
        // int widthOS;
        // int heightOS;

        return vs;
    }
};

int main() {
    FSTUFF_SDLGLRenderer renderer;
    FSTUFF_Simulation sim;
    sim.renderer = &renderer;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    renderer.window = SDL_CreateWindow(
        "Falling Stuff",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1024, 768,
        SDL_WINDOW_OPENGL);
    if (!renderer.window) {
        fprintf(stderr, "SDL_CreateWindow failed with error: \"%s\"\n", SDL_GetError());
        return 1;
    }

    renderer.gl = SDL_GL_CreateContext(renderer.window);
    if (!renderer.gl) {
        fprintf(stderr, "SDL_GL_CreateContext failed with error: \"%s\"\n", SDL_GetError());
        return 1;
    }

    renderer.Init();
    sim.Init();

    while (true) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            SDL_GL_MakeCurrent(renderer.window, renderer.gl);
            renderer.BeginFrame();

            switch (e.type) {
                case SDL_QUIT:
                    std::exit(0);
                    break;
            }

            sim.Update();
            sim.Render();

            // glClearColor(0, 1, 0, 1);
            // glClear(GL_COLOR_BUFFER_BIT);

            SDL_GL_SwapWindow(renderer.window);
        }
    }

    return 0;
}