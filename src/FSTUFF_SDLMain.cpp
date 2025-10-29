
#include "FSTUFF.h"
#include "FSTUFF_OpenGL.h"
#include <cstdio>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#if __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

struct FSTUFF_SDLGLRenderer : public FSTUFF_GLESRenderer {
    SDL_Window * window = nullptr;
    SDL_GLContext gl = nullptr;

    FSTUFF_ViewSize GetViewSize()
    {
        FSTUFF_ViewSize vs;

        #if __EMSCRIPTEN__

        emscripten_get_canvas_element_size("#canvas", &vs.widthPixels, &vs.heightPixels);
        emscripten_get_element_css_size("#canvas", &vs.widthOS, &vs.heightOS);

        const double dpr = emscripten_get_device_pixel_ratio();
        vs.widthOS *= dpr;
        vs.heightOS *= dpr;

        #else

        SDL_GetWindowSize(window, &vs.widthOS, &vs.heightOS);
        SDL_GL_GetDrawableSize(window, &vs.widthPixels, &vs.heightPixels);

        #endif

        const float osToMMApproximate = 1.f / 4.f;
        vs.widthMM = vs.widthOS * osToMMApproximate;
        vs.heightMM = vs.heightOS * osToMMApproximate;

        return vs;
    }

    FSTUFF_CursorInfo GetCursorInfo() override {
        int x = 0.f;
        int y = 0.f;
        const Uint32 sdlButtons = SDL_GetMouseState(&x, &y);
        FSTUFF_CursorInfo cur;
        cur.xOS = (int)x;
        cur.yOS = (int)y;
        cur.pressed = (sdlButtons != 0);
        return cur;
    }
};

FSTUFF_SDLGLRenderer * renderer = nullptr;
FSTUFF_Simulation * sim = nullptr;
static bool didHideLoadingUI = false;

#if __EMSCRIPTEN__

struct CanvasState {
    double width = 0;
    double height = 0;
    bool needsResize = false;
};

CanvasState canvasState;

EM_BOOL onRender(double time, void *userData) {
    // FSTUFF_Log("onRender: time=%f\n", time);

    if (canvasState.needsResize)
    {
        emscripten_set_canvas_element_size("#canvas", 
                                           canvasState.width, 
                                           canvasState.height);
        canvasState.needsResize = false;
    }

    // Your drawing code here
    
    return EM_TRUE;
}

EM_BOOL onWindowResize(int eventType, const EmscriptenUiEvent *e, void *userData) {
    FSTUFF_Log("onWindowResize: eventType=%d\n", eventType);

    double width, height;
    emscripten_get_element_css_size("#canvas", &width, &height);

    if (renderer && sim) {
        FSTUFF_ViewSize viewSize = renderer->GetViewSize();
        sim->ViewChanged(viewSize);
    }
    
    if (canvasState.width != width || canvasState.height != height) {
        canvasState.width = width;
        canvasState.height = height;
        canvasState.needsResize = true;
        emscripten_request_animation_frame(onRender, nullptr);
    }
    
    return EM_TRUE;
}

// This gets called when everything is ready
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void start_application() {
        FSTUFF_Log("start_application\n");

        // Initialize canvas size
        emscripten_get_element_css_size("#canvas", 
                                        &canvasState.width, 
                                        &canvasState.height);
        canvasState.needsResize = true;
        
        // Set up event listeners
        emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 
                                       nullptr, false, onWindowResize);
        
        // Start render loop
        emscripten_request_animation_frame(onRender, nullptr);
    }
}

#endif


void process_event(SDL_Event & e) {
    switch (e.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            const char * keyName = SDL_GetKeyName(e.key.keysym.sym);
            const FSTUFF_EventType eventType = \
                (e.type == SDL_KEYDOWN) ?
                    FSTUFF_EventKeyDown :
                    FSTUFF_EventKeyUp;
            char32_t utf32Char = 0;
            switch (e.key.keysym.sym) {
                case SDLK_DOWN:
                    utf32Char = U'↓';
                    break;
                case SDLK_LEFT:
                    utf32Char = U'←';
                    break;
                case SDLK_RIGHT:
                    utf32Char = U'→';
                    break;
                case SDLK_UP:
                    utf32Char = U'↑';
                    break;
                default:
                    break;
            }
            FSTUFF_Event fstuffEvent;
            if (utf32Char != 0) {
                fstuffEvent = FSTUFF_Event::NewKeyEvent(eventType, utf32Char);
            } else {
                fstuffEvent = FSTUFF_Event::NewKeyEvent(eventType, keyName);
            }
            sim->EventReceived(&fstuffEvent);
        } break;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEMOTION: {
            const FSTUFF_CursorInfo cursorInfo = renderer->GetCursorInfo();
            sim->UpdateCursorInfo(cursorInfo);
        } break;

        case SDL_QUIT:
            std::exit(0);
            break;
        case SDL_WINDOWEVENT:
            switch (e.window.event) {
                #if !__EMSCRIPTEN__

                case SDL_WINDOWEVENT_SIZE_CHANGED: {
                    const FSTUFF_ViewSize viewSize = renderer->GetViewSize();
                    sim->ViewChanged(viewSize);
                } break;

                #else

                // case SDL_WINDOWEVENT_RESIZED: {
                //     if (renderer && sim) {
                //         const FSTUFF_ViewSize viewSize = renderer->GetViewSize();
                //         sim->ViewChanged(viewSize);
                //     }
                //     break;
                // }

                #endif
            }
            break;
    }
}

int FSTUFF_SDL_EventWatcher(void * userdata, SDL_Event * event) {
    if (!event) {
        return 1;
    }

#if __EMSCRIPTEN__
    process_event(*event);
#endif

    return 1;
}

void tick() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        process_event(e);
    }

    if (SDL_GL_MakeCurrent(renderer->window, renderer->gl) != 0) {
        FSTUFF_FatalError("SDL_GL_MakeCurrent failed: %s\n", SDL_GetError());
    }
    sim->Update();
    sim->Render();
    ImDrawData * imGuiDrawData = ImGui::GetDrawData();
    renderer->RenderImGuiDrawData(imGuiDrawData);
    if (!didHideLoadingUI) {
#if __EMSCRIPTEN__
        EM_ASM(
            var loading = document.getElementById("loading");
            if (loading) {
                loading.style.display = "none";
            }
        );
#endif
        didHideLoadingUI = true;
    }
    SDL_GL_SwapWindow(renderer->window);
}

int main(int, char **) {
    renderer = new FSTUFF_SDLGLRenderer;
#if TARGET_OS_OSX
    renderer->glVersion = FSTUFF_GLVersion::GLCorev3;
#else
    renderer->glVersion = FSTUFF_GLVersion::GLESv2;
#endif
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
        case FSTUFF_GLVersion::GLCorev3:
#if __APPLE__
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
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
	
	renderer->window = SDL_CreateWindow(
        "Falling Stuff",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1024, 768,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
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

    {
        const FSTUFF_ViewSize viewSize = renderer->GetViewSize();
        sim->ViewChanged(viewSize);
    }
    renderer->Init();
    sim->Init();

#if __EMSCRIPTEN__
    start_application();
    emscripten_set_main_loop(tick, 0, 1);
#else
    while (true) {
        tick();
    }
#endif

	return 0;
}
