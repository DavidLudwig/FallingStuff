
#include "FSTUFF.h"
#include "FSTUFF_OpenGLES.h"
#include <cstdio>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#if __EMSCRIPTEN__
#include <emscripten.h>
#endif


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
        return vs;
    }
};

FSTUFF_SDLGLRenderer * renderer = nullptr;
FSTUFF_Simulation * sim = nullptr;

void tick() {
    if (SDL_GL_MakeCurrent(renderer->window, renderer->gl) != 0) {
        FSTUFF_FatalError("SDL_GL_MakeCurrent failed: %s\n", SDL_GetError());
    }
    renderer->BeginFrame();

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                std::exit(0);
                break;
        }
    }

    sim->Update();
    sim->Render();
    SDL_GL_SwapWindow(renderer->window);
}

int main(int, char **) {
    renderer = new FSTUFF_SDLGLRenderer;
    renderer->glVersion = FSTUFF_GLVersion::GLESv2;
    renderer->getProcAddress = SDL_GL_GetProcAddress;
	sim = new FSTUFF_Simulation();
	sim->renderer = renderer;
    renderer->sim = sim;

	SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
	SDL_SetHint(SDL_HINT_VIDEO_WIN_D3DCOMPILER, "none");

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		FSTUFF_Log("SDL_Init failed with error: \"%s\"\n", SDL_GetError());
		return 1;
	}

    switch (renderer->glVersion) {
        case FSTUFF_GLVersion::GLESv2:
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            break;
        case FSTUFF_GLVersion::GLESv3:
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            break;
    }
	
	renderer->window = SDL_CreateWindow(
        "Falling Stuff",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1024, 768,
        SDL_WINDOW_OPENGL);
    if (!renderer->window) {
		FSTUFF_Log("SDL_CreateWindow failed with error: \"%s\"\n", SDL_GetError());
        return 1;
    }

    renderer->gl = SDL_GL_CreateContext(renderer->window);
    if (!renderer->gl) {
		FSTUFF_Log("SDL_GL_CreateContext failed with error: \"%s\"\n", SDL_GetError());
        return 1;
    }

	if (SDL_GL_MakeCurrent(renderer->window, renderer->gl) != 0) {
		FSTUFF_Log("SDL_GL_MakeCurrent failed with error: \"%s\"\n", SDL_GetError());
		return 1;
	}

    renderer->Init();
    sim->Init();

#if __EMSCRIPTEN__
    emscripten_set_main_loop(tick, 0, 1);
#else
    while (true) {
        tick();
    }
#endif

	return 0;
}
