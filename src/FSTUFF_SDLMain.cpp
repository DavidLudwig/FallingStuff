
#include "FSTUFF.h"
#include "FSTUFF_OpenGL.h"
#include <cstdio>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#if __EMSCRIPTEN__
#include <emscripten.h>
#endif

SDL_Window * window = nullptr;
SDL_GLContext glContext = nullptr;
FSTUFF_GLESRenderer * renderer = nullptr;
FSTUFF_Simulation<FSTUFF_GLESRenderer> * sim = nullptr;

FSTUFF_ViewSize FSTUFF_SDLGL_GetViewSize()
{
    FSTUFF_ViewSize vs;
    SDL_GetWindowSize(window, &vs.widthOS, &vs.heightOS);
    SDL_GL_GetDrawableSize(window, &vs.widthPixels, &vs.heightPixels);
    const float osToMMApproximate = 1.f/4.f;
    vs.widthMM = vs.widthOS * osToMMApproximate;
    vs.heightMM = vs.heightOS * osToMMApproximate;
    return vs;
}


void tick() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                std::exit(0);
                break;
            case SDL_WINDOWEVENT:
                switch (e.window.event) {
                    case SDL_WINDOWEVENT_SIZE_CHANGED: {
                        const FSTUFF_ViewSize viewSize = FSTUFF_SDLGL_GetViewSize();
                        sim->ViewChanged(viewSize);
                    } break;
                }
                break;
        }
    }

    if (SDL_GL_MakeCurrent(window, glContext) != 0) {
        FSTUFF_FatalError("SDL_GL_MakeCurrent failed: %s\n", SDL_GetError());
    }
    renderer->BeginFrame();
    sim->Update();
    sim->Render();
    SDL_GL_SwapWindow(window);
}

int main(int, char **) {
    renderer = new FSTUFF_GLESRenderer;
#if TARGET_OS_OSX
    renderer->glVersion = FSTUFF_GLVersion::GLCorev3;
#else
    renderer->glVersion = FSTUFF_GLVersion::GLESv2;
#endif
    renderer->getProcAddress = SDL_GL_GetProcAddress;
    sim = new FSTUFF_Simulation<FSTUFF_GLESRenderer>();
    sim->renderer = renderer;
    renderer->sim = sim;

	SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
	SDL_SetHint(SDL_HINT_VIDEO_WIN_D3DCOMPILER, "none");

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		FSTUFF_Log("SDL_Init failed with error: \"%s\"\n", SDL_GetError());
		return 1;
	}

    switch (renderer->glVersion) {
        case FSTUFF_GLVersion::GLCorev3:
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            break;
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
	
	window = SDL_CreateWindow(
        "Falling Stuff",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1024, 768,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
		FSTUFF_Log("SDL_CreateWindow failed with error: \"%s\"\n", SDL_GetError());
        return 1;
    }

    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
		FSTUFF_Log("SDL_GL_CreateContext failed with error: \"%s\"\n", SDL_GetError());
        return 1;
    }

	if (SDL_GL_MakeCurrent(window, glContext) != 0) {
		FSTUFF_Log("SDL_GL_MakeCurrent failed with error: \"%s\"\n", SDL_GetError());
		return 1;
	}

    {
        const FSTUFF_ViewSize viewSize = FSTUFF_SDLGL_GetViewSize();
        sim->ViewChanged(viewSize);
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
